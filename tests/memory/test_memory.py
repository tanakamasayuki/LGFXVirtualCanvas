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

    # guardrail: render() before begin() auto-allocates with the default 3 splits.
    render, ready, n = (int(x) for x in _m(dut, r"GUARDRAIL render=(\d) ready=(\d) N=(\d+)"))
    assert (render, ready) == (1, 1), "guardrail should auto-allocate on first render"
    assert n == 3, "default split count should be 3"

    dut.expect("TEST done", timeout=5)
