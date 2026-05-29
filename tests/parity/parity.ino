// parity — multi-scene pixel-exact parity (Tier 1).
//
// Each scene is rendered through LGFXVirtualScreen at several split counts.
// split=1 (one full-height tile, offsetY=0) is the full-render reference;
// the Python side asserts every other split is pixel-identical to it.
//
// Scenes map to SPEC §13.4:
//   overall (T1-1), shapes (T1-2), circles (T1-3), text (T1-4),
//   boundary (T1-5), clipping (T1-7), fuzz (T1-10), anim0..2 (T1-11).
//   Partial last tile (T1-6) is exercised by split=7 (35px tiles, last 30).
//
// Output: output/<scene>_split_<N>.png

#include <LovyanGFX.hpp>
#include <LGFX_AUTODETECT.hpp>
#include <LGFXVirtualCanvas.h>
#include <stdio.h>
#include <sys/stat.h>

static LGFX lcd;

static bool save_png(LovyanGFX &src, const char *path)
{
    size_t len = 0;
    void *png = src.createPng(&len, 0, 0, src.width(), src.height());
    if (!png || len == 0)
        return false;
    FILE *fp = fopen(path, "wb");
    bool ok = false;
    if (fp)
    {
        ok = (fwrite(png, 1, len, fp) == len);
        fclose(fp);
    }
    free(png);
    return ok;
}

// --- scenes (all pure virtual coordinates) ---

static void sceneOverall(LGFXVirtualCanvas &g) // T1-1
{
    const int W = g.width(), H = g.height(), s = (W < H ? W : H);
    g.fillScreen(TFT_BLACK);
    g.drawRect(0, 0, W, H, TFT_WHITE);
    g.fillRect(W / 4, H / 4, W / 2, H / 2, TFT_RED);
    g.fillCircle(W / 2, H / 2, s / 6, TFT_YELLOW);
    g.drawCircle(W / 2, H / 2, s / 5, TFT_GREEN);
    g.drawLine(0, 0, W, H, TFT_CYAN);
    g.drawFastHLine(0, H / 2, W, TFT_BLUE);
    g.setTextColor(TFT_WHITE);
    g.setCursor(6, 6);
    g.printf("%dx%d", W, H);
    g.drawString("VirtualCanvas", 6, 22);
}

static void sceneShapes(LGFXVirtualCanvas &g) // T1-2
{
    const int W = g.width(), H = g.height();
    g.fillScreen(TFT_BLACK);
    for (int i = 0; i < 8; ++i)
        g.fillRect(i * 10, i * 12, 30, 18, (uint16_t)(0x07E0 + i * 0x0410));
    for (int i = 0; i < 8; ++i)
        g.drawRect(W - 1 - i * 12, i * 14, 40, 22, TFT_WHITE);
    g.drawLine(0, H - 1, W - 1, 0, TFT_RED);
    g.drawLine(0, 0, W - 1, H - 1, TFT_GREEN);
    g.drawFastHLine(0, H / 3, W, TFT_CYAN);
    g.drawFastVLine(W / 3, 0, H, TFT_MAGENTA);
    for (int x = 0; x < W; x += 5)
        g.drawPixel(x, (x * 7) % H, TFT_YELLOW);
}

static void sceneCircles(LGFXVirtualCanvas &g) // T1-3
{
    const int W = g.width(), H = g.height();
    g.fillScreen(TFT_BLACK);
    for (int r = 8; r < H; r += 16)
        g.drawCircle(W / 2, H / 2, r, (uint16_t)(0x001F + r * 0x0801));
    g.fillCircle(W / 4, H / 4, 24, TFT_RED);
    g.fillCircle(3 * W / 4, 3 * H / 4, 30, TFT_GREEN);
    g.fillCircle(W / 2, H / 2, 12, TFT_WHITE);
}

static void sceneText(LGFXVirtualCanvas &g) // T1-4
{
    const int H = g.height();
    g.fillScreen(TFT_NAVY);
    g.setTextColor(TFT_WHITE);
    g.drawString("drawString top", 4, 2);
    g.drawString("drawString mid", 4, H / 2);
    g.drawString("drawString low", 4, H - 16);
    g.setCursor(4, 30);
    g.print("print ");
    g.println("println");
    g.printf("printf %d %s", 42, "x");
}

static void sceneText2(LGFXVirtualCanvas &g) // T1-4: centre/right + numeric print
{
    const int W = g.width(), H = g.height();
    g.fillScreen(TFT_BLACK);
    g.setTextColor(TFT_WHITE);
    g.drawString("left", 0, 4);
    g.drawCentreString("centre", W / 2, H / 2 - 8);
    g.drawRightString("right", W, H - 16);
    g.setCursor(4, 30);
    g.print(42);
    g.print(' ');
    g.print(255, 16); // hex
    g.println();
    g.println(3.14);
}

