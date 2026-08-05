// transparent — Tier 1: transparent transfer (SPEC §22).
//
// The core invariant mirrors parity's: split=1 is one full-surface sprite pushed
// once with the transparent color masked out — i.e. exactly what a hand-written
// LovyanGFX overlay would do — so every other split count and both split axes
// must be pixel-identical to it.
//
//   inv<N>  : base frame + transparent overlay at split N / columns / double buffer
//   opaque  : a fully covering scene pushed transparently == pushed opaquely
//   color   : setTransparentColor() honours the C++ type of the value (RGB565
//             TFT_* constants vs RGB888 uint32_t) — SPEC §22.4
//   diff    : an unchanged overlay transfers nothing; switching Opaque <-> Transparent
//             (and changing the color) invalidates automatically — SPEC §22.5
//   sprite  : LGFXVirtualSprite::renderTransparent() over a placed rectangle
//   inv24   : 24bpp, exercising the color conversion of the masked value
//
// STAT lines carry the counters the Python side asserts on.
// Output: output/t_<case>.png

#include <LovyanGFX.hpp>
#include <LGFX_AUTODETECT.hpp>
#include <LGFXVirtualCanvas.h>
#include <stdio.h>
#include <sys/stat.h>

static LGFX lcd;

// Dialog geometry. Rounded corners are the point: the corner pixels must keep
// showing the base layer, which is what a rectangular transfer cannot do.
static constexpr int DLG_X = 40;
static constexpr int DLG_Y = 70;
static constexpr int DLG_W = 160;
static constexpr int DLG_H = 90;
static constexpr int DLG_R = 12;

// Probe points the Python side reads out of the PNGs.
static constexpr int PROBE_OUT_X = 8; // far outside the dialog
static constexpr int PROBE_OUT_Y = 8;

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

// Base layer: a busy, fully covering image so anything wrongly transferred over
// it is visible, and every tile has deterministic content.
static void baseScene(LGFXVirtualCanvas &g)
{
    const int W = g.width(), H = g.height();
    g.fillScreen(TFT_NAVY);
    for (int y = 0; y < H; y += 16)
        g.drawFastHLine(0, y, W, TFT_DARKGREEN);
    for (int x = 0; x < W; x += 16)
        g.drawFastVLine(x, 0, H, TFT_MAROON);
    g.fillCircle(W - 40, H - 40, 24, TFT_ORANGE);
    g.setTextColor(TFT_WHITE);
    g.drawString("base layer", 6, 30);
}

// Overlay layer: nothing clears the tile — auto-clear already filled it with the
// transparent color, so every pixel not drawn here shows the base layer.
static void dialogScene(LGFXVirtualCanvas &g)
{
    g.fillRoundRect(DLG_X, DLG_Y, DLG_W, DLG_H, DLG_R, TFT_DARKGREY);
    g.drawRoundRect(DLG_X, DLG_Y, DLG_W, DLG_H, DLG_R, TFT_WHITE);
    g.setTextColor(TFT_WHITE);
    g.drawString("Dialog", DLG_X + 12, DLG_Y + 14);
    g.drawString("crossing tiles", DLG_X + 12, DLG_Y + 40);
    // A shadow-ish dotted skirt: isolated pixels, i.e. many short runs per row.
    for (int x = DLG_X; x < DLG_X + DLG_W; x += 3)
        g.drawPixel(x, DLG_Y + DLG_H + 2, TFT_BLACK);
}

// Covers every pixel, so masking can have no effect: transparent and opaque
// transfers of this scene must produce the same panel.
static void coveringScene(LGFXVirtualCanvas &g)
{
    const int W = g.width(), H = g.height();
    g.fillScreen(TFT_PURPLE);
    g.fillRect(10, 10, W - 20, H - 20, TFT_YELLOW);
    g.drawString("covering", 20, 20);
}

// Draws with the RGB565 constant TFT_TRANSPARENT and with the RGB888 spelling of
// the *same* color; both must be masked out when that color is the transparent
// one, whichever way it was handed to setTransparentColor(). Only the marker
// rect may reach the panel.
static void typedColorScene(LGFXVirtualCanvas &g)
{
    g.fillScreen((int)0x0120);             // RGB565 (as TFT_TRANSPARENT is typed)
    g.fillRect(0, 0, 40, 40, (uint32_t)0x002400); // the same color as RGB888
    g.fillRect(DLG_X, DLG_Y, 30, 30, TFT_WHITE);  // the only visible pixels
}

