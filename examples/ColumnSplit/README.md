# ColumnSplit

> 日本語版: [README.ja.md](README.ja.md)

Compare horizontal splitting, buffering and PSRAM choices.

## Prepare and run

1. Check the [example guide](../../docs/GUIDE_EXAMPLES.md) for setup and hardware. Use the root README for supported boards and dependencies.
2. Open [ColumnSplit.ino](ColumnSplit.ino) and select the actual board/port. Prepare the dependencies used by these includes:

   `<M5Unified.h>`, `<LGFXVirtualCanvas.h>`

3. Upload and open the Serial monitor at 115200 baud. Check startup messages and input conditions in the source.

## Follow the sketch

ColumnSplit — split the surface into columns instead of rows, and (optionally)
put the tile buffers in PSRAM (M5Unified).

By default the canvas is cut into horizontal bands sent top to bottom. Some
targets want the opposite: a long strip fed out left to right (long-format
printing, a ticker, a wide waveform). setSplitAxis(Columns) makes each tile a
full-height vertical band and transfers them in that order. The draw callback
does not change at all — it still works in full-surface coordinates, and the
resulting image is identical to row splitting.

The second half shows setUsePsram(true). On a large panel the internal-RAM
default resolves to many small tiles and the callback re-runs for every one of
them; PSRAM buys few large tiles instead. It is a trade, not a free win —
PSRAM is slower and LovyanGFX pushes a PSRAM sprite without DMA (so auto
double-buffering turns off). This sketch prints what was actually resolved.

Board: any M5 device (M5Unified / M5.Display).

These APIs are listed in their first source order. Registering a callback does not execute its body at that point.

1. `screen.begin(...)` — Initialize this component after its prerequisites; inspect the returned status where provided.
2. `screen.render(...)` — Draw a stable scene snapshot; tiled rendering can invoke the callback repeatedly.
3. `screen.setMemoryLimit(...)` — Set a working-buffer budget before allocation and handle a request that cannot fit.

## API usage and precautions

Initialize the real panel before allocating the virtual screen/sprite. A render callback can run once per tile: it must draw the same scene snapshot without advancing animation, consuming input or doing one-shot I/O inside it. Update state outside the callback, then render.

Screen coordinates cover the virtual screen; sprite coordinates are local to its viewport. A memory limit bounds tile storage, not every allocation in the application. Check begin/render outcomes and inspect tile/PSRAM diagnostics rather than assuming requested memory came from a particular region.

Diff mode reduces transfers, not callback execution; after drawing directly to the panel, invalidate the cached comparison before relying on it. Transparent drawing leaves skipped pixels untouched and does not restore the covered background when an overlay closes.

## Inputs and observable output

These are selected output formats from the source. printf specifiers are replaced by runtime values; inspect the surrounding conditions rather than assuming these lines always occur in this order.

```text
%-12s tiles=%d  tile=%dx%d  span=%d  doubleBuffer=%d  psram=%d
psram      : requested but served from internal RAM
done
```

## Verify and troubleshoot

Compare horizontal splitting, buffering and PSRAM choices and verify the result on its output. First use the supplied input, then change one configuration or data source at a time.

| Symptom | First check |
|---|---|
| No output | Board/dependencies, initialization results, output destination and Serial baud |
| Starts but stops making progress | Wait conditions, buffer space, peer/input readiness and service calls |
| Unexpected values | Lengths/units, format, data lifetime, shared state and partial results |

See the [guide](../../docs/GUIDE_EXAMPLES.md) for shared principles and the [example index](../README.md) for the next sketch.
