// BouncingBall — animation with application state (M5Unified).
//
// Shows the recommended pattern: keep state in your own struct, update it,
// then render a pure View function that takes the state by reference. The
// draw function is split-agnostic — it just draws to the virtual screen.
//
// Board: any M5 device (M5Unified / M5.Display).

#include <M5Unified.h>
#include <LGFXVirtualCanvas.h>

struct Ball
{
    int x, y, vx, vy, r;
};
Ball ball{40, 40, 3, 2, 16};

LGFXVirtualScreen screen(M5.Display);

// View: draws the current state to the virtual full-screen canvas.
void drawScene(LGFXVirtualCanvas &g, Ball &b)
{
    g.fillScreen(TFT_BLACK);
    g.drawRect(0, 0, g.width(), g.height(), TFT_DARKGREY);
    g.fillCircle(b.x, b.y, b.r, TFT_CYAN);
    g.setTextColor(TFT_WHITE);
    g.setCursor(4, 4);
    g.printf("x=%d y=%d", b.x, b.y);
}

// Model update: no drawing here.
void update(Ball &b, int w, int h)
{
    b.x += b.vx;
    b.y += b.vy;
    if (b.x < b.r || b.x > w - b.r)
        b.vx = -b.vx;
    if (b.y < b.r || b.y > h - b.r)
        b.vy = -b.vy;
}

void setup()
{
    M5.begin();
    screen.setMemoryLimit(16 * 1024); // optional: cap the tile buffer at 16 KB
    screen.begin();                   // optional: allocate now instead of on first render
}

void loop()
{
    update(ball, M5.Display.width(), M5.Display.height());
    screen.render(drawScene, ball);
    delay(16);
}
