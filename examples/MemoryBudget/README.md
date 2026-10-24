# MemoryBudget

> 日本語版: [README.ja.md](README.ja.md)

Set a tile memory budget and handle allocation failure.

## Prepare and run

1. Check the [example guide](../../docs/GUIDE_EXAMPLES.md) for setup and hardware. Use the root README for supported boards and dependencies.
2. Open [MemoryBudget.ino](MemoryBudget.ino) and select the actual board/port. Prepare the dependencies used by these includes:

   `<M5Unified.h>`, `<LGFXVirtualCanvas.h>`

3. Upload and open the Serial monitor at 115200 baud. Check startup messages and input conditions in the source.

## Follow the sketch

MemoryBudget — cap the tile buffer and handle allocation failure (M5Unified).

setMemoryLimit() bounds the tile sprite to a byte budget; the library picks
the largest tile height that fits and splits accordingly. begin() returns
false if the request cannot be satisfied — there is NO fallback, so you can
detect breakage explicitly.

Board: any M5 device (M5Unified / M5.Display).

These APIs are listed in their first source order. Registering a callback does not execute its body at that point.

1. `screen.setMemoryLimit(...)` — Set a working-buffer budget before allocation and handle a request that cannot fit.
2. `screen.begin(...)` — Initialize this component after its prerequisites; inspect the returned status where provided.
3. `screen.render(...)` — Draw a stable scene snapshot; tiled rendering can invoke the callback repeatedly.

## API usage and precautions

Initialize the real panel before allocating the virtual screen/sprite. A render callback can run once per tile: it must draw the same scene snapshot without advancing animation, consuming input or doing one-shot I/O inside it. Update state outside the callback, then render.

Screen coordinates cover the virtual screen; sprite coordinates are local to its viewport. A memory limit bounds tile storage, not every allocation in the application. Check begin/render outcomes and inspect tile/PSRAM diagnostics rather than assuming requested memory came from a particular region.

Diff mode reduces transfers, not callback execution; after drawing directly to the panel, invalidate the cached comparison before relying on it. Transparent drawing leaves skipped pixels untouched and does not restore the covered background when an overlay closes.

## Inputs and observable output

These are selected output formats from the source. printf specifiers are replaced by runtime values; inspect the surrounding conditions rather than assuming these lines always occur in this order.

```text
allocated: tiles=%d tileH=%d
```

## Verify and troubleshoot

Set a tile memory budget and handle allocation failure and verify the result on its output. First use the supplied input, then change one configuration or data source at a time.

| Symptom | First check |
|---|---|
| No output | Board/dependencies, initialization results, output destination and Serial baud |
| Starts but stops making progress | Wait conditions, buffer space, peer/input readiness and service calls |
| Unexpected values | Lengths/units, format, data lifetime, shared state and partial results |

See the [guide](../../docs/GUIDE_EXAMPLES.md) for shared principles and the [example index](../README.md) for the next sketch.
