# Changelog / 変更履歴

## Unreleased
- (EN) Add diff transfer: `setDiffMode(LGFXVirtualDiffMode::Tile)` skips transferring tiles whose content is unchanged since the previous render, plus `diffMode()`, `invalidate()`, `diffMemoryUsage()`, `diffPushedPixels()`, `diffTotalPixels()`. Disabled by default (`Off` allocates nothing). Whole-tile granularity, 8 bytes of hash per tile (32-bit FNV-1a in two interleaved lanes). Reduces transfer only — drawing is unchanged and hashing is added on top.
- (JA) 差分転送を追加：`setDiffMode(LGFXVirtualDiffMode::Tile)` で前回描画から内容が変化していないタイルの転送を省略。あわせて `diffMode()`, `invalidate()`, `diffMemoryUsage()`, `diffPushedPixels()`, `diffTotalPixels()` を追加。既定は無効（`Off` では確保も行わない）。粒度はタイル単位、ハッシュは 1 タイル 8 バイト（32bit FNV-1a を 2 レーン交互）。削減対象は転送のみで、描画は減らずハッシュ計算が増える。
- (EN) The panel content is assumed to persist where nothing was transferred. Reallocation, config changes, an `LGFXVirtualSprite` position change, and a panel rotation/size/color-depth change invalidate automatically; anything else drawing on the panel requires `invalidate()`.
- (JA) 転送を省略した領域はパネルが前回の絵を保持している前提。再確保・設定変更・`LGFXVirtualSprite` の位置変更・パネルの回転/サイズ/色深度の変化は自動で無効化するが、それ以外がパネルに描画した場合は `invalidate()` の呼び出しが必要。
- (EN) Double-buffer alternation now advances per transfer instead of per tile, so a skipped tile cannot let the next-but-one tile overwrite a buffer whose DMA is still in flight.
- (JA) ダブルバッファの切り替えをタイルごとではなく転送ごとに進めるようにした。これによりタイルをスキップしても、DMA 転送中のバッファを 2 つ先のタイルが上書きすることがない。
- (EN) Specify diff transfer in SPEC §21 (goal and deliberate limits, granularity rationale, hash width, the `invalidate()` contract, invariants, extension seams, test policy) and add Tier 1 case T1-13 plus the `tests/diff/` suite and the `DiffTransfer` example.
- (JA) 差分転送を SPEC §21 として規定（目標設定と割り切り・粒度の根拠・ハッシュ幅・`invalidate()` の契約・不変条件・拡張点・テスト方針）。Tier 1 ケース T1-13、`tests/diff/` テスト、`DiffTransfer` サンプルを追加。

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
