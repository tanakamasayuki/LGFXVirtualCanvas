"""Headless M5Unified rendering smoke: main panel (drawMain) + sprite cases (drawHome)."""

from pathlib import Path

SKETCH_DIR = Path(__file__).parent
CASES = ["stickcplus_p", "stickcplus_l", "core2", "atoms3", "coreink"]


def test_m5unified_smoke(dut):
    dut.expect("TEST start m5unified_smoke", timeout=15)
    dut.expect("MAIN ok", timeout=10)
    dut.expect("TEST done", timeout=5)

    out = SKETCH_DIR / "output"
    assert (out / "main.png").stat().st_size > 100
