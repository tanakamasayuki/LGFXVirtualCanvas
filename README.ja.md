# LGFXVirtualCanvas

> English: [README.md](README.md)

LovyanGFX / M5GFX で、全画面ぶんのダブルバッファが RAM に収まらなくても、
**仮想的な全画面 Canvas** に描いているように扱える分割描画ライブラリ。

画面を縦方向のタイルに分割し、各タイルを小さな再利用 sprite に描画します。
あなたの描画関数は**全画面（仮想）座標**でタイルごとに1回ずつ実行され、
ライブラリが Y オフセット・クリッピング・転送を隠します。タイルは見えません。

```cpp
void drawScene(LGFXVirtualCanvas& g) {
    g.fillScreen(TFT_BLACK);
    g.fillCircle(g.width() / 2, g.height() / 2, 40, TFT_YELLOW);  // 全画面座標で描くだけ
}
```

> **はじめての人へ**: なぜちらつくのか・なぜ全画面バッファが載らないのか・
> タイル分割で何が変わるのかを原理から説明した
> [docs/BEGINNERS_GUIDE.ja.md](docs/BEGINNERS_GUIDE.ja.md) があります。
>
> 設計の根拠と完全な仕様は [SPEC.ja.md](SPEC.ja.md) を参照。

## なぜ

全画面ダブルバッファ（例 320×240×2 = 150 KB）は RAM に収まらないことが多い。
ありがちな回避策——画面を帯に分けてオフセットを自分で管理——は、すべての描画
呼び出しにタイル計算が混ざって煩雑になります。LGFXVirtualCanvas は分割を肩代わり
するので、描画関数は全画面座標で1つ書くだけで、1タイルでも7タイルでも正しく
描画されます。

## 必要環境

- **LovyanGFX** / **M5GFX** / **M5Unified** のいずれかが使える ESP32（や他ボード）。
- `LGFXVirtualCanvas.h` より**前に**グラフィックスライブラリを include すること。

## インストール

**Arduino IDE** — ライブラリマネージャで **LGFXVirtualCanvas** を検索 → インストール。
グラフィックスライブラリ（LovyanGFX / M5GFX / M5Unified のいずれか）も同様に導入。

**PlatformIO** — `platformio.ini` に：

```ini
lib_deps =
    https://github.com/tanakamasayuki/LGFXVirtualCanvas
    lovyan03/LovyanGFX     ; または m5stack/M5GFX, m5stack/M5Unified
```

**手動** — リリースの `.zip` をダウンロードし、Arduino の `libraries/` フォルダへ展開。

## クイックスタート

```cpp
#include <M5Unified.h>
#include <LGFXVirtualCanvas.h>

LGFXVirtualScreen screen(M5.Display);   // 分割数省略 = auto（≈ 19 KB/タイル予算）

void drawScene(LGFXVirtualCanvas& g) {
    g.fillScreen(TFT_NAVY);
    g.setTextColor(TFT_WHITE);
    g.drawCentreString("Hello, tiled world!", g.width() / 2, g.height() / 2);
}

void setup() {
    M5.begin();
    screen.render(drawScene);   // 初回 render でタイルバッファを確保
}

void loop() {}
```

素の LovyanGFX も同じ——`LGFX` パネルを生成して渡すだけ：

```cpp
#include <LovyanGFX.hpp>
#include <LGFX_AUTODETECT.hpp>
#include <LGFXVirtualCanvas.h>

static LGFX lcd;
LGFXVirtualScreen screen(lcd);
// ... lcd.init(); screen.render(drawScene);
```

## アプリ状態を渡す

状態を参照で渡すと、ライブラリが描画関数へそのまま転送します（`void*` 不要・
グローバル不要）：

```cpp
struct AppState { int score, playerX, playerY; };
AppState state;

void drawScene(LGFXVirtualCanvas& g, AppState& s) {
    g.fillScreen(TFT_BLACK);
    g.setCursor(10, 10);
    g.printf("Score %d", s.score);
    g.fillRect(s.playerX, s.playerY, 16, 16, TFT_GREEN);
}

void loop() {
    updateState(state);
    screen.render(drawScene, state);
}
```

