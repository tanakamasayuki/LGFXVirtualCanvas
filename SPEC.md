# LGFXVirtualCanvas — Requirements

> 日本語: [SPEC.ja.md](SPEC.ja.md)

## 0. Decision summary

Key decisions settled so far:

- **Two classes (concrete only — no abstract base, no virtual, no templates).**
  - `LGFXVirtualScreen` … the manager (device side). Holds the real panel, owns the split config, runs `render()`.
  - `LGFXVirtualCanvas` … the drawing surface (draw side). The virtual canvas your draw function receives. Matches the library / include name.
- **Draw-function signature** is `void draw(LGFXVirtualCanvas& g)` or `void draw(LGFXVirtualCanvas& g, T& ctx)`. Function pointers only.
- **The panel is taken as `LovyanGFX&`** (the common base class reference). An `LGFX`, `M5GFX`, or `M5.Display` can all be passed.
- **Memory is allocated lazily.** Nothing is allocated in the constructor (the screen size is unknown before `lcd.init()`). `begin()` can allocate eagerly; if `render()` is called while unallocated it allocates on the spot (guardrail).
- **Allocation failure does not fall back — it is reported as failure.** `begin()` / `render()` return `bool`; while unallocated `render()` draws nothing (so breakage is obvious).
- **Splitting is specified primarily by a memory budget.** `setMemoryLimit(bytes)` (in bytes; default 0 = unset) takes priority, then split count, then a built-in default budget of ≈ 19 KB per tile (`DEFAULT_TILE_BYTES`). **Double-buffering is auto-enabled** when the resolved surface needs ≥ 2 tiles (override with `setDoubleBuffer`).
- **Clipping is automatically safe via the sprite's standard per-pixel clip** (out-of-range drawing simply disappears).
- **Each tile is cleared to a background color before draw (auto-clear, default ON).** The background color is configurable (default: black). Undrawn pixels become deterministic (the background color) so the result is identical regardless of split count. Disable with `setAutoClear(false)`.
- **Two managers**: `LGFXVirtualScreen` (whole panel) and `LGFXVirtualSprite` (a placed sub-region of any size = an internally-tiled sprite, local coordinates). Both share the internal tiling engine and hand your draw function the same `LGFXVirtualCanvas`; only the transfer target (full panel vs a placed rectangle) differs. See §7.1.

The rationale (especially why a dedicated concrete class) is in §6.

## 1. Library name

`LGFXVirtualCanvas` (included via `#include <LGFXVirtualCanvas.h>`; a single header declares `LGFXVirtualScreen`, `LGFXVirtualSprite`, and `LGFXVirtualCanvas`).

## 2. Purpose

In a LovyanGFX / M5GFX environment, provide a tiled-rendering library that lets the user draw as if to a virtual full-screen canvas, even when a full-screen double buffer cannot be allocated.

Internally the screen is split into several vertical tiles, drawn into a small `M5Canvas` / `LGFX_Sprite`-equivalent buffer and pushed to the display in turn.

The goal is that the user draws without being aware of the split count, the offset, or the tile boundaries.

## 3. Core principles

- No full-screen framebuffer.
- Do not record all drawing commands.
- Re-run the draw callback per tile.
- The user draws in a virtual coordinate system.
- The Y offset is hidden inside the library.
- The library holds no state or model.
- The library is responsible only for providing the draw target, splitting, clipping, and flush.
- Memory is allocated lazily; the screen size is read from the real panel at `begin()` / first `render()`.

## 4. Expected usage code (settled)

```cpp
#include <LovyanGFX.hpp>
#include <LGFX_AUTODETECT.hpp>
#include <LGFXVirtualCanvas.h>          // declares both classes (neutral policy: include your gfx library first)

static LGFX lcd;
LGFXVirtualScreen screen(lcd);          // split count omitted = auto. Nothing allocated yet.

// -- App state (the library is not involved at all / §15) --
struct AppState {
    int score;
    int playerX;
    int playerY;
};
AppState state;

// -- Draw function: draws to the virtual full screen. Offset and tile boundaries are invisible. --
void drawScene(LGFXVirtualCanvas& g, AppState& s) {
    g.fillScreen(TFT_BLACK);
    g.setCursor(10, 10);
    g.printf("Score %d", s.score);
    g.fillRect(s.playerX, s.playerY, 16, 16, TFT_GREEN);   // virtual coordinates
}

void setup() {
    lcd.init();                         // screen size becomes known here
    screen.setMemoryLimit(20 * 1024);   // optional: set a RAM budget (the primary knob)
    screen.begin();                     // optional: allocate eagerly here (may be omitted)
}

void loop() {
    updateState(state);
    screen.render(drawScene, state);    // even without begin(), allocates here (guardrail)
}
```

When no state is needed (§16):

```cpp
void drawScene(LGFXVirtualCanvas& g) {
    g.fillScreen(TFT_BLACK);
    g.drawString("Hello", 10, 10);
}

screen.render(drawScene);
```

## 5. Draw callback

### 5.1 The form the user writes

```cpp
void draw(LGFXVirtualCanvas& g);
```

or

```cpp
void draw(LGFXVirtualCanvas& g, Context& ctx);
```

`g` is a reference to the concrete class `LGFXVirtualCanvas` — not a template and not an abstract base (see §6 for why).

### 5.2 Internal form

Internally everything funnels into:

