// en: Animate state outside the tiled render callback
// ja: 状態を描画の外で更新してBallを動かす
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
    // en: Initialize this component after its prerequisites; inspect the returned status where provided.
    // ja: 前提となる機器を準備してから初期化し、結果を返すAPIでは成否を確認します。
    screen.begin();                   // optional: allocate now instead of on first render
}

void loop()
{
    update(ball, M5.Display.width(), M5.Display.height());
    // en: Draw a stable scene snapshot; tiled rendering can invoke the callback repeatedly.
    // ja: 同じ状態のSceneを描きます。タイル描画ではコールバックが複数回呼ばれます。
    screen.render(drawScene, ball);
    delay(16);
}