static void spriteScene(LGFXVirtualCanvas &g)
{
    g.fillCircle(g.width() / 2, g.height() / 2, g.height() / 2 - 2, TFT_CYAN);
    g.drawRect(0, 0, g.width(), g.height(), TFT_RED); // reaches the sprite edges
}

static void reportStats(const char *name, LGFXVirtualTiledBase &s)
{
    Serial.printf("STAT %s pushed=%u total=%u tiles=%d tileH=%d\n", name,
                  (unsigned)s.diffPushedPixels(), (unsigned)s.diffTotalPixels(),
                  s.tileCount(), s.tileHeight());
}

// base frame (opaque, split 1) + dialog overlay at the requested configuration.
// Both managers are separate objects, which is the recommended layering.
static void overlayCase(const char *tag, int split, bool columns, bool doubleBuffer)
{
    lcd.fillScreen(TFT_BLACK);
    {
        LGFXVirtualScreen base(lcd, 1);
        base.render(baseScene);
    }
    {
        LGFXVirtualScreen overlay(lcd, split);
        if (columns)
            overlay.setSplitAxis(LGFXVirtualSplitAxis::Columns);
        if (doubleBuffer)
            overlay.setDoubleBuffer(true);
        overlay.renderTransparent(dialogScene);
        reportStats(tag, overlay);
    }
    char path[64];
    snprintf(path, sizeof(path), "output/t_%s.png", tag);
    save_png(lcd, path);
    Serial.printf("CASE %s done\n", tag);
}

