#pragma once
/// @file LGFXVirtualCanvas.h
/// @brief Virtual full-screen canvas over LovyanGFX / M5GFX via vertical tiling.
///
/// The screen is split into vertical tiles, each rendered into one small reused
/// sprite. Your draw callback runs once per tile in **virtual (full-screen)
/// coordinates**; this library hides the Y offset, clipping, and flush. See
/// SPEC.md / SPEC.ja.md for the full design and rationale.
///
/// @note Include your graphics library **before** this header so the
///       `LovyanGFX` and `LGFX_Sprite` types are in scope:
/// @code
///   #include <LovyanGFX.hpp>          // or <M5GFX.h> / <M5Unified.h>
///   #include <LGFX_AUTODETECT.hpp>
///   #include <LGFXVirtualCanvas.h>
/// @endcode
///
/// Three classes:
///   - ::LGFXVirtualCanvas — the drawing surface handed to your draw function
///                           (local coordinates of the target surface).
///   - ::LGFXVirtualScreen — manager for the whole panel.
///   - ::LGFXVirtualSprite — manager for a placed sub-region (a tiled sprite);
///                           same as a normal sprite but internally tiled.
/// Both managers share the internal tiling engine and hand your draw function
/// an ::LGFXVirtualCanvas; only the transfer target (full panel vs a placed
/// rectangle) differs.

#include <cstdarg>
#include <cstdio>
#include <cstdint>

/// @brief The drawing surface handed to your draw function.
///
/// A concrete class (no virtual, no abstract base) that wraps one tile sprite
/// plus its Y offset. Coordinates are **local to the target surface** (the full
/// screen for LGFXVirtualScreen, or the sub-rectangle for LGFXVirtualSprite),
/// with (0,0) at the surface's top-left. Every method translates that Y into
/// tile-local coordinates (`y -= offsetY`) before forwarding to the sprite;
/// drawing outside the tile is clipped away for free by the sprite's per-pixel
/// clip. You normally never construct this yourself — a manager creates one per
/// tile and passes it to your draw callback.
class LGFXVirtualCanvas
{
public:
    /// @brief Construct over a tile sprite. (Internal: created per tile by a manager.)
    /// @param tile          The reusable tile sprite to draw into.
    /// @param offsetY       Surface Y of this tile's top edge (subtracted from every draw).
    /// @param virtualHeight Full surface height (returned by height()).
    LGFXVirtualCanvas(LGFX_Sprite &tile, int32_t offsetY, int32_t virtualHeight)
        : _tile(tile), _offsetY(offsetY), _virtualHeight(virtualHeight) {}

    /// @brief Surface width in pixels (the full drawable surface, not the tile).
    int32_t width(void) const { return _tile.width(); }
    /// @brief Surface height in pixels (the full drawable surface, not the tile).
    int32_t height(void) const { return _virtualHeight; }

    /// @brief Fill the whole virtual screen with @p color (offset-independent; see SPEC §11).
    template <typename T>
    void fillScreen(const T &color) { _tile.fillScreen(color); }

    /// @brief Draw a single pixel at virtual (@p x, @p y).
    template <typename T>
    void drawPixel(int32_t x, int32_t y, const T &color) { _tile.drawPixel(x, y - _offsetY, color); }

    /// @brief Draw a horizontal line of width @p w from virtual (@p x, @p y).
    template <typename T>
    void drawFastHLine(int32_t x, int32_t y, int32_t w, const T &color) { _tile.drawFastHLine(x, y - _offsetY, w, color); }

    /// @brief Draw a vertical line of height @p h from virtual (@p x, @p y).
    template <typename T>
    void drawFastVLine(int32_t x, int32_t y, int32_t h, const T &color) { _tile.drawFastVLine(x, y - _offsetY, h, color); }

    /// @brief Draw a line between virtual (@p x0, @p y0) and (@p x1, @p y1).
    template <typename T>
    void drawLine(int32_t x0, int32_t y0, int32_t x1, int32_t y1, const T &color) { _tile.drawLine(x0, y0 - _offsetY, x1, y1 - _offsetY, color); }

    /// @brief Fill a rectangle at virtual (@p x, @p y) of size @p w × @p h.
    template <typename T>
    void fillRect(int32_t x, int32_t y, int32_t w, int32_t h, const T &color) { _tile.fillRect(x, y - _offsetY, w, h, color); }

    /// @brief Draw a rectangle outline at virtual (@p x, @p y) of size @p w × @p h.
    template <typename T>
    void drawRect(int32_t x, int32_t y, int32_t w, int32_t h, const T &color) { _tile.drawRect(x, y - _offsetY, w, h, color); }

