// vc_scene.cpp
//
// The same 3-way __has_include chain as the graphics smokes, kept in the .cpp
// so this translation unit can resolve the LovyanGFX class. This switch is a
// TEST-ONLY pattern (see tests/README); production code should include a
// single graphics library directly.

#if __has_include(<M5Unified.h>)
#include <M5Unified.h>
#elif __has_include(<M5GFX.h>)
#include <M5GFX.h>
#elif __has_include(<LovyanGFX.hpp>)
#include <LovyanGFX.hpp>
#include <LGFX_AUTODETECT.hpp>
#else
#error "vc_scene: link one of M5Unified / M5GFX / LovyanGFX"
#endif

#include <LGFXVirtualCanvas.h>
#include "vc_scene.h"

#include <stdio.h>
#include <stdlib.h>

static const uint8_t kBits8x8[] = {
    0x81,
    0x42,
    0x24,
    0x18,
    0x18,
    0x24,
    0x42,
    0x81,
};

void drawVcScene(LGFXVirtualCanvas &g)
{
    const int W = g.width();
    const int H = g.height();
    const int s = (W < H ? W : H);

    g.fillScreen(TFT_NAVY);
    g.drawRect(0, 0, W, H, TFT_WHITE);
    g.fillRect(W / 4, H / 4, W / 2, H / 2, TFT_RED);
    g.fillCircle(W / 2, H / 2, s / 6, TFT_YELLOW);
    g.drawCircle(W / 2, H / 2, s / 5, TFT_GREEN);
    g.drawLine(0, 0, W, H, TFT_CYAN);
    g.drawLine(0, H, W, 0, TFT_MAGENTA);
    g.drawFastHLine(0, H / 2, W, TFT_ORANGE);
    g.drawFastVLine(W / 2, 0, H, TFT_BLUE);
    g.drawBezier(8, H - 30, W / 3, H - 70, W - 8, H - 30, TFT_GREEN);
    g.drawArc(W - 42, 42, 22, 14, 30, 300, TFT_YELLOW);
    g.fillArc(W - 42, 42, 12, 6, 210, 330, TFT_CYAN);
    g.drawEllipseArc(42, H - 42, 8, 20, 14, 28, 20, 320, TFT_PINK);
    g.drawGradientLine(4, H / 2 + 8, W - 4, H / 2 + 18, TFT_RED, TFT_BLUE);
    g.fillGradientRect(8, H / 2 + 24, 42, 16, TFT_GREEN, TFT_BLACK);
    g.drawSmoothLine(54, H / 2 + 32, W / 2 - 8, H / 2 + 12, TFT_WHITE);
    g.drawWideLine(W / 2 + 8, H / 2 + 18, W - 54, H / 2 + 32, 2.0f, TFT_ORANGE);
    g.drawWedgeLine(W / 2 + 8, H / 2 + 38, W - 54, H / 2 + 35, 1.0f, 3.0f, TFT_MAGENTA);
    g.drawSpot(W - 34, H / 2 + 34, 4.0f, TFT_YELLOW);
    g.fillSmoothCircle(34, 42, 8, TFT_SKYBLUE);
    g.fillSmoothRoundRect(50, 34, 28, 18, 5, TFT_DARKCYAN);
    g.drawBitmap(W - 18, H - 18, kBits8x8, 8, 8, TFT_WHITE);
    g.drawXBitmap(W - 30, H - 18, kBits8x8, 8, 8, TFT_GREEN, TFT_BLACK);
    g.qrcode("VC", W - 36, 6, 24, 1);

    g.setTextColor(TFT_WHITE);
    g.setTextSize(1, 1);
    g.setTextPadding(0);
    g.setTextWrap(false);
    g.setCursor(6, 6);
    g.printf("%dx%d", W, H);
    g.drawString("VirtualCanvas", 6, H - 20);
    g.drawCenterString("C", W / 2, 6);
    g.drawNumber(123, W - 42, H / 2 - 10);
    g.drawFloat(3.14f, 2, W - 50, H / 2 + 4);
    g.drawChar('A', 6, H / 2 + 8);
}

bool savePng(LovyanGFX &src, const char *path)
{
    size_t len = 0;
    void *png = src.createPng(&len, 0, 0, src.width(), src.height());
    if (!png || len == 0)
        return false;
    FILE *fp = fopen(path, "wb");
    bool ok = false;
    if (fp)
    {
        ok = (fwrite(png, 1, len, fp) == len);
        fclose(fp);
    }
    free(png);
    return ok;
}