View（`drawScene`）を Model（`AppState`）や更新処理から分離するのが推奨です。
同じ描画関数を通常描画・分割描画・ヘッドレステストで使い回せます。

## メモリの制御

既定では **1タイル ≈ 19 KB** を目標にするので、分割数は面サイズに応じて決まります
（小さなスプライトは1タイル、全画面は数タイル）。さらに **2タイル以上になる面では
自動でダブルバッファを有効化**します（1タイルなら重ねる相手が無いので単一のまま）。
普通は気にするのはタイル数より RAM なので、自分で予算を指定して上書きもできます：

```cpp
void setup() {
    M5.begin();
    screen.setMemoryLimit(20 * 1024);   // タイルバッファを最大 ~20 KB に
    if (!screen.begin()) {              // 任意：ここで確保して確認
        Serial.println("alloc failed"); // フォールバックせず失敗を返す
    }
}
```

確保は**遅延**です。`begin()` か初回 `render()` まで確保しません（`lcd.init()` /
`M5.begin()` 前は画面サイズが不明なため）。要求を満たせない場合は**フォールバック
せず失敗**し、`render()` は何も描かず `false` を返します。

> ⚠️ **画像のデコードはタイルごとでなく一度だけ。** draw コールバックはタイルごとに
> 再実行されるので、タイルのクリップで縮まない処理 ── PNG/JPEG のデコード、
> フラッシュ/SD からの読み込み、PSRAM 上のソース画素のサンプリング ── は*毎タイル*
> 丸ごと payされ、分割数に応じて ≈ N× に増えます。デコード/読み込みは（setup 時か
> 変化時に）**一度だけ**内蔵RAMの sprite に展開し、コールバックではそこから
> `pushImage`/`pushSprite` してください（これはタイルごとにクリップされます）。
> 極端に重いコールバックは少なく大きいタイルに（`setMemoryLimit` / `setSplitCount`）。
> SPEC §10.7 参照。

### 行ではなく列に分割する

既定では面を横帯に切って上から下へ転送します。長尺印刷・ティッカー・横長波形など、
**左から右へ**送り出したい対象では、面いっぱいの高さの**縦帯**に切り替えられます：

```cpp
screen.setSplitAxis(LGFXVirtualSplitAxis::Columns);  // 縦帯、左から右へ転送
screen.setTileWidth(32);                             // 任意：列幅を固定
```

