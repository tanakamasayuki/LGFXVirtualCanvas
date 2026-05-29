// build_m5gfx — Tier 2 cross-library check (M5GFX entry point).
//
// Verifies LGFXVirtualCanvas.h compiles after <M5GFX.h> and that the shared
// drawVcScene renders identically at split=1 (full) and split=3 (tiled).

#include <M5GFX.h>
#include <LGFXVirtualCanvas.h>
#include <vc_scene.h>
#include <stdio.h>
#include <sys/stat.h>

static M5GFX lcd;

static void renderSplit(LovyanGFX &panel, int split, const char *path)
{
    LGFXVirtualScreen screen(panel, split);
    const bool rendered = screen.render(drawVcScene);
    const bool saved = savePng(panel, path);
    Serial.printf("SPLIT %d render=%d N=%d save=%d\n",
                  split, rendered, screen.tileCount(), saved);
}

void setup()
{
    Serial.begin(115200);
    Serial.println("TEST start build_m5gfx");

    mkdir("output", 0755);
    lcd.init();
    Serial.printf("PANEL %dx%d\n", (int)lcd.width(), (int)lcd.height());

    renderSplit(lcd, 1, "output/split_1.png");
    renderSplit(lcd, 3, "output/split_3.png");

    Serial.println("TEST done");
}

void loop()
{
    delay(1000);
}