    /// @brief Draw a circle outline of radius @p r centered at virtual (@p x, @p y).
    template <typename T>
    void drawCircle(int32_t x, int32_t y, int32_t r, const T &color) { _tile.drawCircle(x, y - _offsetY, r, color); }

    /// @brief Fill a circle of radius @p r centered at virtual (@p x, @p y).
    template <typename T>
    void fillCircle(int32_t x, int32_t y, int32_t r, const T &color) { _tile.fillCircle(x, y - _offsetY, r, color); }

    /// @brief Push a @p w × @p h image to virtual (@p x, @p y).
    /// @note The sprite's per-pixel clip discards rows outside the tile
    ///       (including negative dest Y, where LGFX offsets into the source),
    ///       so images straddling tile boundaries reassemble correctly.
    template <typename T>
    void pushImage(int32_t x, int32_t y, int32_t w, int32_t h, const T *data) { _tile.pushImage(x, y - _offsetY, w, h, data); }
    /// @brief Push an image, treating @p transparent as a see-through color.
    template <typename T>
    void pushImage(int32_t x, int32_t y, int32_t w, int32_t h, const T *data, const T &transparent) { _tile.pushImage(x, y - _offsetY, w, h, data, transparent); }

    /// @brief Set the text cursor to virtual (@p x, @p y).
    /// @note `print`/`println`/`printf` then advance the cursor and reproduce
    ///       deterministically across tiles.
    void setCursor(int32_t x, int32_t y) { _tile.setCursor(x, y - _offsetY); }
    /// @brief Current cursor X (virtual coordinate).
    int32_t getCursorX(void) { return _tile.getCursorX(); }
    /// @brief Current cursor Y (virtual coordinate — offset added back).
    int32_t getCursorY(void) { return _tile.getCursorY() + _offsetY; }

    /// @brief Set the text foreground color.
    template <typename T>
    void setTextColor(const T &color) { _tile.setTextColor(color); }
    /// @brief Set the text foreground (@p fg) and background (@p bg) colors.
    template <typename T>
    void setTextColor(const T &fg, const T &bg) { _tile.setTextColor(fg, bg); }
    /// @brief Set the text size multiplier.
    void setTextSize(float size) { _tile.setTextSize(size); }
    /// @brief Set the text datum (alignment anchor) used by drawString().
    void setTextDatum(textdatum_t datum) { _tile.setTextDatum(datum); }

    /// @brief Draw @p str at virtual (@p x, @p y). @p str may be const char* or String.
    template <typename S>
    size_t drawString(const S &str, int32_t x, int32_t y) { return _tile.drawString(str, x, y - _offsetY); }
    /// @brief Draw @p str at virtual (@p x, @p y) with @p font (a uint8_t index or IFont*).
    template <typename S, typename F>
    size_t drawString(const S &str, int32_t x, int32_t y, const F &font) { return _tile.drawString(str, x, y - _offsetY, font); }

    /// @brief Draw @p str horizontally centered on virtual x = @p x, top at @p y.
    template <typename S>
    size_t drawCentreString(const S &str, int32_t x, int32_t y) { return _tile.drawCentreString(str, x, y - _offsetY); }
    /// @brief drawCentreString() with an explicit @p font.
    template <typename S, typename F>
    size_t drawCentreString(const S &str, int32_t x, int32_t y, const F &font) { return _tile.drawCentreString(str, x, y - _offsetY, font); }

    /// @brief Draw @p str right-aligned to virtual x = @p x, top at @p y.
    template <typename S>
    size_t drawRightString(const S &str, int32_t x, int32_t y) { return _tile.drawRightString(str, x, y - _offsetY); }
    /// @brief drawRightString() with an explicit @p font.
    template <typename S, typename F>
    size_t drawRightString(const S &str, int32_t x, int32_t y, const F &font) { return _tile.drawRightString(str, x, y - _offsetY, font); }

    /// @brief Print @p v at the cursor (covers all Arduino Print / LGFX overloads).
    template <typename T>
    size_t print(const T &v) { return _tile.print(v); }
    /// @brief Print @p v in number base @p base (e.g. HEX) at the cursor.
    template <typename T>
    size_t print(const T &v, int base) { return _tile.print(v, base); }
    /// @brief Print @p v followed by a newline.
    template <typename T>
    size_t println(const T &v) { return _tile.println(v); }
    /// @brief Print @p v in base @p base followed by a newline.
    template <typename T>
    size_t println(const T &v, int base) { return _tile.println(v, base); }
    /// @brief Print a newline.
    size_t println(void) { return _tile.println(); }

