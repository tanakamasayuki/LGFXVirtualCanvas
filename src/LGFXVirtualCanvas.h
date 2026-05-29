#pragma once
//
// LGFXVirtualCanvas
//
// A virtual full-screen canvas over LovyanGFX / M5GFX. The screen is split
// into vertical tiles, each rendered into one small reused sprite; your draw
// callback runs once per tile in virtual coordinates and this library hides
// the Y offset, clipping, and flush. See SPEC.ja.md for the full design.
//
// IMPORTANT: include your graphics library BEFORE this header so the
// `LovyanGFX` and `LGFX_Sprite` types are in scope (gfx_demo-style neutrality):
//
//   #include <LovyanGFX.hpp>          // or <M5GFX.h> / <M5Unified.h>
//   #include <LGFX_AUTODETECT.hpp>
//   #include <LGFXVirtualCanvas.h>
//
// Two classes:
//   - LGFXVirtualCanvas : the drawing surface handed to your draw function.
//   - LGFXVirtualScreen : the manager you construct; holds the panel, the
//                         split config, and runs render().

#include <cstdarg>
#include <cstdio>
#include <cstdint>

// ---------------------------------------------------------------------------
// LGFXVirtualCanvas — the drawing surface (concrete class, no virtual / no
// abstract base). Wraps one tile sprite + its Y offset and exposes only the
// supported methods, each translating virtual coordinates into tile-local
// coordinates (y -= offsetY) before forwarding to the sprite. Out-of-tile
// drawing is clipped away for free by the sprite's per-pixel clip.
// ---------------------------------------------------------------------------
class LGFXVirtualCanvas
{
public:
    LGFXVirtualCanvas(LGFX_Sprite &tile, int32_t offsetY, int32_t virtualHeight)
        : _tile(tile), _offsetY(offsetY), _virtualHeight(virtualHeight) {}

    // Virtual canvas geometry (full virtual screen, not the tile).
    int32_t width(void) const { return _tile.width(); }
    int32_t height(void) const { return _virtualHeight; }

    // fillScreen fills the whole tile (offset-independent). See SPEC §11.
    template <typename T>
    void fillScreen(const T &color) { _tile.fillScreen(color); }

    template <typename T>
    void drawPixel(int32_t x, int32_t y, const T &color) { _tile.drawPixel(x, y - _offsetY, color); }

    template <typename T>
    void drawFastHLine(int32_t x, int32_t y, int32_t w, const T &color) { _tile.drawFastHLine(x, y - _offsetY, w, color); }

    template <typename T>
    void drawFastVLine(int32_t x, int32_t y, int32_t h, const T &color) { _tile.drawFastVLine(x, y - _offsetY, h, color); }

    template <typename T>
    void drawLine(int32_t x0, int32_t y0, int32_t x1, int32_t y1, const T &color) { _tile.drawLine(x0, y0 - _offsetY, x1, y1 - _offsetY, color); }

    template <typename T>
    void fillRect(int32_t x, int32_t y, int32_t w, int32_t h, const T &color) { _tile.fillRect(x, y - _offsetY, w, h, color); }

    template <typename T>
    void drawRect(int32_t x, int32_t y, int32_t w, int32_t h, const T &color) { _tile.drawRect(x, y - _offsetY, w, h, color); }

    template <typename T>
    void drawCircle(int32_t x, int32_t y, int32_t r, const T &color) { _tile.drawCircle(x, y - _offsetY, r, color); }

    template <typename T>
    void fillCircle(int32_t x, int32_t y, int32_t r, const T &color) { _tile.fillCircle(x, y - _offsetY, r, color); }

    // --- text ---
    // Cursor Y is a virtual coordinate; print/println/printf advance the
    // (tile-local) cursor and reproduce deterministically across tiles.
    void setCursor(int32_t x, int32_t y) { _tile.setCursor(x, y - _offsetY); }
    int32_t getCursorX(void) { return _tile.getCursorX(); }
    int32_t getCursorY(void) { return _tile.getCursorY() + _offsetY; }

    template <typename T>
    void setTextColor(const T &color) { _tile.setTextColor(color); }
    template <typename T>
    void setTextColor(const T &fg, const T &bg) { _tile.setTextColor(fg, bg); }
    void setTextSize(float size) { _tile.setTextSize(size); }
    void setTextDatum(textdatum_t datum) { _tile.setTextDatum(datum); }

    template <typename T>
    int32_t drawString(const char *str, int32_t x, int32_t y, const T &font) { return _tile.drawString(str, x, y - _offsetY, font); }
    int32_t drawString(const char *str, int32_t x, int32_t y) { return _tile.drawString(str, x, y - _offsetY); }

    size_t print(const char *s) { return _tile.print(s); }
    size_t println(const char *s) { return _tile.println(s); }
    size_t println(void) { return _tile.println(); }

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

// ---------------------------------------------------------------------------
// LGFXVirtualScreen — the manager. Construct with the panel; memory is
// allocated lazily on begin() / first render(). See SPEC §10.
// ---------------------------------------------------------------------------
class LGFXVirtualScreen
{
public:
    using DrawRaw = void (*)(LGFXVirtualCanvas &g, void *ctx);

    // splitCount: 0 = auto (default 3). See SPEC §10.1.
    explicit LGFXVirtualScreen(LovyanGFX &panel, int splitCount = 0)
        : _panel(&panel), _tile(&panel), _splitCount(splitCount) {}

    // --- configuration (call before begin(); each default is 0 = unset) ---
    void setMemoryLimit(size_t bytes) { _memLimit = bytes; _dirty = true; }
    void setSplitCount(int count) { _splitCount = count; _dirty = true; }
    void setTileHeight(int height) { _tileHeightCfg = height; _dirty = true; }
    void setBackgroundColor(uint32_t color) { _bgColor = color; }
    void setAutoClear(bool enable) { _autoClear = enable; }

    // Allocate the reusable tile sprite. Returns false on failure (no
    // fallback). See SPEC §10.3.
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

    bool isReady(void) const { return _ready && !_dirty; }

    int tileCount(void) const { return _tileCount; }
    int tileHeight(void) const { return _tileHeight; }

    // --- render overloads (function pointers only) ---
    bool render(DrawRaw draw, void *ctx = nullptr)
    {
        return renderEach([&](LGFXVirtualCanvas &g)
                          { draw(g, ctx); });
    }

    bool render(void (*draw)(LGFXVirtualCanvas &g))
    {
        return renderEach([&](LGFXVirtualCanvas &g)
                          { draw(g); });
    }

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
