// Dialog — draw a dialog over a screen without redrawing the screen (M5Unified).
//
// renderTransparent() fills the tile with the transparent color and pushes the
// tile with that color masked out, so every pixel you did not draw keeps showing
// whatever is already on the panel. That is what lets a *shaped* overlay (rounded
// corners, a shadow) sit on top of an existing image: the corners are simply not
// transferred.
//
// Note what this example does NOT do: it never re-renders the background while
// the dialog is up. Press A to open, B to close — closing is the only moment the
// background has to be drawn again.
//
// If your dialog were a plain rectangle you would not need transparency at all:
// an LGFXVirtualSprite placed on it already transfers only that rectangle, and it
// is faster. The best of both is what `dialog` below does — a sprite on the
// dialog's bounding box, pushed transparently, so only that box is even scanned.
//
// Board: any M5 device (M5Unified / M5.Display).

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

    if (!background.begin() || !dialog.begin())
    {
        Serial.println("alloc failed");
        return;
    }
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
