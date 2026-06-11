# Changelog / 変更履歴

## Unreleased

## 1.1.0
- (EN) Expand `LGFXVirtualCanvas` API coverage to the set of LovyanGFX/M5GFX wrappers that can be safely provided on a tiled virtual surface: current-color drawing overloads, Bezier/arc/helper shapes, gradients, smooth/wide/spot drawing, bitmap and decoded image helpers, QR code rendering, grayscale/alpha image helpers, image rotate/zoom helpers, readback, palette/state utilities, pivot/gradient helpers, and extended text/font APIs.
- (JA) タイル化された仮想 surface 上で安全に提供できる LovyanGFX/M5GFX ラッパーを一通り追加：current color 描画 overload、Bezier/arc/helper 図形、gradient、smooth/wide/spot 描画、bitmap と decode 画像 helper、QR code、grayscale/alpha 画像 helper、画像 rotate/zoom helper、読み戻し、palette/state ユーティリティ、pivot/gradient helper、text/font API 拡張。
- (EN) Add coordinate-bearing `write*` compatibility wrappers (`writePixel`, `writeFastHLine`, `writeFastVLine`, `writeFillRect`, `writeFillRectPreclipped`). `writeFillRectPreclipped` intentionally routes through clipped `fillRect` semantics instead of trusting caller-side preclip.
- (JA) 座標付き `write*` 互換ラッパー（`writePixel`, `writeFastHLine`, `writeFastVLine`, `writeFillRect`, `writeFillRectPreclipped`）を追加。`writeFillRectPreclipped` は caller 側の preclip を信用せず、clip される `fillRect` 相当として扱う。
- (EN) Document not-adopted API groups and why they are excluded: stream-cursor writes, caller-managed window/transaction controls, scroll/copy, sprite transfer helpers, affine image helpers, and tile-local PNG export helpers.
- (JA) 採用しない API 群と理由を明文化：stream cursor 依存 write、呼び出し側管理の window/transaction 制御、scroll/copy、sprite 転送 helper、affine 画像 helper、tile 単体 PNG export helper。
- (EN) Extend the shared build/parity scene to cover representative wrappers across LovyanGFX, M5GFX, and M5Unified builds.
- (JA) LovyanGFX / M5GFX / M5Unified の build/parity 用共通シーンに代表 wrapper のカバレッジを追加。

## 1.0.2
- (EN) Define `LGFXVIRTUALCANVAS_H` on include so other code/libraries can detect the library with `#if defined(LGFXVIRTUALCANVAS_H)`; the header now also pulls in the `LGFXVIRTUALCANVAS_VERSION_*` macros.
- (JA) include 時に `LGFXVIRTUALCANVAS_H` を定義し、他のコード/ライブラリが `#if defined(LGFXVIRTUALCANVAS_H)` で検出できるようにした。ヘッダが `LGFXVIRTUALCANVAS_VERSION_*` マクロも取り込むようにした。

## 1.0.1
- (EN) Add more `LGFXVirtualCanvas` wrappers: `fillRoundRect`, `drawRoundRect`, `drawEllipse`, `fillEllipse`, `drawTriangle`, `fillTriangle`, `setFont`, and `setTextFont`. Extend the parity test scenes to cover the new shape/font wrappers across split counts.
- (JA) `LGFXVirtualCanvas` のラッパーを追加：`fillRoundRect`, `drawRoundRect`, `drawEllipse`, `fillEllipse`, `drawTriangle`, `fillTriangle`, `setFont`, `setTextFont`。新しい図形/フォントラッパーを分割数違いの parity テストシーンでも検証するようにした。

## 1.0.0
- (EN) Change the no-arg default from a fixed 3 splits to a **size-aware tile budget** (`DEFAULT_TILE_BYTES` ≈ 19 KB/tile): the split count now scales with the surface (small sprite → 1 tile, full screen → several), and **double-buffering is auto-enabled when the surface resolves to ≥ 2 tiles**. Derived from the Core2 benchmark — reproduces the measured optimum split at every tested size, bounds tile RAM to ≈ 2× the budget regardless of size, and avoids large-contiguous-block alloc failures. `setSplitCount` / `setMemoryLimit` / `setDoubleBuffer` still override. See SPEC §10.1 / §10.5.
- (JA) 無指定時のデフォルトを「3分割固定」から **サイズ依存のタイル予算**（`DEFAULT_TILE_BYTES` ≈ 19 KB/タイル）に変更：分割数が面サイズに応じてスケールし（小スプライト→1タイル、全画面→数タイル）、**2タイル以上に解決される面ではダブルバッファを自動有効化**。Core2 ベンチに由来し、測定した全サイズで最適分割を再現、タイルRAMをサイズに依らず ≈ 2×予算に抑え、巨大連続確保の失敗も回避。`setSplitCount` / `setMemoryLimit` / `setDoubleBuffer` で上書き可。SPEC §10.1 / §10.5 参照。
- (EN) Fix tile-buffer reuse race on internal-RAM (DMA) sprites: the render loop now waits for each tile's DMA before reusing the single buffer, so a tile is no longer overwritten mid-transfer. Add opt-in `setDoubleBuffer(true)` (two ping-pong buffers) that overlaps a tile's DMA with the next tile's draw. See SPEC §10.5. Add a real-hardware benchmark under `bench/`.
- (JA) 内蔵RAM（DMA）スプライトでのタイルバッファ再利用レースを修正：描画ループが各タイルの DMA を待ってから単一バッファを再利用するようにし、転送中のタイル上書きを防止。`setDoubleBuffer(true)`（2枚 ping-pong）を opt-in で追加し、あるタイルの DMA と次タイルの描画を重ねる。SPEC §10.5 参照。実機ベンチマークを `bench/` に追加。

## 0.2.0
- (EN) Initial commit
- (JA) 初期コミット
