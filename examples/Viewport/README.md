# Viewport

> 日本語版: [README.ja.md](README.ja.md)

Update a sprite region using local coordinates.

## Prepare and run

1. Check the [example guide](../../docs/GUIDE_EXAMPLES.md) for setup and hardware. Use the root README for supported boards and dependencies.
2. Open [Viewport.ino](Viewport.ino) and select the actual board/port. Prepare the dependencies used by these includes:

   `<M5Unified.h>`, `<LGFXVirtualCanvas.h>`

3. After upload, observe the display or other output used by this example. If using Serial, match its initialization in the source.

## Follow the sketch

Viewport — partial update with LGFXVirtualSprite (M5Unified).

A tiled sub-region (a "viewport") is re-rendered every frame while the rest
of the screen (a title drawn once) is never touched. Inside the viewport you
draw in LOCAL coordinates (0,0 = viewport top-left); internally it is split
into tiles so it needs only a small buffer.

Board: any M5 device (M5Unified / M5.Display).

These APIs are listed in their first source order. Registering a callback does not execute its body at that point.

1. `view.setMemoryLimit(...)` — Set a working-buffer budget before allocation and handle a request that cannot fit.
2. `view.begin(...)` — Initialize this component after its prerequisites; inspect the returned status where provided.
3. `view.render(...)` — Draw a stable scene snapshot; tiled rendering can invoke the callback repeatedly.

## API usage and precautions

Initialize the real panel before allocating the virtual screen/sprite. A render callback can run once per tile: it must draw the same scene snapshot without advancing animation, consuming input or doing one-shot I/O inside it. Update state outside the callback, then render.

Screen coordinates cover the virtual screen; sprite coordinates are local to its viewport. A memory limit bounds tile storage, not every allocation in the application. Check begin/render outcomes and inspect tile/PSRAM diagnostics rather than assuming requested memory came from a particular region.

Diff mode reduces transfers, not callback execution; after drawing directly to the panel, invalidate the cached comparison before relying on it. Transparent drawing leaves skipped pixels untouched and does not restore the covered background when an overlay closes.

## Verify and troubleshoot

Update a sprite region using local coordinates and verify the result on its output. First use the supplied input, then change one configuration or data source at a time.

| Symptom | First check |
|---|---|
| No output | Board/dependencies, initialization results, output destination and Serial baud |
| Starts but stops making progress | Wait conditions, buffer space, peer/input readiness and service calls |
| Unexpected values | Lengths/units, format, data lifetime, shared state and partial results |

See the [guide](../../docs/GUIDE_EXAMPLES.md) for shared principles and the [example index](../README.md) for the next sketch.