```cpp
using DrawRaw = void (*)(LGFXVirtualCanvas& g, void* ctx);
bool render(DrawRaw draw, void* ctx = nullptr);   // true if drawn, false if unallocated (§10.3)
```

### 5.3 Typed convenience API

So the user never touches `void*` directly, typed overloads are provided.

```cpp
bool render(void (*draw)(LGFXVirtualCanvas& g));               // no ctx
template <typename T>
bool render(void (*draw)(LGFXVirtualCanvas& g, T& ctx), T& ctx); // typed ctx
```

Only **function pointers** are accepted (no capturing lambdas, no `std::function`). A deliberate trade-off to keep code size down.

### 5.4 Template usage policy

- **The tile loop body is not templated** (the draw function is fixed to `LGFXVirtualCanvas&`, so it is not duplicated per type).
- Templates are limited to "thin sugar that converts a typed callback into the internal `void*` form" and "preserving the color argument type of drawing methods".
- This keeps code size and compile time down.

## 6. Why a "dedicated concrete class" (the most important design decision)

The user's draw function receives a **dedicated concrete class `LGFXVirtualCanvas&`**, not `LovyanGFX&`. This is a necessary choice based on inspecting the LovyanGFX 1.2.21 implementation.

### 6.1 Constraints on the LovyanGFX side (findings)

- **There is no drawing-origin translation API.** `LGFXBase` has only `setClipRect`; there is no `setOrigin` / `translate` / drawing `setOffset`.
- **Drawing primitives are non-virtual.** `drawPixel` / `fillRect` etc. are all `LGFX_INLINE` (non-virtual). The only virtuals on `LGFXBase` are a few like `init_impl`; none of the drawing methods are virtual.

### 6.2 What follows

- Passing a raw `LovyanGFX&` to a small sprite of height `tileH` gives **no way to translate the virtual Y coordinate into the tile-local Y**. If a virtual `y` is outside the tile it is simply clipped away, and the tiles do not reassemble correctly.
- Because the methods are non-virtual, even if you derive from `LovyanGFX` and override `fillRect` etc., a call through `LovyanGFX&` **always dispatches to the base (non-translating) method** and cannot be intercepted.

Therefore a layer that subtracts `offsetY` and forwards must sit in front of the draw function, and that is impossible with the `LovyanGFX&` type. **A dedicated concrete class whose every method does `y -= offsetY` and forwards to the internal sprite** is the only natural way to do it.

### 6.3 Why neither an abstract base (virtual) nor templates

- **One implementation is enough.** Full-screen rendering is just the special case of "one tile (offsetY=0, height = full screen)" expressed by the same `LGFXVirtualCanvas`. No separate full/tile implementations are needed → no abstract base, no virtual.
- **Predictable code size.** No virtual means no vtable. No templates means the draw function and tile loop are not duplicated per type. Bounded to a dozen-odd forwarding methods plus the loop.
- **IDE completion works, and it is safe.** Only the supported methods are declared on the concrete class, so completion only offers supported methods. Calling an unsupported method is a compile error — it fails safe.
- **Almost nothing is lost.** A virtual base would allow "accept both raw `LovyanGFX&` and the concrete class polymorphically" in the future, but per §6.1 that is impossible anyway since LovyanGFX is non-virtual. If polymorphism is ever needed, extract an interface from this concrete class later (§17).

## 7. Responsibilities of LGFXVirtualCanvas (the drawing surface)

`LGFXVirtualCanvas` wraps the internal tile sprite and converts the user-visible virtual coordinates into real tile coordinates.

Main responsibilities:

- Subtract the tile offset from the virtual Y coordinate (`y -= offsetY`).
- Out-of-tile drawing disappears automatically via the internal sprite's clip (§12).
- Never expose `offsetY` to the user.
- Look as much as possible like a normal LGFX / M5GFX Canvas (same method names and arguments).

Responsibilities of `LGFXVirtualScreen` (the manager):

- Hold the real panel (`LovyanGFX&`).
- Hold the split config (memory budget / split count / tile height).
- Allocate the tile sprite at `begin()` / first `render()`.
- In `render()`, create an `LGFXVirtualCanvas` per tile, re-run draw, and push to the real panel.

### 7.1 LGFXVirtualSprite (partial update / sub-region)

`LGFXVirtualSprite` draws a **sub-region of any position and size**. It feels like a normal `LGFX_Sprite`, but it is internally split into vertical tiles so it only needs a small buffer. Use it to update only the part that changes dynamically (e.g. a fixed viewport inside a 320×240 screen, or a moving icon).

- **Coordinates are local to the sprite** (0,0 = top-left of the sprite), not full-screen virtual. `width()/height()` return the sprite size.
- **The size is fixed at construction** (before `begin()`): `LGFXVirtualSprite spr(panel, w, h, x = 0, y = 0)`. The buffer is allocated as width `w` × `tileH` (from the memory budget / split config).
- **The panel position is movable**: `render(draw)` uses the current position; `render(draw, x, y)` draws there and updates the current position (`setPosition` also works). Moving does not reallocate.
- **The library performs all transfer**: before each push the panel clip rect is set to `(x, y, w, h)`, so the **partial last tile and any screen-edge overhang are clipped automatically** — the user reduces nothing (LovyanGFX's push honors the destination panel's clip rect, verified).
- `LGFXVirtualScreen` is effectively an `LGFXVirtualSprite` of "size = panel, position = (0,0)"; both share the internal engine (`LGFXVirtualTiledBase`).
- The memory budget, auto-clear, and no-fallback-on-failure policies are the same as the full-screen version (§10/§11).

