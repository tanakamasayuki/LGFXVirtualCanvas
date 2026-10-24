// en: Draw a transparent dialog and restore its background
// ja: 透過Dialogを描き、閉じるときに背景を戻す
#include <M5Unified.h>
#include <LGFXVirtualCanvas.h>

static constexpr int DLG_W = 200;
static constexpr int DLG_H = 110;

LGFXVirtualScreen background(M5.Display);
// Placed at construction time in setup() once the display size is known.
LGFXVirtualSprite dialog(M5.Display, DLG_W, DLG_H);

struct State
{
    int frame;
    bool dialogOpen;
};
State state{0, false};

// The screen underneath. Drawn once when it changes — not per frame.
void drawBackground(LGFXVirtualCanvas &g, State &s)
{
    const int W = g.width(), H = g.height();
    g.fillScreen(TFT_NAVY);
    for (int y = 0; y < H; y += 16)
        g.drawFastHLine(0, y, W, TFT_DARKGREEN);
    g.setTextColor(TFT_WHITE);
    g.drawString("background layer", 12, 12);
    g.setCursor(12, H - 24);
    g.printf("redraws: %d", s.frame);
    g.fillCircle(W - 40, H - 40, 24, TFT_ORANGE);
}

// The overlay. No fillScreen() — auto-clear already filled the tile with the
// transparent color, so anything not drawn here shows the background.
void drawDialog(LGFXVirtualCanvas &g)
{
    const int W = g.width(), H = g.height(); // the sprite's own size

    // A soft drop shadow: offset, and clipped to nothing outside the sprite.
    g.fillRoundRect(6, 6, W - 6, H - 6, 10, TFT_BLACK);
    g.fillRoundRect(0, 0, W - 6, H - 6, 10, TFT_DARKGREY);
    g.drawRoundRect(0, 0, W - 6, H - 6, 10, TFT_WHITE);

    g.setTextColor(TFT_WHITE);
    g.drawString("Delete everything?", 14, 18);
    g.drawString("B: cancel", 14, H - 34);
}

void setup()
{
    M5.begin();
    Serial.begin(115200);

    // Centre the dialog. The default transparent color (TFT_TRANSPARENT) is
    // fine unless the artwork above genuinely uses it; then set your own:
    //   dialog.setTransparentColor(M5.Display.color565(1, 2, 3));
    dialog.setPosition((M5.Display.width() - DLG_W) / 2,
                       (M5.Display.height() - DLG_H) / 2);

    // en: Initialize this component after its prerequisites; inspect the returned status where provided.
    // ja: 前提となる機器を準備してから初期化し、結果を返すAPIでは成否を確認します。
    if (!background.begin() || !dialog.begin())
    {
        Serial.println("alloc failed");
        return;
    }
    // en: Draw a stable scene snapshot; tiled rendering can invoke the callback repeatedly.
    // ja: 同じ状態のSceneを描きます。タイル描画ではコールバックが複数回呼ばれます。
    background.render(drawBackground, state);
}

void loop()
{
    M5.update();

    if (M5.BtnA.wasPressed() && !state.dialogOpen)
    {
        state.dialogOpen = true;
        // The background is left exactly as it is on the panel; only the pixels
        // the dialog actually draws are transferred.
        // en: Draw only non-key pixels and restore the background separately when closing the overlay.
        // ja: 抜け色以外を描き、Overlayを閉じる際の背景復元は別に行います。
        dialog.renderTransparent(drawDialog);
        Serial.printf("dialog: %u / %u px pushed (an upper bound: transparent "
                      "pixels inside those tiles are not sent)\n",
                      (unsigned)dialog.diffPushedPixels(),
                      (unsigned)dialog.diffTotalPixels());
    }

    if (M5.BtnB.wasPressed() && state.dialogOpen)
    {
        state.dialogOpen = false;
        // Closing is the one case that needs the layer below repainted.
        state.frame++;
        background.render(drawBackground, state);
        // If the background used diff transfer, the overlay's own hashes would
        // now describe an image the panel no longer shows:
        //   dialog.invalidate();
    }

    delay(16);
}
