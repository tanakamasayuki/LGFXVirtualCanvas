// Viewport — partial update with LGFXVirtualSprite (M5Unified).
//
// A tiled sub-region (a "viewport") is re-rendered every frame while the rest
// of the screen (a title drawn once) is never touched. Inside the viewport you
// draw in LOCAL coordinates (0,0 = viewport top-left); internally it is split
// into tiles so it needs only a small buffer.
//
// Board: any M5 device (M5Unified / M5.Display).

#include <M5Unified.h>
#include <LGFXVirtualCanvas.h>

struct Ball
{
    int x, y, vx, vy;
};
Ball ball{40, 40, 3, 2};

// A 200x150 tiled viewport placed at (20, 60) on the panel.
LGFXVirtualSprite view(M5.Display, 200, 150, 20, 60);

// Drawn in the viewport's local coordinates.
void drawView(LGFXVirtualCanvas &g, Ball &b)
{
    g.fillScreen(TFT_BLACK);
    g.drawRect(0, 0, g.width(), g.height(), TFT_DARKGREY);
    g.fillCircle(b.x, b.y, 10, TFT_CYAN);
}

void setup()
{
    M5.begin();
    M5.Display.fillScreen(TFT_NAVY);
    M5.Display.setTextColor(TFT_WHITE);
    M5.Display.drawString("Static title (drawn once)", 20, 20); // outside the viewport

    view.setMemoryLimit(12 * 1024); // small tile buffer; the region is split internally
    view.begin();
}

void loop()
{
    // Bounce inside the viewport (local coordinates).
    ball.x += ball.vx;
    ball.y += ball.vy;
    if (ball.x < 10 || ball.x > view.width() - 10)
        ball.vx = -ball.vx;
    if (ball.y < 10 || ball.y > view.height() - 10)
        ball.vy = -ball.vy;

    view.render(drawView, ball); // only the 200x150 region is updated
    delay(16);
}
