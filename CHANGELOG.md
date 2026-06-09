# Changelog / 変更履歴

## Unreleased
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
