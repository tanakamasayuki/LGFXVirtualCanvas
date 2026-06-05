// LovyanGFX_Basic — using LGFXVirtualCanvas with plain LovyanGFX.
//
// Same library, non-M5 path: include LovyanGFX + LGFX_AUTODETECT, construct
// your LGFX panel, and pass it to LGFXVirtualScreen. The draw function is
// identical to the M5 examples — it only ever sees LGFXVirtualCanvas.
//
// Board: any LovyanGFX-supported display (auto-detected by LGFX_AUTODETECT).

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
    screen.render(drawScene); // first render allocates the tile buffer
}

void loop()
{
    delay(1000);
}
