# Tests

> 日本語版: [README.ja.md](README.ja.md)

Automated test suite for LGFXVirtualCanvas.

- [pytest-embedded](https://docs.espressif.com/projects/pytest-embedded/en/latest/) + Arduino CLI backend.
- Runs headlessly on the `lang-ship:host` core (`mode=lgfx` for graphics) so LovyanGFX rendering happens on the host and can be captured as PNG via `gfx.createPng()`.
- Per-test subdirectory containing `<name>.ino`, `sketch.yaml`, and `test_<name>.py` (uses the `dut` fixture).
- Sketches that emit artifacts `mkdir("output", 0755)` then write `output/<name>.png`; `conftest.py` wipes `output/` before each test.

## Directory layout

**Tier 1 — functional/correctness (LovyanGFX):**

- `parity/` — Core invariant: the same `drawScene()` rendered through `LGFXVirtualScreen` at several split counts must be **pixel-identical**. `split=1` (one full-height tile, offsetY=0) is the full-render reference; `split=2/3/5/7` exercise tiling, offset, clipping, reassembly, and the partial last tile. Compared with Pillow; on mismatch it saves `full.png` / `virtual.png` / `diff.png`.

**Tier 2 — cross-library build + minimal parity:**

- `build_lovyangfx/`, `build_m5gfx/`, `build_m5unified/` — Verify `LGFXVirtualCanvas.h` compiles after each graphics-library entry point (LovyanGFX / M5GFX / M5Unified) and that the shared scene renders identically at `split=1` vs `split=3`. These double as the build/smoke check for each entry point. The shared scene lives in `common_libs/vc_scene/` and is used by all three.

See [SPEC.md](../SPEC.md) §13 for the test policy and plan (tiers, case list, distribution).

## Running

```sh
# all tests (host, default)
uv run pytest -v

# a single test
uv run pytest parity -v
```

The first run downloads the pinned graphics libraries (e.g. LovyanGFX) into
the arduino-cli environment, so it is slower than subsequent runs.
