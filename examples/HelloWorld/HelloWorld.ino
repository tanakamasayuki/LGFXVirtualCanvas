// HelloWorld — the smallest LGFXVirtualCanvas program (M5Unified).
//
// Draws to a virtual full-screen canvas split into tiles. You never see the
// tiles, the offset, or the buffer: just draw in full-screen coordinates.
//
// Board: any M5 device (uses M5Unified / M5.Display). For plain LovyanGFX see
// the LovyanGFX_Basic example.

#include <M5Unified.h>
#include <LGFXVirtualCanvas.h>

LGFXVirtualScreen screen(M5.Display); // split count omitted = auto (3). Nothing allocated yet.

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
    screen.render(drawScene); // first render allocates the tile buffer (guardrail)
}

void loop()
{
    delay(1000);
}
