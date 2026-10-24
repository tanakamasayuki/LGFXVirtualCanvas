// en: Draw a first tiled full-screen scene
// ja: 最初のタイル分割Screenを描く
#include <M5Unified.h>
#include <LGFXVirtualCanvas.h>

LGFXVirtualScreen screen(M5.Display); // split count omitted = auto (~19 KB/tile). Nothing allocated yet.

void drawScene(LGFXVirtualCanvas &g)
{
    g.fillScreen(TFT_NAVY);
    g.setTextColor(TFT_WHITE);
    g.drawCentreString("LGFXVirtualCanvas", g.width() / 2, g.height() / 2 - 16);
    g.drawCentreString("Hello, tiled world!", g.width() / 2, g.height() / 2 + 4);
}

void setup()
{
    M5.begin();               // panel size becomes known here
    // en: Draw a stable scene snapshot; tiled rendering can invoke the callback repeatedly.
    // ja: 同じ状態のSceneを描きます。タイル描画ではコールバックが複数回呼ばれます。
    screen.render(drawScene); // first render allocates the tile buffer (guardrail)
}

void loop()
{
    delay(1000);
}
