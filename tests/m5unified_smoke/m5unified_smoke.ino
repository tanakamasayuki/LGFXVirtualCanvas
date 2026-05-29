#include <M5Unified.h>
#include <stdio.h>
#include <sys/stat.h>

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
    Serial.println("TEST start m5unified_smoke");

    mkdir("output", 0755);
    M5.begin();

    const int W = M5.Display.width();
    const int H = M5.Display.height();
    M5.Display.fillScreen(TFT_DARKGREEN);
    M5.Display.drawRect(0, 0, W, H, TFT_WHITE);
    M5.Display.fillRect(W / 4, H / 4, W / 2, H / 2, TFT_RED);

    Serial.println(save_png(M5.Display, "output/main.png") ? "MAIN ok" : "MAIN fail");

    Serial.println("TEST done");
}

void loop()
{
    delay(1000);
}
