// sprite — LGFXVirtualSprite: tiled sub-region, local coords, placed at (x,y).
//
// Verifies:
//  - tiling correctness: a 1-tile sprite (reference) vs a many-tile sprite
//    (forced via setMemoryLimit, with a partial last tile) are pixel-identical.
//  - region containment: pixels outside [x,y,w,h] are left untouched, including
//    rows just below a region that ends mid-screen (partial-last-tile clip).
//  - movable position: render(draw, x, y) places the sprite and updates pos.
//
// Output: output/sprite_ref.png, output/sprite_tiled.png, output/sprite_move.png

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

// Local-coordinate scene drawn into the sprite (0,0 = sprite top-left).
static void drawThing(LGFXVirtualCanvas &g)
{
    const int W = g.width(), H = g.height(), s = (W < H ? W : H);
    g.fillScreen(TFT_BLACK);
    g.drawRect(0, 0, W, H, TFT_WHITE);
    g.fillCircle(W / 2, H / 2, s / 3, TFT_YELLOW);
    g.drawLine(0, 0, W, H, TFT_CYAN);
    g.setTextColor(TFT_GREEN);
    g.setCursor(2, 2);
    g.printf("%dx%d", W, H);
}

// Region that ends mid-screen so the partial last tile (case B) must be clipped.
static const int RX = 40, RY = 30, RW = 160, RH = 100;

void setup()
{
    Serial.begin(115200);
    Serial.println("TEST start sprite");
    mkdir("output", 0755);
    lcd.init();
    Serial.printf("PANEL %dx%d\n", (int)lcd.width(), (int)lcd.height());

    // (A) reference: whole region in one tile.
    lcd.fillScreen(TFT_RED);
    {
        LGFXVirtualSprite spr(lcd, RW, RH, RX, RY);
        spr.setSplitCount(1);
        spr.begin();
        spr.render(drawThing);
        Serial.printf("REF tiles=%d tileH=%d\n", spr.tileCount(), spr.tileHeight());
    }
    save_png(lcd, "output/sprite_ref.png");

    // (B) many tiles incl. a partial last tile (forced by a small memory limit).
    lcd.fillScreen(TFT_RED);
    {
        LGFXVirtualSprite spr(lcd, RW, RH, RX, RY);
        spr.setMemoryLimit((size_t)RW * 2 * 17); // ~17 rows/tile -> 6 tiles, last 15
        spr.begin();
        spr.render(drawThing);
        Serial.printf("TILED tiles=%d tileH=%d\n", spr.tileCount(), spr.tileHeight());
    }
    save_png(lcd, "output/sprite_tiled.png");

    // (C) movable: a small sprite placed via render(draw, x, y).
    lcd.fillScreen(TFT_RED);
    {
        LGFXVirtualSprite spr(lcd, 32, 32);
        spr.begin();
        spr.render(drawThing, 100, 80);
        Serial.printf("MOVE x=%d y=%d\n", spr.x(), spr.y());
    }
    save_png(lcd, "output/sprite_move.png");

    Serial.println("TEST done");
}

void loop()
{
    delay(1000);
}
