"""Tier 2 (LovyanGFX): build check + split=1 vs split=3 pixel parity."""

from pathlib import Path

from PIL import Image, ImageChops

SKETCH_DIR = Path(__file__).parent


def test_build_lovyangfx(dut):
    dut.expect("TEST start build_lovyangfx", timeout=15)
    dut.expect("PANEL", timeout=5)
    dut.expect("SPLIT 1 ", timeout=10)
    dut.expect("SPLIT 3 ", timeout=10)
    dut.expect("TEST done", timeout=5)

    out = SKETCH_DIR / "output"
    full = Image.open(out / "split_1.png").convert("RGB")
    tiled = Image.open(out / "split_3.png").convert("RGB")
    assert tiled.size == full.size, f"size {tiled.size} != {full.size}"

    diff = ImageChops.difference(full, tiled)
    if diff.getbbox() is not None:
        full.save(out / "full.png")
        tiled.save(out / "virtual.png")
        diff.point(lambda v: 255 if v else 0).save(out / "diff.png")
        raise AssertionError(
            f"split=3 differs from split=1; diff bbox={diff.getbbox()}. "
            "Artifacts: full.png / virtual.png / diff.png"
        )
