"""Tier 1: transparent transfer (SPEC §22) — output invariance and masking.

`split=1` renders the overlay as one full-surface sprite pushed once with the
transparent color masked out, which is exactly what hand-written LovyanGFX code
would do. Every other split count, both split axes and double-buffering must be
pixel-identical to it — the same load-bearing assertion as parity/diff, applied
to the masked transfer.
"""

import re
from pathlib import Path

from PIL import Image, ImageChops

SKETCH_DIR = Path(__file__).parent

# RGB888 of TFT_TRANSPARENT (RGB565 0x0120): r5=0, g6=9, b5=0 -> g8=0x24.
TFT_TRANSPARENT_RGB888 = 0x002400

STAT_RE = re.compile(rb"STAT (\w+) pushed=(\d+) total=(\d+) tiles=(-?\d+) tileH=(-?\d+)")

# Configurations that must all match the split=1 reference.
VARIANTS = ("inv3", "inv7", "inv13", "col3", "col7", "db3", "coldb7")


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
        "tiles": int(g[3]),
        "tileH": int(g[4]),
    }


def _assert_identical(out, ref_tag, tag):
    ref = Image.open(out / f"t_{ref_tag}.png").convert("RGB")
    img = Image.open(out / f"t_{tag}.png").convert("RGB")
    assert img.size == ref.size, f"{tag}: size mismatch"
    diff = ImageChops.difference(ref, img)
    if diff.getbbox() is not None:
        diff.point(lambda v: 255 if v else 0).save(out / f"t_{tag}_diff.png")
        raise AssertionError(
            f"t_{tag}: differs from the t_{ref_tag} reference, bbox={diff.getbbox()}"
        )


