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

LGFXVirtualScreen screen(M5.Display);   // 分割数省略 = auto（3）

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

## API

### `LGFXVirtualScreen` — マネージャ

| メンバ | 説明 |
|---|---|
| `LGFXVirtualScreen(LovyanGFX& panel, int splitCount = 0)` | パネル上に構築。`0` = auto（≈ 19 KB/タイル予算）。この時点では未確保。 |
| `void setMemoryLimit(size_t bytes)` | タイルバッファの上限。タイル高をここから算出（最優先）。 |
| `void setSplitCount(int count)` | タイル数を固定。 |
| `void setTileHeight(int height)` | タイル高（px）を固定。 |
| `void setBackgroundColor(uint32_t color)` | auto-clear の色（既定 黒）。 |
| `void setAutoClear(bool enable)` | draw 前に各タイルをクリア（既定 `true`）。 |
| `void setDoubleBuffer(bool enable)` | タイルバッファを2枚使い、あるタイルの DMA 転送と次タイルの描画を重ねる（高速、タイルRAM 2倍）。既定の **auto**（2タイル以上で ON、1タイルで OFF）を上書きする。SPEC §10.5 参照。 |
| `bool begin()` | 今すぐ確保。失敗で `false`（フォールバック無し）。 |
| `bool isReady() const` | 確保済みか。 |
| `int tileCount() const` / `int tileHeight() const` | 確保後の確定ジオメトリ。 |
| `bool render(draw)` | `void draw(LGFXVirtualCanvas&)` を描画。 |
| `bool render(draw, ctx)` | `void draw(LGFXVirtualCanvas&, T&)` を `ctx` 付きで描画。 |

`render` はバッファ未確保なら `false`（描画なし）。描画コールバックは
**関数ポインタ**のみ（コードサイズ抑制のため、キャプチャ付きラムダ・
`std::function` は不可）。

複数指定時の優先順位：`setMemoryLimit` ＞ `setSplitCount` ＞ `setTileHeight`
＞ 既定（≈ 19 KB/タイル予算）。何も指定しない場合、面が 2 タイル以上に解決される
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

描画コールバック内の座標は**スプライトのローカル**（0,0 = 左上）、`g.width()/g.height()`
はスプライトのサイズを返します。

### `LGFXVirtualCanvas` — 描画面

描画関数に渡される。通常の LGFX/M5GFX Canvas に見えますが、全画面（仮想）座標を
現在のタイルへマップします。対応：

- ジオメトリ：`width()`, `height()`（仮想全画面）
- 図形：`fillScreen`, `drawPixel`, `drawLine`, `drawFastHLine`,
  `drawFastVLine`, `fillRect`, `drawRect`, `drawCircle`, `fillCircle`
- 画像：`pushImage`
- テキスト：`setCursor`, `getCursorX/Y`, `setTextColor`, `setTextSize`,
  `setTextDatum`, `drawString`, `drawCentreString`, `drawRightString`,
  `print`, `println`, `printf`

未ラップのメソッド呼び出しはコンパイルエラー（仕様）。未対応の描画は黙って
通らず、はっきり失敗します。

## しくみ

LovyanGFX には描画原点の平行移動が無く、プリミティブが非 virtual なので、
オフセットを生の `LovyanGFX&` の裏に隠せません。そこで LGFXVirtualCanvas は、
各メソッドが `y -= offsetY` してタイル sprite へ転送する小さな具象クラスです。
タイル外描画は sprite の clip で消えます。全画面描画は1タイルの特殊ケースなので、
同じ経路で正当性が証明できます。根拠は [SPEC.ja.md §6](SPEC.ja.md)。

## 制限

- **縦分割のみ。**
- 描画コールバックは**関数ポインタのみ**（キャプチャ付きラムダ不可）。
- **タイル境界を跨ぐ近傍依存描画**（アンチエイリアス・ぼかし・近傍参照フィルタ）は
  全面描画と一致しない場合があります。各タイルが独立に再描画・クリップされるため。
  LovyanGFX の既定プリミティブは AA 無しなので通常は問題になりません。
- バッファ高に依存するテキスト挙動（自動スクロール・下端折り返し）は保証外。
  基本の cursor/print/drawString は対応。
- 部分更新 / retained mode は無し：毎フレーム全体を描き直す前提。

## サンプル

[examples/](examples/) を参照：`HelloWorld`, `BouncingBall`（状態＋アニメ）,
`MemoryBudget`（予算＋失敗処理）, `Viewport`（`LGFXVirtualSprite` 部分更新）,
`LovyanGFX_Basic`。

## テスト

正当性は host 上で**ヘッドレス**検証：同じシーンを複数の分割数で描いた結果が、
1タイル（全面）描画と pixel 完全一致すること。[tests/README.ja.md](tests/README.ja.md)
と [SPEC.ja.md §13](SPEC.ja.md) を参照。

## ライセンス

[MIT](LICENSE) © TANAKA Masayuki