```cpp
// A 240x240 tiled sprite placed at (40,0); local coordinates.
LGFXVirtualSprite view(lcd, 240, 240, 40, 0);
view.setMemoryLimit(16 * 1024);
view.begin();
view.render(drawView);            // at the current position
// To move it:
view.render(drawIcon, x, y);      // draw at (x,y) and update the current position
```

## 8. API wrapping policy

Ultimately the major drawing methods are wrapped individually on `LGFXVirtualCanvas`. Only supported methods exist; calling an unsupported method is a compile error (we grow the supported set incrementally).

Priority methods to implement:

- `fillScreen`
- `drawPixel`
- `drawLine`
- `drawFastHLine`
- `drawFastVLine`
- `fillRect`
- `drawRect`
- `fillRoundRect`
- `drawRoundRect`
- `drawCircle`
- `fillCircle`
- `drawEllipse`
- `fillEllipse`
- `drawTriangle`
- `fillTriangle`
- `drawString`
- `drawCentreString`
- `drawRightString`
- `setFont`
- `setTextFont`
- `setCursor`
- `print`
- `println`
- `printf`
- `pushImage`

Color-taking methods preserve the color argument's type via a template and forward it unchanged to the internal sprite (thin type-preserving sugar; §5.4). String-taking methods (`drawString` family) template both the string type (`const char*` / `String`) and the font argument (`uint8_t` / `IFont*`); `print` / `println` generically forward all Arduino Print overloads (numbers, base, etc.).

> Implementation status: **all priority methods above are implemented** (verified by the parity / pushimage tests). `setTextColor` / `setTextSize` / `setTextDatum` / `setCursor` / `getCursorX/Y` (returns virtual coords) are also provided.

## 9. APIs that need special care

### 9.1 Text drawing

The following carry internal state, so take care:

- `setCursor`
- `print`
- `println`
- `printf`
- `drawString`
- `setTextDatum`

The cursor's Y coordinate is also treated as a virtual coordinate (`setCursor` subtracts `offsetY`). Cursor advancement by `print` / `println` / `printf` is reproduced deterministically on each draw re-run and clipped per tile, so text that crosses a tile boundary still reassembles (except for the neighbor-dependency limit in §12.1).

However, **behavior that depends on the internal sprite height (`tileH`)** — text auto-scroll (what happens when the cursor reaches the bottom), wrapping at the bottom edge, etc. — runs against `tileH` rather than the virtual full-screen `height`, so it may not match full-screen rendering. These are **decided by implementation/experiment; any method that cannot be provided safely becomes an unsupported method** (calling it is a compile error; §8). The basic `setCursor` / `print` / `println` / `printf` / `drawString` (with virtual-Y offset) are provided. Cursor-reading APIs like `getCursorY` add `offsetY` back and return virtual coordinates.

### 9.2 pushImage

`pushImage` may cross tile boundaries, so it needs dedicated clipping.

Cases to consider:

- Entirely inside the tile
- Overhangs the top
- Overhangs the bottom
- Straddles a tile boundary
- Off-screen
- The partial height of the last tile

**Experiment result (supported)**: `tests/pushimage` confirms split=1 equals split=2/3/5/7. All the cases above (top/bottom overhang, boundary straddle, off-screen) matched full-screen rendering with the internal sprite's per-pixel clip alone, so it is **supported with `y -= offsetY` forwarding only**. LGFX's `pushImage` offsets into the source for negative destination Y and clips correctly, so no dedicated clipping was needed. The transparent-color variant, palette depths, and rotate/zoom variants are still to be verified.

## 10. Splitting and memory allocation

The screen is split vertically. Specification uses a memory budget as the primary knob, with split count and tile height coexisting.

### 10.1 Specification and priority

At `begin()` / first `render()`, tile height `tileH` and split count `N` are decided in this priority order. Each setting has a default of 0 meaning "unset"; settings that are 0 are skipped and fall through to the next.

1. `setMemoryLimit(bytes)` with `bytes > 0` (**default 0 = unset**, in bytes) → `tileH = floor(bytes / (width * bytesPerPixel))`, `N = ceil(height / tileH)`. ← **primary**
   - The limit is only an upper bound. `tileH` is clamped to the screen height (`tileH > height` → 1 tile).
   - If `bytes` is below one row (`width * bytesPerPixel`) so `tileH < 1`, the request cannot be satisfied and is treated as an allocation failure (§10.2; no silent rounding).
2. Split count `k` (constructor argument or `setSplitCount(k)`, `k > 0`) → `tileH = ceil(height / k)`, `N = k`.
3. `setTileHeight(h)` with `h > 0` → `tileH = h`, `N = ceil(height / h)`.
4. None set (constructor omitted = split count 0 = auto) → **default tile budget `DEFAULT_TILE_BYTES` (≈ 19 KB, = a 320×30 tile at 16 bpp)**, applied exactly like `setMemoryLimit`: `tileH = floor(DEFAULT_TILE_BYTES / (width × bytesPerPixel))` (clamped to `height`), `N = ceil(height / tileH)`. This makes the split count **scale with surface size** — a small surface resolves to a single tile, full-screen to several — while keeping each tile ≈ 19 KB. The value is taken from the Core2 benchmark (`bench/`), where it reproduces the measured optimum split at every tested size.

`width` / `bytesPerPixel` / `height` are read from the real panel (after `lcd.init()`).

