# LGFXVirtualCanvas

> 日本語: [README.ja.md](README.ja.md)

Draw to a **virtual full-screen canvas** on LovyanGFX / M5GFX — even when a
full-screen double buffer does not fit in RAM.

The screen is split into vertical tiles, each rendered into one small reused
sprite. Your draw function runs once per tile in **full-screen (virtual)
coordinates**; the library hides the Y offset, clipping, and flush. You never
see the tiles.

```cpp
void drawScene(LGFXVirtualCanvas& g) {
    g.fillScreen(TFT_BLACK);
    g.fillCircle(g.width() / 2, g.height() / 2, 40, TFT_YELLOW);  // just draw full-screen
}
```

> For the design rationale and the full specification, see [SPEC.md](SPEC.md).

## Why

A full-screen double buffer (e.g. 320×240×2 = 150 KB) often does not fit in
RAM. The usual workaround — splitting the screen into bands and tracking the
offset yourself — clutters every draw call with tile math. LGFXVirtualCanvas
does the splitting for you: you write one draw function in full-screen
coordinates and it renders correctly whether it runs as 1 tile or 7.

## Requirements

- An ESP32 (or any board) with **LovyanGFX**, **M5GFX**, or **M5Unified**.
- Include your graphics library **before** `LGFXVirtualCanvas.h`.

## Install

**Arduino IDE** — Library Manager → search **LGFXVirtualCanvas** → Install. Also
install your graphics library (LovyanGFX, M5GFX, or M5Unified) the same way.

**PlatformIO** — in `platformio.ini`:

```ini
lib_deps =
    https://github.com/tanakamasayuki/LGFXVirtualCanvas
    lovyan03/LovyanGFX     ; or m5stack/M5GFX, m5stack/M5Unified
```

**Manual** — download a release `.zip` and unzip it into your Arduino
`libraries/` folder.

## Quick start

```cpp
#include <M5Unified.h>
#include <LGFXVirtualCanvas.h>

LGFXVirtualScreen screen(M5.Display);   // split count omitted = auto (3)

void drawScene(LGFXVirtualCanvas& g) {
    g.fillScreen(TFT_NAVY);
    g.setTextColor(TFT_WHITE);
    g.drawCentreString("Hello, tiled world!", g.width() / 2, g.height() / 2);
}

void setup() {
    M5.begin();
    screen.render(drawScene);   // first render allocates the tile buffer
}

void loop() {}
```

Plain LovyanGFX is the same — just construct your `LGFX` panel and pass it:

```cpp
#include <LovyanGFX.hpp>
#include <LGFX_AUTODETECT.hpp>
#include <LGFXVirtualCanvas.h>

static LGFX lcd;
LGFXVirtualScreen screen(lcd);
// ... lcd.init(); screen.render(drawScene);
```

## With application state

Pass your state by reference; the library forwards it to your draw function
(no `void*`, no globals required):

```cpp
struct AppState { int score, playerX, playerY; };
AppState state;

void drawScene(LGFXVirtualCanvas& g, AppState& s) {
    g.fillScreen(TFT_BLACK);
    g.setCursor(10, 10);
    g.printf("Score %d", s.score);
    g.fillRect(s.playerX, s.playerY, 16, 16, TFT_GREEN);
}

void loop() {
    updateState(state);
    screen.render(drawScene, state);
}
```

Keeping the View (`drawScene`) separate from the Model (`AppState`) and the
update step is recommended — it makes the same draw function reusable for
normal rendering, tiled rendering, and headless tests.

## Controlling memory

By default the screen targets **≈ 19 KB per tile**, so the split count scales
with the surface: a small sprite stays one tile, a full screen becomes several.
It also **auto-enables double-buffering whenever more than one tile is needed**
(a single tile stays single-buffered — nothing to overlap). You usually care
about RAM, not tile count, so you can set your own budget to override:

```cpp
void setup() {
    M5.begin();
    screen.setMemoryLimit(20 * 1024);   // use at most ~20 KB for the tile buffer
    if (!screen.begin()) {              // optional: allocate now and check
        Serial.println("alloc failed"); // NO fallback — failure is reported
    }
}
```

