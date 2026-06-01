# ベンチマーク

> English: [README.md](README.md)

LGFXVirtualCanvas の実機性能ベンチマーク。[`tests/`](../tests/) のヘッドレス
ホストテストと違い、これは**実機が必要**です。SPI/DMA の転送時間を測るため
で、その時間はホストバックエンドには存在しません。

- ボード: **M5Stack Core2**(ESP32)または **CoreS3**(ESP32-S3)。M5Unified が
  パネルを自動判別。どちらも 320×240。
- 出力: **Serial @115200** の固定幅テーブル — ドキュメントへコピペ可能。

## 何を測るか

同一シーンを、1つの**テンプレート関数**経由で全方式に描画します。これにより
各方式で描画内容がバイト単位で一致し(差は転送経路のみ)、公平に比較できます。
LGFXVirtualCanvas が LGFX の API をミラーしているので、1つのテンプレートが
`LovyanGFX&`・`M5Canvas&`・`LGFXVirtualCanvas&` のすべてに展開されます。

### 方式

| 方式 | 内容 | バッファ | 備考 |
|------|------|----------|------|
| **A** 直接パネル          | `M5.Display` へ直接描画、バッファ無し | なし | 下限(ちらつきあり) |
| **B** フルスプライト      | 320×240 を1枚、`pushSprite` 1回 | 内蔵 **+** PSRAM | 上限(最大メモリ)。描画/転送を分離計測 |
| **C** `LGFXVirtualScreen` | ライブラリ・単一バッファ(既定) | 内蔵 | 分割数掃引 + autoClear on/off。描画と転送は直列 |
| **D** `LGFXVirtualScreen` | ライブラリ・`setDoubleBuffer(true)` | 内蔵 | 分割数掃引。描画と転送をオーバーラップ(タイルRAM 2倍) |
| **C** `LGFXVirtualSprite` | ライブラリ・160×100 部分領域 | 内蔵 | 代表的な分割数1つ |

C は既定の単一バッファ(正しいが直列 — 各タイル push 後に `waitDMA`)。D は
opt-in の2枚 ping-pong で、あるタイルの DMA 転送と次タイルの描画を重ねる。
仕組みは SPEC §10.5 を参照。

### 軸

- **分割数**: 1, 2, 3, 4, 6, 8(方式 C)
- **auto-clear**: ON(既定) vs OFF — `fillScreen` で全面を塗るシーンでの
  二重塗りコストを定量化(SPEC §11.1)
- **バッファ配置**: 内蔵RAM vs PSRAM — **ベースラインのみ。** Phase 1 では
  ライブラリに PSRAM 指定の公開 API が無い(`createSprite` は内蔵RAM既定)ため、
  方式 C は内蔵RAMのみで計測。
- **シーンの重さ**: `light`(転送律速)/ `heavy`(CPU律速)/ `image`
  (`pushImage`、メモリ帯域律速)

### 列

`draw_us` / `xfer_us` は1フレームあたりの CPU描画 と SPI転送 の平均で、ループを
こちらで制御できる方式(B)でのみ分離計測します。A(両者が混在)と C(ライブラリが
ループを保持)は `frame_us` / `fps` のみ。`heapInt` / `heapPSRAM` は確保後の空き
バイト数です。

## 実行方法

```sh
# Core2(既定プロファイル)
arduino-cli compile -e --profile m5stack_core2 bench
arduino-cli upload  -p /dev/ttyUSB0 --profile m5stack_core2 bench
arduino-cli monitor -p /dev/ttyUSB0 -c baudrate=115200

# CoreS3
arduino-cli compile -e --profile m5stack_cores3 bench
```

または Arduino IDE で `bench/bench.ino` を開き、ボードを選択して 115200 で
シリアルモニタを開く。ベンチは `setup()` で一度だけ走り、全テーブルを出力します
(`loop()` は何もしない)。

## 結果の読み方

- **C vs B**: 差がライブラリのタイリングのオーバーヘッド(全画面バッファを
  持たずに済むことの対価)。
- **C vs A**: バッファリングによる効果(ちらつき無し、転送のバッチ化)。
- C の内部では、**分割数**がメモリとフレーム時間をどうトレードするか、
  シーンが元々全面を塗る場合に **autoClear=off** がどれだけ効くかを観察。
- 同じ分割数での **D vs C** の差が、描画/転送オーバーラップの効果。
- `heavy` は CPU律速(分割数の影響が最大)、`light` は転送律速(分割数の影響が
  最小)。後者こそ方式 D のオーバーラップが最も効く領域です。