Once `tileH` / `N` are resolved, the **double-buffer mode** is decided (§10.5): in the default *auto* state, double-buffering turns on when `N ≥ 2` and stays off for a single tile.

### 10.2 Allocation timing (lazy + optional begin)

- **Constructor**: only stores the `LovyanGFX&` reference and the config. Allocates nothing (this may be before `lcd.init()`, when the screen size is still unknown).
- **Setter methods** (`setMemoryLimit` / `setSplitCount` / `setTileHeight`): call before allocation. Each default is 0 (unset).
- **`bool begin()`**: optional. Reads size and color depth from the real panel, decides `tileH` per §10.1, and **allocates one reusable tile sprite of width × tileH**. Returns `true` on success, `false` on failure.
- **Guardrail**: if `render()` is called without `begin()`, it attempts the `begin()`-equivalent allocation on the spot.
- If the config changes after allocation, re-run `begin()` to rebuild.

### 10.3 On allocation failure (no fallback)

Allocation fails mainly when: RAM cannot hold the sprite for the requested `tileH`, or the memory budget is below one row so `tileH < 1`.

In that case **no fallback is performed** (no silently increasing the split count, no shrinking tiles). The caller must be able to reliably detect that the request did not go through.

- `begin()` returns `false` (checking the allocation result in `setup()` is recommended).
- While unallocated (`isReady() == false`), `render()` **draws nothing** and returns `false`. No partial drawing or garbage.
- `render()` itself also returns `bool` (`true` if drawn, `false` if unallocated).
- The failure reason may be logged for debugging (not required).

```cpp
void setup() {
    lcd.init();
    screen.setMemoryLimit(20 * 1024);
    if (!screen.begin()) {
        // Allocation failed. No fallback — you find out here.
        Serial.println("LGFXVirtualScreen: alloc failed");
    }
}
```

State query: `bool isReady() const;` (whether the tile buffer is allocated).

### 10.4 Tile render loop

For each tile `i` (`0..N-1`, `offsetY = i * tileH`):

1. **Clear the reusable tile sprite to the background color** (auto-clear; §11). Skipped when `setAutoClear(false)`.
2. Construct an `LGFXVirtualCanvas`(sprite, offsetY).
3. Call the draw callback.
4. Push the tile content to the real panel with `pushSprite(0, offsetY)`. For a partial last tile, the surplus rows at the bottom of the sprite are discarded by the panel's clip.

The whole loop's panel transfers are wrapped in a single `startWrite()` / `endWrite()`, batching the N pushes into one bus transaction (fewer SPI transaction setups / CS toggles on real hardware). Drawing into the tile is memory-only (no bus), so the draw callback itself needs no such bracketing. No-op on the bus-less host backend. For `LGFXVirtualSprite` the transfer target is `(x, y)` and the panel clip is set to `(x, y, w, h)` before pushing (§7.1).

### 10.5 Buffer reuse, DMA, and double-buffering

`pushSprite` of an internal-RAM (DMA-capable) tile starts an **asynchronous** SPI-DMA that reads the tile buffer directly and returns before the transfer completes; within the loop's outer `startWrite`/`endWrite` no per-tile bus wait happens (the nested per-push `startWrite`/`endWrite` only flush at the outermost level). So with a **single** reused tile buffer the next tile's clear/draw would overwrite the buffer while the previous tile's DMA is still reading it — corrupting the tile being transferred.

Two modes resolve this; the active one is chosen by the **double-buffer mode** (default *auto*, below):

- **Single buffer.** After each `pushSprite` the loop calls `waitDMA()` so the transfer finishes before the buffer is reused. Correct, lowest memory, but draw and transfer are **serialized** (frame ≈ Σ draw + Σ transfer). `split=1` needs no extra wait (one push, then the final `endWrite` flushes). PSRAM tiles are inherently safe: LGFX disables DMA for SPIRAM sprites, so the transfer is synchronous (safe but slower).
- **Double buffer.** Two tile sprites are allocated and ping-ponged (`i & 1`). Tile `i` transfers (async DMA) from one buffer while tile `i+1` is drawn into the other. Consecutive transfers on one SPI bus are serialized by the bus itself, so the buffer reused at tile `i` (last touched at tile `i-2`) is guaranteed free of in-flight DMA — no in-loop wait is needed, and CPU draw overlaps SPI transfer (frame ≈ max(draw, transfer) in the transfer-bound regime). Costs 2× the tile buffer; a `setMemoryLimit` still bounds each individual buffer. Allocation is all-or-nothing: if the second buffer cannot be allocated, `begin()`/`render()` fail (no fallback, per §10.3).

**Mode selection.** `setDoubleBuffer(true|false)` forces the choice. Left unset, the mode is **auto**: double-buffering is enabled whenever the resolved tile count `N ≥ 2`, and disabled for a single tile (`N == 1`, where there is no neighbouring tile to overlap with, so a second buffer would be pure waste). Combined with the default tile budget (§10.1) — which grows `N` with surface size while each tile stays ≈ 19 KB — auto resolves a small surface to **one single-buffered tile** (fastest, minimal RAM) and a large surface to **many small double-buffered tiles** (overlap hides transfer). Total tile RAM is then ≈ 2 × budget regardless of surface size, and the small per-tile allocations avoid the large-contiguous-block failures a full-screen buffer would hit. This auto choice is part of *resolving* the config, not a runtime fallback: if the resolved (possibly double) allocation fails, `begin()`/`render()` still fail per §10.3 — they do **not** silently drop to single buffer.

