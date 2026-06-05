"""Tier 1 T1-9: memory-budget math + allocation-failure semantics (no fallback)."""

import re


def _m(dut, pattern, timeout=10):
    match = dut.expect(re.compile(pattern.encode()), timeout=timeout)
    return [g.decode() for g in match.groups()]


def test_memory(dut):
    dut.expect("TEST start memory", timeout=15)
    W, H, bits = (int(x) for x in _m(dut, r"PANEL W=(\d+) H=(\d+) bits=(\d+)", 5))
    bytes_per_row = (W * bits + 7) // 8

    # generous budget: allocates, and tileH is the largest that fits the budget.
    g = _m(dut, r"GENEROUS limit=(\d+) begin=(\d) ready=(\d) render=(\d) th=(\d+) N=(\d+)")
    limit, begin, ready, render, th, n = (int(x) for x in g)
    assert (begin, ready, render) == (1, 1, 1), "generous budget should allocate and render"
    assert th >= 1
    assert th * bytes_per_row <= limit, "tileH exceeds the budget"
    assert (th + 1) * bytes_per_row > limit or th == H, "tileH is not maximal for the budget"
    assert n == (H + th - 1) // th, "tile count inconsistent with tileH"

    # tiny budget (< one row): cannot satisfy -> fail, no fallback, no draw.
    begin, ready, render = (int(x) for x in _m(dut, r"TINY limit=4 begin=(\d) ready=(\d) render=(\d)"))
    assert (begin, ready, render) == (0, 0, 0), "too-small budget must fail without fallback"

    # guardrail: render() before begin() auto-allocates with the default config.
    # The no-arg default is the size-aware tile budget (SPEC §10.1), so the split
    # count is derived from DEFAULT_TILE_BYTES, not a fixed 3.
    DEFAULT_TILE_BYTES = 19200  # mirror of LGFXVirtualTiledBase::DEFAULT_TILE_BYTES
    default_th = min(H, DEFAULT_TILE_BYTES // bytes_per_row)
    expected_n = (H + default_th - 1) // default_th
    render, ready, n = (int(x) for x in _m(dut, r"GUARDRAIL render=(\d) ready=(\d) N=(\d+)"))
    assert (render, ready) == (1, 1), "guardrail should auto-allocate on first render"
    assert n == expected_n, f"default split count should be {expected_n} for the ~19KB/tile budget"

    dut.expect("TEST done", timeout=5)
