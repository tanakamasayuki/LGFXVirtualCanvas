"""Headless LovyanGFX rendering smoke: main panel (drawMain)."""

from pathlib import Path

SKETCH_DIR = Path(__file__).parent


def test_lovyangfx_smoke(dut):
    dut.expect("TEST start lovyangfx_smoke", timeout=15)
    dut.expect("MAIN ok", timeout=10)
    dut.expect("TEST done", timeout=5)

    out = SKETCH_DIR / "output"
    assert (out / "main.png").stat().st_size > 100
