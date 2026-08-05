# Changelog / 変更履歴

## Unreleased
- (EN) Add transparent transfer: `renderTransparent(...)` mirrors every `render(...)` form and pushes the tile with one color masked out, so whatever is already on the panel shows through — a dialog can be drawn over a screen without redrawing it. The color is configured with `setTransparentColor()` / `transparentColor()` and defaults to `TFT_TRANSPARENT` (RGB565 `0x0120`); plain `render()` is bit-for-bit unaffected.
- (JA) 透過転送を追加：`renderTransparent(...)` は `render(...)` の各形に対応し、1 色をマスクしてタイルを push する。すでにパネルに出ている絵が透けるので、画面を描き直さずにダイアログを重ねられる。色は `setTransparentColor()` / `transparentColor()` で設定し、既定は `TFT_TRANSPARENT`（RGB565 `0x0120`）。通常の `render()` の挙動はビット単位で不変。
- (EN) Whether a frame is transparent is a per-render decision (a method name), while the color is state (a setter with a default): a leftover mode would silently erase the layer underneath, whereas the color is a property of the content and would be noise at every call site. The mode is a method name rather than a `render()` argument because `LGFXVirtualSprite::render()` already carries positional `x, y` and a `ctx`.
- (JA) 「そのフレームが透過か」はレンダごとの判断（メソッド名）、「色」は状態（既定値付きセッター）に分けた。モードが残留すると下のレイヤを静かに消してしまう一方、色はコンテンツの性質であり呼び出しごとに書くとノイズになる。モードを `render()` の引数にしなかったのは、`LGFXVirtualSprite::render()` がすでに位置 `x, y` と `ctx` を取っており、末尾のモード引数では呼び出し行が読めなくなるため。
- (EN) `setTransparentColor()` is a template that preserves the value's C++ type, since LovyanGFX reads `int` / `uint16_t` as RGB565 and `uint32_t` as RGB888; the color is canonicalized to RGB888 for storage. Without this, `setTransparentColor(TFT_TRANSPARENT)` would mask a different color than the sketch draws with — and mask nothing.
- (JA) `setTransparentColor()` は値の C++ 型を保つテンプレートにした（LovyanGFX は `int` / `uint16_t` を RGB565、`uint32_t` を RGB888 として読むため）。保存は RGB888 に正規化する。これをしないと `setTransparentColor(TFT_TRANSPARENT)` はスケッチが描く色とは別の色をマスクし、結果として何もマスクされない。
- (EN) `renderTransparent()` auto-clears the tile with the transparent color (`setBackgroundColor()` is not used), so the draw callback needs no special handling. Diff transfer composes with it: a mode switch or a color change invalidates automatically, including the case where both renders build byte-identical tiles. After anything redraws what lies *underneath* an overlay, call `invalidate()` on the overlay.
- (JA) `renderTransparent()` はタイルを透過色で自動クリアする（`setBackgroundColor()` は使わない）ので、描画コールバック側の対応は不要。差分転送と併用でき、モード切り替えと色変更は自動で無効化される（両者がバイト単位で同一のタイルを作るケースを含む）。オーバーレイの*下*にあるものを描き直したら、オーバーレイ側で `invalidate()` を呼ぶこと。
- (EN) Specify it in SPEC §22 (why a rectangular overlay should use `LGFXVirtualSprite` instead, the API split, tile initialization, the color's type/depth pitfalls, the diff contract, the per-run cost model of LovyanGFX's masked push, invariants, extension seams, test policy) and add Tier 1 case T1-16 plus the `tests/transparent/` suite and the `Dialog` example.
- (JA) SPEC §22 として規定（矩形オーバーレイは `LGFXVirtualSprite` を使うべき理由・API の分割・タイル初期化・色の型と深度の落とし穴・差分との契約・LovyanGFX のマスク付き push の run 単位コストモデル・不変条件・拡張点・テスト方針）。Tier 1 ケース T1-16、`tests/transparent/` テスト、`Dialog` サンプルを追加。

## 1.3.0
- (EN) Add a split axis: `setSplitAxis(LGFXVirtualSplitAxis::Columns)` cuts the surface into full-height vertical bands transferred left to right (for long strips / long-format printing), plus `splitAxis()`, `setTileWidth()`, `tileWidth()`, `tileSpan()`. `Rows` remains the default and the original behavior; the draw callback still works in full-surface coordinates and the output is pixel-identical on both axes (verified by `tests/parity`).
- (JA) 分割軸を追加：`setSplitAxis(LGFXVirtualSplitAxis::Columns)` で面を「高さいっぱいの縦帯」に切り、左から右へ転送する（長尺帯・長尺印刷向け）。あわせて `splitAxis()`, `setTileWidth()`, `tileWidth()`, `tileSpan()` を追加。既定は従来どおり `Rows` で挙動は不変。描画コールバックは変わらず面全体の座標で書け、両軸の出力はピクセル一致する（`tests/parity` で検証）。
- (EN) `LGFXVirtualCanvas` now carries an X offset alongside the Y offset and `width()` reports the surface width instead of the tile width. Column splitting also corrects the newline cursor (LovyanGFX resets it to the *tile's* left edge) and forces X text wrapping off, since wrapping at a tile boundary would make the output depend on the split count.
- (JA) `LGFXVirtualCanvas` が Y に加えて X オフセットを持つようになり、`width()` はタイル幅ではなく面幅を返す。列分割では改行時のカーソル（LovyanGFX は*タイル*の左端に戻す）を補正し、X 方向のテキスト折り返しを無効化する（タイル境界で折り返すと出力が分割数に依存するため）。
- (EN) Add PSRAM tile buffers: `setUsePsram(true)` (default off) allocates the tile sprites in PSRAM so a large panel can use few large tiles instead of hundreds of small ones, with `usePsram()` / `tileIsPsram()`. Auto double-buffering resolves to off for a PSRAM tile (LovyanGFX pushes it without DMA, so there is nothing to overlap). A PSRAM request never fails the allocation — it falls back to internal RAM, which `tileIsPsram()` reports.
- (JA) PSRAM 上のタイルバッファを追加：`setUsePsram(true)`（既定 OFF）でタイル sprite を PSRAM に確保し、大画面で「小タイル数百枚」ではなく「大タイル数枚」を選べるようにした。`usePsram()` / `tileIsPsram()` を追加。PSRAM 上のタイルでは auto のダブルバッファは OFF に解決される（LovyanGFX が DMA 無しで push するため重ねる相手が無い）。PSRAM 要求で確保が失敗することはなく、内蔵RAMへフォールバックした場合は `tileIsPsram()` が報告する。
- (EN) Specify both in SPEC §10.8 (split axis: axis-agnostic budget math, what generalizes, the two text behaviors that do not) and §10.9 (PSRAM: the speed trade, the DMA consequence, and why the fallback is a deliberate exception to §10.3). Extend `tests/parity` to both axes, `tests/memory` to column budget math and the PSRAM request, and add the `ColumnSplit` example.
- (JA) 両機能を SPEC §10.8（分割軸：軸非依存の予算計算・一般化できる部分・一般化できないテキスト2挙動）と §10.9（PSRAM：速度のトレード・DMA への影響・フォールバックを §10.3 の意図的例外とする理由）として規定。`tests/parity` を両軸に、`tests/memory` を列分割の予算計算と PSRAM 要求に拡張し、`ColumnSplit` サンプルを追加。

## 1.2.0
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
