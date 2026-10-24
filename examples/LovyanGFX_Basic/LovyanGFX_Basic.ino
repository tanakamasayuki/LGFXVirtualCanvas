// en: Use a tiled screen with a plain LovyanGFX panel
// ja: 通常のLovyanGFXパネルへタイル描画する
#include <LovyanGFX.hpp>
#include <LGFX_AUTODETECT.hpp>
#include <LGFXVirtualCanvas.h>

static LGFX lcd;
LGFXVirtualScreen screen(lcd); // default: auto (~19 KB/tile, auto double-buffer)

void drawScene(LGFXVirtualCanvas &g)
{
    const int W = g.width(), H = g.height();
    g.fillScreen(TFT_BLACK);
    g.fillRect(W / 4, H / 4, W / 2, H / 2, TFT_RED);
    g.fillCircle(W / 2, H / 2, 30, TFT_YELLOW);
    g.setTextColor(TFT_WHITE);
    g.drawCentreString("LovyanGFX + VirtualCanvas", W / 2, 8);
}

void setup()
{
    lcd.init();               // panel size becomes known here
    // en: Draw a stable scene snapshot; tiled rendering can invoke the callback repeatedly.
    // ja: 同じ状態のSceneを描きます。タイル描画ではコールバックが複数回呼ばれます。
    screen.render(drawScene); // first render allocates the tile buffer
}

void loop()
{
    delay(1000);
}
