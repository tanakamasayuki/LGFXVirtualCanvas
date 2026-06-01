// bench — LGFXVirtualCanvas performance benchmark (real hardware, M5Unified).
//
// Measures frame time / FPS for several drawing strategies and library usage
// patterns, and prints the results as Serial tables for documentation.
//
// PHASE 1 (this file): no library change required.
//   method A  direct panel        — draw straight to M5.Display, no buffer.
//   method B  full sprite         — one full-screen sprite (internal & PSRAM).
//   method C  LGFXVirtualScreen    — the library, internal RAM, split sweep,
//                                     autoClear on/off, plus a Sprite variant.
// PHASE 2 (added later): method D — two-buffer ping-pong (DMA overlap).
//
// The same scene is drawn through every strategy via a TEMPLATED scene function
// so the work is byte-for-byte identical across A / B / C — LGFXVirtualCanvas
// mirrors the LGFX API, so one template instantiates for LovyanGFX&, M5Canvas&,
// and LGFXVirtualCanvas& alike. Only the transfer path differs.
//
// Output: Serial @115200, one fixed-width table per (scene, buffer) block.
// Board:  M5Stack Core2 (ESP32) or CoreS3 (ESP32-S3); M5Unified auto-detects.

#include <M5Unified.h>
#include <LGFXVirtualCanvas.h>
#include "esp_heap_caps.h"

// ---- benchmark configuration -------------------------------------------------
static const int WARMUP = 5;    // frames discarded before timing
static const int FRAMES = 60;   // timed frames per run (averaged)
static const int SPLITS[] = {1, 2, 3, 4, 6, 8};
static const int N_SPLITS = sizeof(SPLITS) / sizeof(SPLITS[0]);

static int PANEL_W = 0, PANEL_H = 0;

// ---- a small image used by the "image" scene (32x32 RGB565, built once) ------
static const int IMG = 32;
static uint16_t imgbuf[IMG * IMG];
static void buildImage()
{
    for (int y = 0; y < IMG; ++y)
        for (int x = 0; x < IMG; ++x)
            imgbuf[y * IMG + x] = M5.Display.color565(x * 8, y * 8, (x ^ y) * 8);
}

// ---- scenes (templated: work identically for any LGFX-like canvas) -----------
// `f` is a frame counter so positions animate — keeps the work realistic and
// prevents the toolchain from hoisting constant draws. All strategies are fed
// the SAME f sequence, so they perform identical work.

template <class GFX>
void sceneLight(GFX &g, int f)
{
    const int W = g.width(), H = g.height();
    g.fillScreen(TFT_NAVY);
    const int o = f % 40;
    g.fillRect(10 + o, 10, 60, 40, TFT_RED);
    g.fillCircle(W / 2, H / 2, 50, TFT_YELLOW);
    g.drawLine(0, 0, W - 1, H - 1, TFT_CYAN);
    g.drawRect(5, 5, W - 10, H - 10, TFT_WHITE);
}

template <class GFX>
void sceneHeavy(GFX &g, int f)
{
    const int W = g.width(), H = g.height();
    g.fillScreen(TFT_BLACK);
    for (int i = 0; i < 200; ++i)
    {
        const int x = (i * 37 + f * 3) % W;
        const int y = (i * 53 + f * 2) % H;
        const int r = 4 + (i % 12);
        const uint16_t c = (uint16_t)((i * 2654435761u) >> 16);
        if (i & 1)
            g.fillCircle(x, y, r, c);
        else
            g.drawLine(x, y, (x + 40) % W, (y + 30) % H, c);
    }
    g.setTextColor(TFT_WHITE);
    for (int t = 0; t < 8; ++t)
    {
        g.setCursor(4, 4 + t * 14);
        g.printf("line %d frame %d", t, f);
    }
}

template <class GFX>
void sceneImage(GFX &g, int f)
{
    const int W = g.width(), H = g.height();
    g.fillScreen(TFT_DARKGREY);
    const int o = f % IMG;
    for (int y = -o; y < H; y += IMG)
        for (int x = -o; x < W; x += IMG)
            g.pushImage(x, y, IMG, IMG, imgbuf);
}

