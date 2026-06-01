# Benchmark

Real-hardware performance benchmark for LGFXVirtualCanvas. Unlike the headless
host tests under [`tests/`](../tests/), this needs an actual device: it measures
SPI/DMA transfer time, which does not exist on the host backend.

- Board: **M5Stack Core2** (ESP32) or **CoreS3** (ESP32-S3). M5Unified
  auto-detects the panel; both are 320×240.
- Output: a fixed-width table over **Serial @115200** — copy/paste into docs.

## What it measures

The same scene is drawn through every strategy via one **templated** scene
function, so the work is byte-for-byte identical across strategies (only the
transfer path differs). LGFXVirtualCanvas mirrors the LGFX API, so a single
template instantiates for `LovyanGFX&`, `M5Canvas&`, and `LGFXVirtualCanvas&`.

### Strategies

| method | what | buffer | notes |
|--------|------|--------|-------|
| **A** direct panel        | draw straight to `M5.Display`, no buffer | none | baseline floor (flicker) |
| **B** full sprite         | one 320×240 sprite, `pushSprite` once | internal **and** PSRAM | baseline ceiling (most RAM); draw/xfer split measured |
| **C** `LGFXVirtualScreen` | the library, single buffer (default) | internal | split sweep + autoClear on/off; draw/transfer serialized |
| **D** `LGFXVirtualScreen` | the library, `setDoubleBuffer(true)` | internal | split sweep; draw/transfer overlap (2× tile RAM) |
| **C/D** `LGFXVirtualSprite` | the library, sub-region size × split sweep | internal | finds the best split per size (Phase 3) |

C is the default single-buffer mode (correct, serialized — `waitDMA` after each
tile push). D is the opt-in two-buffer ping-pong that overlaps a tile's DMA
transfer with the next tile's draw. See SPEC §10.5 for the mechanism.

### Axes

- **split count**: 1, 2, 3, 4, 6, 8 (methods C, D)
- **region size** (Phase 3 sprite sweep): 64×48, 128×96, 160×100, 240×160,
  320×240 — each swept over all split counts, single buffer then double, to
  expose how the optimal split shifts with size (and to inform the no-arg default)
- **auto-clear**: on (default) vs off — quantifies the double-fill cost when the
  scene already paints every pixel with `fillScreen` (SPEC §11.1)
- **buffer placement**: internal RAM vs PSRAM — **baselines only.** The library
  has no public PSRAM API in Phase 1 (`createSprite` defaults to internal RAM),
  so method C is reported on internal RAM only.
- **scene weight**: `light` (transfer-bound) / `heavy` (CPU-bound) / `image`
  (`pushImage`, memory-bandwidth-bound)

### Columns

`draw_us` / `xfer_us` are the per-frame CPU-draw and SPI-transfer averages,
split only where the loop is ours to instrument (method B). For A (intertwined)
and C (the library owns the loop) only `frame_us` / `fps` are reported.
`heapInt` / `heapPSRAM` are free bytes after allocation.

## Running

```sh
# Core2 (default profile)
arduino-cli compile -e --profile m5stack_core2 bench
arduino-cli upload  -p /dev/ttyUSB0 --profile m5stack_core2 bench
arduino-cli monitor -p /dev/ttyUSB0 -c baudrate=115200

# CoreS3
arduino-cli compile -e --profile m5stack_cores3 bench
```

Or open `bench/bench.ino` in the Arduino IDE, select your board, and open the
Serial Monitor at 115200. The benchmark runs once in `setup()` and prints all
tables; `loop()` is idle.

## Reading the results

- Compare **C vs B**: the gap is the library's tiling overhead (the price of not
  needing a full-screen buffer).
- Compare **C vs A**: the win from buffering (no flicker, batched transfer).
- Within C, watch how **split count** trades memory for frame time, and how
  **autoClear=off** helps when the scene fills the screen anyway.
- Compare **D vs C** at the same split: the gain is the draw/transfer overlap.
  It grows with split count and is **largest in the CPU-bound `heavy` scene**
  (≈+40% at split 8), where a big per-tile draw can hide the ~32 ms transfer
  behind it. In the transfer-bound `light` scene the draw is tiny, so there is
  almost nothing to overlap (≈+5%).
- `heavy` is CPU-bound: split count matters most, and **C gets *slower* as
  splits rise** because the scene callback redraws the full region for every
  tile (off-tile pixels are clipped, but the CPU still walks every primitive).
  D absorbs this by overlapping the redundant draw with transfer.
- `light` / `image` are transfer-bound and sit near the **~31 fps full-frame
  SPI ceiling** (320×240×2 = 153,600 B ≈ 32 ms at 40 MHz) almost regardless of
  split — the library reaches that ceiling; direct panel (A) stays below it for
  lack of batching/DMA overlap.
- **The optimal split count depends on the region size.** Large/full-screen
  regions want *more* splits (less RAM per tile; for D, a fuller pipeline).
  Small regions do not: per-tile overhead (DMA setup, `waitDMA`, callback
  re-invocation) and redundant redraw dominate, so **split=1 is best for very
  small sprites**. The 160×100 sprite here is measured at split=3 only as one
  representative point — not as its optimum.