void setup()
{
    Serial.begin(115200);
    Serial.println("TEST start transparent");
    mkdir("output", 0755);
    lcd.init();
    Serial.printf("PANEL %dx%d\n", (int)lcd.width(), (int)lcd.height());
    Serial.printf("DLG x=%d y=%d w=%d h=%d r=%d\n", DLG_X, DLG_Y, DLG_W, DLG_H, DLG_R);
    Serial.printf("PROBE x=%d y=%d\n", PROBE_OUT_X, PROBE_OUT_Y);
    // The default must be the value LovyanGFX calls TFT_TRANSPARENT, expressed as
    // RGB888 (that is how it is stored) — proves the type-aware conversion.
    {
        LGFXVirtualScreen probe(lcd, 1);
        Serial.printf("DEFCOLOR %u\n", (unsigned)probe.transparentColor());
        probe.setTransparentColor(TFT_TRANSPARENT);
        Serial.printf("SETCOLOR565 %u\n", (unsigned)probe.transparentColor());
        probe.setTransparentColor((uint32_t)0x002400);
        Serial.printf("SETCOLOR888 %u\n", (unsigned)probe.transparentColor());
    }

    // inv1 is the reference: one tile → a single full-surface masked push.
    overlayCase("inv1", 1, false, false);
    overlayCase("inv3", 3, false, false);
    overlayCase("inv7", 7, false, false); // 240/7 → partial last tile
    overlayCase("inv13", 13, false, false);
    overlayCase("col3", 3, true, false);
    overlayCase("col7", 7, true, false);
    overlayCase("db3", 3, false, true);
    overlayCase("coldb7", 7, true, true);

    // opaque: a fully covering scene — masking must change nothing.
    lcd.fillScreen(TFT_BLACK);
    {
        LGFXVirtualScreen s(lcd, 7);
        s.render(coveringScene);
    }
    save_png(lcd, "output/t_cover_opaque.png");
    lcd.fillScreen(TFT_BLACK);
    {
        LGFXVirtualScreen s(lcd, 7);
        s.renderTransparent(coveringScene);
    }
    save_png(lcd, "output/t_cover_transp.png");
    Serial.println("CASE opaque done");

    // color: the transparent color given as RGB565 and as RGB888 must mask the
    // same pixels, at any split.
    for (int pass = 0; pass < 2; ++pass)
    {
        lcd.fillScreen(TFT_BLACK);
        {
            LGFXVirtualScreen base(lcd, 1);
            base.render(baseScene);
        }
        {
            LGFXVirtualScreen overlay(lcd, 5);
            if (pass == 0)
                overlay.setTransparentColor(TFT_TRANSPARENT);   // int → RGB565
            else
                overlay.setTransparentColor((uint32_t)0x002400); // uint32_t → RGB888
            overlay.renderTransparent(typedColorScene);
        }
        char path[64];
        snprintf(path, sizeof(path), "output/t_color_%d.png", pass);
        save_png(lcd, path);
    }
    Serial.println("CASE color done");

    // diff: an unchanged overlay transfers nothing, and every change of the
    // effective transparency (mode or color) invalidates by itself.
    lcd.fillScreen(TFT_BLACK);
    {
        LGFXVirtualScreen base(lcd, 1);
        base.render(baseScene);

        LGFXVirtualScreen overlay(lcd, 7);
        overlay.setDiffMode(LGFXVirtualDiffMode::Tile);
        overlay.renderTransparent(dialogScene);
        reportStats("diff_first", overlay);
        overlay.renderTransparent(dialogScene);
        reportStats("diff_same", overlay);
        // Same content, different transparent color → must not be skipped.
        overlay.setTransparentColor((uint32_t)0x010203);
        overlay.renderTransparent(dialogScene);
        reportStats("diff_recolor", overlay);
        overlay.setTransparentColor(TFT_TRANSPARENT);
        overlay.renderTransparent(dialogScene);
        reportStats("diff_recolor2", overlay);
        // Same content, opaque this time → must not be skipped either.
        overlay.render(dialogScene);
        reportStats("diff_opaque", overlay);
        save_png(lcd, "output/t_diff_opaque.png");
    }
    Serial.println("CASE diff done");

    // bgeq: the case where the tile hash alone cannot notice the switch. With the
    // auto-clear color set to the transparent color, render() and
    // renderTransparent() leave *byte-identical* tiles, yet the panel differs
    // (opaque paints that color everywhere, transparent keeps the base layer).
    // Only the tracked transparency state can force the transfer here (§22.5).
    lcd.fillScreen(TFT_BLACK);
    {
        LGFXVirtualScreen base(lcd, 1);
        base.render(baseScene);

        LGFXVirtualScreen overlay(lcd, 7);
        overlay.setDiffMode(LGFXVirtualDiffMode::Tile);
        overlay.setBackgroundColor(overlay.transparentColor());
        overlay.renderTransparent(dialogScene);
        reportStats("bgeq_transp", overlay);
        save_png(lcd, "output/t_bgeq_transp.png");
        overlay.render(dialogScene);
        reportStats("bgeq_opaque", overlay);
        save_png(lcd, "output/t_bgeq_opaque.png");
        overlay.renderTransparent(dialogScene);
        reportStats("bgeq_back", overlay);
    }
    Serial.println("CASE bgeq done");

    // sprite: a placed tiled sprite pushed transparently.
    lcd.fillScreen(TFT_BLACK);
    {
        LGFXVirtualScreen base(lcd, 1);
        base.render(baseScene);
    }
    {
        LGFXVirtualSprite sp(lcd, 120, 60, DLG_X, DLG_Y);
        sp.setSplitCount(1);
        sp.renderTransparent(spriteScene);
        reportStats("sp1", sp);
    }
    save_png(lcd, "output/t_sp1.png");
    lcd.fillScreen(TFT_BLACK);
    {
        LGFXVirtualScreen base(lcd, 1);
        base.render(baseScene);
    }
    {
        LGFXVirtualSprite sp(lcd, 120, 60);
        sp.setSplitCount(4);
        sp.renderTransparent(spriteScene, DLG_X, DLG_Y);
        reportStats("sp4", sp);
    }
    save_png(lcd, "output/t_sp4.png");
    Serial.println("CASE sprite done");

    // inv24: the masked value goes through a different color conversion at 24bpp.
    lcd.setColorDepth(24);
    Serial.printf("DEPTH %d\n", ((int)lcd.getColorDepth()) & 0x00FF);
    overlayCase("d24_1", 1, false, false);
    overlayCase("d24_7", 7, false, false);
    lcd.setColorDepth(16);

    Serial.println("TEST done");
}

void loop()
{
    delay(1000);
}