// Scene dispatch by id (0=light,1=heavy,2=image) for the templated path.
template <class GFX>
void runScene(GFX &g, int scene, int f)
{
    if (scene == 0) sceneLight(g, f);
    else if (scene == 1) sceneHeavy(g, f);
    else sceneImage(g, f);
}

// ---- concrete callbacks for the library (LGFXVirtualScreen / Sprite) ---------
// The library callback is a plain function pointer, so we need one per scene.
static int g_scene = 0, g_frame = 0;
static void sceneVC(LGFXVirtualCanvas &g) { runScene(g, g_scene, g_frame); }

// ---- result row + table printing --------------------------------------------
static void header(const char *scene, const char *buffer)
{
    Serial.println();
    Serial.printf("=== scene=%s  buffer=%s ===\n", scene, buffer);
    Serial.println("method                  split tileH  draw_us  xfer_us frame_us    fps  heapInt heapPSRAM");
}

static void row(const char *method, int split, int tileH,
                long draw_us, long xfer_us, long frame_us)
{
    const float fps = frame_us > 0 ? 1000000.0f / (float)frame_us : 0.0f;
    char ds[12], xs[12], sp[8], th[8];
    if (draw_us < 0) snprintf(ds, sizeof(ds), "%8s", "-"); else snprintf(ds, sizeof(ds), "%8ld", draw_us);
    if (xfer_us < 0) snprintf(xs, sizeof(xs), "%8s", "-"); else snprintf(xs, sizeof(xs), "%8ld", xfer_us);
    if (split < 0) snprintf(sp, sizeof(sp), "%5s", "-"); else snprintf(sp, sizeof(sp), "%5d", split);
    if (tileH < 0) snprintf(th, sizeof(th), "%5s", "-"); else snprintf(th, sizeof(th), "%5d", tileH);
    const size_t hi = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    const size_t hp = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    Serial.printf("%-22s %s %s %s %s %8ld %6.1f %8u %9u\n",
                  method, sp, th, ds, xs, frame_us, fps, (unsigned)hi, (unsigned)hp);
}

// ---- method A: direct panel --------------------------------------------------
static void runDirect(int scene)
{
    for (int i = 0; i < WARMUP; ++i)
    {
        M5.Display.startWrite();
        runScene(M5.Display, scene, i);
        M5.Display.endWrite();
    }
    const uint32_t t0 = micros();
    for (int f = 0; f < FRAMES; ++f)
    {
        M5.Display.startWrite();
        runScene(M5.Display, scene, f);
        M5.Display.endWrite();
    }
    const long frame = (long)((micros() - t0) / FRAMES);
    row("A direct-panel", -1, -1, -1, -1, frame);
}

// ---- method B: one full-screen sprite (draw/xfer split measured) -------------
static void runFullSprite(int scene, bool psram)
{
    M5Canvas spr(&M5.Display);
    spr.setColorDepth(M5.Display.getColorDepth());
    spr.setPsram(psram);
    if (!spr.createSprite(PANEL_W, PANEL_H))
    {
        row(psram ? "B full-sprite(PSRAM!)" : "B full-sprite(int!)", 1, PANEL_H, -1, -1, 0);
        Serial.println("  -- full sprite alloc failed (skipped)");
        return;
    }
    for (int i = 0; i < WARMUP; ++i)
    {
        runScene(spr, scene, i);
        M5.Display.startWrite();
        spr.pushSprite(&M5.Display, 0, 0);
        M5.Display.endWrite();
    }
    long draw_sum = 0, xfer_sum = 0;
    for (int f = 0; f < FRAMES; ++f)
    {
        const uint32_t a = micros();
        runScene(spr, scene, f);
        const uint32_t b = micros();
        M5.Display.startWrite();
        spr.pushSprite(&M5.Display, 0, 0);
        M5.Display.endWrite();
        const uint32_t c = micros();
        draw_sum += (long)(b - a);
        xfer_sum += (long)(c - b);
    }
    const long draw = draw_sum / FRAMES, xfer = xfer_sum / FRAMES;
    row(psram ? "B full-sprite(PSRAM)" : "B full-sprite(int)", 1, PANEL_H,
        draw, xfer, draw + xfer);
    spr.deleteSprite();
}

