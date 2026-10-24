// en: Compare horizontal splitting, buffering and PSRAM choices
// ja: 横方向の分割・バッファ・PSRAMの違いを調べる
#include <M5Unified.h>
#include <LGFXVirtualCanvas.h>

// The same scene for every configuration below: a "strip" of numbered bands so
// the transfer order is visible, plus text (which is where column splitting has
// a documented limitation — see the wrap note at the bottom).
void drawStrip(LGFXVirtualCanvas &g)
{
    const int W = g.width(), H = g.height();
    g.fillScreen(TFT_BLACK);
    for (int i = 0; i < 10; ++i)
    {
        const int x = W * i / 10;
        g.fillRect(x, H / 3, W / 10 - 2, H / 3, (uint16_t)(0x001F + i * 0x0841));
        g.setTextColor(TFT_WHITE);
        g.drawNumber(i, x + 4, H / 3 + 4);
    }
    g.drawRect(0, 0, W, H, TFT_DARKGREY);
    g.setCursor(8, 8);
    g.setTextColor(TFT_WHITE);
    g.print("column split");
    g.println(" demo");   // newline returns to the surface's left edge, not the tile's
    g.printf("%dx%d", W, H);
}

static void report(const char *label, LGFXVirtualScreen &screen)
{
    Serial.printf("%-12s tiles=%d  tile=%dx%d  span=%d  doubleBuffer=%d  psram=%d\n",
                  label, screen.tileCount(), screen.tileWidth(), screen.tileHeight(),
                  screen.tileSpan(), screen.doubleBuffer(), screen.tileIsPsram());
}

void setup()
{
    auto cfg = M5.config();
    M5.begin(cfg);
    Serial.begin(115200);

    // 1. Rows (the default): horizontal bands, top to bottom.
    {
        LGFXVirtualScreen screen(M5.Display);
        // en: Initialize this component after its prerequisites; inspect the returned status where provided.
        // ja: 前提となる機器を準備してから初期化し、結果を返すAPIでは成否を確認します。
        screen.begin();
        // en: Draw a stable scene snapshot; tiled rendering can invoke the callback repeatedly.
        // ja: 同じ状態のSceneを描きます。タイル描画ではコールバックが複数回呼ばれます。
        screen.render(drawStrip);
        report("rows", screen);
    }
    delay(1500);

    // 2. Columns: full-height vertical bands, transferred left to right.
    //    Same callback, same output — only the tile shape and order differ.
    {
        LGFXVirtualScreen screen(M5.Display);
        screen.setSplitAxis(LGFXVirtualSplitAxis::Columns);
        screen.begin();
        screen.render(drawStrip);
        report("columns", screen);
    }
    delay(1500);

    // 3. Columns with a fixed width. setTileWidth is the column-mode counterpart
    //    of setTileHeight (same underlying setting: the span along the split
    //    axis), and setMemoryLimit / setSplitCount keep working on either axis.
    {
        LGFXVirtualScreen screen(M5.Display);
        screen.setSplitAxis(LGFXVirtualSplitAxis::Columns);
        screen.setTileWidth(32);
        screen.begin();
        screen.render(drawStrip);
        report("columns/32", screen);
    }
    delay(1500);

    // 4. PSRAM-backed tiles. The request never fails the allocation: without
    //    PSRAM the buffer comes from internal RAM and tileIsPsram() says so.
    //    Note doubleBuffer=0 in the report when the buffer really is in PSRAM —
    //    a PSRAM sprite is pushed without DMA, so there is nothing to overlap.
    {
        LGFXVirtualScreen screen(M5.Display);
        screen.setSplitAxis(LGFXVirtualSplitAxis::Columns);
        screen.setUsePsram(true);
        screen.setMemoryLimit(256 * 1024); // far more than internal RAM would give
        if (!screen.begin())
        {
            Serial.println("psram      : allocation failed");
        }
        else
        {
            screen.render(drawStrip);
            report("psram", screen);
            if (screen.usePsram() && !screen.tileIsPsram())
                Serial.println("psram      : requested but served from internal RAM");
        }
    }

    // Limitation worth knowing: LovyanGFX wraps text at the tile sprite's right
    // edge, which under column splitting is a tile boundary — that would make
    // the output depend on the split count, so X wrapping is forced off in
    // column mode and long lines are clipped instead. Newlines are corrected, so
    // println/printf behave as expected. See SPEC §10.8.
    Serial.println("done");
}

void loop()
{
    delay(1000);
}
