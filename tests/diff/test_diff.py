"""Tier 1: diff transfer (SPEC §21) — output invariance and skip accounting.

The load-bearing assertion is the same as parity's: enabling diff transfer must
not change a single output pixel. The counters then prove that transfers were
actually skipped (otherwise the invariance check would pass trivially).
"""

import re
from pathlib import Path

from PIL import Image, ImageChops

SKETCH_DIR = Path(__file__).parent
LANES_BYTES = 8  # two 32-bit hash lanes per tile (SPEC §21.4)
STAT_RE = re.compile(
    rb"STAT (\w+) pushed=(\d+) total=(\d+) mem=(\d+) tiles=(-?\d+) tileH=(-?\d+)"
)


def _m(dut, pattern, timeout=10):
    match = dut.expect(re.compile(pattern.encode()), timeout=timeout)
    return [int(g.decode()) for g in match.groups()]


def _stat(dut, name, timeout=20):
    g = dut.expect(STAT_RE, timeout=timeout).groups()
    got = g[0].decode()
    assert got == name, f"expected STAT {name}, got {got}"
    return {
        "pushed": int(g[1]),
        "total": int(g[2]),
        "mem": int(g[3]),
        "tiles": int(g[4]),
        "tileH": int(g[5]),
    }


def _expected_pushed(W, H, tileH, y0, y1):
    """Surface pixels of the tiles intersecting rows [y0, y1)."""
    px = 0
    for top in range(0, H, tileH):
        bottom = min(top + tileH, H)
        if top < y1 and y0 < bottom:
            px += W * (bottom - top)
    return px


def _assert_pixel_identical(out, tag):
    ref = Image.open(out / f"d_{tag}_off.png").convert("RGB")
    img = Image.open(out / f"d_{tag}_tile.png").convert("RGB")
    assert img.size == ref.size, f"{tag}: size mismatch"
    diff = ImageChops.difference(ref, img)
    if diff.getbbox() is not None:
        diff.point(lambda v: 255 if v else 0).save(out / f"d_{tag}_diff.png")
        raise AssertionError(
            f"d_{tag}: diff transfer changed the output, bbox={diff.getbbox()}"
        )


def _check_invariance_stats(name, first, second, W, H, bar_y, bar_h):
    # First render after allocation: nothing is known about the panel, so
    # everything is transferred.
    assert first["pushed"] == first["total"] == W * H, (
        f"{name}: first render must transfer the whole surface"
    )
    assert first["mem"] == first["tiles"] * LANES_BYTES, (
        f"{name}: hash table should be {LANES_BYTES} bytes per tile"
    )
    # Second render: only the tiles the bar touches.
    expected = _expected_pushed(W, H, first["tileH"], bar_y, bar_y + bar_h)
    assert second["pushed"] == expected, (
        f"{name}: expected {expected} px transferred, got {second['pushed']}"
    )
    if first["tiles"] > 1:
        assert second["pushed"] < second["total"], (
            f"{name}: nothing was skipped, so invariance is not being exercised"
        )


def test_diff(dut):
    dut.expect("TEST start diff", timeout=15)
    W, H = _m(dut, r"PANEL (\d+)x(\d+)", 5)
    bar_y, bar_h = _m(dut, r"BAR y=(\d+) h=(\d+)", 5)
    foreign_x, foreign_y = _m(dut, r"FOREIGN x=(\d+) y=(\d+)", 5)

    stats = {}
    for tag in ("inv7", "inv1"):
        first = _stat(dut, f"{tag}_first")
        second = _stat(dut, f"{tag}_second")
        dut.expect(f"CASE {tag} done", timeout=10)
        _check_invariance_stats(tag, first, second, W, H, bar_y, bar_h)
        stats[tag] = (first, second)

    # 24bpp: rows are not a multiple of 4 bytes, exercising the hash tail path.
    (depth,) = _m(dut, r"DEPTH (\d+)", 5)
    first = _stat(dut, "inv24_first")
    second = _stat(dut, "inv24_second")
    dut.expect("CASE inv24 done", timeout=10)
    _check_invariance_stats("inv24", first, second, W, H, bar_y, bar_h)

    # same content twice -> the second render transfers nothing at all.
    first = _stat(dut, "same_first")
    second = _stat(dut, "same_second")
    dut.expect("CASE same done", timeout=10)
    assert first["pushed"] == first["total"], "same: first render should be full"
    assert second["pushed"] == 0, (
        f"same: an unchanged frame must transfer nothing, got {second['pushed']} px"
    )

    # Foreign drawing on the panel is undetectable: it survives until invalidate().
    stale = _stat(dut, "stale")
    healed = _stat(dut, "healed")
    dut.expect("CASE stale done", timeout=10)
    assert stale["pushed"] == 0, "stale: the scene did not change, so nothing to send"
    assert healed["pushed"] == healed["total"], (
        "healed: invalidate() must force a full transfer"
    )

    # LGFXVirtualSprite: a repeat is skipped, a position change invalidates.
    sp_first = _stat(dut, "sp_first")
    sp_same = _stat(dut, "sp_same")
    sp_moved = _stat(dut, "sp_moved")
    dut.expect("CASE sprite done", timeout=10)
    assert sp_first["pushed"] == sp_first["total"], "sp: first render should be full"
    assert sp_same["pushed"] == 0, "sp: an unchanged sprite must transfer nothing"
    assert sp_moved["pushed"] == sp_moved["total"], (
        "sp: a position change must invalidate (the backdrop differs)"
    )

    # Off is the default and allocates nothing.
    off = _stat(dut, "modeoff")
    dut.expect("CASE modeoff done", timeout=10)
    assert off["mem"] == 0, "Off must not allocate a hash table"
    assert off["pushed"] == off["total"], "Off must transfer every tile"

    dut.expect("TEST done", timeout=10)

    out = SKETCH_DIR / "output"

    # The core invariant: identical output with diffing off vs on.
    for tag in ("inv7", "inv1", "inv24"):
        _assert_pixel_identical(out, tag)

    # The stale/healed pair, as pixels: the foreign rect is still there while the
    # hash believes the panel is up to date, and gone after invalidate().
    stale_img = Image.open(out / "d_stale.png").convert("RGB")
    healed_img = Image.open(out / "d_healed.png").convert("RGB")
    probe = (foreign_x + 5, foreign_y + 5)
    background = (6, 6)  # inside the frame outline: plain scene background
    assert stale_img.getpixel(probe) != stale_img.getpixel(background), (
        "stale: the foreign rect should still be visible (nothing was transferred)"
    )
    assert healed_img.getpixel(probe) == healed_img.getpixel(background), (
        "healed: invalidate() should have repainted over the foreign rect"
    )

    print(f"panel={W}x{H} 24bpp-case-depth={depth} inv7={stats['inv7'][1]['pushed']}px")