// ---- method C/D: the library, LGFXVirtualScreen (internal RAM) ---------------
// doubleBuf=false → method C (single buffer, serialized);
// doubleBuf=true  → method D (two-buffer ping-pong, draw/transfer overlap).
static void runLibraryScreen(int scene, int split, bool autoClear, bool doubleBuf)
{
    LGFXVirtualScreen screen(M5.Display, split);
    screen.setAutoClear(autoClear);
    screen.setDoubleBuffer(doubleBuf);
    g_scene = scene;
    if (!screen.begin())
    {
        Serial.printf("  -- library begin() failed (split=%d db=%d, skipped)\n", split, doubleBuf);
        return;
    }
    for (int i = 0; i < WARMUP; ++i) { g_frame = i; screen.render(sceneVC); }
    const uint32_t t0 = micros();
    for (int f = 0; f < FRAMES; ++f) { g_frame = f; screen.render(sceneVC); }
    const long frame = (long)((micros() - t0) / FRAMES);
    char name[28];
    snprintf(name, sizeof(name), "%s screen ac=%s",
             doubleBuf ? "D" : "C", autoClear ? "on" : "off");
    row(name, split, screen.tileHeight(), -1, -1, frame);
}

// ---- method C: the library, LGFXVirtualSprite (placed sub-region) ------------
static void runLibrarySprite(int scene, int split)
{
    const int SW = 160, SH = 100;
    LGFXVirtualSprite spr(M5.Display, SW, SH, (PANEL_W - SW) / 2, (PANEL_H - SH) / 2);
    spr.setSplitCount(split);
    g_scene = scene;
    if (!spr.begin())
    {
        Serial.printf("  -- library sprite begin() failed (split=%d, skipped)\n", split);
        return;
    }
    for (int i = 0; i < WARMUP; ++i) { g_frame = i; spr.render(sceneVC); }
    const uint32_t t0 = micros();
    for (int f = 0; f < FRAMES; ++f) { g_frame = f; spr.render(sceneVC); }
    const long frame = (long)((micros() - t0) / FRAMES);
    row("C sprite 160x100", split, spr.tileHeight(), -1, -1, frame);
}

// ---- one full block for a given scene ----------------------------------------
static void runBlock(int scene, const char *name)
{
    // --- internal-RAM block: A, B(int), full C sweep ---
    header(name, "internal");
    runDirect(scene);
    runFullSprite(scene, false);
    // C: single buffer (correct, serialized) — autoClear on, then off
    for (int s = 0; s < N_SPLITS; ++s) runLibraryScreen(scene, SPLITS[s], true, false);
    for (int s = 0; s < N_SPLITS; ++s) runLibraryScreen(scene, SPLITS[s], false, false);
    // D: double buffer (draw/transfer overlap) — same split sweep, autoClear on
    for (int s = 0; s < N_SPLITS; ++s) runLibraryScreen(scene, SPLITS[s], true, true);
    runLibrarySprite(scene, 3);

    // --- PSRAM block: baseline only (library has no PSRAM API in Phase 1) ---
    header(name, "PSRAM");
    runFullSprite(scene, true);
    Serial.println("  (library C measured on internal RAM only — see README)");
}

void setup()
{
    auto cfg = M5.config();
    M5.begin(cfg);
    Serial.begin(115200);
    delay(300);
    PANEL_W = M5.Display.width();
    PANEL_H = M5.Display.height();
    buildImage();

    Serial.println();
    Serial.println("########## LGFXVirtualCanvas benchmark (Phase 1) ##########");
    Serial.printf("panel %dx%d  depth=%d-bit  frames=%d warmup=%d\n",
                  PANEL_W, PANEL_H, ((int)M5.Display.getColorDepth()) & 0xFF, FRAMES, WARMUP);
    Serial.printf("heap: internal=%u  PSRAM=%u\n",
                  (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                  (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));

    runBlock(0, "light");
    runBlock(1, "heavy");
    runBlock(2, "image");

    Serial.println();
    Serial.println("########## benchmark done ##########");
}

void loop() { delay(1000); }
