// MemoryBudget — cap the tile buffer and handle allocation failure (M5Unified).
//
// setMemoryLimit() bounds the tile sprite to a byte budget; the library picks
// the largest tile height that fits and splits accordingly. begin() returns
// false if the request cannot be satisfied — there is NO fallback, so you can
// detect breakage explicitly.
//
// Board: any M5 device (M5Unified / M5.Display).

#include <M5Unified.h>
#include <LGFXVirtualCanvas.h>

LGFXVirtualScreen screen(M5.Display);

void drawScene(LGFXVirtualCanvas &g)
{
    g.fillScreen(TFT_BLACK);
    g.fillCircle(g.width() / 2, g.height() / 2, 40, TFT_ORANGE);
    g.setTextColor(TFT_GREEN);
    g.setCursor(4, 4);
    g.printf("tiles=%d  tileH=%d", screen.tileCount(), screen.tileHeight());
}

void setup()
{
    M5.begin();
    Serial.begin(115200);

    screen.setMemoryLimit(8 * 1024); // cap the tile buffer at 8 KB

    if (!screen.begin())
    {
        // No fallback: the budget could not be satisfied. Handle it yourself.
        Serial.println("LGFXVirtualScreen: alloc failed");
        M5.Display.fillScreen(TFT_RED);
        M5.Display.drawString("alloc failed", 10, 10);
        return;
    }

    Serial.printf("allocated: tiles=%d tileH=%d\n", screen.tileCount(), screen.tileHeight());
    screen.render(drawScene);
}

void loop()
{
    delay(1000);
}
