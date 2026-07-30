// diff — Tier 1: diff transfer (SPEC §21).
//
// The core invariant is the same as parity's: diff transfer must not change a
// single output pixel. Every case renders frame 0 then frame 1 (only a small
// bar differs between them) and compares the final panel with diffing off vs
// LGFXVirtualDiffMode::Tile.
//
//   inv7  : split=7 over 240 rows → tileH=35, partial last tile
//   inv1  : split=1 (single tile; no double-buffering)
//   inv24 : 24bpp, exercising the non-multiple-of-4 row bytes in the hash tail
//   same  : the same frame twice → the second render must transfer nothing
//   stale : foreign drawing on the panel stays until invalidate() (as specified)
//   sprite: LGFXVirtualSprite skips a repeat, and a position change invalidates
//   modeoff: Off allocates no hash table
//
// STAT lines carry the counters the Python side asserts on.
// Output: output/d_<case>.png

#include <LovyanGFX.hpp>
#include <LGFX_AUTODETECT.hpp>
#include <LGFXVirtualCanvas.h>
#include <stdio.h>
#include <sys/stat.h>

static LGFX lcd;

// The only frame-dependent element, at a fixed Y band so the expected set of
// transferred tiles is computable on the Python side.
static constexpr int BAR_X = 20;
static constexpr int BAR_Y = 100;
static constexpr int BAR_W = 60;
static constexpr int BAR_H = 10;

// Where the foreign (direct-to-panel) drawing of the `stale` case goes.
static constexpr int FOREIGN_X = 200;
static constexpr int FOREIGN_Y = 150;
static constexpr int FOREIGN_W = 40;
static constexpr int FOREIGN_H = 20;

struct Frame
{
    int id;
};

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

// Fully covering, so every tile has deterministic content and only the bar
// tiles differ between frames.
static void scene(LGFXVirtualCanvas &g, Frame &f)
{
    const int W = g.width(), H = g.height();
    g.fillScreen(TFT_NAVY);
    g.drawRect(4, 4, W - 8, H - 8, TFT_WHITE);
    g.drawString("diff", 10, 20);
    g.fillCircle(W - 40, H - 40, 12, TFT_CYAN);
    g.fillRect(BAR_X, BAR_Y, BAR_W, BAR_H, f.id == 0 ? TFT_RED : TFT_GREEN);
}

static void spriteScene(LGFXVirtualCanvas &g)
{
    g.fillScreen(TFT_ORANGE);
    g.drawRect(0, 0, g.width(), g.height(), TFT_BLACK);
}

// Takes the shared base so one function serves both managers. (Not a template:
// the .ino auto-prototype generator cannot handle one.)
static void reportStats(const char *name, LGFXVirtualTiledBase &s)
{
    Serial.printf("STAT %s pushed=%u total=%u mem=%u tiles=%d tileH=%d\n", name,
                  (unsigned)s.diffPushedPixels(), (unsigned)s.diffTotalPixels(),
                  (unsigned)s.diffMemoryUsage(), s.tileCount(), s.tileHeight());
}

// Render frame 0 then frame 1 with diffing off, then the same pair with
// Tile diffing, saving both results for a pixel comparison.
static void invariance(const char *tag, int split)
{
    char path[64];

    lcd.fillScreen(TFT_BLACK);
    {
        LGFXVirtualScreen screen(lcd, split);
        Frame f{0};
        screen.render(scene, f);
        f.id = 1;
        screen.render(scene, f);
        snprintf(path, sizeof(path), "output/d_%s_off.png", tag);
        save_png(lcd, path);
    }

    lcd.fillScreen(TFT_BLACK);
    {
        LGFXVirtualScreen screen(lcd, split);
        screen.setDiffMode(LGFXVirtualDiffMode::Tile);
        Frame f{0};
        screen.render(scene, f);
        char name[32];
        snprintf(name, sizeof(name), "%s_first", tag);
        reportStats(name, screen);
        f.id = 1;
        screen.render(scene, f);
        snprintf(name, sizeof(name), "%s_second", tag);
        reportStats(name, screen);
        snprintf(path, sizeof(path), "output/d_%s_tile.png", tag);
        save_png(lcd, path);
    }
    Serial.printf("CASE %s done\n", tag);
}

void setup()
{
    Serial.begin(115200);
    Serial.println("TEST start diff");
    mkdir("output", 0755);
    lcd.init();
    Serial.printf("PANEL %dx%d\n", (int)lcd.width(), (int)lcd.height());
    Serial.printf("BAR y=%d h=%d\n", BAR_Y, BAR_H);
    Serial.printf("FOREIGN x=%d y=%d\n", FOREIGN_X, FOREIGN_Y);

    invariance("inv7", 7);
    invariance("inv1", 1);

    // 24bpp: 3 bytes per pixel, so a row is not a multiple of 4 bytes and the
    // hash's tail path runs. Falls back to whatever the panel supports; the
    // invariant holds either way (the depth is reported for the record).
    lcd.setColorDepth(24);
    Serial.printf("DEPTH %d\n", ((int)lcd.getColorDepth()) & 0x00FF);
    invariance("inv24", 7);
    lcd.setColorDepth(16);

    // same: identical content twice → the second render transfers nothing.
    lcd.fillScreen(TFT_BLACK);
    {
        LGFXVirtualScreen screen(lcd, 7);
        screen.setDiffMode(LGFXVirtualDiffMode::Tile);
        Frame f{0};
        screen.render(scene, f);
        reportStats("same_first", screen);
        screen.render(scene, f);
        reportStats("same_second", screen);
    }
    Serial.println("CASE same done");

    // stale: drawing straight to the panel is undetectable, so the foreign rect
    // survives a re-render (specified behaviour) and invalidate() clears it.
    lcd.fillScreen(TFT_BLACK);
    {
        LGFXVirtualScreen screen(lcd, 7);
        screen.setDiffMode(LGFXVirtualDiffMode::Tile);
        Frame f{0};
        screen.render(scene, f);
        lcd.fillRect(FOREIGN_X, FOREIGN_Y, FOREIGN_W, FOREIGN_H, TFT_MAGENTA);
        screen.render(scene, f);
        reportStats("stale", screen);
        save_png(lcd, "output/d_stale.png");
        screen.invalidate();
        screen.render(scene, f);
        reportStats("healed", screen);
        save_png(lcd, "output/d_healed.png");
    }
    Serial.println("CASE stale done");

    // sprite: a repeat is skipped; moving the sprite invalidates (the backdrop
    // at the new position is a different image).
    lcd.fillScreen(TFT_BLACK);
    {
        LGFXVirtualSprite sp(lcd, 100, 60, 10, 10);
        sp.setDiffMode(LGFXVirtualDiffMode::Tile);
        sp.render(spriteScene);
        reportStats("sp_first", sp);
        sp.render(spriteScene);
        reportStats("sp_same", sp);
        sp.render(spriteScene, 120, 40);
        reportStats("sp_moved", sp);
    }
    Serial.println("CASE sprite done");

    // modeoff: the default costs no memory and transfers everything.
    {
        LGFXVirtualScreen screen(lcd, 7);
        Frame f{0};
        screen.render(scene, f);
        reportStats("modeoff", screen);
    }
    Serial.println("CASE modeoff done");

    Serial.println("TEST done");
}

void loop()
{
    delay(1000);
}
