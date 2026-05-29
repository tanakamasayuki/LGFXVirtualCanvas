// build_m5unified — Tier 2 cross-library check (M5Unified entry point).
//
// Verifies LGFXVirtualCanvas.h compiles after <M5Unified.h> and that the
// shared drawVcScene renders identically at split=1 (full) and split=3
// (tiled), driving M5.Display.

#include <M5Unified.h>
#include <LGFXVirtualCanvas.h>
#include <vc_scene.h>
#include <stdio.h>
#include <sys/stat.h>

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
    Serial.println("TEST start build_m5unified");

    mkdir("output", 0755);
    M5.begin();
    Serial.printf("PANEL %dx%d\n", (int)M5.Display.width(), (int)M5.Display.height());

    renderSplit(M5.Display, 1, "output/split_1.png");
    renderSplit(M5.Display, 3, "output/split_3.png");

    Serial.println("TEST done");
}

void loop()
{
    delay(1000);
}