    /// @brief printf-style formatted text at the cursor (up to 159 chars per call).
    int printf(const char *format, ...)
    {
        char buf[160];
        va_list ap;
        va_start(ap, format);
        int n = vsnprintf(buf, sizeof(buf), format, ap);
        va_end(ap);
        _tile.print(buf);
        return n;
    }

private:
    LGFX_Sprite &_tile;
    int32_t _offsetY;
    int32_t _virtualHeight;
};

/// @brief Internal vertical-tiling engine shared by LGFXVirtualScreen and
///        LGFXVirtualSprite. Not intended for direct use.
///
/// Holds the panel, the reusable tile sprite, the split configuration, and the
/// per-tile render loop for a `regionW × regionH` drawable surface pushed to a
/// panel position. Memory is allocated lazily; allocation failure is reported,
/// never silently worked around. See SPEC §10.
class LGFXVirtualTiledBase
{
public:
    /// @brief Raw draw-callback type (function pointer + ctx).
    using DrawRaw = void (*)(LGFXVirtualCanvas &g, void *ctx);

    /// @brief Cap the tile buffer to @p bytes; tile height is derived from it (highest priority). 0 = unset.
    void setMemoryLimit(size_t bytes) { _memLimit = bytes; _dirty = true; }
    /// @brief Use a fixed number of tiles @p count. 0 = unset.
    void setSplitCount(int count) { _splitCount = count; _dirty = true; }
    /// @brief Use a fixed tile @p height in pixels. 0 = unset.
    void setTileHeight(int height) { _tileHeightCfg = height; _dirty = true; }
    /// @brief Set the auto-clear background color (default black / 0).
    void setBackgroundColor(uint32_t color) { _bgColor = color; }
    /// @brief Enable/disable clearing each tile before draw (default enabled). See SPEC §11.
    void setAutoClear(bool enable) { _autoClear = enable; }
    /// @brief Enable double-buffering: two tile sprites ping-ponged so a tile's
    ///        DMA transfer overlaps the next tile's draw (faster, 2× tile RAM).
    ///
    /// Default OFF (single buffer). With a single buffer the tile is reused for
    /// the next tile, so the transfer must complete before the next draw — the
    /// render loop waits for DMA after each push (correct, but draw and transfer
    /// are serialized). With double-buffering, tile `i` transfers (async DMA)
    /// from one buffer while tile `i+1` is drawn into the other; the SPI bus
    /// serializes consecutive transfers, so a buffer is never overwritten while
    /// its DMA is still in flight. Costs 2× the tile buffer (memoryLimit, if
    /// set, still bounds each individual buffer). @see SPEC §10.5.
    void setDoubleBuffer(bool enable) { _doubleBuffer = enable; _dirty = true; }
    /// @brief Whether double-buffering is enabled.
    bool doubleBuffer(void) const { return _doubleBuffer; }

    /// @brief Whether the tile buffer is allocated and current.
    bool isReady(void) const { return _ready && !_dirty; }
    /// @brief Number of tiles resolved at allocation.
    int tileCount(void) const { return _tileCount; }
    /// @brief Tile height in pixels resolved at allocation.
    int tileHeight(void) const { return _tileHeight; }

protected:
    LGFXVirtualTiledBase(LovyanGFX &panel) : _panel(&panel), _tile(&panel), _tile2(&panel) {}

    /// @brief Allocate the reusable tile sprite for a regionW × regionH surface.
    /// @return false on failure (no fallback — see SPEC §10.3).
    bool beginRegion(int32_t regionW, int32_t regionH)
    {
        if (!_panel || regionW <= 0 || regionH <= 0)
            return false;
        const int bits = ((int)_panel->getColorDepth()) & 0x00FF; // color_depth_t::bit_mask
        const int32_t tileH = computeTileHeight(regionW, regionH, bits);
        if (tileH < 1)
        {
            _ready = false;
            return false; // requested geometry cannot be satisfied
        }
        _tile.deleteSprite();
        _tile2.deleteSprite();
        _tile.setColorDepth(_panel->getColorDepth());
        if (_tile.createSprite(regionW, tileH) == nullptr)
        {
            _ready = false;
            return false; // out of RAM
        }
        if (_doubleBuffer)
        {
            _tile2.setColorDepth(_panel->getColorDepth());
            if (_tile2.createSprite(regionW, tileH) == nullptr)
            {
                _tile.deleteSprite(); // keep all-or-nothing: no half-allocated state
                _ready = false;
                return false; // out of RAM for the second buffer
            }
        }
        _regionW = regionW;
        _regionH = regionH;
        _tileHeight = tileH;
        _tileCount = (regionH + tileH - 1) / tileH;
        _ready = true;
        _dirty = false;
        return true;
    }

