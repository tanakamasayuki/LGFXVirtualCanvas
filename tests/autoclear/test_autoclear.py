"""Tier 1 T1-8: auto-clear makes undrawn pixels deterministic & split-invariant."""

from pathlib import Path

from PIL import Image, ImageChops

SKETCH_DIR = Path(__file__).parent
CASES = ["black", "navy", "off"]


def _open(out, case, split):
    return Image.open(out / f"ac_{case}_{split}.png").convert("RGB")


def test_autoclear(dut):
    dut.expect("TEST start autoclear", timeout=15)
    dut.expect("PANEL", timeout=5)
    for case in CASES:
        dut.expect(f"CASE {case} done", timeout=10)
    dut.expect("TEST done", timeout=5)

    out = SKETCH_DIR / "output"

    # Each case: split=1 vs split=3 must be pixel-identical.
    for case in CASES:
        ref = _open(out, case, 1)
        img = _open(out, case, 3)
        assert img.size == ref.size, f"{case}: size mismatch"
        diff = ImageChops.difference(ref, img)
        if diff.getbbox() is not None:
            ref.save(out / f"ac_{case}_full.png")
            img.save(out / f"ac_{case}_virtual.png")
            diff.point(lambda v: 255 if v else 0).save(out / f"ac_{case}_diff.png")
            raise AssertionError(f"ac_{case}: split=3 != split=1, bbox={diff.getbbox()}")

    # setBackgroundColor actually changed the undrawn area: a top-left corner
    # pixel (outside the central shapes) differs between black and navy.
    black = _open(out, "black", 1)
    navy = _open(out, "navy", 1)
    assert black.getpixel((0, 0)) != navy.getpixel((0, 0)), (
        "setBackgroundColor had no visible effect on the undrawn corner"
    )
