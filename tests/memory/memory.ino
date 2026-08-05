// memory — Tier 1 T1-9: memory budget math + allocation-failure semantics.
//
// Verifies SPEC §10: setMemoryLimit derives tileH; a too-small budget makes
// begin()/render() fail WITHOUT falling back; and the guardrail auto-allocates
// when render() is called before begin(). All reported over Serial; the Python
// side parses the lines (no PNG).

#include <LovyanGFX.hpp>
#include <LGFX_AUTODETECT.hpp>
#include <LGFXVirtualCanvas.h>
#include <stdio.h>

static LGFX lcd;

static void sceneNoop(LGFXVirtualCanvas &g) { g.fillScreen(TFT_BLACK); }

void setup()
{
    Serial.begin(115200);
    Serial.println("TEST start memory");

    lcd.init();
    const int W = lcd.width();
    const int H = lcd.height();
    const int bits = ((int)lcd.getColorDepth()) & 0x00FF;
    Serial.printf("PANEL W=%d H=%d bits=%d\n", W, H, bits);

    const size_t bytesPerRow = ((size_t)W * (size_t)bits + 7) / 8;

    // generous: a budget for ~30 rows comfortably allocates.
    {
        const size_t lim = bytesPerRow * 30;
        LGFXVirtualScreen screen(lcd);
        screen.setMemoryLimit(lim);
        const bool began = screen.begin();
        const bool ready = screen.isReady();
        const bool rendered = screen.render(sceneNoop);
        Serial.printf("GENEROUS limit=%u begin=%d ready=%d render=%d th=%d N=%d\n",
                      (unsigned)lim, began, ready, rendered,
                      screen.tileHeight(), screen.tileCount());
    }

    // tiny: below one row -> cannot satisfy -> fail, no fallback.
    {
        LGFXVirtualScreen screen(lcd);
        screen.setMemoryLimit(4);
        const bool began = screen.begin();
        const bool ready = screen.isReady();
        const bool rendered = screen.render(sceneNoop);
        Serial.printf("TINY limit=4 begin=%d ready=%d render=%d\n",
                      began, ready, rendered);
    }

    // guardrail: render() before begin() auto-allocates (default config).
    {
        LGFXVirtualScreen screen(lcd);
        const bool rendered = screen.render(sceneNoop);
        Serial.printf("GUARDRAIL render=%d ready=%d N=%d\n",
                      rendered, screen.isReady(), screen.tileCount());
    }

    // column splitting: the same budget now bounds a full-height column, so the
    // span it buys is a tile *width* (SPEC §10.8).
    {
        const size_t lim = ((size_t)H * (size_t)bits + 7) / 8 * 20; // ~20 columns
        LGFXVirtualScreen screen(lcd);
        screen.setSplitAxis(LGFXVirtualSplitAxis::Columns);
        screen.setMemoryLimit(lim);
        const bool began = screen.begin();
        const bool rendered = screen.render(sceneNoop);
        Serial.printf("COLUMNS limit=%u begin=%d render=%d tw=%d th=%d span=%d N=%d\n",
                      (unsigned)lim, began, rendered, screen.tileWidth(),
                      screen.tileHeight(), screen.tileSpan(), screen.tileCount());
    }

    // column splitting by split count: N full-height columns, no leftover config.
    {
        LGFXVirtualScreen screen(lcd, 4);
        screen.setSplitAxis(LGFXVirtualSplitAxis::Columns);
        const bool began = screen.begin();
        Serial.printf("COLSPLIT begin=%d tw=%d th=%d N=%d\n",
                      began, screen.tileWidth(), screen.tileHeight(), screen.tileCount());
    }

    // PSRAM request: honoured where PSRAM exists, silently served from internal
    // RAM otherwise (SPEC §10.9) — either way the render must succeed, and
    // tileIsPsram() reports what actually happened.
    {
        LGFXVirtualScreen screen(lcd, 3);
        screen.setUsePsram(true);
        const bool began = screen.begin();
        const bool rendered = screen.render(sceneNoop);
        Serial.printf("PSRAM request=%d begin=%d render=%d actual=%d db=%d\n",
                      screen.usePsram(), began, rendered, screen.tileIsPsram(),
                      screen.doubleBuffer());
    }

    Serial.println("TEST done");
}

void loop()
{
    delay(1000);
}