それ以外は何も変わりません。draw コールバックは従来どおり面全体の座標で書け、
出力画像も行分割と同一です（parity テストがピクセル単位で検証しています）。
影響を受けるのはテキストの折り返しだけです（[制限](#制限) 参照）。

### タイルバッファを PSRAM に置く

大きなパネルでは内蔵RAM既定だとタイル数が非常に多くなり、そのすべてで draw
コールバックが再実行されます。`setUsePsram(true)` を使うと、遅いメモリと引き換えに
「少なく大きいタイル」を選べます：

```cpp
screen.setUsePsram(true);
screen.setMemoryLimit(2 * 1024 * 1024);   // 数百タイルではなく数タイルに
screen.begin();
if (!screen.tileIsPsram()) {              // PSRAM が無い/足りない → 内蔵RAMに確保された
    Serial.println("fell back to internal RAM");
}
```

これは速度とのトレードであり、無条件に速くなるわけではありません。PSRAM への描画も
読み出しも遅く、LovyanGFX は PSRAM 上の sprite を DMA 無しで push するため、
ダブルバッファは重ねる相手が無くなり auto では OFF に解決されます。コールバックが
律速なら勝ち、転送が律速なら負ける ── 実測してください。PSRAM 要求で確保が失敗する
ことはありません（内蔵RAMに確保され、`tileIsPsram()` がそれを報告します）。
SPEC §10.9 参照。

## `LGFXVirtualSprite` で部分更新

画面の一部だけ更新したいとき（ステータス領域・動くアイコン・固定ビューポート）は
`LGFXVirtualSprite` を使います。任意サイズのサブ領域で、普通のスプライトのように
使えますが内部はタイル分割なので小バッファで済みます。描画はそのスプライトの
**ローカル座標**（0,0 = 左上）。ライブラリがタイル分割・クリップ・パネルへの転送を
行います。

```cpp
LGFXVirtualSprite view(lcd, 200, 150, 20, 60);  // (20,60) に置く 200x150
view.setMemoryLimit(12 * 1024);
view.begin();

void drawView(LGFXVirtualCanvas& g) {            // ローカル座標 0..200, 0..150
    g.fillScreen(TFT_BLACK);
    g.fillCircle(100, 75, 30, TFT_CYAN);
}

view.render(drawView);            // その領域だけ更新（画面の他は触らない）
view.render(drawIcon, x, y);      // 位置を変えて描画（現在位置も更新）
```

転送・最終端数タイル・画面端のはみ出しは全部ライブラリ側。`LGFXVirtualScreen` は
「画面全体」という特殊ケースで、両者は同じタイル分割エンジンを共有します。

## 差分転送で転送量を減らす

前フレームから変化していないタイルの転送を省略できます。大きくて転送の遅いパネル
（フルHD の USB ディスプレイなど）向けの機能です。**既定は無効**で、有効にしない
限りメモリも CPU も一切使いません。

```cpp
screen.setDiffMode(LGFXVirtualDiffMode::Tile);
```

各タイルは draw 後にハッシュ（1タイル 8 バイト）を取り、前回と一致したら転送を
飛ばします。**出力画素は一切変わりません** ── 転送だけの最適化です。

減るのは**転送だけ**です。描画コールバックは従来どおり全タイルで走り、さらに
ハッシュ計算（タイルバッファ1回の線形走査）が増えます。描画律速の構成では効果が
出ないどころか少し遅くなるので、効いているかは実測してください：

```cpp
screen.render(drawScene);
Serial.printf("%u / %u px 転送\n",
              (unsigned)screen.diffPushedPixels(),
              (unsigned)screen.diffTotalPixels());
```

> ⚠️ **他が画面に触ったら `invalidate()` を呼ぶ。** 転送を省略できるのは「送らな
> かった場所にはまだ前回の絵が残っている」前提が成り立つ間だけです。再確保・設定
> 変更・スプライトの移動・パネルの回転/サイズ/色深度の変化は自動で検知しますが、
> それ以外は検知できません。`lcd.fillRect()` などで直接描いた／別の面を重ねた／
> パネルをスリープ・リセットした／USB ディスプレイが再接続した ── こうしたときは
> `screen.invalidate()` を呼んでください（次の `render()` が全タイルを転送します）。

粒度はタイル単位のみです。タイル高はメモリ予算で決まるため大きなパネルでは自動的に
細かくなります（フルHD / 16bpp・既定予算ならタイル高 5 行＝画面の 1/216）。
詳細は [SPEC.ja.md §21](SPEC.ja.md) を参照。

## 透過転送でオーバーレイ／ダイアログを出す

残しておきたい画面の上にダイアログを出すときは、**オーバーレイ**として描く。
`renderTransparent()` はタイルを透過色で塗り、その上に描いた画素だけを転送するので、
パネルの残りの部分はそのまま残る。

```cpp
LGFXVirtualScreen base(lcd);      // 下になる画面
LGFXVirtualScreen overlay(lcd);   // ダイアログのレイヤ

void drawDialog(LGFXVirtualCanvas& g) {   // fillScreen しない：描かない部分は透ける
    g.fillRoundRect(40, 70, 160, 90, 12, TFT_DARKGREY);
    g.drawRoundRect(40, 70, 160, 90, 12, TFT_WHITE);
    g.drawString("Are you sure?", 52, 84);
}

base.render(drawScene);                 // 画面はパネルに残したまま…
overlay.renderTransparent(drawDialog);  // …ダイアログだけを送る
```

角丸の角には下の画面が出たままになる。それがこの機能の目的である。
**オーバーレイが単なる矩形なら、この機能は不要**：ダイアログ位置に置いた
`LGFXVirtualSprite` なら、その矩形だけを転送できるし、そのほうが速い。
形のあるダイアログでの最良の組み合わせは両方＝ダイアログの外接矩形に置いた
スプライトを透過 push することである。

```cpp
LGFXVirtualSprite dialog(lcd, 160, 90, 40, 70);
dialog.renderTransparent(drawDialog);   // ローカル座標。この矩形だけを走査する
```

`render(...)` の各形には、同じ引数の `renderTransparent(...)` が対応して存在する。
通常の `render()` の挙動は一切変わらない。

透過色の既定値は `TFT_TRANSPARENT`（RGB565 `0x0120`）。描画が本当にその色を
使ってしまう場合だけ変更する。

```cpp
overlay.setTransparentColor(lcd.color565(1, 2, 3));
```

> ⚠️ **色は C++ の型に従って解釈される**（LovyanGFX の他の場所とまったく同じ）。
> `int` / `uint16_t`（`TFT_*` 定数、`color565()` の戻り値）は RGB565、
> `uint32_t` は RGB888 である。したがって
> `setTransparentColor(TFT_TRANSPARENT)` と `setTransparentColor((uint32_t)0x002400)`
> は同じ色を意味するが、`setTransparentColor((uint32_t)0x0120)` は違う色になる。

コスト：マスク付き転送は 1 走査線ずつ処理し、見える画素の run ごとにウィンドウ設定を
発行するので、**画素あたり**では `render()` より遅い。得をするのは送る画素数が大幅に
減るからである（全透過の走査線はバス転送ゼロ）。描画コールバックは全タイルで走る。
差分転送と併用する場合は規則が 1 つ増える：オーバーレイの**下**にあるものを何かが
描き直したら `overlay.invalidate()` を呼ぶこと。[SPEC.ja.md §22](SPEC.ja.md) 参照。

## API

### `LGFXVirtualScreen` — マネージャ

| メンバ | 説明 |
|---|---|
| `LGFXVirtualScreen(LovyanGFX& panel, int splitCount = 0)` | パネル上に構築。`0` = auto（≈ 19 KB/タイル予算）。この時点では未確保。 |
| `void setMemoryLimit(size_t bytes)` | タイルバッファの上限。タイル高をここから算出（最優先）。 |
| `void setSplitCount(int count)` | タイル数を固定。 |
| `void setTileHeight(int height)` | タイル高（px）を固定（行分割）。 |
| `void setTileWidth(int width)` | タイル幅（px）を固定（列分割）。`setTileHeight` と同一の内部設定。 |
| `void setSplitAxis(LGFXVirtualSplitAxis axis)` | `Rows`（既定、横帯を上から下へ）／`Columns`（面いっぱいの高さの縦帯を左から右へ）。SPEC §10.8 参照。 |
| `LGFXVirtualSplitAxis splitAxis() const` | 現在の分割軸。 |
| `void setUsePsram(bool enable)` | タイルバッファを PSRAM に確保（既定 OFF）。メモリが遅く DMA も効かない（auto のダブルバッファは OFF に）。確保できなければ内蔵RAMにフォールバック。SPEC §10.9 参照。 |
| `bool usePsram() const` / `bool tileIsPsram() const` | 要求した値 ／ 実際に確保された場所。 |
| `void setBackgroundColor(color)` | auto-clear の色（既定 黒）。値の C++ 型に従って解釈される（`TFT_*` / `int` は RGB565、`uint32_t` は RGB888）。`renderTransparent*()` では使われない。 |
| `void setAutoClear(bool enable)` | draw 前に各タイルをクリア（既定 `true`）。 |
| `void setDoubleBuffer(bool enable)` | タイルバッファを2枚使い、あるタイルの DMA 転送と次タイルの描画を重ねる（高速、タイルRAM 2倍）。既定の **auto**（2タイル以上で ON、1タイルで OFF）を上書きする。SPEC §10.5 参照。 |
| `void setTransparentColor(color)` | `renderTransparent*()` が転送から除外する色（既定 `TFT_TRANSPARENT` = RGB565 `0x0120`）。値の C++ 型に従って解釈される。SPEC §22.4 参照。 |
| `uint32_t transparentColor() const` | 現在の透過色（RGB888 で返る）。 |
| `void setDiffMode(LGFXVirtualDiffMode mode)` | 差分転送の粒度。`Off`（既定）／`Tile`（前回と変化していないタイルの転送を省略）。転送のみ削減。SPEC §21 参照。 |
| `LGFXVirtualDiffMode diffMode() const` | 現在の差分転送モード。 |
| `void invalidate()` | パネルの内容が信用できなくなったことを通知（次の `render()` が全タイル転送）。自分以外が画面に触ったら呼ぶ。 |
| `size_t diffMemoryUsage() const` | ハッシュ表のバイト数（`Off` なら 0）。 |
| `uint32_t diffPushedPixels() const` / `uint32_t diffTotalPixels() const` | 直前の `render()` が実際に転送した画素数 ／ 差分なしなら転送していた画素数。 |
| `bool begin()` | 今すぐ確保。失敗で `false`（フォールバック無し）。 |
| `bool isReady() const` | 確保済みか。 |
| `int tileCount() const` / `int tileHeight() const` / `int tileWidth() const` / `int tileSpan() const` | 確保後の確定ジオメトリ（`tileSpan` は分割軸方向の長さ）。 |
| `bool render(draw)` | `void draw(LGFXVirtualCanvas&)` を描画。 |
| `bool render(draw, ctx)` | `void draw(LGFXVirtualCanvas&, T&)` を `ctx` 付きで描画。 |
| `bool renderTransparent(draw)` / `renderTransparent(draw, ctx)` | 同じものをオーバーレイとして描画。タイルは `transparentColor()` で塗られ、その色の画素は転送されない。SPEC §22 参照。 |

`render` はバッファ未確保なら `false`（描画なし）。描画コールバックは
**関数ポインタ**のみ（コードサイズ抑制のため、キャプチャ付きラムダ・
`std::function` は不可）。

複数指定時の優先順位：`setMemoryLimit` ＞ `setSplitCount` ＞
`setTileHeight` / `setTileWidth` ＞ 既定（≈ 19 KB/タイル予算）。何も指定しない場合、面が 2 タイル以上に解決される
ときはダブルバッファを自動で有効化します。

### `LGFXVirtualSprite` — タイル分割サブ領域

`LGFXVirtualScreen` と同じ設定・`render(...)` に加えて：

| メンバ | 説明 |
|---|---|
| `LGFXVirtualSprite(LovyanGFX& panel, int w, int h, int x = 0, int y = 0)` | パネル位置 `(x,y)` の `w × h` タイル分割スプライト。サイズ固定・未確保。 |
| `void setPosition(int x, int y)` | 位置変更（再確保なし）。 |
| `int x()` / `int y()` / `int width()` / `int height()` | 現在位置／サイズ。 |
| `bool render(draw)` / `render(draw, x, y)` | 現在位置／指定位置に描画。`(x,y)` 指定時は現在位置も更新。 |
| `bool render(draw, ctx)` / `render(draw, ctx, x, y)` | 型付き ctx 版。 |
| `bool renderTransparent(draw)` / `(draw, x, y)` / `(draw, ctx)` / `(draw, ctx, x, y)` | 各 `render` 形のオーバーレイ版。`transparentColor()` の画素は転送されない。SPEC §22 参照。 |

描画コールバック内の座標は**スプライトのローカル**（0,0 = 左上）、`g.width()/g.height()`
はスプライトのサイズを返します。

### `LGFXVirtualCanvas` — 描画面

描画関数に渡される。通常の LGFX/M5GFX Canvas に見えますが、全画面（仮想）座標を
現在のタイルへマップします。

現在のリリースでは、タイル化された仮想 surface から安全に提供できる
LovyanGFX/M5GFX 互換 API を一通りラップしています。対象は、座標付き描画、
テキスト、画像 decode / push helper、色・状態ユーティリティ、現在 tile からの
読み戻しです。stream cursor、呼び出し側管理の window、tile をまたぐ既存ピクセル
移動、sprite 自体の転送に依存する API は意図的に外しており、後述の
「採用しない関数群」にまとめています。

対応：

- ジオメトリ：`width()`, `height()`（仮想全画面）
- 色ユーティリティ：`color332`, `color565`, `color888`, `swap565`,
  `swap888`, `color16to8`, `color8to16`, `color16to24`, `color24to16`
- 状態：`setColor`, `setRawColor`, `getRawColor`, `setBaseColor`,
  `getBaseColor`, `getColorDepth`, `hasPalette`, `getPaletteCount`,
  `getPalette`, `setPaletteColor`, `setPivot`, `getPivotX/Y`, `createGradient`,
  `mapGradient`
- 図形：`fillScreen`, `drawPixel`, `drawLine`, `drawFastHLine`,
  `drawFastVLine`, `writePixel`, `writeFastHLine`, `writeFastVLine`,
  `fillRect`, `writeFillRect`, `writeFillRectPreclipped`, `drawRect`,
  `fillRoundRect`,
  `drawRoundRect`, `drawCircle`, `fillCircle`, `drawEllipse`,
  `fillEllipse`, `drawTriangle`, `fillTriangle`, `drawBezier`,
  `drawEllipseArc`, `fillEllipseArc`, `drawArc`, `fillArc`, `clear`,
  `clearDisplay`, `drawCircleHelper`, `fillCircleHelper`,
  `drawGradientHLine`, `drawGradientVLine`, `drawGradientLine`,
  `fillGradientRect`, `fillRectAlpha`, `drawSmoothLine`, `drawWideLine`,
  `drawWedgeLine`, `drawSpot`, `drawGradientSpot`, `fillSmoothRoundRect`,
  `fillSmoothCircle`
- 画像：`pushImage`, `pushImageDMA`, `pushImageRotateZoom`,
  `pushImageRotateZoomWithAA`, `pushGrayscaleImage`,
  `pushGrayscaleImageRotateZoom`, `pushAlphaImage`, `drawBitmap`,
  `drawXBitmap`, `setSwapBytes`, `getSwapBytes`, `drawBmp`, `drawBmpFile`,
  `drawJpg`, `drawJpgFile`, `drawPng`, `drawPngFile`, `drawQoi`,
  `drawQoiFile`, `qrcode`
- 読み戻し：`readPixel`, `readPixelRGB`, `readPixelValue`, `readRectRGB`,
  `readRect`
- テキスト：`setCursor`, `getCursorX/Y`, `setTextColor`, `setTextSize`,
  `getTextSizeX/Y`, `setTextDatum`, `getTextDatum`, `setTextPadding`,
  `getTextPadding`, `setTextWrap`, `setTextScroll`, `setEmojiCallback`,
  `getEmojiCallback`, `setTextStyle`, `getTextStyle`, `setFont`,
  `setTextFont`, `setFreeFont`, `getFont`, `getTextFont`, `fontHeight`,
  `fontWidth`, `textWidth`, `textLength`, `drawString`, `drawCentreString`,
  `drawCenterString`, `drawRightString`, `drawNumber`, `drawFloat`,
  `drawChar`, `print`, `println`, `write`, `printf`, `vprintf`

未ラップのメソッド呼び出しはコンパイルエラー（仕様）。未対応または安全に扱えない
描画経路は、黙って通らず、はっきり失敗します。

採用しない関数群：

- 低レベルのストリーミング描画：`writeColor`, `pushBlock`,
  `writePixels`, `writePixelsDMA`, `pushPixels`, `pushPixelsDMA`,
  `pushColor`, `pushColors`。
  これらは呼び出し側が管理する write window / stream cursor に依存し、
  安全なタイル単位クリップに必要な仮想座標情報を十分に持ちません。
- ウィンドウ・クリップ・トランザクション制御：`setWindow`,
  `startWrite`, `endWrite`, `beginTransaction`, `endTransaction`, `initDMA`,
  `waitDMA`。タイル描画中は `LGFXVirtualScreen` / `LGFXVirtualSprite` が
  これらを管理します。コールバック内の Canvas に公開すると、管理側の
  クリップや DMA 順序保証を壊せてしまいます。
- スクロール・コピー：`scroll`, `copyRect`, scroll-rect 系 API。
  既存ピクセルを surface 内で移動・コピーする API ですが、1つの tile には
  仮想 surface の一部の帯しか存在しないため、tile をまたぐ元/先ピクセルを
  参照できません。
- Sprite 転送ヘルパ：`pushSprite`, `pushRotated`, `pushRotatedWithAA`,
  `pushRotateZoom`, `pushRotateZoomWithAA`, `pushAffine`, `pushAffineWithAA`。
  これらは `LGFX_Sprite` 自体を別 destination へ転送するためのメソッドです。
  `LGFXVirtualCanvas` はすでに描画先 surface であり、tile の flush は管理側が
  行います。
- affine 画像ヘルパ：`pushImageAffine`, `pushImageAffineWithAA`,
  `pushGrayscaleImageAffine`。affine 行列には destination 座標が埋め込まれるため、
  単純な `y -= offsetY` ラップではすべての行列に対して安全に扱えません。
- 出力・エクスポート：`createPng`, `releasePngMemory`。これらは現在の tile
  バッファに対する操作で、仮想 surface 全体の出力ではありません。必要なら将来、
  管理側 API として追加する対象です。

### 取り込み済みの検出

`LGFXVirtualCanvas.h` を include すると `LGFXVIRTUALCANVAS_H` が定義されるので、
他のコードやライブラリから存在を検出して任意に連携できます：

```cpp
#if defined(LGFXVIRTUALCANVAS_H)
    // LGFXVirtualCanvas が利用可能（LGFXVIRTUALCANVAS_VERSION_STR 等も使える）
#endif
```

ヘッダは `LGFXVIRTUALCANVAS_VERSION_MAJOR` / `_MINOR` / `_PATCH` / `_STR` も
取り込むので、バージョン判定にも使えます。

## しくみ

LovyanGFX には描画原点の平行移動が無く、プリミティブが非 virtual なので、
オフセットを生の `LovyanGFX&` の裏に隠せません。そこで LGFXVirtualCanvas は、
各メソッドが `y -= offsetY` してタイル sprite へ転送する小さな具象クラスです。
タイル外描画は sprite の clip で消えます。全画面描画は1タイルの特殊ケースなので、
同じ経路で正当性が証明できます。根拠は [SPEC.ja.md §6](SPEC.ja.md)。

## 制限

- **分割は1次元のみ**：行（既定）または列であり、2次元グリッドではありません。
- 描画コールバックは**関数ポインタのみ**（キャプチャ付きラムダ不可）。
- **列分割ではテキストの折り返しが無効になります。** LovyanGFX はタイル sprite 自身の
  右端で折り返すため、列分割ではタイル境界で折り返してしまい出力が分割数に依存します。
  そのため列分割では `setTextWrap(true)` を無視し、長い行は折り返さずクリップします。
  改行は面の左端に戻すよう補正しているので、`println` / `printf` は期待どおり動きます。
- **タイル境界を跨ぐ近傍依存描画**（アンチエイリアス・smooth/wide line・
  ぼかし・近傍参照フィルタ）は全面描画と一致しない場合があります。
  各タイルが独立に再描画・クリップされるため。LovyanGFX の既定プリミティブは
  AA 無しなので通常は問題になりません。
- 画像デコーダ（`drawBmp` / `drawJpg` / `drawPng` / `drawQoi` と `*File`）は
  カバレッジ優先でラップしていますが、タイルごとに実行されます。性能重視なら
  一度だけ sprite / 画像バッファへデコードし、可能なら `pushImage` を使ってください。
- バッファ高に依存するテキスト挙動（自動スクロール・下端折り返し）は保証外。
  基本の cursor/print/drawString は対応。
- retained mode / 描画命令の記録は無し：毎フレーム全体を描き直す前提です。
  差分転送（既定 OFF）はタイル単位で**転送だけ**を削減するもので、描画は減りません。

## サンプル

[examples/](examples/) を参照：`HelloWorld`, `BouncingBall`（状態＋アニメ）,
`MemoryBudget`（予算＋失敗処理）, `Viewport`（`LGFXVirtualSprite` 部分更新）,
`DiffTransfer`（差分転送）, `ColumnSplit`（列分割＋PSRAM バッファ）,
`Dialog`（透過オーバーレイ）, `LovyanGFX_Basic`。

読む順番・各例の論点・ビルド方法は [examples/README.ja.md](examples/README.ja.md) に
まとめてあります。

## テスト

正当性は host 上で**ヘッドレス**検証：同じシーンを複数の分割数で描いた結果が、
1タイル（全面）描画と pixel 完全一致すること。[tests/README.ja.md](tests/README.ja.md)
と [SPEC.ja.md §13](SPEC.ja.md) を参照。

## ライセンス

[MIT](LICENSE) © TANAKA Masayuki
