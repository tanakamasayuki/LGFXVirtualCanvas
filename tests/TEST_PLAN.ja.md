# LGFXVirtualCanvas テスト計画

> ステータス: ドラフト。SPEC.ja.md §13 を実行計画に落としたもの。

## 1. 前提と方針

- LGFXVirtualCanvas のヘッダは **gfx 中立**（`LovyanGFX` 基底クラスと `LGFX_Sprite` にのみ依存）。
- 対応エントリポイントは **LovyanGFX / M5GFX / M5Unified** の3つ。ただし **M5GFX / M5Unified の描画エンジンは内部的に LovyanGFX** であり、タイル分割ロジックの正当性（parity）は3つで本質的に同じ。
- したがってテストは2層に分ける：
  - **Tier 1（機能・正当性）**: ライブラリのロジックを LovyanGFX 1本で網羅的に検証する。
  - **Tier 2（クロスライブラリ ビルド＋最小描画チェック）**: 「ヘッダが各エントリポイントでコンパイルでき、最小描画が parity を満たす」ことを3ライブラリで担保する。smoke の役割（ビルド確認）はここが兼ねる。
- すべて `lang-ship:host`（`mode=lgfx`）上でヘッドレス実行し、`gfx.createPng()` の PNG を Pillow で pixel 比較する。

### 配分の考え方

ロジックの網羅は LovyanGFX 1本（Tier 1）。M5GFX / M5Unified は「ビルド＋最小 parity」だけ（Tier 2）。
理由：3ライブラリで全ケースを回しても描画エンジンが同一なため**新しいロジック網羅は得られず**、CI 時間とライブラリ取得だけが3倍になる。クロスライブラリで本当に守りたいのは「**ヘッダが各 include 順序・各ライブラリの型でコンパイル＆描画できる**」こと＝Tier 2 で十分。

## 2. Tier 1 — 機能・正当性（LovyanGFX 基準）

中核の不変条件: **同一 `drawScene()` を分割数だけ変えた結果が pixel 完全一致**（`split=1` を全面描画の基準とする。SPEC §13.1）。各ケースは複数の split（例 1/2/3/5/7）で一致を確認する。

| # | ケース | 検証内容 | SPEC |
|---|---|---|---|
| T1-1 | parity（総合） | 図形＋テキスト混在シーンが split 不変 | §13 ✅実装済 |
| T1-2 | 基本図形 | fillRect / drawRect / drawLine / drawPixel / drawFastH/VLine | §13.3 |
| T1-3 | 円 | drawCircle / fillCircle | §13.3 |
| T1-4 | テキスト | drawString / setCursor＋print/println/printf、cursor Y 仮想座標 | §9.1 §13.3 |
| T1-5 | タイル境界 | `y = tileH-1 / tileH / tileH+1` を跨ぐ描画 | §12 |
| T1-6 | 端数タイル | 画面高が分割数で割り切れない（例 split=7 で最終30行）| §12 ✅一部 |
| T1-7 | クリッピング安全性 | 画面外・負座標・下端超過でクラッシュせず一致 | §12 |
| T1-8 | auto-clear | 未描画画素が背景色で決定的／`setBackgroundColor`／`setAutoClear(false)` 挙動 | §11 |
| T1-9 | メモリ／確保失敗 | `setMemoryLimit` で tileH 算出、過小指定で `begin()`＝false・`isReady()`＝false・`render()`＝false（フォールバックしない） | §10.3 |
| T1-10 | ランダム fuzz | 乱数シードで図形列を生成、各 split で一致（シードをログ出力し再現可能に） | §13.3 |
| T1-11 | アニメーション | フレーム列を回し、各フレームが split 不変 | §13.3 |
| T1-12 | pushImage（実験） | クリップのみで一致するか実験。困難なら非対応メソッド化を判断 | §9.2 |

## 3. Tier 2 — クロスライブラリ ビルド＋最小描画

各エントリポイント（LovyanGFX / M5GFX / M5Unified）で：

1. それぞれの gfx ヘッダを include した後 `LGFXVirtualCanvas.h` を include → **コンパイルが通ること**（API ドリフト検出）。
2. 共有の最小 `drawScene()` を `split=1` と `split=3` で描画し、**pixel 一致**＋ PNG 生成を確認。

- 最小シーンは `tests/common_libs/` の共有ライブラリに置き、3テストで使い回す（gfx_demo と同じ流儀）。draw 関数は `LGFXVirtualCanvas&` を取るので3ライブラリ共通で書ける。
- これが従来の smoke（ビルド確認）を置き換える。汎用 smoke は廃止して構わない。

## 4. （任意）Tier 3 — 実機/ターゲットビルド

- `esp32:esp32:esp32` でのコンパイル確認（compile-only、フラッシュ不要）を CI に1本置くと、host では出ないターゲット依存のビルド破綻を早期検出できる。実描画の検証は host（Tier 1/2）に委ねる。

## 5. 実装順序

1. Tier 1 のケースを `tests/parity/` を起点に拡充（T1-2〜T1-8 を追加 → fuzz/animation → pushImage 実験）。
2. `tests/common_libs/` に共有最小シーンを作成。
3. Tier 2 の3ディレクトリ（lovyangfx / m5gfx / m5unified のビルド＋最小 parity）を作成し、汎用 smoke を置換。
4. （任意）Tier 3 の esp32 compile-only を追加。

## 6. 共有ルール

- 1テスト = 1ディレクトリ（`<name>.ino` / `sketch.yaml` / `test_<name>.py`）。
- 成果物は `output/<name>.png`、`conftest.py` が各テスト前に wipe。
- 失敗時は `full.png` / `virtual.png` / `diff.png` を保存（SPEC §13.2）。
- fuzz はシード固定＋ログ出力で再現可能にする。
