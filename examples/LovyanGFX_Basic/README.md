# LovyanGFX_Basic

> 日本語版: [README.ja.md](README.ja.md)

Use a tiled screen with a plain LovyanGFX panel.

## Prepare and run

1. Check the [example guide](../../docs/GUIDE_EXAMPLES.md) for setup and hardware. Use the root README for supported boards and dependencies.
2. Open [LovyanGFX_Basic.ino](LovyanGFX_Basic.ino) and select the actual board/port. Prepare the dependencies used by these includes:

   `<LovyanGFX.hpp>`, `<LGFX_AUTODETECT.hpp>`, `<LGFXVirtualCanvas.h>`

3. After upload, observe the display or other output used by this example. If using Serial, match its initialization in the source.

## Follow the sketch

LovyanGFX_Basic — using LGFXVirtualCanvas with plain LovyanGFX.

Same library, non-M5 path: include LovyanGFX + LGFX_AUTODETECT, construct
your LGFX panel, and pass it to LGFXVirtualScreen. The draw function is
identical to the M5 examples — it only ever sees LGFXVirtualCanvas.

Board: any LovyanGFX-supported display (auto-detected by LGFX_AUTODETECT).

These APIs are listed in their first source order. Registering a callback does not execute its body at that point.

1. `screen.render(...)` — Draw a stable scene snapshot; tiled rendering can invoke the callback repeatedly.

## API usage and precautions

Initialize the real panel before allocating the virtual screen/sprite. A render callback can run once per tile: it must draw the same scene snapshot without advancing animation, consuming input or doing one-shot I/O inside it. Update state outside the callback, then render.

Screen coordinates cover the virtual screen; sprite coordinates are local to its viewport. A memory limit bounds tile storage, not every allocation in the application. Check begin/render outcomes and inspect tile/PSRAM diagnostics rather than assuming requested memory came from a particular region.

Diff mode reduces transfers, not callback execution; after drawing directly to the panel, invalidate the cached comparison before relying on it. Transparent drawing leaves skipped pixels untouched and does not restore the covered background when an overlay closes.

## Verify and troubleshoot

Use a tiled screen with a plain LovyanGFX panel and verify the result on its output. First use the supplied input, then change one configuration or data source at a time.

| Symptom | First check |
|---|---|
| No output | Board/dependencies, initialization results, output destination and Serial baud |
| Starts but stops making progress | Wait conditions, buffer space, peer/input readiness and service calls |
| Unexpected values | Lengths/units, format, data lifetime, shared state and partial results |

See the [guide](../../docs/GUIDE_EXAMPLES.md) for shared principles and the [example index](../README.md) for the next sketch.
