// autoclear — Tier 1 T1-8: auto-clear / setBackgroundColor / setAutoClear.
//
// Three pairs, each rendered at split=1 and split=3 and asserted identical:
//   black : non-covering scene, default auto-clear (black bg). Undrawn pixels
//           must be deterministic (black), so split-invariant.
//   navy  : same non-covering scene with setBackgroundColor(NAVY). Undrawn
//           pixels follow the configured bg; still split-invariant.
//   off   : a fully-covering scene with setAutoClear(false). With the draw
//           covering every pixel there is no leftover, so still identical.
//
// The Python side also checks black-corner != navy-corner (setBackgroundColor
// actually changed the undrawn area).
//
// Output: output/ac_<case>_<split>.png

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

// Non-covering: deliberately does NOT fillScreen, so undrawn pixels come from
// auto-clear.
static void sceneNoFill(LGFXVirtualCanvas &g)
{
    const int W = g.width(), H = g.height();
    g.fillRect(W / 4, H / 4, W / 2, H / 2, TFT_RED);
    g.fillCircle(W / 2, H / 2, 28, TFT_YELLOW);
}

// Fully covering: paints every pixel via fillScreen.
static void sceneFull(LGFXVirtualCanvas &g)
{
    const int W = g.width(), H = g.height();
    g.fillScreen(TFT_DARKGREEN);
    g.fillRect(W / 4, H / 4, W / 2, H / 2, TFT_RED);
}

void setup()
{
    Serial.begin(115200);
    Serial.println("TEST start autoclear");
    mkdir("output", 0755);
    lcd.init();
    Serial.printf("PANEL %dx%d\n", (int)lcd.width(), (int)lcd.height());

    // black: default auto-clear, non-covering
    for (int s : {1, 3})
    {
        LGFXVirtualScreen screen(lcd, s);
        screen.render(sceneNoFill);
        char p[64];
        snprintf(p, sizeof(p), "output/ac_black_%d.png", s);
        save_png(lcd, p);
    }
    Serial.println("CASE black done");

    // navy: custom background color, non-covering
    for (int s : {1, 3})
    {
        LGFXVirtualScreen screen(lcd, s);
        screen.setBackgroundColor(TFT_NAVY);
        screen.render(sceneNoFill);
        char p[64];
        snprintf(p, sizeof(p), "output/ac_navy_%d.png", s);
        save_png(lcd, p);
    }
    Serial.println("CASE navy done");

    // off: auto-clear disabled, fully-covering scene
    for (int s : {1, 3})
    {
        LGFXVirtualScreen screen(lcd, s);
        screen.setAutoClear(false);
        screen.render(sceneFull);
        char p[64];
        snprintf(p, sizeof(p), "output/ac_off_%d.png", s);
        save_png(lcd, p);
    }
    Serial.println("CASE off done");

    // bgtype: setBackgroundColor() must read a color the same way every other
    // drawing call does — by the value's C++ type (RGB565 for TFT_* / int,
    // RGB888 for uint32_t). The reference is the panel painted with the very
    // same constant, so this holds at any color depth.
    lcd.fillScreen(TFT_BLACK);
    lcd.fillRect(0, 0, 20, 20, TFT_NAVY); // what TFT_NAVY actually looks like
    save_png(lcd, "output/ac_bgref.png");
    {
        LGFXVirtualScreen screen(lcd, 3);
        screen.setBackgroundColor(TFT_NAVY); // int -> RGB565
        screen.render(sceneNoFill);
        save_png(lcd, "output/ac_bg565.png");
    }
    {
        LGFXVirtualScreen screen(lcd, 3);
        // The RGB888 spelling of the same color: TFT_NAVY is RGB565 0x000F,
        // i.e. b5=15 -> b8=(15<<3)|(15>>2)=123.
        screen.setBackgroundColor((uint32_t)0x00007B);
        screen.render(sceneNoFill);
        save_png(lcd, "output/ac_bg888.png");
    }
    Serial.println("CASE bgtype done");

    Serial.println("TEST done");
}

void loop()
{
    delay(1000);
}
