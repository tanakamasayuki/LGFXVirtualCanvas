"""Pixel-exact parity: tiled render (split=2/3/5/7) must equal full render (split=1)."""

from pathlib import Path

from PIL import Image, ImageChops

SKETCH_DIR = Path(__file__).parent
REFERENCE = 1
SPLITS = [2, 3, 5, 7]


def test_parity(dut):
    dut.expect("TEST start parity", timeout=15)
    dut.expect("PANEL", timeout=5)
    for s in [REFERENCE] + SPLITS:
        dut.expect(f"SPLIT {s} ", timeout=10)
    dut.expect("TEST done", timeout=5)

    out = SKETCH_DIR / "output"
    ref_path = out / f"split_{REFERENCE}.png"
    assert ref_path.stat().st_size > 100, "reference render missing"
    ref = Image.open(ref_path).convert("RGB")

    for s in SPLITS:
        img_path = out / f"split_{s}.png"
        assert img_path.exists(), f"missing {img_path}"
        img = Image.open(img_path).convert("RGB")
        assert img.size == ref.size, f"split {s}: size {img.size} != ref {ref.size}"

        diff = ImageChops.difference(ref, img)
        if diff.getbbox() is not None:
            # Persist artifacts for inspection (SPEC §13.2).
            ref.save(out / "full.png")
            img.save(out / "virtual.png")
            # Amplify the diff so it is visible.
            diff.point(lambda v: 255 if v else 0).save(out / "diff.png")
            bbox = diff.getbbox()
            raise AssertionError(
                f"split {s} differs from full render (split={REFERENCE}); "
                f"diff bbox={bbox}. Artifacts: full.png / virtual.png / diff.png"
            )
