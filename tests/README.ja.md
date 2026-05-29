# Tests

> English: [README.md](README.md)

LGFXVirtualCanvas の自動テストスイート。

- [pytest-embedded](https://docs.espressif.com/projects/pytest-embedded/en/latest/) + Arduino CLI バックエンド
- `lang-ship:host` コア(描画は `mode=lgfx`)上でヘッドレス実行し、LovyanGFX 描画を host 上で行って `gfx.createPng()` で PNG として取得する
- テストごとのサブディレクトリに `<name>.ino` / `sketch.yaml` / `test_<name>.py`(`dut` フィクスチャを使用)
- 成果物を出すスケッチは `mkdir("output", 0755)` してから `output/<name>.png` を書く。`conftest.py` が各テスト前に `output/` を消す

## ディレクトリ構成

**Tier 1 — 機能・正当性(LovyanGFX):**

- `parity/` — 中核の不変条件: 同じ `drawScene()` を複数の分割数で描いた結果が **ピクセル完全一致** すること。複数シーン（図形・円・テキスト・境界・クリッピング・fuzz・アニメ）。不一致時は `*_full.png` / `*_virtual.png` / `*_diff.png` を保存
- `autoclear/` — auto-clear の決定性・`setBackgroundColor`・`setAutoClear(false)`
- `memory/` — `setMemoryLimit` のタイル高算出と確保失敗の扱い（フォールバック無し・`begin()`/`render()` が `false`）
- `pushimage/` — `pushImage` がタイル境界を跨いでも split 不変
- `sprite/` — `LGFXVirtualSprite`：タイル分割の正しさ（端数タイル含む）・領域外不可侵・位置移動

**Tier 2 — クロスライブラリ ビルド＋最小 parity:**

- `build_lovyangfx/`, `build_m5gfx/`, `build_m5unified/` — `LGFXVirtualCanvas.h` が各エントリポイント(LovyanGFX / M5GFX / M5Unified)でコンパイルでき、共有シーンが `split=1` と `split=3` で一致することを検証。各エントリポイントのビルド/smoke チェックを兼ねる。共有シーンは `common_libs/vc_scene/` に置き3テストで使い回す

テスト方針・計画（Tier 構成・ケース一覧・配分）は [SPEC.ja.md](../SPEC.ja.md) §13 を参照。

## 実行

```sh
# 全テスト(host・デフォルト)
uv run pytest -v

# 単一テスト
uv run pytest parity -v
```

初回実行ではピン留めされた描画ライブラリ(LovyanGFX など)を arduino-cli 環境へ
ダウンロードするため、2回目以降より時間がかかる。
