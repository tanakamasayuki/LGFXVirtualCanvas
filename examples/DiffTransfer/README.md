# DiffTransfer

> 日本語版: [README.ja.md](README.ja.md)

Skip transferring tiles whose content did not change.

## Prepare and run

1. Check the [example guide](../../docs/GUIDE_EXAMPLES.md) for setup and hardware. Use the root README for supported boards and dependencies.
2. Open [DiffTransfer.ino](DiffTransfer.ino) and select the actual board/port. Prepare the dependencies used by these includes:

   `<M5Unified.h>`, `<LGFXVirtualCanvas.h>`

3. Upload and open the Serial monitor at 115200 baud. Check startup messages and input conditions in the source.

## Follow the sketch

DiffTransfer — skip transferring tiles that did not change (M5Unified).

The scene is mostly static: a title, a frame, and a clock that only changes
once a second. With LGFXVirtualDiffMode::Tile, each tile is hashed after your
draw callback and its transfer is skipped when the hash matches the previous
render, so most frames only push the tiles the clock sits in.

It reduces TRANSFER only: the draw callback still runs for every tile and
hashing is added on top. So it pays off on a large, slow panel (a Full-HD USB
display, say) and not on a small fast one — which is why it is off by default
and why this sketch prints the pushed/total ratio. On a Core2 (320x240 over
fast SPI) expect the ratio to drop but the frame rate to barely move; the
numbers are the point of the example.

Board: any M5 device (M5Unified / M5.Display).

These APIs are listed in their first source order. Registering a callback does not execute its body at that point.

1. `screen.begin(...)` — Initialize this component after its prerequisites; inspect the returned status where provided.
2. `screen.render(...)` — Draw a stable scene snapshot; tiled rendering can invoke the callback repeatedly.
3. `screen.invalidate(...)` — Discard cached comparisons after external drawing changes the panel.

## API usage and precautions

Initialize the real panel before allocating the virtual screen/sprite. A render callback can run once per tile: it must draw the same scene snapshot without advancing animation, consuming input or doing one-shot I/O inside it. Update state outside the callback, then render.

Screen coordinates cover the virtual screen; sprite coordinates are local to its viewport. A memory limit bounds tile storage, not every allocation in the application. Check begin/render outcomes and inspect tile/PSRAM diagnostics rather than assuming requested memory came from a particular region.

Diff mode reduces transfers, not callback execution; after drawing directly to the panel, invalidate the cached comparison before relying on it. Transparent drawing leaves skipped pixels untouched and does not restore the covered background when an overlay closes.

Compare pushed pixels with total pixels while the scene changes. A lower transferred ratio need not increase frame rate if drawing, hashing or the panel itself dominates the time.

## Inputs and observable output

Button conditions used by this sketch: `BtnA.wasPressed()`. Distinguish press edges, release edges and a continuously held button.

These are selected output formats from the source. printf specifiers are replaced by runtime values; inspect the surrounding conditions rather than assuming these lines always occur in this order.

```text
%d tiles of %d rows, hash table %u bytes
transferred %6u / %6u px
```

## Verify and troubleshoot

Skip transferring tiles whose content did not change and verify the result on its output. First use the supplied input, then change one configuration or data source at a time.

| Symptom | First check |
|---|---|
| No output | Board/dependencies, initialization results, output destination and Serial baud |
| Starts but stops making progress | Wait conditions, buffer space, peer/input readiness and service calls |
| Unexpected values | Lengths/units, format, data lifetime, shared state and partial results |

See the [guide](../../docs/GUIDE_EXAMPLES.md) for shared principles and the [example index](../README.md) for the next sketch.
