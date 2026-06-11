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

/// @def LGFXVIRTUALCANVAS_H
/// @brief Presence guard. Other code / libraries can test
///        `#if defined(LGFXVIRTUALCANVAS_H)` to detect that this header has been
///        included (e.g. to optionally integrate when it is available). The
///        `LGFXVIRTUALCANVAS_VERSION_*` macros (from lgfxvirtualcanvas_version.h,
///        included below) are also available for version checks.
#define LGFXVIRTUALCANVAS_H

#include <cstdarg>
#include <cstdio>
#include <cstdint>
#include <utility>

#include "lgfxvirtualcanvas_version.h"

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

    /// @brief Set the current drawing color.
    template <typename T>
    void setColor(const T &color) { _tile.setColor(color); }
    /// @brief Set the current drawing color from RGB components.
    void setColor(uint8_t r, uint8_t g, uint8_t b) { _tile.setColor(r, g, b); }
    /// @brief Set the current drawing raw color.
    void setRawColor(uint32_t color) { _tile.setRawColor(color); }
    /// @brief Current raw drawing color.
    uint32_t getRawColor(void) const { return _tile.getRawColor(); }
    /// @brief Set the base/background color used by clear/scroll-style LGFX APIs.
    template <typename T>
    void setBaseColor(const T &color) { _tile.setBaseColor(color); }
    /// @brief Current base/background color as RGB888.
    uint32_t getBaseColor(void) const { return _tile.getBaseColor(); }
    /// @brief Current color depth of the tile sprite.
    lgfx::v1::color_depth_t getColorDepth(void) const { return _tile.getColorDepth(); }
    /// @brief Whether the tile sprite currently has a palette.
    bool hasPalette(void) const { return _tile.hasPalette(); }
    /// @brief Number of palette entries in the tile sprite.
    uint32_t getPaletteCount(void) const { return _tile.getPaletteCount(); }
    /// @brief Pointer to the tile sprite palette, if any.
    auto getPalette(void) const -> decltype(std::declval<LGFX_Sprite &>().getPalette()) { return _tile.getPalette(); }
    /// @brief Set one palette entry on the tile sprite.
    template <typename T>
    void setPaletteColor(size_t index, const T &color) { _tile.setPaletteColor(index, color); }
    /// @brief Set one palette entry from RGB components.
    void setPaletteColor(size_t index, uint8_t r, uint8_t g, uint8_t b) { _tile.setPaletteColor(index, r, g, b); }

    /// @brief Fill the whole virtual screen with @p color (offset-independent; see SPEC §11).
    template <typename T>
    void fillScreen(const T &color) { _tile.fillScreen(color); }
    /// @brief Fill the whole virtual screen with the current drawing color.
    void fillScreen(void) { _tile.fillScreen(); }
    /// @brief Clear the virtual screen using the current base color.
    void clear(void) { _tile.fillScreen(_tile.getBaseColor()); }
    /// @brief Clear the virtual screen using @p color.
    template <typename T>
    void clear(const T &color) { _tile.fillScreen(color); }
    /// @brief Clear the virtual display using the current base color.
    void clearDisplay(void) { clear(); }
    /// @brief Clear the virtual display using @p color.
    template <typename T>
    void clearDisplay(const T &color) { clear(color); }

    /// @brief Draw a single pixel at virtual (@p x, @p y).
    template <typename T>
    void drawPixel(int32_t x, int32_t y, const T &color) { _tile.drawPixel(x, y - _offsetY, color); }
    /// @brief Draw a single pixel with the current drawing color.
    void drawPixel(int32_t x, int32_t y) { _tile.drawPixel(x, y - _offsetY); }
    /// @brief Read a pixel from virtual (@p x, @p y) as RGB565 from the current tile.
    uint16_t readPixel(int32_t x, int32_t y) { return _tile.readPixel(x, y - _offsetY); }
    /// @brief Read a pixel from virtual (@p x, @p y) as an RGB color object from the current tile.
    auto readPixelRGB(int32_t x, int32_t y) -> decltype(std::declval<LGFX_Sprite &>().readPixelRGB(0, 0)) { return _tile.readPixelRGB(x, y - _offsetY); }
    /// @brief Read a raw pixel value from virtual (@p x, @p y) from the current tile.
    uint32_t readPixelValue(int32_t x, int32_t y) { return _tile.readPixelValue(x, y - _offsetY); }

    /// @brief Draw a horizontal line of width @p w from virtual (@p x, @p y).
    template <typename T>
    void drawFastHLine(int32_t x, int32_t y, int32_t w, const T &color) { _tile.drawFastHLine(x, y - _offsetY, w, color); }
    /// @brief Draw a horizontal line with the current drawing color.
    void drawFastHLine(int32_t x, int32_t y, int32_t w) { _tile.drawFastHLine(x, y - _offsetY, w); }

    /// @brief Draw a vertical line of height @p h from virtual (@p x, @p y).
    template <typename T>
    void drawFastVLine(int32_t x, int32_t y, int32_t h, const T &color) { _tile.drawFastVLine(x, y - _offsetY, h, color); }
    /// @brief Draw a vertical line with the current drawing color.
    void drawFastVLine(int32_t x, int32_t y, int32_t h) { _tile.drawFastVLine(x, y - _offsetY, h); }

    /// @brief Draw a line between virtual (@p x0, @p y0) and (@p x1, @p y1).
    template <typename T>
    void drawLine(int32_t x0, int32_t y0, int32_t x1, int32_t y1, const T &color) { _tile.drawLine(x0, y0 - _offsetY, x1, y1 - _offsetY, color); }
    /// @brief Draw a line with the current drawing color.
    void drawLine(int32_t x0, int32_t y0, int32_t x1, int32_t y1) { _tile.drawLine(x0, y0 - _offsetY, x1, y1 - _offsetY); }

    /// @brief Fill a rectangle at virtual (@p x, @p y) of size @p w × @p h.
    template <typename T>
    void fillRect(int32_t x, int32_t y, int32_t w, int32_t h, const T &color) { _tile.fillRect(x, y - _offsetY, w, h, color); }
    /// @brief Fill a rectangle with the current drawing color.
    void fillRect(int32_t x, int32_t y, int32_t w, int32_t h) { _tile.fillRect(x, y - _offsetY, w, h); }
    /// @brief Fill a rectangle with alpha compositing.
    template <typename T>
    void fillRectAlpha(int32_t x, int32_t y, int32_t w, int32_t h, uint8_t alpha, const T &color) { _tile.fillRectAlpha(x, y - _offsetY, w, h, alpha, color); }

    /// @brief Draw a rectangle outline at virtual (@p x, @p y) of size @p w × @p h.
    template <typename T>
    void drawRect(int32_t x, int32_t y, int32_t w, int32_t h, const T &color) { _tile.drawRect(x, y - _offsetY, w, h, color); }
    /// @brief Draw a rectangle outline with the current drawing color.
    void drawRect(int32_t x, int32_t y, int32_t w, int32_t h) { _tile.drawRect(x, y - _offsetY, w, h); }

    /// @brief Fill a rounded rectangle at virtual (@p x, @p y) of size @p w × @p h.
    template <typename T>
    void fillRoundRect(int32_t x, int32_t y, int32_t w, int32_t h, int32_t r, const T &color) { _tile.fillRoundRect(x, y - _offsetY, w, h, r, color); }
    /// @brief Fill a rounded rectangle with the current drawing color.
    void fillRoundRect(int32_t x, int32_t y, int32_t w, int32_t h, int32_t r) { _tile.fillRoundRect(x, y - _offsetY, w, h, r); }

    /// @brief Draw a rounded rectangle outline at virtual (@p x, @p y) of size @p w × @p h.
    template <typename T>
    void drawRoundRect(int32_t x, int32_t y, int32_t w, int32_t h, int32_t r, const T &color) { _tile.drawRoundRect(x, y - _offsetY, w, h, r, color); }
    /// @brief Draw a rounded rectangle outline with the current drawing color.
    void drawRoundRect(int32_t x, int32_t y, int32_t w, int32_t h, int32_t r) { _tile.drawRoundRect(x, y - _offsetY, w, h, r); }

    /// @brief Draw a circle outline of radius @p r centered at virtual (@p x, @p y).
    template <typename T>
    void drawCircle(int32_t x, int32_t y, int32_t r, const T &color) { _tile.drawCircle(x, y - _offsetY, r, color); }
    /// @brief Draw a circle outline with the current drawing color.
    void drawCircle(int32_t x, int32_t y, int32_t r) { _tile.drawCircle(x, y - _offsetY, r); }

    /// @brief Fill a circle of radius @p r centered at virtual (@p x, @p y).
    template <typename T>
    void fillCircle(int32_t x, int32_t y, int32_t r, const T &color) { _tile.fillCircle(x, y - _offsetY, r, color); }
    /// @brief Fill a circle with the current drawing color.
    void fillCircle(int32_t x, int32_t y, int32_t r) { _tile.fillCircle(x, y - _offsetY, r); }

    /// @brief Draw an ellipse outline centered at virtual (@p x, @p y).
    template <typename T>
    void drawEllipse(int32_t x, int32_t y, int32_t rx, int32_t ry, const T &color) { _tile.drawEllipse(x, y - _offsetY, rx, ry, color); }
    /// @brief Draw an ellipse outline with the current drawing color.
    void drawEllipse(int32_t x, int32_t y, int32_t rx, int32_t ry) { _tile.drawEllipse(x, y - _offsetY, rx, ry); }

    /// @brief Fill an ellipse centered at virtual (@p x, @p y).
    template <typename T>
    void fillEllipse(int32_t x, int32_t y, int32_t rx, int32_t ry, const T &color) { _tile.fillEllipse(x, y - _offsetY, rx, ry, color); }
    /// @brief Fill an ellipse with the current drawing color.
    void fillEllipse(int32_t x, int32_t y, int32_t rx, int32_t ry) { _tile.fillEllipse(x, y - _offsetY, rx, ry); }

    /// @brief Draw a triangle through virtual points (@p x0,@p y0), (@p x1,@p y1), (@p x2,@p y2).
    template <typename T>
    void drawTriangle(int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t x2, int32_t y2, const T &color) { _tile.drawTriangle(x0, y0 - _offsetY, x1, y1 - _offsetY, x2, y2 - _offsetY, color); }
    /// @brief Draw a triangle with the current drawing color.
    void drawTriangle(int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t x2, int32_t y2) { _tile.drawTriangle(x0, y0 - _offsetY, x1, y1 - _offsetY, x2, y2 - _offsetY); }

    /// @brief Fill a triangle through virtual points (@p x0,@p y0), (@p x1,@p y1), (@p x2,@p y2).
    template <typename T>
    void fillTriangle(int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t x2, int32_t y2, const T &color) { _tile.fillTriangle(x0, y0 - _offsetY, x1, y1 - _offsetY, x2, y2 - _offsetY, color); }
    /// @brief Fill a triangle with the current drawing color.
    void fillTriangle(int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t x2, int32_t y2) { _tile.fillTriangle(x0, y0 - _offsetY, x1, y1 - _offsetY, x2, y2 - _offsetY); }

    /// @brief Draw a quadratic Bezier curve through virtual points.
    template <typename T>
    void drawBezier(int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t x2, int32_t y2, const T &color) { _tile.drawBezier(x0, y0 - _offsetY, x1, y1 - _offsetY, x2, y2 - _offsetY, color); }
    /// @brief Draw a cubic Bezier curve through virtual points.
    template <typename T>
    void drawBezier(int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t x2, int32_t y2, int32_t x3, int32_t y3, const T &color) { _tile.drawBezier(x0, y0 - _offsetY, x1, y1 - _offsetY, x2, y2 - _offsetY, x3, y3 - _offsetY, color); }

    /// @brief Draw an elliptical arc centered at virtual (@p x, @p y).
    template <typename T>
    void drawEllipseArc(int32_t x, int32_t y, int32_t r0x, int32_t r1x, int32_t r0y, int32_t r1y, float angle0, float angle1, const T &color) { _tile.drawEllipseArc(x, y - _offsetY, r0x, r1x, r0y, r1y, angle0, angle1, color); }
    /// @brief Fill an elliptical arc centered at virtual (@p x, @p y).
    template <typename T>
    void fillEllipseArc(int32_t x, int32_t y, int32_t r0x, int32_t r1x, int32_t r0y, int32_t r1y, float angle0, float angle1, const T &color) { _tile.fillEllipseArc(x, y - _offsetY, r0x, r1x, r0y, r1y, angle0, angle1, color); }
    /// @brief Draw a circular arc centered at virtual (@p x, @p y).
    template <typename T>
    void drawArc(int32_t x, int32_t y, int32_t r0, int32_t r1, float angle0, float angle1, const T &color) { _tile.drawArc(x, y - _offsetY, r0, r1, angle0, angle1, color); }
    /// @brief Fill a circular arc centered at virtual (@p x, @p y).
    template <typename T>
    void fillArc(int32_t x, int32_t y, int32_t r0, int32_t r1, float angle0, float angle1, const T &color) { _tile.fillArc(x, y - _offsetY, r0, r1, angle0, angle1, color); }

    /// @brief Draw a circle corner helper using the current drawing color.
    void drawCircleHelper(int32_t x, int32_t y, int32_t r, uint_fast8_t cornername) { _tile.drawCircleHelper(x, y - _offsetY, r, cornername); }
    /// @brief Draw a circle corner helper.
    template <typename T>
    void drawCircleHelper(int32_t x, int32_t y, int32_t r, uint_fast8_t cornername, const T &color) { _tile.drawCircleHelper(x, y - _offsetY, r, cornername, color); }
    /// @brief Fill a circle corner helper using the current drawing color.
    void fillCircleHelper(int32_t x, int32_t y, int32_t r, uint_fast8_t corners, int32_t delta) { _tile.fillCircleHelper(x, y - _offsetY, r, corners, delta); }
    /// @brief Fill a circle corner helper.
    template <typename T>
    void fillCircleHelper(int32_t x, int32_t y, int32_t r, uint_fast8_t corners, int32_t delta, const T &color) { _tile.fillCircleHelper(x, y - _offsetY, r, corners, delta, color); }

    /// @brief Set the pivot point in virtual coordinates.
    void setPivot(float x, float y) { _tile.setPivot(x, y - _offsetY); }
    /// @brief Current pivot X.
    float getPivotX(void) const { return _tile.getPivotX(); }
    /// @brief Current pivot Y in virtual coordinates.
    float getPivotY(void) const { return _tile.getPivotY() + _offsetY; }

    /// @brief Create a LovyanGFX gradient color table from an array.
    template <const uint32_t N>
    const lgfx::v1::colors_t createGradient(const lgfx::v1::rgb888_t (&colors)[N]) { return _tile.createGradient(colors); }
    /// @brief Create a LovyanGFX gradient color table from a pointer/count pair.
    const lgfx::v1::colors_t createGradient(const lgfx::v1::rgb888_t *colors, const uint32_t count) { return _tile.createGradient(colors, count); }
    /// @brief Map a scalar to a gradient color.
    template <typename T = lgfx::v1::rgb888_t>
    T mapGradient(float val, float min, float max, const lgfx::v1::colors_t gradient) { return _tile.mapGradient<T>(val, min, max, gradient); }
    /// @brief Map a scalar to a gradient color from a raw RGB888 color table.
    template <typename T = lgfx::v1::rgb888_t>
    T mapGradient(float val, float min, double max, const lgfx::v1::rgb888_t *colors, uint32_t count) { return _tile.mapGradient<T>(val, min, max, colors, count); }

    /// @brief Draw a horizontal gradient line from virtual (@p x, @p y).
    template <typename T>
    void drawGradientHLine(int32_t x, int32_t y, int32_t w, const T &start, const T &end) { _tile.drawGradientHLine(x, y - _offsetY, w, start, end); }
    /// @brief Draw a vertical gradient line from virtual (@p x, @p y).
    template <typename T>
    void drawGradientVLine(int32_t x, int32_t y, int32_t h, const T &start, const T &end) { _tile.drawGradientVLine(x, y - _offsetY, h, start, end); }
    /// @brief Draw a gradient line between virtual points.
    template <typename T>
    void drawGradientLine(int32_t x0, int32_t y0, int32_t x1, int32_t y1, const T &start, const T &end) { _tile.drawGradientLine(x0, y0 - _offsetY, x1, y1 - _offsetY, start, end); }
    /// @brief Draw a gradient line between virtual points from a LovyanGFX color table.
    void drawGradientLine(int32_t x0, int32_t y0, int32_t x1, int32_t y1, const lgfx::v1::colors_t colors) { _tile.drawGradientLine(x0, y0 - _offsetY, x1, y1 - _offsetY, colors); }
    /// @brief Draw a horizontal gradient line from a LovyanGFX color table.
    void drawGradientHLine(int32_t x, int32_t y, uint32_t w, const lgfx::v1::colors_t colors) { _tile.drawGradientHLine(x, y - _offsetY, w, colors); }
    /// @brief Draw a vertical gradient line from a LovyanGFX color table.
    void drawGradientVLine(int32_t x, int32_t y, uint32_t h, const lgfx::v1::colors_t colors) { _tile.drawGradientVLine(x, y - _offsetY, h, colors); }
    /// @brief Fill a rectangle with a gradient.
    template <typename T>
    void fillGradientRect(int32_t x, int32_t y, uint32_t w, uint32_t h, const T &start, const T &end, lgfx::v1::gradient_fill_styles::fill_style_t style = lgfx::v1::gradient_fill_styles::RADIAL) { _tile.fillGradientRect(x, y - _offsetY, w, h, start, end, style); }
    /// @brief Fill a rectangle with a gradient from a LovyanGFX color table.
    void fillGradientRect(int32_t x, int32_t y, uint32_t w, uint32_t h, const lgfx::v1::colors_t colors, lgfx::v1::gradient_fill_styles::fill_style_t style = lgfx::v1::gradient_fill_styles::RADIAL) { _tile.fillGradientRect(x, y - _offsetY, w, h, colors, style); }

    /// @brief Draw an anti-aliased line between virtual points.
    template <typename T>
    void drawSmoothLine(int32_t x0, int32_t y0, int32_t x1, int32_t y1, const T &color) { _tile.drawSmoothLine(x0, y0 - _offsetY, x1, y1 - _offsetY, color); }
    /// @brief Draw a wide line between virtual points.
    template <typename T>
    void drawWideLine(int32_t x0, int32_t y0, int32_t x1, int32_t y1, float r, const T &color) { _tile.drawWideLine(x0, y0 - _offsetY, x1, y1 - _offsetY, r, color); }
    /// @brief Draw a gradient wide line between virtual points.
    void drawWideLine(int32_t x0, int32_t y0, int32_t x1, int32_t y1, float r, const lgfx::v1::colors_t colors) { _tile.drawWideLine(x0, y0 - _offsetY, x1, y1 - _offsetY, r, colors); }
    /// @brief Draw a wedge line between virtual points.
    template <typename T>
    void drawWedgeLine(int32_t x0, int32_t y0, int32_t x1, int32_t y1, float r0, float r1, const T &color) { _tile.drawWedgeLine(x0, y0 - _offsetY, x1, y1 - _offsetY, r0, r1, color); }
    /// @brief Draw a gradient wedge line between virtual points.
    void drawWedgeLine(int32_t x0, int32_t y0, int32_t x1, int32_t y1, float r0, float r1, const lgfx::v1::colors_t colors) { _tile.drawWedgeLine(x0, y0 - _offsetY, x1, y1 - _offsetY, r0, r1, colors); }
    /// @brief Draw a circular spot centered at virtual (@p x, @p y).
    template <typename T>
    void drawSpot(int32_t x, int32_t y, float r, const T &color) { _tile.drawSpot(x, y - _offsetY, r, color); }
    /// @brief Draw a gradient spot centered at virtual (@p x, @p y).
    void drawGradientSpot(int32_t x, int32_t y, float r, const lgfx::v1::colors_t colors) { _tile.drawGradientSpot(x, y - _offsetY, r, colors); }
    /// @brief Fill an anti-aliased rounded rectangle.
    template <typename T>
    void fillSmoothRoundRect(int32_t x, int32_t y, int32_t w, int32_t h, int32_t r, const T &color) { _tile.fillSmoothRoundRect(x, y - _offsetY, w, h, r, color); }
    /// @brief Fill an anti-aliased circle centered at virtual (@p x, @p y).
    template <typename T>
    void fillSmoothCircle(int32_t x, int32_t y, int32_t r, const T &color) { _tile.fillSmoothCircle(x, y - _offsetY, r, color); }

    /// @brief Push a @p w × @p h image to virtual (@p x, @p y).
    /// @note The sprite's per-pixel clip discards rows outside the tile
    ///       (including negative dest Y, where LGFX offsets into the source),
    ///       so images straddling tile boundaries reassemble correctly.
    template <typename T>
    void pushImage(int32_t x, int32_t y, int32_t w, int32_t h, const T *data) { _tile.pushImage(x, y - _offsetY, w, h, data); }
    /// @brief Push an image, treating @p transparent as a see-through color.
    template <typename T>
    void pushImage(int32_t x, int32_t y, int32_t w, int32_t h, const T *data, const T &transparent) { _tile.pushImage(x, y - _offsetY, w, h, data, transparent); }
    /// @brief Push an image with explicit source color depth and palette.
    template <typename T>
    void pushImage(int32_t x, int32_t y, int32_t w, int32_t h, const void *data, lgfx::v1::color_depth_t depth, const T *palette) { _tile.pushImage(x, y - _offsetY, w, h, data, depth, palette); }
    /// @brief Push an image with explicit source color depth, palette, and transparent raw color.
    template <typename T>
    void pushImage(int32_t x, int32_t y, int32_t w, int32_t h, const void *data, uint32_t transparent, lgfx::v1::color_depth_t depth, const T *palette) { _tile.pushImage(x, y - _offsetY, w, h, data, transparent, depth, palette); }
    /// @brief Push an image using LovyanGFX's DMA-capable path when available.
    template <typename T>
    void pushImageDMA(int32_t x, int32_t y, int32_t w, int32_t h, const T *data) { _tile.pushImageDMA(x, y - _offsetY, w, h, data); }
    /// @brief Push an image with explicit source color depth and palette using the DMA-capable path when available.
    template <typename T>
    void pushImageDMA(int32_t x, int32_t y, int32_t w, int32_t h, const void *data, lgfx::v1::color_depth_t depth, const T *palette) { _tile.pushImageDMA(x, y - _offsetY, w, h, data, depth, palette); }
    /// @brief Push an image with rotation and scaling.
    template <typename T>
    void pushImageRotateZoom(float dst_x, float dst_y, float src_x, float src_y, float angle, float zoom_x, float zoom_y, int32_t w, int32_t h, const T *data) { _tile.pushImageRotateZoom(dst_x, dst_y - _offsetY, src_x, src_y, angle, zoom_x, zoom_y, w, h, data); }
    /// @brief Push an image with rotation, scaling, and transparency.
    template <typename T1, typename T2>
    void pushImageRotateZoom(float dst_x, float dst_y, float src_x, float src_y, float angle, float zoom_x, float zoom_y, int32_t w, int32_t h, const T1 *data, const T2 &transparent) { _tile.pushImageRotateZoom(dst_x, dst_y - _offsetY, src_x, src_y, angle, zoom_x, zoom_y, w, h, data, transparent); }
    /// @brief Push an image with rotation/scaling, explicit source color depth, and palette.
    template <typename T>
    void pushImageRotateZoom(float dst_x, float dst_y, float src_x, float src_y, float angle, float zoom_x, float zoom_y, int32_t w, int32_t h, const void *data, lgfx::v1::color_depth_t depth, const T *palette) { _tile.pushImageRotateZoom(dst_x, dst_y - _offsetY, src_x, src_y, angle, zoom_x, zoom_y, w, h, data, depth, palette); }
    /// @brief Push an image with rotation/scaling, explicit source color depth, palette, and transparent raw color.
    template <typename T>
    void pushImageRotateZoom(float dst_x, float dst_y, float src_x, float src_y, float angle, float zoom_x, float zoom_y, int32_t w, int32_t h, const void *data, uint32_t transparent, lgfx::v1::color_depth_t depth, const T *palette) { _tile.pushImageRotateZoom(dst_x, dst_y - _offsetY, src_x, src_y, angle, zoom_x, zoom_y, w, h, data, transparent, depth, palette); }
    /// @brief Push an image with anti-aliased rotation and scaling.
    template <typename T>
    void pushImageRotateZoomWithAA(float dst_x, float dst_y, float src_x, float src_y, float angle, float zoom_x, float zoom_y, int32_t w, int32_t h, const T *data) { _tile.pushImageRotateZoomWithAA(dst_x, dst_y - _offsetY, src_x, src_y, angle, zoom_x, zoom_y, w, h, data); }
    /// @brief Push an image with anti-aliased rotation, scaling, and transparency.
    template <typename T1, typename T2>
    void pushImageRotateZoomWithAA(float dst_x, float dst_y, float src_x, float src_y, float angle, float zoom_x, float zoom_y, int32_t w, int32_t h, const T1 *data, const T2 &transparent) { _tile.pushImageRotateZoomWithAA(dst_x, dst_y - _offsetY, src_x, src_y, angle, zoom_x, zoom_y, w, h, data, transparent); }
    /// @brief Push an image with anti-aliased rotation/scaling, explicit source color depth, and palette.
    template <typename T>
    void pushImageRotateZoomWithAA(float dst_x, float dst_y, float src_x, float src_y, float angle, float zoom_x, float zoom_y, int32_t w, int32_t h, const void *data, lgfx::v1::color_depth_t depth, const T *palette) { _tile.pushImageRotateZoomWithAA(dst_x, dst_y - _offsetY, src_x, src_y, angle, zoom_x, zoom_y, w, h, data, depth, palette); }
    /// @brief Push an image with anti-aliased rotation/scaling, explicit source color depth, palette, and transparent raw color.
    template <typename T>
    void pushImageRotateZoomWithAA(float dst_x, float dst_y, float src_x, float src_y, float angle, float zoom_x, float zoom_y, int32_t w, int32_t h, const void *data, uint32_t transparent, lgfx::v1::color_depth_t depth, const T *palette) { _tile.pushImageRotateZoomWithAA(dst_x, dst_y - _offsetY, src_x, src_y, angle, zoom_x, zoom_y, w, h, data, transparent, depth, palette); }
    /// @brief Draw a 1-bit bitmap at virtual (@p x, @p y).
    template <typename T>
    void drawBitmap(int32_t x, int32_t y, const uint8_t *bitmap, int32_t w, int32_t h, const T &color) { _tile.drawBitmap(x, y - _offsetY, bitmap, w, h, color); }
    /// @brief Draw a 1-bit bitmap with foreground/background colors.
    template <typename T>
    void drawBitmap(int32_t x, int32_t y, const uint8_t *bitmap, int32_t w, int32_t h, const T &fg, const T &bg) { _tile.drawBitmap(x, y - _offsetY, bitmap, w, h, fg, bg); }
    /// @brief Draw an XBM-format bitmap at virtual (@p x, @p y).
    template <typename T>
    void drawXBitmap(int32_t x, int32_t y, const uint8_t *bitmap, int32_t w, int32_t h, const T &color) { _tile.drawXBitmap(x, y - _offsetY, bitmap, w, h, color); }
    /// @brief Draw an XBM-format bitmap with foreground/background colors.
    template <typename T>
    void drawXBitmap(int32_t x, int32_t y, const uint8_t *bitmap, int32_t w, int32_t h, const T &fg, const T &bg) { _tile.drawXBitmap(x, y - _offsetY, bitmap, w, h, fg, bg); }

    /// @brief Whether 16-bit image data byte swapping is enabled.
    bool getSwapBytes(void) const { return _tile.getSwapBytes(); }
    /// @brief Enable/disable 16-bit image data byte swapping for pushImage().
    void setSwapBytes(bool swap) { _tile.setSwapBytes(swap); }

    /// @brief Decode and draw a BMP image from memory at virtual (@p x, @p y).
    bool drawBmp(const uint8_t *data, uint32_t len, int32_t x = 0, int32_t y = 0, int32_t maxWidth = 0, int32_t maxHeight = 0, int32_t offX = 0, int32_t offY = 0, float scaleX = 1.0f, float scaleY = 0.0f, datum_t datum = datum_t::top_left) { return _tile.drawBmp(data, len, x, y - _offsetY, maxWidth, maxHeight, offX, offY, scaleX, scaleY, datum); }
    /// @brief Decode and draw a BMP image from a LovyanGFX DataWrapper.
    bool drawBmp(lgfx::v1::DataWrapper *data, int32_t x = 0, int32_t y = 0, int32_t maxWidth = 0, int32_t maxHeight = 0, int32_t offX = 0, int32_t offY = 0, float scaleX = 1.0f, float scaleY = 0.0f, datum_t datum = datum_t::top_left) { return _tile.drawBmp(data, x, y - _offsetY, maxWidth, maxHeight, offX, offY, scaleX, scaleY, datum); }
    /// @brief Decode and draw a BMP file from the default filesystem.
    bool drawBmpFile(const char *path, int32_t x = 0, int32_t y = 0, int32_t maxWidth = 0, int32_t maxHeight = 0, int32_t offX = 0, int32_t offY = 0, float scaleX = 1.0f, float scaleY = 0.0f, datum_t datum = datum_t::top_left) { return _tile.drawBmpFile(path, x, y - _offsetY, maxWidth, maxHeight, offX, offY, scaleX, scaleY, datum); }
    /// @brief Decode and draw a BMP file from @p fs.
    template <typename FS>
    bool drawBmpFile(FS &fs, const char *path, int32_t x = 0, int32_t y = 0, int32_t maxWidth = 0, int32_t maxHeight = 0, int32_t offX = 0, int32_t offY = 0, float scaleX = 1.0f, float scaleY = 0.0f, datum_t datum = datum_t::top_left) { return _tile.drawBmpFile(fs, path, x, y - _offsetY, maxWidth, maxHeight, offX, offY, scaleX, scaleY, datum); }

    /// @brief Decode and draw a JPEG image from memory at virtual (@p x, @p y).
    bool drawJpg(const uint8_t *data, uint32_t len, int32_t x = 0, int32_t y = 0, int32_t maxWidth = 0, int32_t maxHeight = 0, int32_t offX = 0, int32_t offY = 0, float scaleX = 1.0f, float scaleY = 0.0f, datum_t datum = datum_t::top_left) { return _tile.drawJpg(data, len, x, y - _offsetY, maxWidth, maxHeight, offX, offY, scaleX, scaleY, datum); }
    /// @brief Decode and draw a JPEG image from a LovyanGFX DataWrapper.
    bool drawJpg(lgfx::v1::DataWrapper *data, int32_t x = 0, int32_t y = 0, int32_t maxWidth = 0, int32_t maxHeight = 0, int32_t offX = 0, int32_t offY = 0, float scaleX = 1.0f, float scaleY = 0.0f, datum_t datum = datum_t::top_left) { return _tile.drawJpg(data, x, y - _offsetY, maxWidth, maxHeight, offX, offY, scaleX, scaleY, datum); }
    /// @brief Decode and draw a JPEG file from the default filesystem.
    bool drawJpgFile(const char *path, int32_t x = 0, int32_t y = 0, int32_t maxWidth = 0, int32_t maxHeight = 0, int32_t offX = 0, int32_t offY = 0, float scaleX = 1.0f, float scaleY = 0.0f, datum_t datum = datum_t::top_left) { return _tile.drawJpgFile(path, x, y - _offsetY, maxWidth, maxHeight, offX, offY, scaleX, scaleY, datum); }
    /// @brief Decode and draw a JPEG file from @p fs.
    template <typename FS>
    bool drawJpgFile(FS &fs, const char *path, int32_t x = 0, int32_t y = 0, int32_t maxWidth = 0, int32_t maxHeight = 0, int32_t offX = 0, int32_t offY = 0, float scaleX = 1.0f, float scaleY = 0.0f, datum_t datum = datum_t::top_left) { return _tile.drawJpgFile(fs, path, x, y - _offsetY, maxWidth, maxHeight, offX, offY, scaleX, scaleY, datum); }

    /// @brief Decode and draw a PNG image from memory at virtual (@p x, @p y).
    bool drawPng(const uint8_t *data, uint32_t len, int32_t x = 0, int32_t y = 0, int32_t maxWidth = 0, int32_t maxHeight = 0, int32_t offX = 0, int32_t offY = 0, float scaleX = 1.0f, float scaleY = 0.0f, datum_t datum = datum_t::top_left) { return _tile.drawPng(data, len, x, y - _offsetY, maxWidth, maxHeight, offX, offY, scaleX, scaleY, datum); }
    /// @brief Decode and draw a PNG image from a LovyanGFX DataWrapper.
    bool drawPng(lgfx::v1::DataWrapper *data, int32_t x = 0, int32_t y = 0, int32_t maxWidth = 0, int32_t maxHeight = 0, int32_t offX = 0, int32_t offY = 0, float scaleX = 1.0f, float scaleY = 0.0f, datum_t datum = datum_t::top_left) { return _tile.drawPng(data, x, y - _offsetY, maxWidth, maxHeight, offX, offY, scaleX, scaleY, datum); }
    /// @brief Decode and draw a PNG file from the default filesystem.
    bool drawPngFile(const char *path, int32_t x = 0, int32_t y = 0, int32_t maxWidth = 0, int32_t maxHeight = 0, int32_t offX = 0, int32_t offY = 0, float scaleX = 1.0f, float scaleY = 0.0f, datum_t datum = datum_t::top_left) { return _tile.drawPngFile(path, x, y - _offsetY, maxWidth, maxHeight, offX, offY, scaleX, scaleY, datum); }
    /// @brief Decode and draw a PNG file from @p fs.
    template <typename FS>
    bool drawPngFile(FS &fs, const char *path, int32_t x = 0, int32_t y = 0, int32_t maxWidth = 0, int32_t maxHeight = 0, int32_t offX = 0, int32_t offY = 0, float scaleX = 1.0f, float scaleY = 0.0f, datum_t datum = datum_t::top_left) { return _tile.drawPngFile(fs, path, x, y - _offsetY, maxWidth, maxHeight, offX, offY, scaleX, scaleY, datum); }

    /// @brief Decode and draw a QOI image from memory at virtual (@p x, @p y).
    bool drawQoi(const uint8_t *data, uint32_t len, int32_t x = 0, int32_t y = 0, int32_t maxWidth = 0, int32_t maxHeight = 0, int32_t offX = 0, int32_t offY = 0, float scaleX = 1.0f, float scaleY = 0.0f, datum_t datum = datum_t::top_left) { return _tile.drawQoi(data, len, x, y - _offsetY, maxWidth, maxHeight, offX, offY, scaleX, scaleY, datum); }
    /// @brief Decode and draw a QOI image from a LovyanGFX DataWrapper.
    bool drawQoi(lgfx::v1::DataWrapper *data, int32_t x = 0, int32_t y = 0, int32_t maxWidth = 0, int32_t maxHeight = 0, int32_t offX = 0, int32_t offY = 0, float scaleX = 1.0f, float scaleY = 0.0f, datum_t datum = datum_t::top_left) { return _tile.drawQoi(data, x, y - _offsetY, maxWidth, maxHeight, offX, offY, scaleX, scaleY, datum); }
    /// @brief Decode and draw a QOI file from the default filesystem.
    bool drawQoiFile(const char *path, int32_t x = 0, int32_t y = 0, int32_t maxWidth = 0, int32_t maxHeight = 0, int32_t offX = 0, int32_t offY = 0, float scaleX = 1.0f, float scaleY = 0.0f, datum_t datum = datum_t::top_left) { return _tile.drawQoiFile(path, x, y - _offsetY, maxWidth, maxHeight, offX, offY, scaleX, scaleY, datum); }
    /// @brief Decode and draw a QOI file from @p fs.
    template <typename FS>
    bool drawQoiFile(FS &fs, const char *path, int32_t x = 0, int32_t y = 0, int32_t maxWidth = 0, int32_t maxHeight = 0, int32_t offX = 0, int32_t offY = 0, float scaleX = 1.0f, float scaleY = 0.0f, datum_t datum = datum_t::top_left) { return _tile.drawQoiFile(fs, path, x, y - _offsetY, maxWidth, maxHeight, offX, offY, scaleX, scaleY, datum); }

    /// @brief Generate and draw a QR code at virtual (@p x, @p y).
    void qrcode(const char *string, int32_t x = -1, int32_t y = -1, int32_t width = -1, uint8_t version = 1, bool margin = false)
    {
        const int32_t w = (width < 0) ? ((_tile.width() < _virtualHeight) ? _tile.width() : _virtualHeight) : width;
        const int32_t vx = (x < 0) ? ((_tile.width() - w) / 2) : x;
        const int32_t vy = (y < 0) ? ((_virtualHeight - w) / 2) : y;
        _tile.qrcode(string, vx, vy - _offsetY, w, version, margin);
    }