Both modes produce **pixel-identical** output (verified by the parity test's double-buffer cases). On the bus-less host backend `waitDMA()` is a no-op and both modes render the same.

### 10.6 Why ≈ 19 KB per tile, and the draw-bound vs transfer-bound trade-off

The default budget (§10.1) is a single number that has to pick a good split count *and* the auto double-buffer decision (§10.5) across every surface size, without knowing what the draw callback does. Here is the reasoning, because the result is subtle — and inverts a common intuition.

**The two costs per frame.** With double-buffering the frame time is, roughly, a pipeline of per-tile draws overlapped with per-tile transfers:

```
frame ≈ draw₀ + Σ max(drawᵢ₊₁, xferᵢ) + xfer_last   ≈   max(Σdraw, Σxfer)
```

so the frame is governed by whichever total is larger:

- **`Σxfer` (SPI transfer) is essentially fixed per frame.** Every pixel of the surface is pushed exactly once, so `Σxfer` depends only on area, not on split count. A full 320×240×16bpp frame ≈ 32 ms on Core2 (≈ 31 fps — the hardware SPI floor); a smaller surface is proportionally less.
- **`Σdraw` (CPU draw) is *not* fixed — it grows with the split count.** The draw callback is re-run once per tile (§5.4): every primitive is walked for every tile and clipped to it, even though only its in-tile span rasterizes. So a scene of *few large* primitives grows slowly with `N`, but a scene of *many* primitives (lots of shapes, text) has `Σdraw` rise steeply with `N`.

**The consequence (this is the part that inverts intuition).** Increasing the split count never reduces `Σdraw`; it only ever adds redundant per-primitive work and per-tile overhead (`fillScreen`, `pushSprite`, `waitDMA`). So:

- **Transfer-bound (`Σdraw < Σxfer`).** Transfer is the floor and overlap hides the draw. More, *smaller* tiles overlap more finely (and, double-buffered, cost *less* total RAM) → **increasing `N` helps**, up to the point where the transfer is fully overlapped.
- **Draw-bound (`Σdraw > Σxfer`).** Transfer is already hidden behind the draw; adding tiles only piles on redundant redraw. → **you want *fewer*, larger tiles** — ideally a single tile (one draw pass, nothing to overlap). **Increasing `N` makes it worse.**

So "two buffers overlapping draw and transfer is fastest" is true *only in the transfer-bound regime*. When the draw is the bottleneck, the best you can do is one draw pass (`Σdraw` at `N=1`) — and a single buffer already achieves that. The optimum `N` is the interior balance between overlap gain (which saturates at the transfer floor) and redundant-draw + overhead (which grows with `N`); where it lands depends on the **scene's** draw cost.

**Where ≈ 19 KB comes from.** 19,200 B = a 320×30 tile at 16 bpp. A 30-row tile transfers in ≈ 4 ms and, for typical buffered GUI/animation scenes, draws in less — so a *large* surface stays transfer-bound at the resolved split and sits on the SPI floor, while a *small* surface resolves to a single tile (the draw-bound regime, where one tile is exactly right). On the Core2 benchmark (`bench/`) this one value reproduces the **measured-optimal** split at every tested size:

| surface  | resolved split (19 KB) | matches bench optimum |
|----------|------------------------|-----------------------|
| 64×48    | 1 (single buffer)      | ✓ |
| 128×96   | 2 (double)             | ✓ |
| 160×100  | 2 (double)             | ✓ |
| 240×160  | 4 (double)             | ✓ |
| 320×240  | 8 (double)             | ✓ |

It also bounds double-buffer RAM to ≈ 2 × 19 KB ≈ 38 KB *regardless of surface size* (only two tiles exist at once), and keeps every allocation small — avoiding the ~150 KB contiguous block a full-screen buffer needs (which fails even with 300 KB free; see `bench/`).

**Can the library detect the regime and pick `N` itself?** Not from geometry alone: the draw callback is arbitrary user code whose per-frame cost the library cannot see ahead of time, and it can vary frame to frame. The default is therefore a fixed heuristic tuned for the transfer-bound-to-balanced workloads that buffered GUIs and animations usually are. **If your scene is unusually draw-heavy** — so that even the resolved tiles are draw-bound — prefer *fewer, larger* tiles: raise `setMemoryLimit()` or pin a low `setSplitCount()` (and a single buffer is often best). A runtime auto-tuner (measure `drawᵢ`/`xferᵢ` and adapt `N` with hysteresis) is feasible but is deliberately **out of scope for the default**, since it implies reallocation and frame-time jitter.

### 10.7 Costly per-callback work (image decode, flash / PSRAM reads)

§10.6 showed `Σdraw` grows with `N` because the callback re-runs per tile. For ordinary primitives that growth is modest. It becomes **severe** when the callback does fixed-cost work that the tile clip rectangle does *not* shrink — that work is paid in full on every tile, so it scales ≈ linearly with `N`:

- **Decoding a compressed image (PNG / JPEG) in the callback.** The decoder must process the whole compressed stream to emit any row (DEFLATE / MCU decode is sequential); the clip only discards out-of-tile *output* pixels, not the decode. So a full-screen `drawPng` / `drawJpg` inside the tiled callback is **re-decoded on every tile** — `N` tiles ≈ `N` full decodes.
- **Reading an asset from flash / SD / a file.** Each tile re-opens / re-reads (and usually re-decodes) the source; flash latency and file I/O are paid `N` times.
- **Sampling source pixels held in PSRAM.** PSRAM is much slower than internal RAM, and the source is re-read once per tile. (A PSRAM-backed *tile* buffer also disables DMA, making the transfer synchronous — §10.5 — which compounds this.)
- Any other fixed per-frame setup the clip can't reduce: large LUT builds, layout/measure passes, sensor polling, etc.

By contrast, `pushImage` from an **in-RAM** buffer *is* clipped per tile — only the tile's source rows are read and pushed — which is why the benchmark's `image` scene (a tiled `pushImage` from a small RAM array) splits cheaply. The problem is specifically work the clip cannot reduce.

**Rule of thumb:** if the callback's cost is dominated by such non-clippable work, splitting degrades performance ≈ `N`×, and the auto default's many-small-tiles shape is *wrong* for that workload.

**Mitigations (in order):**

1. **Decode / read once, then blit.** Do the expensive decode or flash read **outside** the tiled callback — once, into an internal-RAM sprite or buffer (at setup, or only when the image changes) — and in the per-tile callback just `pushImage` / `pushSprite` from that decoded buffer. `pushImage` is clipped per tile, so this turns `N`× decode into **1× decode + `N`× cheap clipped blit**. Recommended for static or rarely-changing images.
2. **Use fewer / larger tiles.** Raise `setMemoryLimit()` or set a low `setSplitCount()` so `N` is small (trading internal RAM); a single tile pays the decode exactly once.
3. **Skip tiling for a full-screen image that fits** — draw it straight to the panel or one full-screen sprite, and use the tiled canvas only for the dynamic overlay.

## 11. Tile initialization (auto-clear) and fillScreen

### 11.1 auto-clear (the tile's initial state)

One tile sprite is reused across all tiles and frames, so before a draw call the sprite still holds leftovers from the previous tile / previous frame. Left alone, undrawn pixels would differ between "split=1 (full)" and "split=N (tiled)", breaking parity and producing ghosting on real hardware.

So **each tile is cleared to the background color before draw is called (auto-clear, default ON)**.

- The default background is black. Change it with `setBackgroundColor(color)`.
- This makes **undrawn pixels always deterministic (the background color)**, so the result is identical regardless of split count even if draw does not paint every pixel.
- Since there is no full-screen framebuffer, "preserving the previous frame (partial update)" is fundamentally impossible; redrawing everything every frame is the premise. auto-clear states that premise explicitly in the spec.
- If the user always paints the whole screen with `fillScreen`, auto-clear is a double fill. Advanced users who want to avoid it can disable with `setAutoClear(false)` (when disabled, undrawn pixels are undefined).

### 11.2 fillScreen

`fillScreen(color)` is treated as filling the virtual full screen.

During each tile's draw it fills the entire current tile sprite (offset-independent). Even for a partial last tile, it fills the whole sprite, and when pushing to the panel the surplus rows are clipped, so the virtual full screen ends up filled. When auto-clear is ON, a `fillScreen(color)` at the start of draw overrides the background clear (if `color` equals the background, it is effectively the same operation).

## 12. Clipping

All drawing must be safe even when it goes outside the tile. Via the internal sprite's per-pixel clip, drawing outside the tile (negative coordinates, or `>= tileH`) disappears automatically, so safety is ensured with no special handling in the library (only `pushImage` needs dedicated care; §9.2).

Cases to verify in particular:

- `y = tileHeight - 1`
- `y = tileHeight`
- `y = tileHeight + 1`
- The last tile being shorter than a normal tile
- Off-screen negative coordinates
- Drawing past the bottom edge

### 12.1 Known limitation: neighbor-dependent drawing across tile boundaries

The draw function is re-run per tile, each run clipped by the sprite and reassembled. Therefore **drawing whose output at a pixel depends on neighbor pixels across a tile boundary** (anti-aliasing, blur, neighbor-sampling filters, etc.) may not match between tiled and full-screen rendering. LovyanGFX's default primitives are not anti-aliased so this is usually not an issue, but keep it in mind when using AA drawing.

## 13. Test policy and plan

Tested in a headless rendering environment on GitHub Actions. The host uses the `lang-ship:host` `mode=lgfx` profile; the `pytest-embedded-arduino-cli` `dut` fixture waits on serial output, and PNGs produced by the sketch via `gfx.createPng()` are compared pixel-by-pixel with Pillow.

> For **how to run, the directory layout, and current state**, see [tests/README](tests/README.md) (EN+JA). This section defines the **test policy and plan** as design.

### 13.1 Two-tier structure and distribution

The LGFXVirtualCanvas header is gfx-neutral; the supported entry points are **LovyanGFX / M5GFX / M5Unified**. However, **the rendering engine of M5GFX / M5Unified is internally LovyanGFX**, so the correctness of the tiling logic is essentially the same across the three. So tests are split into two tiers.

- **Tier 1 (functional / correctness)**: verify the library's logic comprehensively with **LovyanGFX alone** (§13.4).
- **Tier 2 (cross-library build + minimal render)**: guarantee that the header **compiles against each entry point and that a minimal render satisfies parity**, across **all three libraries**. Minimal content — just `split=1` vs `split=3` equality plus PNG generation — which also serves the smoke (build-check) role. The shared minimal scene lives in `tests/common_libs/` and is reused by all three.
- **(Optional) Tier 3**: one `esp32:esp32:esp32` compile-only test to catch target-dependent build breakage that does not surface on the host. Actual-render verification is left to the host.

**Rationale for the distribution**: running all cases on all three libraries yields **no new logic coverage** (same rendering engine) and only increases CI time and library downloads. What we want to guard across libraries is "**compiles and renders** with each include order and each library's types," and Tier 2 is enough for that.

### 13.2 Comparison method (same draw function, only the split count varies)

Because the draw function takes `LGFXVirtualCanvas&`, "full-screen rendering to a normal Canvas" is also expressed as `LGFXVirtualScreen` with **1 split (split=1, offsetY=0)**. At 1 split the forwarding is pass-through (offsetY=0), so this is the full-render reference.

- Reference: the PNG rendered at `split = 1`
- Under test: the PNGs rendered at `split = 2, 3, 5, …`

Compare the decoded pixel arrays for equality (not PNG byte equality). This directly verifies the invariant that **splitting does not change the result**.

### 13.3 Failure artifacts

On failure, save `full.png` (reference) / `virtual.png` (tiled) / `diff.png` (difference).

### 13.4 Tier 1 case list (LovyanGFX)

Each case verifies pixel equality across several splits (e.g. 1/2/3/5/7).

| # | Case | What it checks |
|---|---|---|
| T1-1 | parity (overall) | A mixed shapes+text scene is split-invariant |
| T1-2 | basic shapes | fillRect / drawRect / drawLine / drawPixel / drawFastH/VLine |
| T1-3 | circles | drawCircle / fillCircle |
| T1-4 | text | drawString / setCursor + print/println/printf, cursor Y in virtual coords (§9.1) |
| T1-5 | tile boundary | drawing across `y = tileH-1 / tileH / tileH+1` (§12) |
| T1-6 | partial tile | screen height not divisible by split count (e.g. split=7, last 30 rows) (§12) |
| T1-7 | clipping safety | off-screen / negative / past-bottom does not crash and still matches (§12) |
| T1-8 | auto-clear | undrawn pixels deterministic at background / `setBackgroundColor` / `setAutoClear(false)` (§11) |
| T1-9 | memory / alloc failure | `setMemoryLimit` derives tileH; too-small → `begin()`=false, `isReady()`=false, `render()`=false (no fallback) (§10.3) |
| T1-10 | random fuzz | generate shape sequences from a seed; equal across splits (log the seed for reproducibility) |
| T1-11 | animation | run a sequence of frames; each frame split-invariant |
| T1-12 | pushImage | split-invariant with clip only (overhangs, boundary straddle, off-screen). Verified by experiment; supported (§9.2) |

### 13.5 Shared rules

- One test = one directory (`<name>.ino` / `sketch.yaml` / `test_<name>.py`).
- Artifacts go to `output/<name>.png`; `conftest.py` wipes them before each test.
- Fuzz uses a fixed seed + logging for reproducibility.

## 14. Recommended separation of the draw function

This library recommends separating drawing from `loop()` and from state-update code.

```cpp
void updateState(AppState& state);
void drawScene(LGFXVirtualCanvas& g, AppState& state);
```

This makes the following easy:

- Normal rendering
- Tiled rendering
- Headless rendering
- Golden image tests
- Layout tests
- Animation capture

## 15. View / Model separation

The library does not manage the Model. The Model / State is managed by the application.

```cpp
struct AppState {
    int score;
    int playerX;
    int playerY;
};
```

The draw function reads the state and renders the View.

```cpp
void drawScene(LGFXVirtualCanvas& g, AppState& state);
```

## 16. Global state

A draw function that references global state is also allowed.

```cpp
AppState state;

void drawScene(LGFXVirtualCanvas& g) {
    g.drawNumber(state.score, 10, 10);
}
```

That said, for testing, multiple screens, previews, and reusability, passing context (`render(draw, state)`) is recommended.

## 17. Relationship to a layout library

If a layout library is built in the future, LGFXVirtualCanvas is prepared first as the foundation. The layout library is unaware of tiled rendering.

```cpp
root.layout({0, 0, 320, 240});
root.draw(g);
```

The draw target `g` is passed as an `LGFXVirtualCanvas` (the virtual canvas). If a need arises to swap a normal Canvas and a virtual Canvas polymorphically, extract an interface from `LGFXVirtualCanvas` at that point (YAGNI — for now, a single concrete class).

## 18. Future ideas

The design allows adding the following on top of LGFXVirtualCanvas:

- LGFXLayout
- LGFXWidgets
- LGFXView
- layout preview
- per-component render tests
- headless screenshot tests
- animation capture
- sharing the tile sprite across multiple `LGFXVirtualScreen` instances (an option that reuses a sprite allocated at the largest width for screens of equal-or-smaller width; saves RAM)

## 19. Non-goals

The following are not required in the initial stage:

- Full coverage of the entire LGFX / M5GFX API
- Tiling in arbitrary directions (vertical only)
- Partial-update optimization
- Full recording of drawing commands
- A retained-mode UI framework
- An automatic layout engine
- Accepting capturing lambdas / `std::function` (function pointers only)
- Sharing the tile sprite across multiple `LGFXVirtualScreen` instances (initially each screen owns its own; sharing is a future option; §18)
- Fallback on allocation failure (e.g. auto-increasing the split count; failure is reported as failure; §10.3)

## 20. Minimum viable goal

The minimum viable version satisfies:

- Can split the screen vertically and render.
- The user is not aware of the offset.
- Works with the minimal `LGFXVirtualScreen screen(lcd);` (lazy allocation, default ≈ 19 KB/tile budget with auto double-buffering).
- A RAM budget can be set with `setMemoryLimit()`.
- `render(draw)` works.
- `render(draw, state)` works.
- Basic shapes and text rendering work.
- The result is identical regardless of split count (the full render = split:1 vs tiled-render PNG comparison test passes).
- Testable on GitHub Actions.

## Appendix: LGFXBase → `LGFXVirtualCanvas` mapping (table)

The table below maps representative LovyanGFX APIs (`LGFXBase` / `LGFX_Sprite`) to where they are handled in this library and the current support status.

| LovyanGFX API group | `LGFXVirtualCanvas` / manager mapping | Support | Notes |
|---|---:|:--:|---|
| geometry (`width`, `height`) | `LGFXVirtualCanvas` | Supported | returns the full virtual surface size |
| color utilities (`color332`, `color565`, `color888`, `swap565`, `swap888`, conversions) | `LGFXVirtualCanvas` static helpers | Supported | forwards to LGFX-compatible conversion helpers |
| state / palette / pivot / gradient helpers | `LGFXVirtualCanvas` | Supported | state is held by the current tile sprite; pivot Y is converted to/from virtual coordinates |
| basic and advanced shapes, coordinate-bearing `write*` drawing | `LGFXVirtualCanvas` | Supported | all public wrappers apply virtual-to-tile Y correction where needed |
| gradient, smooth, wide, and spot drawing | `LGFXVirtualCanvas` | Supported | tile clipping is handled by the sprite; parity tests cover representative cases |
| image push / decode / QR / grayscale / alpha | `LGFXVirtualCanvas` | Supported | decode helpers are available, but expensive decodes should generally be done outside the per-tile callback |
| readback (`readPixel`, `readPixelRGB`, `readPixelValue`, `readRectRGB`, `readRect`) | `LGFXVirtualCanvas` | Supported | reads from the current tile after virtual-to-tile Y correction |
| text and metrics | `LGFXVirtualCanvas` | Supported | cursor Y is converted to/from virtual coordinates; font metrics are delegated to the tile |
| window / clip / transfer / DMA control | `LGFXVirtualTiledBase` | Supported (manager-side) | renderRegion owns panel clipping, tile flushing, and DMA wait ordering |
| management (`setMemoryLimit`, `setSplitCount`, `setTileHeight`, `setBackgroundColor`, `setAutoClear`, `setDoubleBuffer`, `doubleBuffer`, `isReady`, `tileCount`, `tileHeight`) | `LGFXVirtualTiledBase` / `LGFXVirtualScreen` / `LGFXVirtualSprite` | Supported | tile allocation, split resolution and auto-clear/double-buffer policies are implemented here |

Not adopted function groups:

- Low-level streaming writes (`writeColor`, `pushBlock`, `writePixels`,
  `writePixelsDMA`, `pushPixels`, `pushPixelsDMA`, `pushColor`, `pushColors`)
  depend on a caller-managed write window / stream cursor and do not carry
  enough virtual coordinate information for safe per-tile clipping.
  Coordinate-bearing write APIs such as `writePixel`, `writeFastHLine`,
  `writeFastVLine`, `writeFillRect`, and `writeFillRectPreclipped` are supported
  because their virtual Y coordinate can be translated safely. For
  `writeFillRectPreclipped`, VirtualCanvas intentionally routes through clipped
  `fillRect` semantics instead of trusting the caller's preclip.
- Window, clip, and transaction controls (`setWindow`, `startWrite`,
  `endWrite`, `beginTransaction`, `endTransaction`, `initDMA`, `waitDMA`) are
  owned by the tiled manager. Exposing them on the callback canvas would allow
  user code to break manager-side clipping and DMA ordering.
- Scroll and copy APIs (`scroll`, `copyRect`, scroll-rect APIs) need pixels from
  other tiles when source/destination rectangles cross tile boundaries. A
  callback canvas only owns the current tile band.
- Sprite transfer helpers (`pushSprite`, `pushRotated`, `pushRotatedWithAA`,
  `pushRotateZoom`, `pushRotateZoomWithAA`, `pushAffine`, `pushAffineWithAA`)
  transfer an `LGFX_Sprite` itself to another destination. `LGFXVirtualCanvas`
  is already the destination; tile transfer is the manager's responsibility.
- Affine image helpers (`pushImageAffine`, `pushImageAffineWithAA`,
  `pushGrayscaleImageAffine`) embed destination coordinates in the affine
  matrix. A simple virtual-to-tile Y offset wrapper is not correct for every
  matrix.
- Output/export helpers (`createPng`, `releasePngMemory`) operate on the
  current tile buffer, not the full virtual surface. They should be manager-level
  APIs if added later.

Notes:
- `Supported` marks implemented APIs in `src/LGFXVirtualCanvas.h`. The "not
  adopted" groups above are intentionally omitted from the supported mapping,
  not simply forgotten.
- LovyanGFX exposes many template/overload combinations; a full 1:1 mapping is large. Recommended next steps:
    1. auto-extract public LovyanGFX drawing APIs to CSV (names/signatures/overload counts),
    2. auto-extract implemented Canvas methods and produce a diff CSV,
    3. review newly found gaps against the not-adopted groups before adding wrappers.
