# examples

> 日本語: [README.ja.md](README.ja.md)

Each example makes exactly one point. Read them in order and you go
"get it running" → "pass state" → "decide memory" → "send less".

For the reasoning from first principles see
[docs/BEGINNERS_GUIDE.md](../docs/BEGINNERS_GUIDE.md); for the API tables see
[README.md](../README.md); for the design rationale see [SPEC.md](../SPEC.md).

## Reading order

| # | Example | The point | APIs |
| --- | --- | --- | --- |
| 1 | [HelloWorld](HelloWorld/) | The smallest working sketch: declare, draw, `render()` | `LGFXVirtualScreen` / `render` |
| 1' | [LovyanGFX_Basic](LovyanGFX_Basic/) | The same thing on plain LovyanGFX (`LGFX_AUTODETECT` + `lcd.init()`) | same |
| 2 | [BouncingBall](BouncingBall/) | **Pass state as an argument.** View (draw) separated from Model (update) | `render(draw, ctx)` / `setMemoryLimit` |
| 3 | [MemoryBudget](MemoryBudget/) | **Budget and failure.** Confirms allocation failure does *not* fall back | `setMemoryLimit` / `begin` / `tileCount` |
| 4 | [Viewport](Viewport/) | **Partial update.** Only a 200×150 region is redrawn each frame | `LGFXVirtualSprite` (local coordinates) |
| 5 | [DiffTransfer](DiffTransfer/) | **Skip unchanged tiles**, and print how much it actually saved | `setDiffMode` / `diffPushedPixels` / `invalidate` |
| 6 | [Dialog](Dialog/) | **Transparent overlay.** A dialog over a screen that is never redrawn | `renderTransparent` / `setTransparentColor` |
| 7 | [ColumnSplit](ColumnSplit/) | **Column splitting and PSRAM tiles**, reporting what was resolved | `setSplitAxis` / `setTileWidth` / `setUsePsram` / `tileIsPsram` |

Everything except `LovyanGFX_Basic` targets M5Unified (`M5.Display`), but the library
behaves identically on plain LovyanGFX. Only the panel setup differs — the draw functions
see nothing but `LGFXVirtualCanvas`, so they **carry over unchanged**.

## Three things that trip people up

**1. The draw callback runs once per tile.**
Eight times per frame at the 320×240 default. Clipping keeps the image correct, but
**any computation inside runs eight times**. Put `millis()` / `analogRead()` / random /
networking in the callback and the bands disagree with each other. Advance state in `loop()`
and let the callback **only read** it (`BouncingBall` and `DiffTransfer` show the shape).

**2. `drawPng` / `drawJpg` decode fully for every tile.**
Clipping discards output pixels, not decoding work. Decode once into a sprite in `setup()`
and `pushImage` from it in the callback (`pushImage` *is* clipped per tile).

**3. Allocation failure is never patched up silently.**
`begin()` returns `false`, and `render()` on an unallocated surface **draws nothing** and
returns `false`. There is deliberately no fallback that quietly increases the split count
(see `MemoryBudget`).

All three are explained from first principles in
[docs/BEGINNERS_GUIDE.md](../docs/BEGINNERS_GUIDE.md).

## Building

Every directory carries a self-contained `sketch.yaml` pinning the platform and library
versions, so no board-manager setup is needed:

```sh
arduino-cli compile --profile m5stack_core2 examples/HelloWorld
arduino-cli upload -p /dev/ttyUSB0 --profile m5stack_core2 examples/HelloWorld
```

In the Arduino IDE, open the `.ino`, pick your board, and upload.
`ColumnSplit`, `Dialog`, `DiffTransfer` and `MemoryBudget` **report their results over
serial (115200)** — open the serial monitor to see the point they are making.

For another board, swap the `fqbn` in `sketch.yaml`. No sketch hard-codes a screen size;
they all lay out from `g.width()` / `g.height()`.

## If you want measured numbers

[bench/](../bench/) measures what split count, double buffering and PSRAM actually do on
device (Core2 / CoreS3, printed as tables over serial). [tests/](../tests/) is where
split-count independence of the output is verified, headless on the host.
