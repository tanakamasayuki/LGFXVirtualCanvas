# Tests

> 日本語版: [README.ja.md](README.ja.md)

Automated test suite.

- [pytest-embedded](https://docs.espressif.com/projects/pytest-embedded/en/latest/) + Arduino CLI backend.
- Two profiles: `lang-ship:host` (logic verification, large fixtures, fast CI) and `esp32:esp32:esp32` (real hardware verification, footprint measurement).
- Per-feature subdirectory containing `<feature>.ino`, `sketch.yaml`, `test_<feature>.py`, and an `input/` directory of fixtures.
- Assertions use the `EXPECT_TRUE` / `EXPECT_EQ` / `EXPECT_NEAR` macros and the `TEST done N/M` Serial protocol.

## Directory layout

- `smoke/` — Template smoke test (host profile). Verifies the test infrastructure itself.

## Test design

## Running

```sh
# host (default)
uv run --env-file .env pytest

# real ESP32
uv run --env-file .env pytest --profile=esp32

# single test
uv run --env-file .env pytest smoke/
```
