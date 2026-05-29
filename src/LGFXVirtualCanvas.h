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
/// Two classes:
///   - ::LGFXVirtualCanvas — the drawing surface handed to your draw function.
///   - ::LGFXVirtualScreen — the manager you construct; holds the panel, the
///                           split config, and runs render().

#include <cstdarg>
#include <cstdio>
#include <cstdint>

/// @brief The drawing surface handed to your draw function.
///
/// A concrete class (no virtual, no abstract base) that wraps one tile sprite
/// plus its Y offset. Every method translates virtual coordinates into
/// tile-local coordinates (`y -= offsetY`) before forwarding to the sprite;
/// drawing outside the tile is clipped away for free by the sprite's per-pixel
/// clip. You normally never construct this yourself — LGFXVirtualScreen creates
/// one per tile and passes it to your draw callback. All coordinates are in the
/// virtual full-screen space.
class LGFXVirtualCanvas
{
public:
    /// @brief Construct over a tile sprite. (Internal: created per tile by LGFXVirtualScreen.)
    /// @param tile          The reusable tile sprite to draw into.
    /// @param offsetY       Virtual Y of this tile's top edge (subtracted from every draw).
    /// @param virtualHeight Full virtual screen height (returned by height()).
    LGFXVirtualCanvas(LGFX_Sprite &tile, int32_t offsetY, int32_t virtualHeight)
        : _tile(tile), _offsetY(offsetY), _virtualHeight(virtualHeight) {}

    /// @brief Virtual canvas width in pixels (full screen, not the tile).
    int32_t width(void) const { return _tile.width(); }
    /// @brief Virtual canvas height in pixels (full screen, not the tile).
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

/// @brief The manager: holds the panel and split config, and runs render().
///
/// Construct one over your panel. Memory is allocated lazily on begin() or the
/// first render() (the screen size is unknown before `lcd.init()` /
/// `M5.begin()`). Allocation failure is reported, never silently worked around.
/// See SPEC §10.
class LGFXVirtualScreen
{
public:
    /// @brief Raw draw-callback type used internally (function pointer + ctx).
    using DrawRaw = void (*)(LGFXVirtualCanvas &g, void *ctx);

    /// @brief Construct over @p panel. Nothing is allocated yet.
    /// @param panel      The real display (an LGFX, M5GFX, or M5.Display).
    /// @param splitCount Number of tiles; 0 = auto (default 3). See SPEC §10.1.
    explicit LGFXVirtualScreen(LovyanGFX &panel, int splitCount = 0)
        : _panel(&panel), _tile(&panel), _splitCount(splitCount) {}

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

    /// @brief Allocate the reusable tile sprite now.
    /// @return true on success; false on failure (no fallback — see SPEC §10.3).
    bool begin(void)
    {
        if (!_panel)
            return false;
        const int32_t W = _panel->width();
        const int32_t H = _panel->height();
        if (W <= 0 || H <= 0)
            return false; // panel not initialized yet

        const int bits = ((int)_panel->getColorDepth()) & 0x00FF; // color_depth_t::bit_mask
        const int32_t tileH = computeTileHeight(W, H, bits);
        if (tileH < 1)
        {
            _ready = false;
            return false; // requested geometry cannot be satisfied
        }

        _tile.deleteSprite();
        _tile.setColorDepth(_panel->getColorDepth());
        if (_tile.createSprite(W, tileH) == nullptr)
        {
            _ready = false;
            return false; // out of RAM
        }

        _virtualHeight = H;
        _tileHeight = tileH;
        _tileCount = (H + tileH - 1) / tileH;
        _ready = true;
        _dirty = false;
        return true;
    }

    /// @brief Whether the tile buffer is allocated and current.
    bool isReady(void) const { return _ready && !_dirty; }

    /// @brief Number of tiles resolved at allocation.
    int tileCount(void) const { return _tileCount; }
    /// @brief Tile height in pixels resolved at allocation.
    int tileHeight(void) const { return _tileHeight; }

    /// @brief Render via a raw `void(LGFXVirtualCanvas&, void*)` callback.
    /// @return true if drawn; false if not allocated (then nothing is drawn).
    bool render(DrawRaw draw, void *ctx = nullptr)
    {
        return renderEach([&](LGFXVirtualCanvas &g)
                          { draw(g, ctx); });
    }

    /// @brief Render via a `void(LGFXVirtualCanvas&)` callback.
    /// @return true if drawn; false if not allocated.
    bool render(void (*draw)(LGFXVirtualCanvas &g))
    {
        return renderEach([&](LGFXVirtualCanvas &g)
                          { draw(g); });
    }

    /// @brief Render via a typed `void(LGFXVirtualCanvas&, T&)` callback with your @p ctx.
    /// @return true if drawn; false if not allocated.
    template <typename T>
    bool render(void (*draw)(LGFXVirtualCanvas &g, T &ctx), T &ctx)
    {
        return renderEach([&](LGFXVirtualCanvas &g)
                          { draw(g, ctx); });
    }

private:
    // Decide tile height from config + panel size. SPEC §10.1 priority:
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

    // The only templated piece is the thin per-tile callback dispatch; the
    // heavy machinery (clear / push) stays non-template. SPEC §5.4.
    template <typename F>
    bool renderEach(F &&drawFn)
    {
        if (!_ready || _dirty)
        {
            if (!begin())
                return false; // not ready → draw nothing (SPEC §10.3)
        }
        for (int i = 0; i < _tileCount; ++i)
        {
            const int32_t offsetY = (int32_t)i * _tileHeight;
            if (_autoClear)
                _tile.fillScreen(_bgColor);
            LGFXVirtualCanvas g(_tile, offsetY, _virtualHeight);
            drawFn(g);
            _tile.pushSprite(_panel, 0, offsetY);
        }
        return true;
    }

    LovyanGFX *_panel;
    LGFX_Sprite _tile;

    // config
    size_t _memLimit = 0;
    int _splitCount = 0;
    int _tileHeightCfg = 0;
    uint32_t _bgColor = 0; // black
    bool _autoClear = true;

    // computed state
    int32_t _virtualHeight = 0;
    int32_t _tileHeight = 0;
    int _tileCount = 0;
    bool _ready = false;
    bool _dirty = true;
};