static void sceneBoundary(LGFXVirtualCanvas &g) // T1-5
{
    const int W = g.width(), H = g.height();
    g.fillScreen(TFT_BLACK);
    // Dense horizontal features at every few rows so that, for whatever
    // tile height each split produces, many land on / straddle a boundary.
    for (int y = 0; y < H; y += 3)
        g.drawFastHLine(0, y, W, ((y / 3) & 1) ? TFT_WHITE : TFT_RED);
    for (int y = 0; y < H; y += 11)
        g.fillRect(0, y, W, 2, TFT_GREEN);
}

static void sceneClipping(LGFXVirtualCanvas &g) // T1-7
{
    const int W = g.width(), H = g.height();
    g.fillScreen(TFT_BLACK);
    g.fillRect(-20, -20, 60, 60, TFT_RED);           // top-left overhang
    g.fillCircle(W + 10, H + 10, 40, TFT_YELLOW);    // bottom-right off-screen
    g.fillCircle(-10, H / 2, 30, TFT_CYAN);          // left off-screen
    g.drawRect(-5, -5, W + 10, H + 10, TFT_WHITE);   // frame larger than screen
    g.drawLine(-50, -50, W + 50, H + 50, TFT_GREEN); // diagonal beyond corners
    g.fillRect(W - 10, -30, 40, H + 60, TFT_BLUE);   // right edge, past top & bottom
}

static void sceneFuzz(LGFXVirtualCanvas &g) // T1-10
{
    const int W = g.width(), H = g.height();
    g.fillScreen(TFT_BLACK);
    uint32_t s = 0x1234567u; // fixed seed (logged below)
    auto rnd = [&]()
    { s = s * 1664525u + 1013904223u; return s; };
    for (int i = 0; i < 200; ++i)
    {
        const int x = rnd() % W, y = rnd() % H;
        const int w = 1 + rnd() % 40, h = 1 + rnd() % 40;
        const uint16_t c = (uint16_t)rnd();
        switch (rnd() % 4)
        {
        case 0:
            g.fillRect(x, y, w, h, c);
            break;
        case 1:
            g.drawRect(x, y, w, h, c);
            break;
        case 2:
            g.fillCircle(x, y, 1 + rnd() % 20, c);
            break;
        default:
            g.drawLine(x, y, x + w, y + h, c);
            break;
        }
    }
}

static void drawAnim(LGFXVirtualCanvas &g, int frame) // T1-11
{
    const int W = g.width(), H = g.height();
    g.fillScreen(TFT_NAVY);
    const int x = (W - 24) * frame / 4;
    const int y = (H - 24) * frame / 4;
    g.fillRect(x, y, 24, 24, TFT_GREEN);
    g.drawCircle(x + 12, y + 12, 16, TFT_YELLOW);
    g.setTextColor(TFT_WHITE);
    g.setCursor(4, 4);
    g.printf("frame %d", frame);
}
static void sceneAnim0(LGFXVirtualCanvas &g) { drawAnim(g, 0); }
static void sceneAnim1(LGFXVirtualCanvas &g) { drawAnim(g, 2); }
static void sceneAnim2(LGFXVirtualCanvas &g) { drawAnim(g, 4); }

struct Scene
{
    const char *name;
    void (*fn)(LGFXVirtualCanvas &);
};

static const Scene scenes[] = {
    {"overall", sceneOverall},
    {"shapes", sceneShapes},
    {"circles", sceneCircles},
    {"text", sceneText},
    {"text2", sceneText2},
    {"boundary", sceneBoundary},
    {"clipping", sceneClipping},
    {"fuzz", sceneFuzz},
    {"anim0", sceneAnim0},
    {"anim1", sceneAnim1},
    {"anim2", sceneAnim2},
};

static const int splits[] = {1, 2, 3, 5, 7};

void setup()
{
    Serial.begin(115200);
    Serial.println("TEST start parity");

    mkdir("output", 0755);
    lcd.init();
    Serial.printf("PANEL %dx%d\n", (int)lcd.width(), (int)lcd.height());
    Serial.println("FUZZ seed 0x1234567");

    for (const auto &sc : scenes)
    {
        for (int s : splits)
        {
            LGFXVirtualScreen screen(lcd, s);
            screen.render(sc.fn);
            char path[80];
            snprintf(path, sizeof(path), "output/%s_split_%d.png", sc.name, s);
            save_png(lcd, path);
        }
        Serial.printf("SCENE %s done\n", sc.name);
    }

    Serial.println("TEST done");
}

void loop()
{
    delay(1000);
}
