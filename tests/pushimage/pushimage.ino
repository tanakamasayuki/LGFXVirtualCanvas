// pushimage — Tier 1 T1-12 (experiment): does pushImage reassemble across
// tile boundaries with clip-only forwarding?
//
// An RGB565 image is pushed at several virtual positions that straddle tile
// boundaries / overhang the edges, rendered at split=1 (reference) vs
// split=2/3/5/7. If all match, pushImage is supportable as-is (SPEC §9.2);
// if not, it should become an unsupported method.
//
// Output: output/img_split_<N>.png

#include <LovyanGFX.hpp>
#include <LGFX_AUTODETECT.hpp>
#include <LGFXVirtualCanvas.h>
#include <stdio.h>
#include <sys/stat.h>

static LGFX lcd;

static constexpr int IMG_W = 40;
static constexpr int IMG_H = 40;
static uint16_t img[IMG_W * IMG_H];

static void buildImage()
{
    for (int y = 0; y < IMG_H; ++y)
        for (int x = 0; x < IMG_W; ++x)
        {
            const uint8_t r = (uint8_t)(x * 255 / (IMG_W - 1));
            const uint8_t g = (uint8_t)(y * 255 / (IMG_H - 1));
            const uint8_t b = (uint8_t)((x + y) * 255 / (IMG_W + IMG_H - 2));
            img[y * IMG_W + x] = (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
        }
}

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

static void sceneImg(LGFXVirtualCanvas &g)
{
    const int W = g.width(), H = g.height();
    g.fillScreen(TFT_BLACK);
    g.pushImage(10, 0, IMG_W, IMG_H, img);                                // top edge
    g.pushImage(10, H / 3, IMG_W, IMG_H, img);                            // somewhere mid
    g.pushImage(W / 2 - IMG_W / 2, H / 2 - IMG_H / 2, IMG_W, IMG_H, img); // center
    g.pushImage(W - IMG_W - 10, H - IMG_H - 1, IMG_W, IMG_H, img);        // bottom-right
    g.pushImage(W / 4, -IMG_H / 2, IMG_W, IMG_H, img);                    // overhang top
    g.pushImage(W - IMG_W / 2, H * 2 / 3, IMG_W, IMG_H, img);             // overhang right
    g.pushImage(-IMG_W / 2, H - IMG_H / 2, IMG_W, IMG_H, img);            // overhang bottom-left
}

void setup()
{
    Serial.begin(115200);
    Serial.println("TEST start pushimage");

    mkdir("output", 0755);
    lcd.init();
    buildImage();
    Serial.printf("PANEL %dx%d\n", (int)lcd.width(), (int)lcd.height());

    for (int s : {1, 2, 3, 5, 7})
    {
        LGFXVirtualScreen screen(lcd, s);
        const bool rendered = screen.render(sceneImg);
        char path[64];
        snprintf(path, sizeof(path), "output/img_split_%d.png", s);
        const bool saved = save_png(lcd, path);
        Serial.printf("SPLIT %d render=%d save=%d\n", s, rendered, saved);
    }

    Serial.println("TEST done");
}

void loop()
{
    delay(1000);
}
