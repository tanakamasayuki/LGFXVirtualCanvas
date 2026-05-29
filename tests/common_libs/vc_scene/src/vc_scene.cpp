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

    g.setTextColor(TFT_WHITE);
    g.setCursor(6, 6);
    g.printf("%dx%d", W, H);
    g.drawString("VirtualCanvas", 6, H - 20);
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
