// en: Set a tile memory budget and handle allocation failure
// ja: タイルのメモリ予算と確保失敗を扱う
#include <M5Unified.h>
#include <LGFXVirtualCanvas.h>

LGFXVirtualScreen screen(M5.Display);

void drawScene(LGFXVirtualCanvas &g)
{
    g.fillScreen(TFT_BLACK);
    g.fillCircle(g.width() / 2, g.height() / 2, 40, TFT_ORANGE);
    g.setTextColor(TFT_GREEN);
    g.setCursor(4, 4);
    g.printf("tiles=%d  tileH=%d", screen.tileCount(), screen.tileHeight());
}

void setup()
{
    M5.begin();
    Serial.begin(115200);

    screen.setMemoryLimit(8 * 1024); // cap the tile buffer at 8 KB

    // en: Initialize this component after its prerequisites; inspect the returned status where provided.
    // ja: 前提となる機器を準備してから初期化し、結果を返すAPIでは成否を確認します。
    if (!screen.begin())
    {
        // No fallback: the budget could not be satisfied. Handle it yourself.
        Serial.println("LGFXVirtualScreen: alloc failed");
        M5.Display.fillScreen(TFT_RED);
        M5.Display.drawString("alloc failed", 10, 10);
        return;
    }

    Serial.printf("allocated: tiles=%d tileH=%d\n", screen.tileCount(), screen.tileHeight());
    // en: Draw a stable scene snapshot; tiled rendering can invoke the callback repeatedly.
    // ja: 同じ状態のSceneを描きます。タイル描画ではコールバックが複数回呼ばれます。
    screen.render(drawScene);
}

void loop()
{
    delay(1000);
}