    /// @brief Render the regionW × regionH surface at panel position (posX, posY).
    ///
    /// The panel clip rect is set to the target rectangle so the last partial
    /// tile and any screen-edge overhang are clipped automatically. The only
    /// templated piece is the thin per-tile dispatch; clear/push stay
    /// non-template (SPEC §5.4).
    template <typename F>
    bool renderRegion(int32_t regionW, int32_t regionH, int32_t posX, int32_t posY, F &&drawFn)
    {
        if (!_ready || _dirty || _regionW != regionW || _regionH != regionH)
        {
            if (!beginRegion(regionW, regionH))
                return false; // not ready → draw nothing (SPEC §10.3)
        }
        // Batch all tile transfers into one bus transaction. Drawing into the
        // tile sprite is memory-only (no bus), so only the per-tile pushSprite
        // to the panel benefits — holding one startWrite/endWrite around the
        // loop avoids N transaction setups on real hardware. (No-op on the
        // bus-less host backend.)
        _panel->startWrite();
        _panel->setClipRect(posX, posY, _regionW, _regionH);
        for (int i = 0; i < _tileCount; ++i)
        {
            const int32_t offsetY = (int32_t)i * _tileHeight;
            // Double-buffer: alternate tiles so tile i transfers (async DMA)
            // while tile i+1 is drawn into the other buffer. The SPI bus
            // serializes consecutive transfers, so the buffer reused here (last
            // touched two tiles ago) is guaranteed free of in-flight DMA.
            LGFX_Sprite &buf = (_doubleBuffer && (i & 1)) ? _tile2 : _tile;
            if (_autoClear)
                buf.fillScreen(_bgColor);
            LGFXVirtualCanvas g(buf, offsetY, _regionH);
            drawFn(g);
            buf.pushSprite(_panel, posX, posY + offsetY);
            // Single buffer: the same sprite is reused for the next tile, so we
            // must not start overwriting it until this tile's DMA has finished
            // reading it. (Without this the next fillScreen would corrupt the
            // tile still being transferred — see SPEC §10.5.)
            if (!_doubleBuffer)
                _panel->waitDMA();
        }
        _panel->clearClipRect();
        _panel->endWrite();
        return true;
    }

private:
    // Decide tile height from config + surface size. SPEC §10.1 priority:
    // memoryLimit > splitCount > tileHeight > default(3).
    int32_t computeTileHeight(int32_t W, int32_t H, int bits) const
    {
        if (_memLimit > 0)
        {
            const size_t bytesPerRow = ((size_t)W * (size_t)bits + 7) / 8;
            if (bytesPerRow == 0)
                return 0;
            int32_t th = (int32_t)(_memLimit / bytesPerRow);
            if (th < 1)
                return 0; // cannot fit even one row → fail (no rounding)
            return (th > H) ? H : th;
        }
        if (_splitCount > 0)
            return (H + _splitCount - 1) / _splitCount;
        if (_tileHeightCfg > 0)
            return (_tileHeightCfg > H) ? H : _tileHeightCfg;
        return (H + 2) / 3; // default 3 splits
    }

protected:
    LovyanGFX *_panel;
    LGFX_Sprite _tile;
    LGFX_Sprite _tile2; // second buffer, allocated only when _doubleBuffer

    // config
    size_t _memLimit = 0;
    int _splitCount = 0;
    int _tileHeightCfg = 0;
    uint32_t _bgColor = 0; // black
    bool _autoClear = true;
    bool _doubleBuffer = false;

    // computed state
    int32_t _regionW = 0;
    int32_t _regionH = 0;
    int32_t _tileHeight = 0;
    int _tileCount = 0;
    bool _ready = false;
    bool _dirty = true;
};

/// @brief Manager for the whole panel: draw a virtual full-screen canvas.
///
/// Construct one over your panel. Memory is allocated lazily on begin() or the
/// first render() (the screen size is unknown before `lcd.init()` /
/// `M5.begin()`). The draw callback receives an LGFXVirtualCanvas whose
/// coordinates span the full screen.
class LGFXVirtualScreen : public LGFXVirtualTiledBase
{
public:
    /// @brief Construct over @p panel. Nothing is allocated yet.
    /// @param panel      The real display (an LGFX, M5GFX, or M5.Display).
    /// @param splitCount Number of tiles; 0 = auto (default 3). See SPEC §10.1.
    explicit LGFXVirtualScreen(LovyanGFX &panel, int splitCount = 0)
        : LGFXVirtualTiledBase(panel) { _splitCount = splitCount; }

