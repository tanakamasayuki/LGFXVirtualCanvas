# Tests

> English: [README.md](README.md)

LGFXVirtualCanvas の自動テストスイート。

- [pytest-embedded](https://docs.espressif.com/projects/pytest-embedded/en/latest/) + Arduino CLI バックエンド
- `lang-ship:host` コア(描画は `mode=lgfx`)上でヘッドレス実行し、LovyanGFX 描画を host 上で行って `gfx.createPng()` で PNG として取得する
- テストごとのサブディレクトリに `<name>.ino` / `sketch.yaml` / `test_<name>.py`(`dut` フィクスチャを使用)
- 成果物を出すスケッチは `mkdir("output", 0755)` してから `output/<name>.png` を書く。`conftest.py` が各テスト前に `output/` を消す

## ディレクトリ構成

- `parity/` — 中核の不変条件: 同じ `drawScene()` を `LGFXVirtualScreen` で複数の分割数で描いた結果が **ピクセル完全一致** すること。`split=1`(全高1タイル・offsetY=0)が全面描画の基準、`split=2/3/5/7` がタイル分割・オフセット・クリッピング・再構成・端数タイルを検証する。Pillow で比較し、不一致時は `full.png` / `virtual.png` / `diff.png` を保存
- `lovyangfx_smoke/`, `m5gfx_smoke/`, `m5unified_smoke/` — LovyanGFX 系3つのエントリポイント(LovyanGFX / M5GFX / M5Unified)に対する host 描画環境の smoke。小さなパターンを描いて PNG 生成を確認するだけで、LGFXVirtualCanvas 自体は検証しない

## 実行

```sh
# 全テスト(host・デフォルト)
uv run pytest -v

# 単一テスト
uv run pytest parity -v
```

初回実行ではピン留めされた描画ライブラリ(LovyanGFX など)を arduino-cli 環境へ
ダウンロードするため、2回目以降より時間がかかる。
