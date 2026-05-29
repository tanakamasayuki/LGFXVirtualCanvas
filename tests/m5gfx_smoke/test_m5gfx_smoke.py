"""Headless M5GFX rendering smoke: main panel (drawMain)."""

from pathlib import Path

SKETCH_DIR = Path(__file__).parent


def test_m5gfx_smoke(dut):
    dut.expect("TEST start m5gfx_smoke", timeout=15)
    dut.expect("MAIN ok", timeout=10)
    dut.expect("TEST done", timeout=5)

    out = SKETCH_DIR / "output"
    assert (out / "main.png").stat().st_size > 100
