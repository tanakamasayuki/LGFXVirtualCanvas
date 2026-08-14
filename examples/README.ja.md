# examples

> English: [README.md](README.md)

1 つの例が 1 つの論点だけを示します。上から順に読むと、
「動かす」→「状態を渡す」→「メモリを決める」→「転送を減らす」の順に進みます。

原理から知りたい場合は [docs/BEGINNERS_GUIDE.ja.md](../docs/BEGINNERS_GUIDE.ja.md)、
API の一覧は [README.ja.md](../README.ja.md)、設計の根拠は [SPEC.ja.md](../SPEC.ja.md)。

## 読む順番

| # | 例 | 論点 | 関連 API |
| --- | --- | --- | --- |
| 1 | [HelloWorld](HelloWorld/) | 動く最小構成。宣言・描画関数・`render()` の 3 つだけ | `LGFXVirtualScreen` / `render` |
| 1' | [LovyanGFX_Basic](LovyanGFX_Basic/) | 同じことを素の LovyanGFX で（`LGFX_AUTODETECT` ＋ `lcd.init()`） | 同上 |
| 2 | [BouncingBall](BouncingBall/) | **状態を引数で渡す。** View（描画）と Model（更新）を分ける形 | `render(draw, ctx)` / `setMemoryLimit` |
| 3 | [MemoryBudget](MemoryBudget/) | **予算と失敗処理。** 確保に失敗しても*フォールバックしない*ことの確認 | `setMemoryLimit` / `begin` / `tileCount` |
| 4 | [Viewport](Viewport/) | **部分更新。** 200×150 の領域だけを毎フレーム更新する | `LGFXVirtualSprite`（ローカル座標） |
| 5 | [DiffTransfer](DiffTransfer/) | **変わっていないタイルを送らない。** 効果を数値で出す | `setDiffMode` / `diffPushedPixels` / `invalidate` |
| 6 | [Dialog](Dialog/) | **透過オーバーレイ。** 下の画面を描き直さずにダイアログを重ねる | `renderTransparent` / `setTransparentColor` |
| 7 | [ColumnSplit](ColumnSplit/) | **列分割と PSRAM タイル。** 解決結果をシリアルに出力して比較する | `setSplitAxis` / `setTileWidth` / `setUsePsram` / `tileIsPsram` |

`HelloWorld` と `LovyanGFX_Basic` 以外は M5Unified（`M5.Display`）向けですが、
ライブラリ側は素の LovyanGFX でも同じです。パネルの作り方が違うだけで、
描画関数は `LGFXVirtualCanvas` しか見ていないので**そのまま使い回せます**。

## つまずきやすい 3 点

**1. 描画コールバックはタイル数ぶん呼ばれる。**
1 フレームで、320×240 の既定なら 8 回走ります。絵はクリップで正しくなりますが、
**中でやった計算は 8 回**です。`millis()` / `analogRead()` / 乱数 / 通信を
コールバックに置くと、帯ごとに違う値で描かれて絵が食い違います。
状態は `loop()` で進め、コールバックは**読むだけ**にしてください
（`BouncingBall` と `DiffTransfer` がその形です）。

**2. `drawPng` / `drawJpg` はタイルごとにフルデコードされる。**
クリップで捨てられるのは出力画素だけで、デコードの手間は減りません。
setup で 1 回だけ sprite に展開し、コールバックでは `pushImage` してください
（`pushImage` は正しくクリップされます）。

**3. 確保の失敗は黙って直されない。**
`begin()` は `false` を返し、未確保のまま `render()` を呼ぶと**何も描かず** `false` を返します。
分割数を勝手に増やすようなフォールバックは意図的にありません（`MemoryBudget` を参照）。

いずれも [docs/BEGINNERS_GUIDE.ja.md](../docs/BEGINNERS_GUIDE.ja.md) で原理から説明しています。

## ビルド

各ディレクトリが自己完結した `sketch.yaml` を持ち、プラットフォームとライブラリの
バージョンを固定するので、ボードマネージャの設定は要りません。

```sh
arduino-cli compile --profile m5stack_core2 examples/HelloWorld
arduino-cli upload -p /dev/ttyUSB0 --profile m5stack_core2 examples/HelloWorld
```

Arduino IDE の場合は `.ino` を開き、ボードを選んでそのまま書き込んでください。
`ColumnSplit` / `Dialog` / `DiffTransfer` / `MemoryBudget` は**シリアル（115200）に
結果を出力**するので、シリアルモニタを開いて見るのが前提です。

別のボードで動かすときは `sketch.yaml` の `fqbn` を差し替えます。
どのスケッチも画面サイズを決め打ちしておらず、`g.width()` / `g.height()` から
レイアウトを作っています。

## 実測値が見たい

分割数・ダブルバッファ・PSRAM が実機でどれだけ効くかは
[bench/](../bench/) が測ります（Core2 / CoreS3、シリアルに表として出力）。
分割数に依らず絵が一致することの検証は [tests/](../tests/) にあります（ホスト上でヘッドレス実行）。