Allocation is **lazy**: nothing is allocated until `begin()` or the first
`render()` (the screen size is unknown before `lcd.init()` / `M5.begin()`).
If allocation cannot satisfy the request, it fails — there is no silent
fallback — and `render()` returns `false` without drawing.

> ⚠️ **Decode images once, not per tile.** The draw callback re-runs for every
> tile, so anything the tile clip can't shrink — decoding a PNG/JPEG, reading
> from flash/SD, or sampling source pixels in PSRAM — is paid in full on *every*
> tile and scales ≈ N× with the split count. Do the decode/read **once** (at
> setup or on change) into an internal-RAM sprite, then `pushImage`/`pushSprite`
> from it in the callback (that *is* clipped per tile). For very heavy callbacks,
> use fewer/larger tiles (`setMemoryLimit` / `setSplitCount`). See SPEC §10.7.

### Splitting into columns instead of rows

By default the surface is cut into horizontal bands sent top to bottom. For a
long strip that is consumed left to right — long-format printing, a ticker, a
wide waveform — cut it into **full-height columns** instead:

```cpp
screen.setSplitAxis(LGFXVirtualSplitAxis::Columns);  // vertical bands, left to right
screen.setTileWidth(32);                             // optional: fixed column width
```

Nothing else changes: the draw callback still works in full-surface coordinates
and the image is identical to row splitting (the parity tests assert this pixel
for pixel). Only text wrapping is affected — see [Limitations](#limitations).

### Tile buffers in PSRAM

On a large panel the internal-RAM default resolves to a lot of tiles, and the
draw callback is re-run for every one of them. `setUsePsram(true)` lets you
trade slower memory for far fewer, larger tiles:

```cpp
screen.setUsePsram(true);
screen.setMemoryLimit(2 * 1024 * 1024);   // e.g. a few big tiles instead of hundreds
screen.begin();
if (!screen.tileIsPsram()) {              // PSRAM absent/full -> served from internal RAM
    Serial.println("fell back to internal RAM");
}
```

This is a speed trade, not a free win: drawing into PSRAM and reading it back
are both slower, and LovyanGFX pushes a PSRAM sprite without DMA, so
double-buffering has nothing to overlap and auto mode turns it off. It wins
when the callback is the bottleneck and loses when the transfer is — measure.
A PSRAM request never fails the allocation: if PSRAM is unavailable the buffer
comes from internal RAM and `tileIsPsram()` reports it. See SPEC §10.9.

## Partial updates with `LGFXVirtualSprite`

To update only a part of the screen (a status area, a moving icon, a fixed
viewport), use `LGFXVirtualSprite` — a sub-region of any size that behaves like
a normal sprite but is internally tiled, so it only needs a small buffer. You
draw in the sprite's **local coordinates** (0,0 = its top-left); the library
tiles, clips, and transfers it to the panel position.

```cpp
LGFXVirtualSprite view(lcd, 200, 150, 20, 60);  // 200x150 placed at (20,60)
view.setMemoryLimit(12 * 1024);
view.begin();

void drawView(LGFXVirtualCanvas& g) {            // local coords: 0..200, 0..150
    g.fillScreen(TFT_BLACK);
    g.fillCircle(100, 75, 30, TFT_CYAN);
}

view.render(drawView);            // update just that region (rest of screen untouched)
view.render(drawIcon, x, y);      // or draw at a new position (and remember it)
```

The library handles all transfer, including the partial last tile and any
screen-edge overhang. `LGFXVirtualScreen` is just the special case "the whole
panel" — both share the same tiling engine.

## Cutting transfer with diff transfer

Tiles unchanged since the previous frame can have their transfer skipped. This
targets large, slow panels (a Full-HD USB display, say). It is **disabled by
default** and costs neither memory nor CPU until you turn it on.

```cpp
screen.setDiffMode(LGFXVirtualDiffMode::Tile);
```

Each tile is hashed after your draw callback (8 bytes per tile) and the transfer
is skipped when the hash matches the previous render. **Output pixels never
change** — this is a transfer optimization only.

Only **transfer** shrinks. The draw callback still runs for every tile, and
hashing (one linear pass over each tile buffer) is added on top, so a draw-bound
setup gains nothing and gets slightly slower. Measure whether it pays off:

```cpp
screen.render(drawScene);
Serial.printf("%u / %u px transferred\n",
              (unsigned)screen.diffPushedPixels(),
              (unsigned)screen.diffTotalPixels());
```

> ⚠️ **Call `invalidate()` when anything else touches the screen.** Skipping is
> only correct while "the area we did not send still shows the previous image"
> holds. Reallocation, config changes, a sprite move, and a panel
> rotation/size/color-depth change are detected automatically; nothing else is.
> If you drew directly with `lcd.fillRect()`, overlapped another surface, slept
> or reset the panel, or a USB display reconnected, call `screen.invalidate()`
> (the next `render()` then transfers every tile).

Granularity is whole tiles only. Tile height is derived from the memory budget,
so it gets finer automatically on large panels (Full HD / 16bpp at the default
budget gives a 5-row tile = 1/216 of the screen). See [SPEC.md §21](SPEC.md).

## Overlays and dialogs with transparent transfer

To draw a dialog over a screen you want to keep, render it as an **overlay**:
`renderTransparent()` fills the tile with a transparent color and transfers only
the pixels you drew over it, so the rest of the panel is left as it is.

```cpp
LGFXVirtualScreen base(lcd);      // the screen underneath
LGFXVirtualScreen overlay(lcd);   // the dialog layer

void drawDialog(LGFXVirtualCanvas& g) {   // no fillScreen: undrawn = see-through
    g.fillRoundRect(40, 70, 160, 90, 12, TFT_DARKGREY);
    g.drawRoundRect(40, 70, 160, 90, 12, TFT_WHITE);
    g.drawString("Are you sure?", 52, 84);
}

base.render(drawScene);              // the screen stays on the panel...
overlay.renderTransparent(drawDialog);  // ...and only the dialog is sent
```

The rounded corners keep showing the screen below — that is the point. **If your
overlay is a plain rectangle you do not need this**: an `LGFXVirtualSprite`
placed on the dialog already transfers only that rectangle, and it is faster.
The best combination for a shaped dialog is both — a sprite on the dialog's
bounding box, pushed transparently:

```cpp
LGFXVirtualSprite dialog(lcd, 160, 90, 40, 70);
dialog.renderTransparent(drawDialog);   // local coords, only this box is scanned
```

Every `render(...)` form has a `renderTransparent(...)` twin with the same
arguments. Plain `render()` is completely unaffected.

The transparent color defaults to `TFT_TRANSPARENT` (RGB565 `0x0120`). Change it
only if your drawing genuinely uses that color:

```cpp
overlay.setTransparentColor(lcd.color565(1, 2, 3));
```

> ⚠️ **Colors are read according to their C++ type**, exactly as everywhere else
> in LovyanGFX: `int` / `uint16_t` (the `TFT_*` constants, `color565()`) is
> RGB565, while `uint32_t` is RGB888. `setTransparentColor(TFT_TRANSPARENT)` and
> `setTransparentColor((uint32_t)0x002400)` therefore mean the same color, but
> `setTransparentColor((uint32_t)0x0120)` does not.

Cost: the masked transfer walks one scanline at a time and issues a window setup
per run of visible pixels, so it is slower **per pixel** than `render()` — it wins
by sending far fewer of them (a fully transparent scanline costs no bus traffic at
all). The draw callback still runs for every tile. Combined with diff transfer,
one extra rule applies: after anything redraws what is *underneath* the overlay,
call `overlay.invalidate()`. See [SPEC.md §22](SPEC.md).

## API

### `LGFXVirtualScreen` — the manager

| Member | Description |
|---|---|
| `LGFXVirtualScreen(LovyanGFX& panel, int splitCount = 0)` | Construct over a panel. `0` = auto (≈ 19 KB/tile budget). Nothing is allocated yet. |
| `void setMemoryLimit(size_t bytes)` | Cap the tile buffer; tile height is derived from it (highest priority). |
| `void setSplitCount(int count)` | Fixed number of tiles. |
| `void setTileHeight(int height)` | Fixed tile height in pixels (row splitting). |
| `void setTileWidth(int width)` | Fixed tile width in pixels (column splitting). Same underlying setting as `setTileHeight`. |
| `void setSplitAxis(LGFXVirtualSplitAxis axis)` | `Rows` (default, horizontal bands top to bottom) or `Columns` (full-height bands left to right). See SPEC §10.8. |
| `LGFXVirtualSplitAxis splitAxis() const` | Current split axis. |
| `void setUsePsram(bool enable)` | Allocate the tile buffer(s) in PSRAM (default off). Slower memory, no DMA (auto double-buffering turns off), falls back to internal RAM if unavailable. See SPEC §10.9. |
| `bool usePsram() const` / `bool tileIsPsram() const` | What was requested / where the buffer actually landed. |
| `void setBackgroundColor(uint32_t color)` | auto-clear color (default black). Not used by `renderTransparent*()`. |
| `void setAutoClear(bool enable)` | Clear each tile before draw (default `true`). |
| `void setDoubleBuffer(bool enable)` | Use two tile buffers so a tile's DMA transfer overlaps the next tile's draw (faster, 2× tile RAM). Overrides the default **auto** mode (on when ≥ 2 tiles, off for a single tile). See SPEC §10.5. |
| `void setTransparentColor(color)` | Color `renderTransparent*()` leaves out of the transfer (default `TFT_TRANSPARENT` = RGB565 `0x0120`). Read according to the value's C++ type. See SPEC §22.4. |
| `uint32_t transparentColor() const` | Current transparent color, as RGB888. |
| `void setDiffMode(LGFXVirtualDiffMode mode)` | Diff-transfer granularity: `Off` (default) or `Tile` (skip transferring tiles unchanged since the previous render). Reduces transfer only. See SPEC §21. |
| `LGFXVirtualDiffMode diffMode() const` | Current diff-transfer mode. |
| `void invalidate()` | Declare that the panel content can no longer be trusted (the next `render()` transfers every tile). Call it whenever something other than this object touched the screen. |
| `size_t diffMemoryUsage() const` | Hash table size in bytes (0 when `Off`). |
| `uint32_t diffPushedPixels() const` / `uint32_t diffTotalPixels() const` | Pixels the last `render()` actually transferred / would have transferred without diffing. |
| `bool begin()` | Allocate the tile buffer now. `false` on failure (no fallback). |
| `bool isReady() const` | Whether the buffer is allocated. |
| `int tileCount() const` / `int tileHeight() const` / `int tileWidth() const` / `int tileSpan() const` | Resolved geometry after allocation (`tileSpan` = the extent along the split axis). |
| `bool render(draw)` | Render `void draw(LGFXVirtualCanvas&)`. |
| `bool render(draw, ctx)` | Render `void draw(LGFXVirtualCanvas&, T&)` with your `ctx`. |
| `bool renderTransparent(draw)` / `renderTransparent(draw, ctx)` | Same, as an overlay: the tile starts filled with `transparentColor()` and those pixels are not transferred. See SPEC §22. |

`render` returns `false` (and draws nothing) if the buffer is not allocated.
The draw callback must be a **function pointer** (capturing lambdas /
`std::function` are not accepted, to keep code size down).

Priority when several are set: `setMemoryLimit` > `setSplitCount` >
`setTileHeight` / `setTileWidth` > default (≈ 19 KB/tile budget). With nothing set,
double-buffering is auto-enabled when the surface resolves to ≥ 2 tiles.

### `LGFXVirtualSprite` — a tiled sub-region

Same configuration and `render(...)` overloads as `LGFXVirtualScreen`, plus:

| Member | Description |
|---|---|
| `LGFXVirtualSprite(LovyanGFX& panel, int w, int h, int x = 0, int y = 0)` | A `w × h` tiled sprite at panel position `(x, y)`. Size is fixed; nothing allocated yet. |
| `void setPosition(int x, int y)` | Move the sprite (no reallocation). |
| `int x()` / `int y()` / `int width()` / `int height()` | Current position / size. |
| `bool render(draw)` / `render(draw, x, y)` | Draw at the current / given position. `(x, y)` also updates the stored position. |
| `bool render(draw, ctx)` / `render(draw, ctx, x, y)` | Typed-context variants. |
| `bool renderTransparent(draw)` / `(draw, x, y)` / `(draw, ctx)` / `(draw, ctx, x, y)` | Overlay versions of each `render` form: `transparentColor()` pixels are not transferred. See SPEC §22. |

Inside the draw callback, coordinates are **local to the sprite** (0,0 = its
top-left), and `g.width()/g.height()` return the sprite size.

### `LGFXVirtualCanvas` — the drawing surface

Passed to your draw function. It looks like a normal LGFX/M5GFX canvas but
maps your full-screen (virtual) coordinates onto the current tile.

The current release wraps the LovyanGFX/M5GFX APIs that can be provided safely
from a tiled virtual surface: coordinate-bearing drawing, text, image decode /
push helpers, color/state utilities, and readback from the current tile. APIs
that rely on stream cursors, caller-managed windows, cross-tile pixel movement,
or sprite-to-destination transfer are intentionally left out and listed below.

Supported:

- Geometry: `width()`, `height()` (the full virtual screen)
- Color utilities: `color332`, `color565`, `color888`, `swap565`,
  `swap888`, `color16to8`, `color8to16`, `color16to24`, `color24to16`
- State: `setColor`, `setRawColor`, `getRawColor`, `setBaseColor`,
  `getBaseColor`, `getColorDepth`, `hasPalette`, `getPaletteCount`,
  `getPalette`, `setPaletteColor`, `setPivot`, `getPivotX/Y`, `createGradient`,
  `mapGradient`
- Shapes: `fillScreen`, `drawPixel`, `drawLine`, `drawFastHLine`,
  `drawFastVLine`, `writePixel`, `writeFastHLine`, `writeFastVLine`,
  `fillRect`, `writeFillRect`, `writeFillRectPreclipped`, `drawRect`,
  `fillRoundRect`,
  `drawRoundRect`, `drawCircle`, `fillCircle`, `drawEllipse`,
  `fillEllipse`, `drawTriangle`, `fillTriangle`, `drawBezier`,
  `drawEllipseArc`, `fillEllipseArc`, `drawArc`, `fillArc`, `clear`,
  `clearDisplay`, `drawCircleHelper`, `fillCircleHelper`,
  `drawGradientHLine`, `drawGradientVLine`, `drawGradientLine`,
  `fillGradientRect`, `fillRectAlpha`, `drawSmoothLine`, `drawWideLine`,
  `drawWedgeLine`, `drawSpot`, `drawGradientSpot`, `fillSmoothRoundRect`,
  `fillSmoothCircle`
- Image: `pushImage`, `pushImageDMA`, `pushImageRotateZoom`,
  `pushImageRotateZoomWithAA`, `pushGrayscaleImage`,
  `pushGrayscaleImageRotateZoom`, `pushAlphaImage`, `drawBitmap`,
  `drawXBitmap`, `setSwapBytes`, `getSwapBytes`, `drawBmp`, `drawBmpFile`,
  `drawJpg`, `drawJpgFile`, `drawPng`, `drawPngFile`, `drawQoi`,
  `drawQoiFile`, `qrcode`
- Readback: `readPixel`, `readPixelRGB`, `readPixelValue`, `readRectRGB`,
  `readRect`
- Text: `setCursor`, `getCursorX/Y`, `setTextColor`, `setTextSize`,
  `getTextSizeX/Y`, `setTextDatum`, `getTextDatum`, `setTextPadding`,
  `getTextPadding`, `setTextWrap`, `setTextScroll`, `setEmojiCallback`,
  `getEmojiCallback`, `setTextStyle`, `getTextStyle`, `setFont`,
  `setTextFont`, `setFreeFont`, `getFont`, `getTextFont`, `fontHeight`,
  `fontWidth`, `textWidth`, `textLength`, `drawString`, `drawCentreString`,
  `drawCenterString`, `drawRightString`, `drawNumber`, `drawFloat`,
  `drawChar`, `print`, `println`, `write`, `printf`, `vprintf`

Calling a method that is not wrapped is a compile error — by design, so
unsupported or unsafe drawing paths fail loudly rather than silently.

Not adopted function groups:

- Low-level streaming writes: `writeColor`, `pushBlock`, `writePixels`,
  `writePixelsDMA`, `pushPixels`, `pushPixelsDMA`, `pushColor`, `pushColors`.
  These APIs depend on the caller-managed write window / stream cursor and do
  not carry enough virtual coordinate information for safe per-tile clipping.
- Window, clip, and transaction controls: `setWindow`, `startWrite`,
  `endWrite`, `beginTransaction`, `endTransaction`, `initDMA`, `waitDMA`.
  `LGFXVirtualScreen` / `LGFXVirtualSprite` own those operations while rendering
  tiles; exposing them on the callback canvas would let user code break the
  manager's clipping and DMA ordering guarantees.
- Scroll and copy: `scroll`, `copyRect`, scroll-rect APIs. These mutate or copy
  existing pixels across a surface. A tile only contains one band of the virtual
  surface, so cross-tile source/destination pixels are not available.
- Sprite-to-panel transfer helpers: `pushSprite`, `pushRotated`,
  `pushRotatedWithAA`, `pushRotateZoom`, `pushRotateZoomWithAA`, `pushAffine`,
  `pushAffineWithAA`. These are methods for transferring an `LGFX_Sprite`
  itself to another destination; `LGFXVirtualCanvas` is already the destination
  surface, and tile flushing is handled by the manager.
- Affine image helpers: `pushImageAffine`, `pushImageAffineWithAA`,
  `pushGrayscaleImageAffine`. The affine matrix embeds destination coordinates,
  so a simple `y -= offsetY` wrapper is not sufficient for all matrices.
- Output/export helpers: `createPng`, `releasePngMemory`. These operate on the
  current tile buffer, not the full virtual surface, so they belong on a future
  manager-level export API if needed.

### Detecting availability

Including `LGFXVirtualCanvas.h` defines `LGFXVIRTUALCANVAS_H`, so other code or
libraries can detect it and integrate optionally:

```cpp
#if defined(LGFXVIRTUALCANVAS_H)
    // LGFXVirtualCanvas is available — LGFXVIRTUALCANVAS_VERSION_STR etc. are too
#endif
```

The header also pulls in `LGFXVIRTUALCANVAS_VERSION_MAJOR` / `_MINOR` / `_PATCH`
/ `_STR` for version checks.

## How it works

LovyanGFX has no drawing-origin translation and its primitives are
non-virtual, so the offset cannot be hidden behind a raw `LovyanGFX&`.
LGFXVirtualCanvas is therefore a small concrete class whose every method does
`y -= offsetY` and forwards to the tile sprite; out-of-tile drawing is clipped
away by the sprite. Full-screen rendering is just the 1-tile case, so the same
code path proves correctness. Rationale in [SPEC.md §6](SPEC.md).

## Limitations

- **One-dimensional tiling**: rows (default) or columns, not a 2-D grid.
- **Function pointers only** for the draw callback (no capturing lambdas).
- **Text wrapping is disabled under column splitting.** LovyanGFX wraps at the
  tile sprite's own right edge, which is a tile boundary there — that would make
  the output depend on the split count, so `setTextWrap(true)` is ignored in
  column mode and long lines are clipped instead. Newlines are corrected back to
  the surface's left edge, so `println` / `printf` behave as expected.
- **Neighbor-dependent drawing across tile boundaries** (anti-aliasing,
  smooth/wide lines, blur, neighbor-sampling filters) may not match a full
  render, because each tile is redrawn and clipped independently. LovyanGFX's
  default primitives are not anti-aliased, so this is usually a non-issue.
- Image decoders (`drawBmp` / `drawJpg` / `drawPng` / `drawQoi` and `*File`)
  are wrapped for coverage, but they run once per tile. For performance, decode
  once into a sprite/image buffer and use `pushImage` when possible.
- Text behavior that depends on the buffer height (auto-scroll, bottom-edge
  wrap) is not guaranteed; basic cursor/print/drawString are.
- No retained mode / draw-command recording: redraw the whole scene each frame.
  Diff transfer (off by default) skips **transfer** for unchanged tiles; it does
  not reduce drawing.

## Examples

See [examples/](examples/): `HelloWorld`, `BouncingBall` (state + animation),
`MemoryBudget` (budget + failure handling), `Viewport`
(`LGFXVirtualSprite` partial update), `DiffTransfer` (diff transfer),
`ColumnSplit` (column tiles + PSRAM buffers), `Dialog` (transparent overlay),
`LovyanGFX_Basic`.

## Testing

Correctness is verified **headlessly** on the host: the same scene rendered at
several split counts must be pixel-identical to the single-tile (full) render.
See [tests/README.md](tests/README.md) and [SPEC.md §13](SPEC.md).

## License

[MIT](LICENSE) © TANAKA Masayuki