#ifdef ARDUINO
    /// @brief Generate and draw a QR code from an Arduino String.
    void qrcode(const String &string, int32_t x = -1, int32_t y = -1, int32_t width = -1, uint8_t version = 1) { qrcode(string.c_str(), x, y, width, version); }
#endif

    /// @brief Set the text cursor to virtual (@p x, @p y).
    /// @note `print`/`println`/`printf` then advance the cursor and reproduce
    ///       deterministically across tiles.
    void setCursor(int32_t x, int32_t y) { _tile.setCursor(x, y - _offsetY); }
    /// @brief Set the text cursor and built-in font.
    void setCursor(int32_t x, int32_t y, uint8_t font) { _tile.setCursor(x, y - _offsetY, font); }
    /// @brief Set the text cursor and font pointer.
    void setCursor(int32_t x, int32_t y, const lgfx::v1::IFont *font) { _tile.setCursor(x, y - _offsetY, font); }
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
    /// @brief Set independent X/Y text size multipliers.
    void setTextSize(float sx, float sy) { _tile.setTextSize(sx, sy); }
    /// @brief Current X text size multiplier.
    float getTextSizeX(void) const { return _tile.getTextSizeX(); }
    /// @brief Current Y text size multiplier.
    float getTextSizeY(void) const { return _tile.getTextSizeY(); }
    /// @brief Set the text datum (alignment anchor) used by drawString().
    void setTextDatum(textdatum_t datum) { _tile.setTextDatum(datum); }
    /// @brief Set the text datum by numeric LovyanGFX datum value.
    void setTextDatum(uint8_t datum) { _tile.setTextDatum(datum); }
    /// @brief Current text datum.
    textdatum_t getTextDatum(void) const { return _tile.getTextDatum(); }
    /// @brief Set text padding used by aligned text drawing.
    void setTextPadding(uint32_t padding) { _tile.setTextPadding(padding); }
    /// @brief Current text padding.
    uint32_t getTextPadding(void) const { return _tile.getTextPadding(); }
    /// @brief Enable/disable cursor wrapping in X/Y directions.
    void setTextWrap(bool wrapX, bool wrapY = false) { _tile.setTextWrap(wrapX, wrapY); }
    /// @brief Enable/disable text auto-scroll. See limitations for tile-boundary behavior.
    void setTextScroll(bool scroll) { _tile.setTextScroll(scroll); }
    /// @brief Set the emoji drawing callback.
    void setEmojiCallback(lgfx::v1::emoji_draw_cb_t cb) { _tile.setEmojiCallback(cb); }
    /// @brief Current emoji drawing callback.
    lgfx::v1::emoji_draw_cb_t getEmojiCallback(void) const { return _tile.getEmojiCallback(); }
    /// @brief Set the whole LovyanGFX text style.
    void setTextStyle(const lgfx::v1::TextStyle &style) { _tile.setTextStyle(style); }
    /// @brief Current LovyanGFX text style.
    const lgfx::v1::TextStyle &getTextStyle(void) const { return _tile.getTextStyle(); }
    /// @brief Set the current text font (e.g. a built-in font pointer).
    template <typename F>
    void setFont(const F &font) { _tile.setFont(font); }
    /// @brief Set the current built-in text font by number.
    void setTextFont(uint8_t font) { _tile.setTextFont(font); }
    /// @brief Set the current text font by pointer.
    void setTextFont(const lgfx::v1::IFont *font) { _tile.setTextFont(font); }
    /// @brief Deprecated LovyanGFX-compatible free-font setter.
    void setFreeFont(const lgfx::v1::IFont *font = nullptr) { _tile.setFreeFont(font); }
    /// @brief Current font pointer.
    const lgfx::v1::IFont *getFont(void) const { return _tile.getFont(); }
    /// @brief Current built-in text font number when available.
    uint_fast8_t getTextFont(void) const { return _tile.getTextFont(); }
    /// @brief Current font height in pixels.
    int32_t fontHeight(void) const { return _tile.fontHeight(); }
    /// @brief Font height in pixels for @p font.
    template <typename F>
    int32_t fontHeight(const F &font) const { return _tile.fontHeight(font); }
    /// @brief Current font width in pixels.
    int32_t fontWidth(void) const { return _tile.fontWidth(); }
    /// @brief Font width in pixels for @p font.
    template <typename F>
    int32_t fontWidth(const F &font) const { return _tile.fontWidth(font); }
    /// @brief Text pixel width with the current font.
    template <typename S>
    int32_t textWidth(const S &str) { return _tile.textWidth(str); }
    /// @brief Text pixel width with an explicit font.
    template <typename S, typename F>
    int32_t textWidth(const S &str, const F &font) { return _tile.textWidth(str, font); }
    /// @brief Number of text characters that fit in @p width pixels.
    template <typename S>
    int32_t textLength(const S &str, int32_t width) { return _tile.textLength(str, width); }

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

    /// @brief Alias of drawCentreString(), matching LovyanGFX's US spelling.
    template <typename S>
    size_t drawCenterString(const S &str, int32_t x, int32_t y) { return _tile.drawCenterString(str, x, y - _offsetY); }
    /// @brief drawCenterString() with an explicit @p font.
    template <typename S, typename F>
    size_t drawCenterString(const S &str, int32_t x, int32_t y, const F &font) { return _tile.drawCenterString(str, x, y - _offsetY, font); }

    /// @brief Draw @p str right-aligned to virtual x = @p x, top at @p y.
    template <typename S>
    size_t drawRightString(const S &str, int32_t x, int32_t y) { return _tile.drawRightString(str, x, y - _offsetY); }
    /// @brief drawRightString() with an explicit @p font.
    template <typename S, typename F>
    size_t drawRightString(const S &str, int32_t x, int32_t y, const F &font) { return _tile.drawRightString(str, x, y - _offsetY, font); }

    /// @brief Draw a number at virtual (@p x, @p y).
    size_t drawNumber(long num, int32_t x, int32_t y) { return _tile.drawNumber(num, x, y - _offsetY); }
    /// @brief Draw a number at virtual (@p x, @p y) with an explicit font.
    template <typename F>
    size_t drawNumber(long num, int32_t x, int32_t y, const F &font) { return _tile.drawNumber(num, x, y - _offsetY, font); }
    /// @brief Draw a float at virtual (@p x, @p y).
    size_t drawFloat(float value, uint8_t dp, int32_t x, int32_t y) { return _tile.drawFloat(value, dp, x, y - _offsetY); }
    /// @brief Draw a float at virtual (@p x, @p y) with an explicit font.
    template <typename F>
    size_t drawFloat(float value, uint8_t dp, int32_t x, int32_t y, const F &font) { return _tile.drawFloat(value, dp, x, y - _offsetY, font); }
    /// @brief Draw a Unicode code point at virtual (@p x, @p y).
    size_t drawChar(uint16_t code, int32_t x, int32_t y) { return _tile.drawChar(code, x, y - _offsetY); }
    /// @brief Draw a Unicode code point at virtual (@p x, @p y) with a built-in font.
    size_t drawChar(uint16_t code, int32_t x, int32_t y, uint8_t font) { return _tile.drawChar(code, x, y - _offsetY, font); }
    /// @brief Draw a Unicode code point with explicit colors and uniform text size.
    template <typename T>
    size_t drawChar(int32_t x, int32_t y, uint16_t code, const T &color, const T &bg, float size) { return _tile.drawChar(x, y - _offsetY, code, color, bg, size); }
    /// @brief Draw a Unicode code point with explicit colors and X/Y text sizes.
    template <typename T>
    size_t drawChar(int32_t x, int32_t y, uint16_t code, const T &color, const T &bg, float sx, float sy) { return _tile.drawChar(x, y - _offsetY, code, color, bg, sx, sy); }

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
    /// @brief Write one UTF-8 byte/code unit at the cursor.
    size_t write(uint8_t utf8) { return _tile.write(utf8); }
    /// @brief Write a byte buffer at the cursor.
    size_t write(const uint8_t *buf, size_t size) { return _tile.write(buf, size); }
    /// @brief Write a C string at the cursor.
    size_t write(const char *str) { return _tile.write(str); }
    /// @brief Write a C string span at the cursor.
    size_t write(const char *buf, size_t size) { return _tile.write(buf, size); }

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
    /// @brief vprintf-style formatted text at the cursor.
    size_t vprintf(const char *format, va_list arg) { return _tile.vprintf(format, arg); }

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

    /// @brief Default per-tile memory budget for the no-arg/auto path (≈ 19 KB).
    ///
    /// When nothing is configured (no memoryLimit, splitCount, or tileHeight),
    /// the split count is derived so each tile fits this budget — exactly like
    /// setMemoryLimit() — so it scales with surface size (small surface → 1 tile,
    /// full screen → several). 19200 = a 320×30 tile at 16 bpp; taken from the
    /// Core2 benchmark, where it reproduces the measured-optimal split at every
    /// tested size while keeping each tile small (double-buffer total ≈ 2× this,
    /// and the small allocations avoid large-contiguous-block failures). A
    /// draw-bound workload may prefer fewer/larger tiles — override with
    /// setSplitCount()/setMemoryLimit(). See SPEC §10.1.
    static constexpr size_t DEFAULT_TILE_BYTES = 19200;

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
    /// @brief Force double-buffering on/off, overriding the default auto mode.
    ///
    /// **Default is auto** (this setter unset): double-buffering is enabled when
    /// the surface resolves to ≥ 2 tiles, and stays off for a single tile (where
    /// there is no neighbouring tile to overlap with, so a second buffer is pure
    /// waste). Call this to override that decision explicitly.
    ///
    /// With a single buffer the tile is reused for the next tile, so the transfer
    /// must complete before the next draw — the render loop waits for DMA after
    /// each push (correct, but draw and transfer are serialized). With
    /// double-buffering, tile `i` transfers (async DMA) from one buffer while
    /// tile `i+1` is drawn into the other; the SPI bus serializes consecutive
    /// transfers, so a buffer is never overwritten while its DMA is still in
    /// flight. Costs 2× the tile buffer (memoryLimit, if set, still bounds each
    /// individual buffer). @see SPEC §10.5.
    void setDoubleBuffer(bool enable) { _dbMode = enable ? DBMode::On : DBMode::Off; _dirty = true; }
    /// @brief Whether double-buffering is active (resolved at begin(); false before allocation).
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
        const int tileCount = (int)((regionH + tileH - 1) / tileH);
        // Resolve the double-buffer mode now that the tile count is known. Auto
        // enables it only when there are ≥ 2 tiles (a single tile has nothing to
        // overlap with). An explicit setDoubleBuffer() wins. See SPEC §10.5.
        bool wantDouble;
        switch (_dbMode)
        {
        case DBMode::On:
            wantDouble = true;
            break;
        case DBMode::Off:
            wantDouble = false;
            break;
        default:
            wantDouble = (tileCount >= 2);
            break;
        }
        _tile.deleteSprite();
        _tile2.deleteSprite();
        _tile.setColorDepth(_panel->getColorDepth());
        if (_tile.createSprite(regionW, tileH) == nullptr)
        {
            _ready = false;
            return false; // out of RAM
        }
        if (wantDouble)
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
        _tileCount = tileCount;
        _doubleBuffer = wantDouble; // resolved effective mode (read by renderRegion)
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
    // memoryLimit > splitCount > tileHeight > default tile budget (size-aware).
    int32_t computeTileHeight(int32_t W, int32_t H, int bits) const
    {
        if (_memLimit > 0)
            return tileHForBudget(W, H, bits, _memLimit);
        if (_splitCount > 0)
            return (H + _splitCount - 1) / _splitCount;
        if (_tileHeightCfg > 0)
            return (_tileHeightCfg > H) ? H : _tileHeightCfg;
        // Nothing set: derive split from the default per-tile budget so it scales
        // with surface size (small → 1 tile, full screen → several). See SPEC §10.1.
        return tileHForBudget(W, H, bits, DEFAULT_TILE_BYTES);
    }

    // Largest tile height whose row span fits @p budget bytes, clamped to H.
    // Returns 0 if a single row already exceeds the budget (cannot satisfy).
    static int32_t tileHForBudget(int32_t W, int32_t H, int bits, size_t budget)
    {
        const size_t bytesPerRow = ((size_t)W * (size_t)bits + 7) / 8;
        if (bytesPerRow == 0)
            return 0;
        int32_t th = (int32_t)(budget / bytesPerRow);
        if (th < 1)
            return 0; // cannot fit even one row → fail (no rounding)
        return (th > H) ? H : th;
    }

protected:
    LovyanGFX *_panel;
    LGFX_Sprite _tile;
    LGFX_Sprite _tile2; // second buffer, allocated only when _doubleBuffer

    // Double-buffer request: Auto resolves to On when the surface needs ≥ 2 tiles
    // (decided in beginRegion); setDoubleBuffer() sets On/Off explicitly.
    enum class DBMode { Auto, Off, On };

    // config
    size_t _memLimit = 0;
    int _splitCount = 0;
    int _tileHeightCfg = 0;
    uint32_t _bgColor = 0; // black
    bool _autoClear = true;
    DBMode _dbMode = DBMode::Auto;
    bool _doubleBuffer = false; // resolved effective mode (set in beginRegion)

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
    /// @param splitCount Number of tiles; 0 = auto (size-aware ~19 KB/tile budget). See SPEC §10.1.
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
