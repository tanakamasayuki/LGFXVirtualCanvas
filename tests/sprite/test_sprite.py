"""LGFXVirtualSprite: tiling correctness + region containment + movable position."""

import re
from pathlib import Path

from PIL import Image, ImageChops

SKETCH_DIR = Path(__file__).parent
RED = (255, 0, 0)
RX, RY, RW, RH = 40, 30, 160, 100


def test_sprite(dut):
    dut.expect("TEST start sprite", timeout=15)
    dut.expect("PANEL", timeout=5)
    dut.expect("REF ", timeout=10)
    tiled = dut.expect(re.compile(rb"TILED tiles=(\d+) tileH=(\d+)"), timeout=10)
    n_tiles = int(tiled.group(1))
    move = dut.expect(re.compile(rb"MOVE x=(\d+) y=(\d+)"), timeout=10)
    dut.expect("TEST done", timeout=5)

    out = SKETCH_DIR / "output"
    ref = Image.open(out / "sprite_ref.png").convert("RGB")
    tiledimg = Image.open(out / "sprite_tiled.png").convert("RGB")
    move_img = Image.open(out / "sprite_move.png").convert("RGB")

    # The many-tile render (incl. partial last tile) must equal the 1-tile one.
    assert n_tiles >= 3, f"expected the budget to force several tiles, got {n_tiles}"
    diff = ImageChops.difference(ref, tiledimg)
    if diff.getbbox() is not None:
        ref.save(out / "full.png")
        tiledimg.save(out / "virtual.png")
        diff.point(lambda v: 255 if v else 0).save(out / "diff.png")
        raise AssertionError(f"tiled sprite != 1-tile reference, bbox={diff.getbbox()}")

    # Region containment on the reference image:
    # - far-outside pixel stays background RED
    assert ref.getpixel((2, 2)) == RED, "top-left outside region was modified"
    # - just below a region that ends mid-screen stays RED (partial last tile clipped)
    assert ref.getpixel((RX + RW // 2, RY + RH + 3)) == RED, "rows below the region were clobbered"
    # - just right of the region stays RED
    assert ref.getpixel((RX + RW + 3, RY + RH // 2)) == RED, "columns right of the region were clobbered"
    # - inside the region is drawn (center is the yellow circle, not RED)
    assert ref.getpixel((RX + RW // 2, RY + RH // 2)) != RED, "region interior was not drawn"

    # Movable sprite: drawn at (100,80) size 32x32; outside stays RED, inside drawn.
    mx, my = int(move.group(1)), int(move.group(2))
    assert (mx, my) == (100, 80)
    assert move_img.getpixel((2, 2)) == RED
    assert move_img.getpixel((mx + 16, my + 16)) != RED, "moved sprite interior was not drawn"
    assert move_img.getpixel((mx + 48, my + 16)) == RED, "pixels outside the moved sprite were modified"
