#include <M5GFX.h>
#include <stdio.h>
#include <sys/stat.h>

static M5GFX gfx;

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

void setup()
{
    Serial.begin(115200);
    Serial.println("TEST start m5gfx_smoke");

    mkdir("output", 0755);
    gfx.init();

    const int W = gfx.width();
    const int H = gfx.height();
    gfx.fillScreen(TFT_DARKGREEN);
    gfx.drawRect(0, 0, W, H, TFT_WHITE);
    gfx.fillRect(W / 4, H / 4, W / 2, H / 2, TFT_RED);

    Serial.println(save_png(gfx, "output/main.png") ? "MAIN ok" : "MAIN fail");

    Serial.println("TEST done");
}

void loop()
{
    delay(1000);
}
