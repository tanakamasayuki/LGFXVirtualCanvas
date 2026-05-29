// parity — pixel-exact parity between full render (split=1) and tiled render.
//
// The same drawScene() is rendered through LGFXVirtualScreen at several split
// counts. split=1 is one full-height tile (offsetY=0) and serves as the
// "full render" reference; split=2/3/5/7 exercise the tiling, offset, clip
// and reassembly. The Python side asserts every PNG is pixel-identical to
// split_1.png. See SPEC §13.
//
// Output: output/split_<N>.png

#include <LovyanGFX.hpp>
#include <LGFX_AUTODETECT.hpp>
#include <LGFXVirtualCanvas.h>
#include <stdio.h>
#include <sys/stat.h>

static LGFX lcd;

static bool save_png(LovyanGFX &src, const char *path)
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

// Scene under test — pure virtual coordinates, covers the whole canvas via
// fillScreen, and includes shapes/text that straddle tile boundaries.
static void drawScene(LGFXVirtualCanvas &g)
{
    const int W = g.width();
    const int H = g.height();
    const int s = (W < H ? W : H);

    g.fillScreen(TFT_BLACK);
    g.drawRect(0, 0, W, H, TFT_WHITE);
    g.fillRect(W / 4, H / 4, W / 2, H / 2, TFT_RED);
    g.fillCircle(W / 2, H / 2, s / 6, TFT_YELLOW);
    g.drawCircle(W / 2, H / 2, s / 5, TFT_GREEN);
    g.drawLine(0, 0, W, H, TFT_CYAN);
    g.drawLine(0, H, W, 0, TFT_MAGENTA);
    g.drawFastHLine(0, H / 2, W, TFT_BLUE);
    g.drawFastVLine(W / 2, 0, H, TFT_ORANGE);
    for (int i = 0; i < W; i += 7)
        g.drawPixel(i, (i * 3) % H, TFT_WHITE);

    g.setTextColor(TFT_WHITE);
    g.setCursor(6, 6);
    g.printf("W=%d H=%d", W, H);
    g.drawString("VirtualCanvas", 6, 22);
}

void setup()
{
    Serial.begin(115200);
    Serial.println("TEST start parity");

    mkdir("output", 0755);
    lcd.init();
    Serial.printf("PANEL %dx%d\n", (int)lcd.width(), (int)lcd.height());

    const int splits[] = {1, 2, 3, 5, 7};
    for (int s : splits)
    {
        LGFXVirtualScreen screen(lcd, s);
        const bool began = screen.begin();
        const bool rendered = screen.render(drawScene);

        char path[64];
        snprintf(path, sizeof(path), "output/split_%d.png", s);
        const bool saved = save_png(lcd, path);

        Serial.printf("SPLIT %d begin=%d render=%d N=%d th=%d save=%d\n",
                      s, began, rendered, screen.tileCount(), screen.tileHeight(), saved);
    }

    Serial.println("TEST done");
}

void loop()
{
    delay(1000);
}
