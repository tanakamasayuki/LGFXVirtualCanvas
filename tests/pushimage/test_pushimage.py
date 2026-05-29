"""Tier 1 T1-12 (experiment): pushImage split=2/3/5/7 must equal split=1."""

from pathlib import Path

from PIL import Image, ImageChops

SKETCH_DIR = Path(__file__).parent
REFERENCE = 1
SPLITS = [2, 3, 5, 7]


def test_pushimage(dut):
    dut.expect("TEST start pushimage", timeout=15)
    dut.expect("PANEL", timeout=5)
    for s in [REFERENCE] + SPLITS:
        dut.expect(f"SPLIT {s} ", timeout=10)
    dut.expect("TEST done", timeout=5)

    out = SKETCH_DIR / "output"
    ref = Image.open(out / f"img_split_{REFERENCE}.png").convert("RGB")
    failures = []
    for s in SPLITS:
        img = Image.open(out / f"img_split_{s}.png").convert("RGB")
        assert img.size == ref.size, f"split {s}: size mismatch"
        diff = ImageChops.difference(ref, img)
        if diff.getbbox() is not None:
            ref.save(out / "full.png")
            img.save(out / "virtual.png")
            diff.point(lambda v: 255 if v else 0).save(out / "diff.png")
            failures.append(f"split{s} bbox={diff.getbbox()}")

    assert not failures, "pushImage not split-invariant (see full/virtual/diff.png): " + "; ".join(failures)
