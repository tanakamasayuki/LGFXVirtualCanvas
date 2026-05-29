"""Tier 1 multi-scene parity: every split must equal split=1 for each scene."""

from pathlib import Path

from PIL import Image, ImageChops

SKETCH_DIR = Path(__file__).parent
REFERENCE = 1
SPLITS = [2, 3, 5, 7]
SCENES = [
    "overall",
    "shapes",
    "circles",
    "text",
    "text2",
    "boundary",
    "clipping",
    "fuzz",
    "anim0",
    "anim1",
    "anim2",
]


def test_parity(dut):
    dut.expect("TEST start parity", timeout=15)
    dut.expect("PANEL", timeout=5)
    for name in SCENES:
        dut.expect(f"SCENE {name} done", timeout=20)
    dut.expect("TEST done", timeout=5)

    out = SKETCH_DIR / "output"
    failures = []
    for name in SCENES:
        ref_path = out / f"{name}_split_{REFERENCE}.png"
        assert ref_path.stat().st_size > 100, f"{name}: reference missing"
        ref = Image.open(ref_path).convert("RGB")
        for s in SPLITS:
            img_path = out / f"{name}_split_{s}.png"
            assert img_path.exists(), f"missing {img_path}"
            img = Image.open(img_path).convert("RGB")
            assert img.size == ref.size, f"{name} split {s}: size mismatch"
            diff = ImageChops.difference(ref, img)
            if diff.getbbox() is not None:
                ref.save(out / f"{name}_full.png")
                img.save(out / f"{name}_virtual.png")
                diff.point(lambda v: 255 if v else 0).save(out / f"{name}_diff.png")
                failures.append(f"{name}@split{s} bbox={diff.getbbox()}")

    assert not failures, "split-variant scenes (see *_full/_virtual/_diff.png): " + "; ".join(failures)
