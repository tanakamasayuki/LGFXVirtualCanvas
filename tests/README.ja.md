# Tests

> English: [README.md](README.md)

自動テストスイート。

- [pytest-embedded](https://docs.espressif.com/projects/pytest-embedded/en/latest/) + Arduino CLI バックエンド
- 2 プロファイル: `lang-ship:host`(ロジック検証・大サイズ fixture・高速 CI)と `esp32:esp32:esp32`(実機検証・フットプリント計測)
- 機能ごとのサブディレクトリに `<feature>.ino` / `sketch.yaml` / `test_<feature>.py` / `input/` fixture
- アサーションは `EXPECT_TRUE` / `EXPECT_EQ` / `EXPECT_NEAR` マクロ + Serial 経由の `TEST done N/M` プロトコル

## ディレクトリ構成

- `smoke/` — テンプレート smoke テスト(host プロファイル)。テスト基盤そのものを検証

## テスト設計

## 実行

```sh
# host (デフォルト)
uv run --env-file .env pytest

# 実機 ESP32
uv run --env-file .env pytest --profile=esp32

# 単一テスト
uv run --env-file .env pytest smoke/
```