    /// @brief Allocate the tile buffer now. @return false on failure (no fallback).
    bool begin(void) { return beginRegion(_panel->width(), _panel->height()); }

    /// @brief Render via a raw `void(LGFXVirtualCanvas&, void*)` callback.
    /// @return true if drawn; false if not allocated (then nothing is drawn).
    bool render(DrawRaw draw, void *ctx = nullptr)
    {
        return renderRegion(_panel->width(), _panel->height(), 0, 0,
                            [&](LGFXVirtualCanvas &g) { draw(g, ctx); });
    }
    /// @brief Render via a `void(LGFXVirtualCanvas&)` callback.
    bool render(void (*draw)(LGFXVirtualCanvas &g))
    {
        return renderRegion(_panel->width(), _panel->height(), 0, 0,
                            [&](LGFXVirtualCanvas &g) { draw(g); });
    }
    /// @brief Render via a typed `void(LGFXVirtualCanvas&, T&)` callback with your @p ctx.
    template <typename T>
    bool render(void (*draw)(LGFXVirtualCanvas &g, T &ctx), T &ctx)
    {
        return renderRegion(_panel->width(), _panel->height(), 0, 0,
                            [&](LGFXVirtualCanvas &g) { draw(g, ctx); });
    }
};

/// @brief Manager for a placed sub-region: a tiled sprite.
///
/// Behaves like a normal `LGFX_Sprite` of size `w × h`, except it is internally
/// split into vertical tiles so it needs only a small buffer. The draw callback
/// receives an LGFXVirtualCanvas in the sprite's **local coordinates**
/// (0,0 = top-left of the sprite). The size is fixed at construction; the panel
/// position can be changed any time (no reallocation). The library performs all
/// transfer, including the partial last tile and screen-edge clipping.
class LGFXVirtualSprite : public LGFXVirtualTiledBase
{
public:
    /// @brief Construct a @p w × @p h tiled sprite, placed at panel (@p x, @p y).
    /// @param panel The real display.
    /// @param w,h   Sprite size (the drawable surface; fixed). Allocated at begin()/first render().
    /// @param x,y   Default panel position (may be overridden per render()).
    LGFXVirtualSprite(LovyanGFX &panel, int w, int h, int x = 0, int y = 0)
        : LGFXVirtualTiledBase(panel), _w(w), _h(h), _x(x), _y(y) {}

    /// @brief Set the panel position (no reallocation).
    void setPosition(int x, int y) { _x = x; _y = y; }
    /// @brief Current panel X.
    int x(void) const { return _x; }
    /// @brief Current panel Y.
    int y(void) const { return _y; }
    /// @brief Sprite width (local coordinate space).
    int width(void) const { return _w; }
    /// @brief Sprite height (local coordinate space).
    int height(void) const { return _h; }

    /// @brief Allocate the tile buffer now. @return false on failure (no fallback).
    bool begin(void) { return beginRegion(_w, _h); }

    /// @brief Render at the current position.
    bool render(void (*draw)(LGFXVirtualCanvas &g))
    {
        return renderRegion(_w, _h, _x, _y, [&](LGFXVirtualCanvas &g) { draw(g); });
    }
    /// @brief Render at panel (@p x, @p y); also updates the current position.
    bool render(void (*draw)(LGFXVirtualCanvas &g), int x, int y)
    {
        _x = x;
        _y = y;
        return renderRegion(_w, _h, _x, _y, [&](LGFXVirtualCanvas &g) { draw(g); });
    }
    /// @brief Render at the current position with typed @p ctx.
    template <typename T>
    bool render(void (*draw)(LGFXVirtualCanvas &g, T &ctx), T &ctx)
    {
        return renderRegion(_w, _h, _x, _y, [&](LGFXVirtualCanvas &g) { draw(g, ctx); });
    }
    /// @brief Render at panel (@p x, @p y) with typed @p ctx; also updates the current position.
    template <typename T>
    bool render(void (*draw)(LGFXVirtualCanvas &g, T &ctx), T &ctx, int x, int y)
    {
        _x = x;
        _y = y;
        return renderRegion(_w, _h, _x, _y, [&](LGFXVirtualCanvas &g) { draw(g, ctx); });
    }
    /// @brief Render via a raw `void(LGFXVirtualCanvas&, void*)` callback at the current position.
    bool render(DrawRaw draw, void *ctx = nullptr)
    {
        return renderRegion(_w, _h, _x, _y, [&](LGFXVirtualCanvas &g) { draw(g, ctx); });
    }

private:
    int _w, _h, _x, _y;
};
