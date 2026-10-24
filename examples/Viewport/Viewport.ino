// en: Update a sprite region using local coordinates
// ja: ローカル座標でSprite領域だけを更新する
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
    // en: Initialize this component after its prerequisites; inspect the returned status where provided.
    // ja: 前提となる機器を準備してから初期化し、結果を返すAPIでは成否を確認します。
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

    // en: Draw a stable scene snapshot; tiled rendering can invoke the callback repeatedly.
    // ja: 同じ状態のSceneを描きます。タイル描画ではコールバックが複数回呼ばれます。
    view.render(drawView, ball); // only the 200x150 region is updated
    delay(16);
}
