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

    # column splitting: the budget bounds a full-height column, so it resolves a
    # tile *width*; height is the whole surface (SPEC §10.8).
    bytes_per_col = (H * bits + 7) // 8
    g = _m(dut, r"COLUMNS limit=(\d+) begin=(\d) render=(\d) tw=(\d+) th=(\d+) span=(\d+) N=(\d+)")
    limit, begin, render, tw, th, span, n = (int(x) for x in g)
    assert (begin, render) == (1, 1), "column budget should allocate and render"
    assert th == H, "a column tile spans the full surface height"
    assert span == tw, "span is the tile width when splitting into columns"
    assert tw * bytes_per_col <= limit, "tile width exceeds the budget"
    assert (tw + 1) * bytes_per_col > limit or tw == W, "tile width is not maximal for the budget"
    assert n == (W + tw - 1) // tw, "tile count inconsistent with tile width"

    # split count applies to the split axis, whichever it is.
    begin, tw, th, n = (int(x) for x in _m(dut, r"COLSPLIT begin=(\d) tw=(\d+) th=(\d+) N=(\d+)"))
    assert begin == 1
    assert (n, th) == (4, H), "4 full-height columns"
    assert tw == (W + 3) // 4

    # PSRAM: request is allowed to fall back to internal RAM (the host has no
    # PSRAM), the render still succeeds, and tileIsPsram() reports the truth.
    req, begin, render, actual, db = (int(x) for x in _m(dut, r"PSRAM request=(\d) begin=(\d) render=(\d) actual=(\d) db=(\d)"))
    assert (req, begin, render) == (1, 1, 1), "a PSRAM request must never fail the allocation"
    assert actual == 0, "the host backend has no PSRAM, so the fallback must be reported"
    assert db == 1, "internal-RAM fallback keeps auto double-buffering for 3 tiles"

    dut.expect("TEST done", timeout=5)
