# Getting started with LGFXVirtualCanvas — what flicker really is, and why tiling answers it

> 日本語: [BEGINNERS_GUIDE.ja.md](BEGINNERS_GUIDE.ja.md)

This guide is for people who want to put something on an M5Stack / ESP32 screen but do not
yet know **why it flickers**, **why RAM runs out**, or **what DMA actually buys them**.

It starts from **how the hardware paints pixels**, then covers flicker, double buffering,
why a full-screen buffer does not fit, tiling, and finally what LGFXVirtualCanvas takes
care of for you — and what it deliberately does **not** (its limits).
Every number here is measured on an M5Stack Core2 (320×240 / 16-bit / SPI); see [`bench/`](../bench/).

This is not an API reference. For the full specification and the reasoning behind it see
[SPEC.md](../SPEC.md); for a usage summary see [README.md](../README.md); for working code
see [examples/](../examples/).

**Contents**

1. [How the screen actually lights up](#1-how-the-screen-actually-lights-up)
2. [Why it flickers](#2-why-it-flickers)
3. [Double buffering — and why it does not fit](#3-double-buffering--and-why-it-does-not-fit)
4. [Tiling, the compromise](#4-tiling-the-compromise)
5. [Getting the first frame on screen](#5-getting-the-first-frame-on-screen)
6. [Writing the draw function (separate View from Model)](#6-writing-the-draw-function-separate-view-from-model)
7. [Choosing memory and split count](#7-choosing-memory-and-split-count)
8. [Speed — draw-bound vs transfer-bound](#8-speed--draw-bound-vs-transfer-bound)
9. [The big gotcha: your callback runs once per tile](#9-the-big-gotcha-your-callback-runs-once-per-tile)
10. [Updating part of the screen — LGFXVirtualSprite](#10-updating-part-of-the-screen--lgfxvirtualsprite)
11. [Diff transfer — skipping unchanged tiles](#11-diff-transfer--skipping-unchanged-tiles)
12. [Transparent overlays (dialogs)](#12-transparent-overlays-dialogs)
13. [Column splitting and PSRAM](#13-column-splitting-and-psram)
14. [LovyanGFX / M5GFX quirks](#14-lovyangfx--m5gfx-quirks)
15. [The APIs that are not there, and why](#15-the-apis-that-are-not-there-and-why)
16. [Strengths and limits](#16-strengths-and-limits)
17. [Troubleshooting](#17-troubleshooting)

---

## 1. How the screen actually lights up

The biggest difference from PC graphics is that **the MCU has no "screen" of its own.**

The small LCDs attached to an M5Stack or ESP32 are usually **panels with a built-in
controller** (ILI9341, ST7789, …). The panel contains memory for its pixels (GRAM), and its
display circuit reads that memory continuously to keep the LCD lit. All the MCU does is:

> send, over SPI (or an I80 parallel bus), **"write these pixels into this rectangle"**

The moment it arrives, the panel's GRAM changes, and the next refresh shows it.

```
  ESP32                            LCD panel
  ┌────────────────┐  SPI 40–80MHz  ┌────────────────────┐
  │   your code    │ ─────────────► │ controller          │
  │ lcd.fillRect() │  "these pixels │   ↓ write           │
  └────────────────┘   at x,y,w,h"  │ GRAM (pixel memory) │
                                     │   ↓ read forever    │
                                     │ LCD panel           │
                                     └────────────────────┘
```

Almost everything about embedded drawing follows from this.

- **Whatever you draw is visible immediately.** `fillScreen()` then `drawString()` shows
  **both** the all-black screen and the screen with text, in that order. There is no
  frame-level swap like on a PC.
- **Bytes are time.** A 320×240 16-bit full screen is 320 × 240 × 2 = **153,600 bytes**.
  At 40 MHz that is about **32 ms**; at 80 MHz about 15 ms. So as long as you redraw the
  whole screen, **roughly 31 fps is a hard ceiling on a Core2**. No drawing trick lowers it.
- **Reading back is usually impossible or slow.** On most setups panel read-out is
  unsupported or very slow. That is why "blend something over what is already there" is hard.

### Colors are RGB565

You send 16 bits per pixel: 5 red / 6 green / 5 blue. A 24-bit color like `#4CAF50` is
always quantized to that grid. Banding in gradients is a consequence, not a bug.

---

## 2. Why it flickers

A direct consequence of "whatever you draw is visible immediately". The usual update loop:

```cpp
void loop() {
  lcd.fillScreen(TFT_BLACK);          // the screen really does go black here (you see it!)
  lcd.drawString("Temp", 10, 10);     // the text comes back
  lcd.drawString("24.5C", 10, 40);
  delay(100);
}
```

To a human eye that is **a black blink ten times a second**. That is what flicker is: the
"erase" half of "erase, then draw" is on screen long enough to see.

There are two naive workarounds, and both hit a wall.

**(a) Don't erase; overwrite only what changed.**
When a number goes `100` → `99` the old digit stays. Paint the background first and now
that little rectangle flickers instead. As the number of widgets grows, tracking "what to
erase" collapses.

**(b) Only draw when something changes.**
Fewer flickers, but each one is still there. Useless for animation or live values.

The real problem is that **the screen update is not atomic** — intermediate states are
visible. So build the intermediate state **somewhere invisible**.

---

## 3. Double buffering — and why it does not fit

The answer is clear: **keep a buffer the size of the screen in RAM, draw into it, and push
the finished image to the panel in one go.** That is double buffering. The panel only ever
sees finished frames, so intermediate states cannot be seen, by construction.

The problem is the price.

| Screen | Full-screen buffer (16-bit) |
| --- | --- |
| 128×128 (AtomS3) | 32 KB |
| 135×240 (StickC Plus) | 63 KB |
| 240×135 (Cardputer) | 63 KB |
| **320×240 (Core / Core2)** | **150 KB** |
| 320×480 (CoreS3 portrait) | 300 KB |
| 720×1280 (Tab5) | 1.8 MB |

An ESP32 has roughly 320 KB of internal DRAM, minus the WiFi/BLE stack, minus
fragmentation, minus your own buffers. And the binding constraint is not the total but
**contiguous** space:

> **Measured (Core2, `bench/`)**: with **310 KB of free heap**, allocating the 320×240
> full-screen sprite (a **contiguous** 150 KB) **failed**. The benchmark log says
> `full sprite alloc failed (skipped)`.

So "it should fit on paper" is not enough. This is the central dilemma of embedded GUIs:
**killing flicker needs a buffer; there is no contiguous RAM to put one in.**

Boards with PSRAM have the capacity. But PSRAM is slow, and
**LovyanGFX pushes a PSRAM sprite without DMA.** Measured on the same Core2:

| Approach | Draw | Transfer | Frame | fps |
| --- | --- | --- | --- | --- |
| Full-screen sprite in PSRAM | 13.8 ms | 32.0 ms | **45.8 ms** | 21.8 |
| Tiled (this library, 38 KB of internal RAM) | — | — | **31.0 ms** | **32.3** |

**150 KB is slower than 38 KB.** That is the reason this library exists.

---

## 4. Tiling, the compromise

The compromise: do not hold a full-screen buffer. **Cut the surface into horizontal bands
(tiles), finish one at a time, and send it.**

```
   surface (virtual)             actual tile buffers in RAM
  ┌─────────────┐  tile 0        ┌─────────────┐
  │   tile 0    │ ──draw─────►   │  ~19 KB × 2 │ ──DMA──► panel
  ├─────────────┤  tile 1        └─────────────┘
  │   tile 1    │ ──draw─────►      (reused)
  ├─────────────┤
  │     …       │
  └─────────────┘
```

For 320×240 one row is 320 × 2 = 640 bytes. With the default budget of 19,200 bytes,
19200 / 640 = **30 rows** per tile, i.e. **8 tiles**. The RAM needed is 19 KB × 2 =
**about 38 KB** — a quarter of 150 KB — and because each allocation is small it
**survives a fragmented heap** (the "failed with 310 KB free" case above cannot happen).

### Your draw function still uses full-screen coordinates

Write tiling by hand and the Y-offset arithmetic contaminates every drawing call.
LGFXVirtualCanvas instead **lets you write one draw function in full-screen coordinates and
runs it once per tile.** The offset and the clipping stay inside the library.

```cpp
void drawScene(LGFXVirtualCanvas& g) {
  g.fillScreen(TFT_BLACK);
  g.fillCircle(160, 120, 40, TFT_YELLOW);   // full-screen coordinates, as usual
}
screen.render(drawScene);                    // called 8 times internally
```

**This is the single most important quirk.** Your draw function runs **once per tile**, so
8 times per frame at the default. `fillCircle` is simply clipped away outside the tile so
the image is correct — but **any computation or sensor read inside that function happens 8
times** (see [chapter 9](#9-the-big-gotcha-your-callback-runs-once-per-tile)).

### This is where DMA pays off

DMA (Direct Memory Access) is hardware that moves data **between memory and a peripheral
without the CPU**. When LovyanGFX pushes a sprite that lives in internal RAM, it starts an
**asynchronous SPI-DMA transfer and returns without waiting for it to finish.** The CPU is
free immediately.

That is what the second tile buffer is for:

```
  time ────────────────────────────────────────────►
  CPU:  draw T0 │ draw T1 │ draw T2 │ draw T3 │ …
  DMA:          │ xfer T0 │ xfer T1 │ xfer T2 │ …
                 ↑ overlapped
```

While DMA transfers tile 0, the CPU draws tile 1 into the other buffer (ping-pong). The
result is **frame time ≈ max(total draw, total transfer)**. With a single buffer you must
`waitDMA()` before reusing it — otherwise you overwrite a buffer mid-transfer and corrupt
the image — so the two costs **add up** instead. This is why LGFXVirtualCanvas
**enables double buffering automatically for any surface that resolves to 2+ tiles**
(a single tile has nothing to overlap with, so it stays single-buffered — SPEC §10.5).

---

## 5. Getting the first frame on screen

Three things: **declare the surface, write a draw function, render.**

```cpp
#include <M5Unified.h>          // graphics library first
#include <LGFXVirtualCanvas.h>  // then this

LGFXVirtualScreen screen(M5.Display);   // nothing allocated yet

void drawScene(LGFXVirtualCanvas& g) {
  g.fillScreen(TFT_NAVY);
  g.setTextColor(TFT_WHITE);
  g.drawCentreString("Hello, tiled world!", g.width() / 2, g.height() / 2);
}

void setup() {
  M5.begin();                 // the panel size becomes known here
  screen.render(drawScene);   // the first render allocates the tile buffers
}

void loop() {}
```

Plain LovyanGFX is the same — construct your `LGFX` panel and pass it in:

```cpp
#include <LovyanGFX.hpp>
#include <LGFX_AUTODETECT.hpp>
#include <LGFXVirtualCanvas.h>

static LGFX lcd;
LGFXVirtualScreen screen(lcd);
// setup(): lcd.init();  screen.render(drawScene);
```

### Ordering rules (get these wrong and nothing works)

1. **Include `LGFXVirtualCanvas.h` after your graphics library.** The header inspects which
   of LovyanGFX / M5GFX / M5Unified is present and builds itself accordingly.
2. **The constructor allocates nothing.** That is why a global instance is fine: before
   `M5.begin()` the screen size is unknown, so there is nothing to allocate.
3. **Allocation happens in `begin()` or the first `render()`** — both must be called
   **after** `M5.begin()` / `lcd.init()`.

### The three types

| Type | What it is |
| --- | --- |
| `LGFXVirtualScreen` | Manages the **whole panel**: settings, allocation, the tile loop, transfers |
| `LGFXVirtualSprite` | Manages **a rectangle** of the panel — same engine ([chapter 10](#10-updating-part-of-the-screen--lgfxvirtualsprite)) |
| `LGFXVirtualCanvas` | The **drawing surface**: the `g` handed to your draw function |

`g` looks like a canvas the size of the whole surface. `g.width()` returns the **surface**
width, not the tile width. Underneath it is a thin forwarder onto the current tile sprite:
it subtracts the offset and lets the sprite's clipping discard whatever falls outside
(SPEC §6).

---

## 6. Writing the draw function (separate View from Model)

### Callbacks are function pointers only

To keep code size down, neither `std::function` nor capturing lambdas can be passed.

```cpp
int score = 0;
screen.render([&](LGFXVirtualCanvas& g){ ... });   // ✗ compile error (captures)
screen.render([](LGFXVirtualCanvas& g){ ... });    // ✓ non-capturing lambda is fine
```

### Pass state as an argument

You do not need globals. Pass your state as a second argument and it is forwarded to the
draw function — no `void*` casts, and the type is checked.

```cpp
struct AppState { int score, playerX, playerY; };
AppState state;

void drawScene(LGFXVirtualCanvas& g, AppState& s) {   // typed
  g.fillScreen(TFT_BLACK);
  g.setCursor(10, 10);
  g.printf("Score %d", s.score);
  g.fillRect(s.playerX, s.playerY, 16, 16, TFT_GREEN);
}

void loop() {
  updateState(state);                 // the Model advances here, once per frame
  screen.render(drawScene, state);    // the View only draws the current state
}
```

This shape (**View separated from Model**) is strongly recommended, for three reasons:

- It makes it **safe for the draw function to run once per tile**
  ([chapter 9](#9-the-big-gotcha-your-callback-runs-once-per-tile)), because there is exactly
  one place where state advances.
- The same draw function can be **reused** for direct drawing, tiled drawing, and headless
  host tests.
- It lets you verify mechanically that the image does not depend on the split count — which
  is exactly what this library's own tests do (see [tests/](../tests/)).

---

## 7. Choosing memory and split count

### What happens if you configure nothing

The default is "**split so that one tile is ≈ 19 KB**". It fixes the *tile size*, not the
split count, so the number of tiles scales with the surface:

| Surface | Split resolved by default | Total tile RAM |
| --- | --- | --- |
| 64×48 | 1 (single-buffered) | 6 KB |
| 128×96 | 2 (double) | ~24 KB |
| 240×160 | 4 (double) | ~38 KB |
| 320×240 | 8 (double) | ~38 KB |

On the Core2 benchmark this single number **reproduces the measured optimum at every size
tested** (SPEC §10.6). "Configure nothing" really is a sound default.

### Configuring it yourself (with priority)

```cpp
screen.setMemoryLimit(20 * 1024);   // 1. byte budget for a tile buffer (the main knob)
screen.setSplitCount(4);            // 2. fix the number of tiles
screen.setTileHeight(40);           // 3. fix the tile height (setTileWidth for columns)
```

Priority when several are set: **1 > 2 > 3 > default (19 KB)**. All of them must be called
before allocation; if you change something afterwards, call `begin()` again to rebuild.

Prefer thinking in **RAM rather than tile count** — the split count is a result, not a goal.

### Failure is never silently patched up

This is deliberate. **When allocation fails, the library does not quietly increase the split
count or shrink the tile.** The caller always learns that the request was not met.

```cpp
void setup() {
  M5.begin();
  screen.setMemoryLimit(8 * 1024);
  if (!screen.begin()) {                     // optional: allocate now and check
    Serial.println("alloc failed");          // you find out here
    M5.Display.drawString("alloc failed", 10, 10);
    return;
  }
  Serial.printf("tiles=%d tileH=%d\n", screen.tileCount(), screen.tileHeight());
}
```

- `begin()` returns `false` on failure.
- `render()` on an unallocated surface **draws nothing** and returns `false` (no garbage).
- You may skip `begin()`; the first `render()` allocates as a guardrail. But writing
  `begin()` and checking its result makes diagnosis much easier.

Working code: [examples/MemoryBudget](../examples/MemoryBudget/).

### auto-clear

Each tile is cleared to the background color (black by default) before your callback runs.
A single buffer is reused for every tile, so without this you would see the previous tile's
leftovers.

```cpp
screen.setBackgroundColor(TFT_NAVY);   // change the clear color
screen.setAutoClear(false);            // advanced: skip double-painting if you always fillScreen
```

> ⚠️ **Colors are interpreted by their C++ type** (exactly as everywhere else in LovyanGFX):
> `int` / `uint16_t` (the `TFT_*` constants, the result of `color565()`) is **RGB565**,
> `uint32_t` is **RGB888**. So `setBackgroundColor(TFT_NAVY)` and
> `setBackgroundColor((uint32_t)0x000080)` mean the same color, but
> `setBackgroundColor((uint32_t)0x000F)` is not navy.

With `setAutoClear(false)`, **pixels you do not draw are undefined**. Use it only when the
scene always paints the entire surface.

---

## 8. Speed — draw-bound vs transfer-bound

This is where intuition goes wrong, so pin it down with numbers. With double buffering,
frame time is roughly **max(Σ draw, Σ transfer)**.

- **Σ transfer does not depend on the split count.** Every pixel of the surface is sent
  exactly once, so it is set by area and SPI clock alone — about 32 ms for 320×240.
- **Σ draw *grows* with the split count.** The callback re-runs per tile, and every
  primitive is walked and clipped per tile (only in-tile pixels are rasterized, but the
  bookkeeping is paid every time).

Therefore:

| Situation | What is happening | What to do |
| --- | --- | --- |
| **Transfer-bound** (Σ draw < Σ transfer) | Drawing hides behind the transfer | **More tiles is faster** (finer overlap) |
| **Draw-bound** (Σ draw > Σ transfer) | Transfer is already hidden; extra tiles only add redundant drawing | **Fewer, larger tiles.** Ideally one |

### Measured on a Core2 (320×240, `bench/`)

**Light scene (transfer-bound)**

| Approach | Frame | fps |
| --- | --- | --- |
| A direct panel (no buffer, flickers) | 36.9 ms | 27.1 |
| C single buffer, split=8 | 32.6 ms | 30.7 |
| **D double buffer, split=8 (the default)** | **31.0 ms** | **32.3** |

It lands right on the transfer floor (~32 ms). It beats direct drawing because the tiles are
batched into one transaction and the DMA overlaps the drawing.

**Heavy scene (draw-bound)**

| Approach | split=2 | split=8 |
| --- | --- | --- |
| A direct panel | 12.2 fps | — |
| C single buffer | 24.9 fps | **18.0 fps** (worse with more tiles) |
| D double buffer | 28.2 fps | 30.1 fps |

Single buffering gets slower as you split, because redundant drawing lands directly in the
frame time; double buffering absorbs it under the transfer. That is why two buffers are the
default.

### Principles worth memorizing

- **More tiles never reduces Σ draw.** It only adds per-tile overhead. Tiles are a tool for
  *using less RAM*, not for going faster (they end up faster only insofar as double
  buffering can hide the transfer).
- **Transfer time is fixed by area and SPI clock.** Optimizing your drawing will not lower
  it. To lower it, send less area
  ([chapter 10](#10-updating-part-of-the-screen--lgfxvirtualsprite)) or send less often
  ([chapter 11](#11-diff-transfer--skipping-unchanged-tiles)).
- **If drawing is heavy, split less**: raise `setMemoryLimit()` or set `setSplitCount(1)`.
  The library does not auto-detect which regime you are in — the cost of an arbitrary
  callback is not knowable in advance (SPEC §10.6).

---

## 9. The big gotcha: your callback runs once per tile

**Your draw function is called once per tile, every frame** — 8 times at the 320×240
default. The image comes out right (out-of-tile drawing is clipped). What breaks is
**side effects, and work that clipping cannot shrink.**

### What not to do

```cpp
// ✗ bad
void bad(LGFXVirtualCanvas& g) {
  int v = analogRead(36);          // read per tile → 8 different values, bands disagree
  int t = millis();                // same; animation shears at tile boundaries
  g.drawPng(logo, sizeof(logo), 0, 0);  // full PNG decode, 8 times
  g.fillRect(0, 0, v, 10, TFT_RED);
}
```

### The right shape

```cpp
// ✓ advance state in loop(); the draw function only reads
struct App { int level; uint32_t t; };
App app;

void good(LGFXVirtualCanvas& g, App& s) {
  g.fillScreen(TFT_BLACK);
  g.fillRect(0, 0, s.level, 10, TFT_RED);
  g.pushImage(0, 20, LOGO_W, LOGO_H, logo565);  // raw pixels in RAM: cheap (see below)
}

void loop() {
  app.level = analogRead(36);      // once per frame
  app.t     = millis();
  screen.render(good, app);
}
```

### Work that clipping shrinks, and work it does not

This is the dividing line:

| Work | Under tiling |
| --- | --- |
| `fillRect` / `drawString` / `fillCircle` … | **Clipped** — only in-tile pixels are painted; only the bookkeeping repeats |
| `pushImage` from pixels **in RAM** | **Clipped** — only that tile's source rows are read. Cheap |
| `drawPng` / `drawJpg` / `drawBmp` | **Not reduced.** Producing even one row requires processing the whole compressed stream, so it is **a full decode per tile** |
| Reading assets from SD / flash | **Not reduced.** Reopen and reread per tile |
| Sampling source pixels in PSRAM | **Not reduced.** Slow memory, re-read per tile |
| Building a big LUT, layout passes, sensor polling, networking | **Not reduced.** N× straight through |

**There is one fix: do it once, then blit.**

```cpp
static M5Canvas logo(&M5.Display);      // lives in internal RAM

void setup() {
  M5.begin();
  logo.createSprite(LOGO_W, LOGO_H);
  logo.drawPng(logoPng, sizeof(logoPng), 0, 0);   // decoded exactly once, here
}

void drawScene(LGFXVirtualCanvas& g) {
  g.pushImage(10, 10, LOGO_W, LOGO_H, (uint16_t*)logo.getBuffer());  // per tile, but cheap
}
```

That turns **N decodes** into **1 decode + N cheap clipped blits**. If heavy work genuinely
has to stay in the callback, lower N instead: bigger `setMemoryLimit()`, smaller
`setSplitCount()`. See SPEC §10.7.

---

## 10. Updating part of the screen — `LGFXVirtualSprite`

If most of the screen is static and only a region moves, there is no reason to send the
whole panel. `LGFXVirtualSprite` manages **an arbitrary sub-rectangle** with the same tiling
engine.

```cpp
LGFXVirtualSprite view(M5.Display, 200, 150, 20, 60);  // 200×150 placed at (20,60)

void drawView(LGFXVirtualCanvas& g, Ball& b) {   // coordinates are the sprite's LOCAL ones
  g.fillScreen(TFT_BLACK);                       // (0,0) = top-left of the region
  g.drawRect(0, 0, g.width(), g.height(), TFT_DARKGREY);  // width() returns 200
  g.fillCircle(b.x, b.y, 10, TFT_CYAN);
}

void setup() {
  M5.begin();
  M5.Display.fillScreen(TFT_NAVY);
  M5.Display.drawString("Static title", 20, 20);  // outside the region; never touched again
  view.setMemoryLimit(12 * 1024);
  view.begin();
}

void loop() {
  update(ball);
  view.render(drawView, ball);       // only 200×150 is updated (~1/2.5 of the transfer)
  delay(16);
}
```

- Every setting (`setMemoryLimit` / `setSplitCount` / diff / transparency …) works exactly
  as on `LGFXVirtualScreen`.
- `setPosition(x, y)` moves it **without reallocating**; `render(draw, x, y)` draws at a new
  position and remembers it — handy for a moving icon or cursor.
- Regions that stick out past the panel edge are safe (panel-side clipping absorbs them).

`LGFXVirtualScreen` is just the special case where the surface is the whole panel; both
share one engine. Working code: [examples/Viewport](../examples/Viewport/).

> **Note:** pixels the sprite does not touch are **never repainted by anyone**. The
> "Static title" above is drawn once. If something behind the region changes, redrawing it
> is the application's responsibility.

---

## 11. Diff transfer — skipping unchanged tiles

A tile whose content is identical to last frame does not need to be sent at all.
`setDiffMode(LGFXVirtualDiffMode::Tile)` enables that (**off by default**).

```cpp
screen.setDiffMode(LGFXVirtualDiffMode::Tile);
screen.begin();

// …after rendering
Serial.printf("%u / %u px pushed\n",
              (unsigned)screen.diffPushedPixels(),
              (unsigned)screen.diffTotalPixels());
```

Each tile is hashed after your callback (8 bytes per tile) and its transfer is skipped when
the hash matches the previous render. **Not a single output pixel changes** — this is purely
a transfer optimization.

**It only pays off in specific situations.**

- It reduces **transfer only**. The draw callback still runs for every tile, and hashing
  (one linear pass over the tile buffer) is **added** on top.
- So it targets **large, slow panels** (a Full-HD USB display, say). On something small and
  fast like a Core2's 320×240, the pushed-pixel count drops but the frame time barely moves.
  That is why it is off by default — measure before keeping it.
- Granularity is **per tile only**. Tile height comes from the memory budget, so it gets
  fine automatically on big panels (Full-HD at the default budget → 5-row tiles, 1/216 of
  the screen).

> ⚠️ **Call `invalidate()` when anything else touches the panel.** Skipping a transfer is
> only valid while "whatever we did not send still shows the previous image" holds.
> Reallocation, config changes, moving a sprite, and panel rotation/size/depth changes are
> detected automatically; nothing else is. **Drew directly with `lcd.fillRect()`? Pushed
> another surface over it? Slept or reset the panel? USB display reconnected?** Then call
> `screen.invalidate()` and the next `render()` transfers every tile.

Working code: [examples/DiffTransfer](../examples/DiffTransfer/). See also SPEC §21.

---

## 12. Transparent overlays (dialogs)

"Put a confirmation dialog over a running screen, without redrawing the screen underneath
(it is expensive, and it blinks)." That is `renderTransparent()`.

The mechanism is simple: the tile is cleared to a **transparent color**, and only pixels
that are *not* that color are transferred. Untouched pixels are never sent, so the panel
keeps whatever was there — which is why **the background shows through the rounded corners
of a dialog.**

```cpp
LGFXVirtualScreen background(M5.Display);
LGFXVirtualSprite  dialog(M5.Display, 200, 110);   // the dialog's bounding box

void drawDialog(LGFXVirtualCanvas& g) {   // no fillScreen: whatever you skip stays see-through
  g.fillRoundRect(0, 0, g.width() - 6, g.height() - 6, 10, TFT_DARKGREY);
  g.drawRoundRect(0, 0, g.width() - 6, g.height() - 6, 10, TFT_WHITE);
  g.drawString("Delete everything?", 14, 18);
}

void loop() {
  M5.update();
  if (M5.BtnA.wasPressed()) dialog.renderTransparent(drawDialog);      // background untouched
  if (M5.BtnB.wasPressed()) background.render(drawBackground, state);  // closing = repaint below
}
```

**Things to know.**

1. **You repaint the layer below when closing.** The library manages no layers. Nobody can
   restore pixels the dialog never touched, so that is the application's job.
2. **A rectangular dialog does not need transparency at all.** An `LGFXVirtualSprite` placed
   there already transfers only that rectangle, and it is faster. **For a shaped dialog the
   best answer is both**: a sprite on the bounding box, pushed transparently — which is what
   the code above does.
3. **No alpha blending.** The mask is all-or-nothing per pixel. Real compositing needs a
   full framebuffer or panel read-back.
4. **The cost profile differs.** A masked push walks one scanline at a time and issues a
   window setup per run of visible pixels, so **per pixel it is slower** than a normal
   `render()`. It wins because far fewer pixels are sent (a fully transparent scanline costs
   zero bus time). The draw callback still runs for every tile.
5. Combined with diff transfer, one more rule: **if something repaints what is under the
   overlay, call `overlay.invalidate()`.**

The default transparent color is `TFT_TRANSPARENT` (RGB565 `0x0120`). Change it only if your
artwork genuinely uses that color.

```cpp
dialog.setTransparentColor(M5.Display.color565(1, 2, 3));
```

> ⚠️ The [color-typing rule](#auto-clear) applies here too:
> `setTransparentColor(TFT_TRANSPARENT)` and `setTransparentColor((uint32_t)0x002400)` are
> the same color, but `setTransparentColor((uint32_t)0x0120)` is **a different one**.

Every form of `render(...)` has a matching `renderTransparent(...)` with the same arguments,
and plain `render()` behaves exactly as before. Working code:
[examples/Dialog](../examples/Dialog/). See also SPEC §22.

---

## 13. Column splitting and PSRAM

Two settings you usually do not need — and clearly do, when you do.

### Column splitting

The default cuts horizontal bands, top to bottom. `Columns` instead makes each tile a
**full-height vertical band**, transferred left to right — the natural order for long-format
printing, a ticker, or a wide waveform.

```cpp
screen.setSplitAxis(LGFXVirtualSplitAxis::Columns);
screen.setTileWidth(32);   // optional; the column-mode counterpart of setTileHeight
```

**Your draw callback does not change at all,** and the output image is bit-identical to row
splitting (parity tests verify pixel equality at every split count).

**The one exception is text wrapping.** LovyanGFX wraps at the tile sprite's right edge,
which under column splitting is a tile boundary — that would make the output depend on the
split count. So **X wrapping is forced off in column mode** and long lines are clipped
instead. Newlines are corrected back to the surface's left edge, so `println` / `printf`
behave as expected.

### Tile buffers in PSRAM

On a large panel the internal-RAM default resolves to a great many tiles, and the callback
re-runs for every one of them (200+ tiles at Full-HD). `setUsePsram(true)` is the escape
hatch: fewer, larger tiles in exchange for slower memory.

```cpp
screen.setUsePsram(true);
screen.setMemoryLimit(2 * 1024 * 1024);
screen.begin();
if (!screen.tileIsPsram()) Serial.println("served from internal RAM");
```

- **Not a free win.** PSRAM is slower to draw into and to read out of, and
  **LovyanGFX pushes a PSRAM sprite without DMA** — there is nothing left to overlap, so
  *auto* double buffering resolves to **off**. It wins when the callback is the bottleneck
  and loses when the transfer is. Measure.
- **This is the one place that falls back.** Without enough PSRAM the buffer comes from
  internal RAM and drawing still succeeds (the geometry and the output are unchanged; only
  speed differs). `tileIsPsram()` reports where it actually landed; `usePsram()` reports what
  you asked for.

Working code for both: [examples/ColumnSplit](../examples/ColumnSplit/).

---

## 14. LovyanGFX / M5GFX quirks

`LGFXVirtualCanvas` mirrors the LovyanGFX API, so the quirks of that API come along with it.

### Drawing state is sticky (the classic trap)

`setTextColor` / `setTextDatum` / `setTextSize` / `setFont` **persist once set.**

```cpp
g.setTextDatum(middle_center);
g.drawString("A", 100, 60);
g.drawString("B", 100, 90);   // still middle_center
```

**This matters more under tiling.** Each tile gets a fresh `LGFXVirtualCanvas`, so "I set it
on the previous tile" does not hold. **Set every piece of state you rely on, inside the draw
function, every time.** Never assume "it should still be like this" — that discipline is
what makes the image independent of the split count.

### Colors accept 24-bit but round to 16-bit

`0x4CAF50` is accepted, then quantized to RGB565. **Two colors you meant to be different can
become the same** — watch out for collisions with the transparent color. On top of that,
there is the [type-dependent interpretation](#auto-clear) described in chapter 7.

### Fonts eat flash; CJK fonts eat orders of magnitude more

Preset fonts are baked into the binary. Latin faces are a few KB to tens of KB; **Japanese,
Chinese and Korean faces are hundreds of KB to megabytes.**

### Do not call `startWrite` / `endWrite` yourself

During tiled drawing, `LGFXVirtualScreen` / `LGFXVirtualSprite` own the transaction and the
DMA ordering — which is why the canvas handed to your callback **does not expose them**
(calling them is a compile error). The library wraps the whole tile loop in a single
`startWrite()` / `endWrite()`, batching N transfers into one transaction.

### Anti-aliasing and tile boundaries

Neighborhood-dependent primitives (`drawSmoothLine`, `fillSmoothCircle`, …) may not match
full-surface drawing exactly at tile boundaries, since each tile is drawn and clipped
independently. LovyanGFX's default primitives are not anti-aliased, so this rarely bites.

---

## 15. The APIs that are not there, and why

`LGFXVirtualCanvas` wraps everything that can be offered safely on a tiled virtual surface:
shapes, text, images, color and state helpers, and read-back from the current tile.
**Calling a method that is not wrapped is a compile error** — a deliberate choice: failing
loudly beats silently drawing the wrong thing.

Four families are excluded. Knowing why makes it easy to pick a replacement.

| Excluded | Why |
| --- | --- |
| `writeColor`, `pushPixels`, `pushBlock`, `writePixels`, `pushColors` … | They depend on a caller-managed write window / stream cursor and carry too little virtual-coordinate information to clip per tile safely |
| `setWindow`, `startWrite`, `endWrite`, `beginTransaction`, `initDMA`, `waitDMA` | Owned by the manager during tiled drawing; exposing them would let you break the clipping and the DMA ordering guarantees |
| `scroll`, `copyRect` and scroll-rect APIs | They move existing pixels within a surface, but a tile holds only one band of it — source pixels across a tile boundary are simply not there |
| `pushSprite`, `pushRotated`, `pushAffine` … / `pushImageAffine*` / `createPng` | The `pushSprite` family transfers a sprite *to another destination*; the canvas already **is** the destination. Affine matrices embed destination coordinates, so a plain offset correction cannot make them safe |

**What to use instead:**

- To place a block of pixels → `pushImage` (correctly clipped per tile).
- To update only a region → `LGFXVirtualSprite`
  ([chapter 10](#10-updating-part-of-the-screen--lgfxvirtualsprite)).
- To scroll → keep the offset in your own state and **redraw every frame**.

The complete table is in [README.md](../README.md#api) and the SPEC appendix.

---

## 16. Strengths and limits

### Good at

- **Removing flicker without a full-screen buffer** — about 38 KB for 320×240, split into
  small allocations that survive a fragmented heap.
- **Keeping the split out of your drawing code.** One function in full-screen coordinates
  produces the same image at 1 tile or 8 (headless tests verify pixel equality).
- **Being near the measured optimum out of the box.** Tile budget and the double-buffer
  decision follow from the surface size.
- **Speed.** On a Core2 it beats direct drawing (27 fps, flickering) and sits on the SPI
  floor (~31 fps).
- **Partial updates and transparent overlays through the same engine.**
- **Testability**: the same draw function runs on the host.

### Not for (deliberate non-goals)

- **It is not a GUI framework.** No buttons, touch, focus, or events. Input is yours. If you
  need that, look at LVGL.
- **It is not a layout engine.** You pick the coordinates. (If you want layout outside your
  code, [LGFXScreenBuilder](https://github.com/tanakamasayuki/LGFXScreenBuilder) sits on top
  of this.)
- **It is not retained mode.** No display list, so every frame redraws everything. Diff
  transfer (off by default) reduces **transfer** only, never drawing.
- **No 2-D grid splitting** — rows or columns, one dimension.
- **No capturing lambdas.**
- **No alpha compositing.** Transparency is an all-or-nothing mask.
- **Neighborhood-dependent drawing across tile boundaries** (AA, blur) may not match
  full-surface output.
- **Image decoding does not get cheaper.** Clipping cannot shrink it; you have to reduce it
  to once yourself ([chapter 9](#9-the-big-gotcha-your-callback-runs-once-per-tile)).

In short, it is optimized for **producing a flicker-free image from a full redraw every
frame, in a small amount of RAM.**

---

## 17. Troubleshooting

| Symptom | Where to look |
| --- | --- |
| Nothing appears | The return value of `begin()` / `render()`. `false` means allocation failed (there is no fallback). Are they called after `M5.begin()` / `lcd.init()`? |
| `begin()` returns false | `setMemoryLimit()` is smaller than one row (width × 2 bytes), or the heap is exhausted. Raise the budget, or suspect fragmentation |
| Compile error: no such member | One of the [deliberately excluded APIs](#15-the-apis-that-are-not-there-and-why). Pick a replacement |
| Compile error: cannot pass a lambda | Callbacks are function pointers only; captures are not allowed. Pass state as the second argument |
| Bands disagree with each other | The draw function reads changing values (`millis()`, `analogRead()`, random). Advance state in `loop()`; the draw function should only read |
| Visible seams at band boundaries | Text state may not be re-set inside the draw function (it is sticky). Are you using AA primitives? |
| Slow (heavy scene) | Draw-bound. **Split less**: bigger `setMemoryLimit()`, smaller `setSplitCount()` |
| Slow (using `drawPng`) | A full decode per tile. Decode once into a sprite in `setup()` and `pushImage` from it |
| Slower than direct drawing | Are the tiles in PSRAM (no DMA, double buffering off)? Check `tileIsPsram()` |
| Garbage where nothing was drawn | `setAutoClear(false)` is set — undrawn pixels are undefined |
| Background/transparent color is wrong | The C++ type of the value: `TFT_*` / `int` is RGB565, `uint32_t` is RGB888 |
| Diff transfer leaves a stale image | Something else touched the panel. Call `invalidate()` |
| Diff transfer does not speed anything up | Expected on a small fast panel; it targets large, transfer-bound displays |
| The screen under a dialog disappears | On closing you must `render()` the lower surface again |
| Part of the dialog is missing | That color matches the transparent color (RGB565 `0x0120` by default) |
| Long lines do not wrap in column mode | By design (wrapping at a tile boundary would make output depend on the split count). Newlines are corrected |

## What to read next

- [README.md](../README.md) — usage summary and the API tables
- [examples/](../examples/) — from the smallest sketch to diff transfer and transparent dialogs
- [SPEC.md](../SPEC.md) — full specification and rationale (§10.6 choosing the split, §10.7 expensive callbacks, §21 diff transfer, §22 transparency)
- [bench/](../bench/) — how to run the on-device benchmark and how to read it
- [tests/](../tests/) — how split-count independence is verified
