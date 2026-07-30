// DiffTransfer — skip transferring tiles that did not change (M5Unified).
//
// The scene is mostly static: a title, a frame, and a clock that only changes
// once a second. With LGFXVirtualDiffMode::Tile, each tile is hashed after your
// draw callback and its transfer is skipped when the hash matches the previous
// render, so most frames only push the tiles the clock sits in.
//
// It reduces TRANSFER only: the draw callback still runs for every tile and
// hashing is added on top. So it pays off on a large, slow panel (a Full-HD USB
// display, say) and not on a small fast one — which is why it is off by default
// and why this sketch prints the pushed/total ratio. On a Core2 (320x240 over
// fast SPI) expect the ratio to drop but the frame rate to barely move; the
// numbers are the point of the example.
//
// Board: any M5 device (M5Unified / M5.Display).

#include <M5Unified.h>
#include <LGFXVirtualCanvas.h>

LGFXVirtualScreen screen(M5.Display);

struct State
{
    int seconds;
    int spinner; // changes every frame, in the same band as the clock
};
State state{0, 0};

void drawScene(LGFXVirtualCanvas &g, State &s)
{
    const int W = g.width(), H = g.height();

    // Static parts: identical every frame, so their tiles are never re-sent.
    g.fillScreen(TFT_NAVY);
    g.drawRect(4, 4, W - 8, H - 8, TFT_DARKGREY);
    g.setTextColor(TFT_WHITE);
    g.drawString("Diff transfer demo", 16, 16);
    g.drawString("static area", 16, H - 30);

    // Changing parts, deliberately kept inside one horizontal band so only the
    // tiles covering that band need to be transferred.
    g.setCursor(16, H / 2);
    g.printf("uptime %3d s", s.seconds);
    g.fillCircle(W - 40, H / 2 + 6, 8 + (s.spinner & 3), TFT_CYAN);
}

void setup()
{
    M5.begin();
    Serial.begin(115200);

    // Off by default; opt in explicitly.
    screen.setDiffMode(LGFXVirtualDiffMode::Tile);

    if (!screen.begin()) // optional: allocate now (tile buffers + hash table)
    {
        Serial.println("alloc failed");
        return;
    }
    Serial.printf("%d tiles of %d rows, hash table %u bytes\n",
                  screen.tileCount(), screen.tileHeight(),
                  (unsigned)screen.diffMemoryUsage());
}

void loop()
{
    M5.update();

    static uint32_t lastSecond = 0;
    const uint32_t now = millis();
    if (now - lastSecond >= 1000)
    {
        lastSecond = now;
        state.seconds++;
    }
    state.spinner++;

    screen.render(drawScene, state);

    // How much the diff actually saved on this frame.
    static uint32_t lastReport = 0;
    if (now - lastReport >= 1000)
    {
        lastReport = now;
        Serial.printf("transferred %6u / %6u px\n",
                      (unsigned)screen.diffPushedPixels(),
                      (unsigned)screen.diffTotalPixels());
    }

    // Anything that draws on the panel behind our back invalidates the
    // assumption that untransferred areas still show the previous image.
    if (M5.BtnA.wasPressed())
    {
        M5.Display.fillCircle(30, 30, 12, TFT_RED); // foreign drawing
        // Without this, the red dot would survive: no tile content changed, so
        // nothing would be re-sent. Try commenting it out to see that.
        screen.invalidate();
    }

    delay(16);
}
