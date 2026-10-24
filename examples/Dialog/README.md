# Dialog

> 日本語版: [README.ja.md](README.ja.md)

Draw a transparent dialog and restore its background.

## Prepare and run

1. Check the [example guide](../../docs/GUIDE_EXAMPLES.md) for setup and hardware. Use the root README for supported boards and dependencies.
2. Open [Dialog.ino](Dialog.ino) and select the actual board/port. Prepare the dependencies used by these includes:

   `<M5Unified.h>`, `<LGFXVirtualCanvas.h>`

3. Upload and open the Serial monitor at 115200 baud. Check startup messages and input conditions in the source.

## Follow the sketch

Dialog — draw a dialog over a screen without redrawing the screen (M5Unified).

renderTransparent() fills the tile with the transparent color and pushes the
tile with that color masked out, so every pixel you did not draw keeps showing
whatever is already on the panel. That is what lets a *shaped* overlay (rounded
corners, a shadow) sit on top of an existing image: the corners are simply not
transferred.

Note what this example does NOT do: it never re-renders the background while
the dialog is up. Press A to open, B to close — closing is the only moment the
background has to be drawn again.

If your dialog were a plain rectangle you would not need transparency at all:
an LGFXVirtualSprite placed on it already transfers only that rectangle, and it
is faster. The best of both is what `dialog` below does — a sprite on the
dialog's bounding box, pushed transparently, so only that box is even scanned.

Board: any M5 device (M5Unified / M5.Display).

These APIs are listed in their first source order. Registering a callback does not execute its body at that point.

1. `background.begin(...)` — Initialize this component after its prerequisites; inspect the returned status where provided.
2. `background.render(...)` — Draw a stable scene snapshot; tiled rendering can invoke the callback repeatedly.
3. `dialog.renderTransparent(...)` — Draw only non-key pixels and restore the background separately when closing the overlay.

Review these application constants before adapting the sketch: `DLG_W`, `DLG_H`. Match pins, channels and capacities to your application.

## API usage and precautions

Initialize the real panel before allocating the virtual screen/sprite. A render callback can run once per tile: it must draw the same scene snapshot without advancing animation, consuming input or doing one-shot I/O inside it. Update state outside the callback, then render.

Screen coordinates cover the virtual screen; sprite coordinates are local to its viewport. A memory limit bounds tile storage, not every allocation in the application. Check begin/render outcomes and inspect tile/PSRAM diagnostics rather than assuming requested memory came from a particular region.

Diff mode reduces transfers, not callback execution; after drawing directly to the panel, invalidate the cached comparison before relying on it. Transparent drawing leaves skipped pixels untouched and does not restore the covered background when an overlay closes.

Button A draws the overlay and B redraws the background. Transparent corners preserve existing panel pixels; closing the dialog is a separate restoration operation.

## Inputs and observable output

Button conditions used by this sketch: `BtnA.wasPressed()`, `BtnB.wasPressed()`. Distinguish press edges, release edges and a continuously held button.

These are selected output formats from the source. printf specifiers are replaced by runtime values; inspect the surrounding conditions rather than assuming these lines always occur in this order.

```text
dialog: %u / %u px pushed (an upper bound: transparent
```

## Verify and troubleshoot

Draw a transparent dialog and restore its background and verify the result on its output. First use the supplied input, then change one configuration or data source at a time.

| Symptom | First check |
|---|---|
| No output | Board/dependencies, initialization results, output destination and Serial baud |
| Starts but stops making progress | Wait conditions, buffer space, peer/input readiness and service calls |
| Unexpected values | Lengths/units, format, data lifetime, shared state and partial results |

See the [guide](../../docs/GUIDE_EXAMPLES.md) for shared principles and the [example index](../README.md) for the next sketch.