def test_transparent(dut):
    dut.expect("TEST start transparent", timeout=15)
    W, H = _m(dut, r"PANEL (\d+)x(\d+)", 5)
    dlg_x, dlg_y, dlg_w, dlg_h, dlg_r = _m(
        dut, r"DLG x=(\d+) y=(\d+) w=(\d+) h=(\d+) r=(\d+)", 5
    )
    probe_x, probe_y = _m(dut, r"PROBE x=(\d+) y=(\d+)", 5)

    # setTransparentColor() keeps the color's meaning, which depends on the C++
    # type of the value: TFT_* / int is RGB565, uint32_t is RGB888 (SPEC §22.4).
    (defcolor,) = _m(dut, r"DEFCOLOR (\d+)", 5)
    (set565,) = _m(dut, r"SETCOLOR565 (\d+)", 5)
    (set888,) = _m(dut, r"SETCOLOR888 (\d+)", 5)
    assert defcolor == TFT_TRANSPARENT_RGB888, (
        f"default transparent color should be TFT_TRANSPARENT as RGB888 "
        f"(0x{TFT_TRANSPARENT_RGB888:06X}), got 0x{defcolor:06X}"
    )
    assert set565 == TFT_TRANSPARENT_RGB888, (
        f"setTransparentColor(TFT_TRANSPARENT) must be read as RGB565, "
        f"got 0x{set565:06X}"
    )
    assert set888 == TFT_TRANSPARENT_RGB888, (
        f"setTransparentColor(uint32_t) must be read as RGB888, got 0x{set888:06X}"
    )

    stats = {}
    for tag in ("inv1",) + VARIANTS:
        stats[tag] = _stat(dut, tag)
        dut.expect(f"CASE {tag} done", timeout=10)
    dut.expect("CASE opaque done", timeout=15)
    dut.expect("CASE color done", timeout=15)

    diff_first = _stat(dut, "diff_first")
    diff_same = _stat(dut, "diff_same")
    diff_recolor = _stat(dut, "diff_recolor")
    diff_recolor2 = _stat(dut, "diff_recolor2")
    diff_opaque = _stat(dut, "diff_opaque")
    dut.expect("CASE diff done", timeout=15)

    bgeq_transp = _stat(dut, "bgeq_transp")
    bgeq_opaque = _stat(dut, "bgeq_opaque")
    bgeq_back = _stat(dut, "bgeq_back")
    dut.expect("CASE bgeq done", timeout=15)

    sp1 = _stat(dut, "sp1")
    sp4 = _stat(dut, "sp4")
    dut.expect("CASE sprite done", timeout=15)

    (depth,) = _m(dut, r"DEPTH (\d+)", 5)
    for tag in ("d24_1", "d24_7"):
        stats[tag] = _stat(dut, tag)
        dut.expect(f"CASE {tag} done", timeout=10)

    dut.expect("TEST done", timeout=15)

    out = SKETCH_DIR / "output"

    # --- the core invariant -------------------------------------------------
    for tag in VARIANTS:
        _assert_identical(out, "inv1", tag)
    _assert_identical(out, "d24_1", "d24_7")

    # --- masking actually happened -----------------------------------------
    # The reference image must still show the base layer outside the dialog, and
    # the dialog's rounded corner must NOT have been painted over.
    ref = Image.open(out / "t_inv1.png").convert("RGB")
    base_only = Image.open(out / "t_cover_opaque.png")  # sanity: distinct images
    assert base_only.size == ref.size
    outside = ref.getpixel((probe_x, probe_y))
    corner = ref.getpixel((dlg_x + 1, dlg_y + 1))  # inside the bbox, outside the round
    inside = ref.getpixel((dlg_x + dlg_w // 2, dlg_y + dlg_h // 2))
    assert corner == outside, (
        f"the rounded corner at {(dlg_x + 1, dlg_y + 1)} was transferred "
        f"({corner}) instead of showing the base layer ({outside})"
    )
    assert inside != outside, "the dialog body was not drawn at all"

    # --- a fully covering scene is unaffected by masking -------------------
    _assert_identical(out, "cover_opaque", "cover_transp")

    # --- the transparent color's type does not change which pixels are masked
    _assert_identical(out, "color_0", "color_1")
    # In that scene everything but one white rect is the transparent color, so
    # the base layer must survive everywhere else — including under the RGB888
    # spelling of the same color.
    color = Image.open(out / "t_color_0.png").convert("RGB")
    assert color.getpixel((probe_x, probe_y)) == outside, (
        "an RGB565-typed transparent color did not mask the RGB565 fill"
    )
    assert color.getpixel((20, 20)) == ref.getpixel((20, 20)), (
        "the RGB888 spelling of the transparent color was transferred"
    )
    assert color.getpixel((dlg_x + 15, dlg_y + 15)) != outside, (
        "the marker rect (the only opaque part) never reached the panel"
    )

    # --- diff transfer on top of masked transfer ---------------------------
    assert diff_first["pushed"] == diff_first["total"], (
        "the first overlay render must push every tile"
    )
    assert diff_same["pushed"] == 0, (
        f"an unchanged overlay must transfer nothing, got {diff_same['pushed']} px"
    )
    assert diff_recolor["pushed"] == diff_recolor["total"], (
        "changing the transparent color must invalidate (§22.5)"
    )
    assert diff_recolor2["pushed"] == diff_recolor2["total"], (
        "changing the transparent color back must invalidate too"
    )
    assert diff_opaque["pushed"] == diff_opaque["total"], (
        "switching from renderTransparent() to render() must invalidate (§22.5)"
    )
    # ...and the opaque re-render really did wipe the base layer around the dialog.
    opaque = Image.open(out / "t_diff_opaque.png").convert("RGB")
    assert opaque.getpixel((probe_x, probe_y)) != outside, (
        "render() after renderTransparent() should have painted the whole surface"
    )

    # --- the switch the tile hash cannot see -------------------------------
    # Auto-clear color == transparent color, so both renders build byte-identical
    # tiles. Skipping the second would leave the base layer showing through where
    # the opaque render must have painted over it.
    assert bgeq_transp["pushed"] == bgeq_transp["total"], "bgeq: first render is full"
    assert bgeq_opaque["pushed"] == bgeq_opaque["total"], (
        "bgeq: Transparent -> Opaque must invalidate even though the tile bytes "
        "are unchanged (§22.5)"
    )
    assert bgeq_back["pushed"] == bgeq_back["total"], (
        "bgeq: Opaque -> Transparent must invalidate as well"
    )
    bg_t = Image.open(out / "t_bgeq_transp.png").convert("RGB")
    bg_o = Image.open(out / "t_bgeq_opaque.png").convert("RGB")
    assert bg_t.getpixel((probe_x, probe_y)) == outside, (
        "bgeq: the transparent render should have kept the base layer"
    )
    assert bg_o.getpixel((probe_x, probe_y)) != outside, (
        "bgeq: the opaque render should have painted over the base layer"
    )

    # --- LGFXVirtualSprite -------------------------------------------------
    _assert_identical(out, "sp1", "sp4")
    assert sp1["total"] == sp4["total"] == 120 * 60, "sprite surface accounting"
    sp = Image.open(out / "t_sp1.png").convert("RGB")
    # Outside the sprite rect the base layer is untouched; inside, the circle's
    # corner gap still shows it (that is what transparency buys).
    assert sp.getpixel((probe_x, probe_y)) == outside, "the sprite leaked outside"
    assert sp.getpixel((dlg_x + 3, dlg_y + 3)) == ref.getpixel((dlg_x + 3, dlg_y + 3)), (
        "the corner gap between the circle and the sprite edge was overwritten"
    )

    print(
        f"panel={W}x{H} 24bpp-case-depth={depth} "
        f"tiles(inv7)={stats['inv7']['tiles']} dlg_r={dlg_r}"
    )
