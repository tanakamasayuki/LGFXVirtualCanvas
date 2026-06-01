# LGFXVirtualCanvas 要件定義

> English: [SPEC.md](SPEC.md)

## 0. 決定事項サマリ

これまでの検討で確定した主要事項：

- **クラス構成は2つ（具象クラスのみ。抽象基底・virtual・テンプレートは使わない）**
  - `LGFXVirtualScreen` … マネージャ（装置側）。実パネルを保持し、分割と `render()` を担う。
  - `LGFXVirtualCanvas` … 描画面（描く側）。ユーザの draw 関数が受け取る仮想キャンバス。ライブラリ名／インクルード名と一致。
- **描画関数のシグネチャ**は `void draw(LGFXVirtualCanvas& g)` または `void draw(LGFXVirtualCanvas& g, T& ctx)`。関数ポインタのみ受ける。
- **パネルは `LovyanGFX&`（共通底辺クラス参照）**で受ける。LGFX でも M5GFX/M5.Display でも渡せる。
- **メモリは遅延確保**。コンストラクタでは確保しない（`lcd.init()` 前は画面サイズ不明のため）。`begin()` で前倒し確保でき、未確保のまま `render()` が来たらその場で自動確保（ガードレール）。
- **確保失敗はフォールバックせず失敗として返す**。`begin()` / `render()` は `bool` を返し、未確保時 `render()` は何も描かない（壊れたと分かるようにする）。
- **分割の指定は「メモリ上限」が主役**。`setMemoryLimit(bytes)`（バイト単位・既定 0＝指定なし）を最優先、次に分割数、未指定ならデフォルト3分割。
- **クリッピングは sprite 標準の per-pixel clip で自動的に安全**（範囲外描画は消えるだけ）。
- **各タイルは draw 前に背景色でクリアする（auto-clear、既定 ON）**。背景色は設定可（既定: 黒）。未描画画素は背景色で決定的になり、分割数に依らず結果が一致する。`setAutoClear(false)` で無効化できる。
- **マネージャは2種**：`LGFXVirtualScreen`（全画面）と `LGFXVirtualSprite`（任意位置・サイズのサブ領域＝内部分割スプライト。ローカル座標）。両者は内部のタイル分割エンジンを共有し、draw に同じ `LGFXVirtualCanvas` を渡す。違いは転送先（全画面か、置いた矩形か）だけ。詳細は §7.1。

これらの根拠（特になぜ専用具象クラスにするか）は §6 を参照。

## 1. ライブラリ名

`LGFXVirtualCanvas`（インクルードは `#include <LGFXVirtualCanvas.h>`。ヘッダ1枚で `LGFXVirtualScreen` / `LGFXVirtualSprite` / `LGFXVirtualCanvas` を宣言する）

## 2. 目的

LovyanGFX / M5GFX 環境で、全画面分のダブルバッファを確保できない場合でも、ユーザーからは仮想的な全画面 Canvas に描画しているように扱える分割描画ライブラリを提供する。

内部では画面を複数の縦方向タイルに分割し、小さな `M5Canvas` / `LGFX_Sprite` 相当のバッファに描画して順次表示へ転送する。

ユーザーは分割数、オフセット、タイル境界を意識せずに描画できることを目標とする。

## 3. 基本方針

- 全画面 framebuffer は持たない。
- 描画命令を全て記録する方式は採用しない。
- 描画コールバックをタイルごとに再実行する。
- ユーザーには仮想座標系で描画させる。
- Y オフセットはライブラリ内部で隠蔽する。
- 状態やモデルはライブラリでは保持しない。
- ライブラリは描画先の提供、分割、クリッピング、flush のみ担当する。
- メモリは遅延確保し、画面サイズは `begin()`／初回 `render()` 時点の実パネルから読む。

## 4. 想定する利用コード（確定版）

```cpp
#include <LovyanGFX.hpp>
#include <LGFX_AUTODETECT.hpp>
#include <LGFXVirtualCanvas.h>          // 両クラスを宣言（gfx ライブラリを先に include する中立方針）

static LGFX lcd;
LGFXVirtualScreen screen(lcd);          // 分割数省略 = auto。ここでは未確保

// ── アプリ状態（ライブラリは一切関与しない / §15）──
struct AppState {
    int score;
    int playerX;
    int playerY;
};
AppState state;

// ── 描画関数：仮想全画面に描く。offset もタイル境界も見えない ──
void drawScene(LGFXVirtualCanvas& g, AppState& s) {
    g.fillScreen(TFT_BLACK);
    g.setCursor(10, 10);
    g.printf("Score %d", s.score);
    g.fillRect(s.playerX, s.playerY, 16, 16, TFT_GREEN);   // 仮想座標
}

void setup() {
    lcd.init();                         // ここで初めて画面サイズが確定
    screen.setMemoryLimit(20 * 1024);   // 任意：RAM 上限を指定（主役）
    screen.begin();                     // 任意：ここで確保を前倒し（省略可）
}

void loop() {
    updateState(state);
    screen.render(drawScene, state);    // begin 未呼びでも、ここで自動確保（ガードレール）
}
```

状態が不要な場合（§16）：

```cpp
void drawScene(LGFXVirtualCanvas& g) {
    g.fillScreen(TFT_BLACK);
    g.drawString("Hello", 10, 10);
}

screen.render(drawScene);
```

## 5. 描画コールバック

### 5.1 ユーザが書く形

```cpp
void draw(LGFXVirtualCanvas& g);
```

または

```cpp
void draw(LGFXVirtualCanvas& g, Context& ctx);
```

`g` は具象クラス `LGFXVirtualCanvas` の参照。テンプレートでも抽象基底でもない（理由は §6）。

### 5.2 内部形

内部的には以下の形に集約する。

```cpp
using DrawRaw = void (*)(LGFXVirtualCanvas& g, void* ctx);
bool render(DrawRaw draw, void* ctx = nullptr);   // 描画できたら true、未確保なら false（§10.3）
```

### 5.3 型付き便利 API

ユーザーが `void*` を直接扱わなくてよいように、型付き overload を提供する。

```cpp
bool render(void (*draw)(LGFXVirtualCanvas& g));               // ctx なし
template <typename T>
bool render(void (*draw)(LGFXVirtualCanvas& g, T& ctx), T& ctx); // 型付き ctx
```

受け付けるのは**関数ポインタのみ**（キャプチャ付きラムダ・`std::function` は受けない）。コードサイズを抑えるための割り切り。

### 5.4 テンプレート利用方針

- **タイル分割ループ本体はテンプレート化しない**（draw 関数も `LGFXVirtualCanvas&` 固定なので型ごとに複製されない）。
- テンプレートは「型付き callback を内部 `void*` 版へ変換する薄い sugar」と「描画メソッドの color 引数の型保存」に限定する。
- これにより、コードサイズとコンパイル時間の増加を抑える。

## 6. なぜ「専用の具象クラス」にするか（最重要の設計判断）

ユーザの draw 関数は `LovyanGFX&` ではなく**専用具象クラス `LGFXVirtualCanvas&`** を受け取る。これは LovyanGFX 1.2.21 の実装調査に基づく必然の選択である。

### 6.1 LovyanGFX 側の制約（調査結果）

- **描画原点の平行移動 API が無い。** `LGFXBase` にあるのは `setClipRect`（クリップ矩形）のみで、`setOrigin` / `translate` / 描画用の `setOffset` は存在しない。
- **描画プリミティブが非 virtual。** `drawPixel` / `fillRect` 等はすべて `LGFX_INLINE`（非仮想）。`LGFXBase` の virtual は `init_impl` 等ごく一部で、描画系は一つも無い。

### 6.2 そこから導かれる結論

- 生の `LovyanGFX&` を高さ `tileH` の小さな sprite に渡しても、**仮想 Y 座標を tileY へ変換する手段が無い**。仮想 y がタイル範囲外なら sprite にクリップされて消えるだけで、タイルが正しく組み上がらない。
- 非 virtual ゆえ `LovyanGFX` を継承して `fillRect` 等を override しても、`LovyanGFX&` 経由の呼び出しは**常に本体（平行移動しない方）**にディスパッチされ傍受できない。

したがって「offsetY を差し引いて転送する層」を draw 関数の手前に必ず挟む必要があり、それは `LovyanGFX&` という型では不可能。**自前の具象クラスがメソッドごとに `y -= offsetY` して内部 sprite へ転送する**のが唯一素直な実現方法。

### 6.3 抽象基底（virtual）もテンプレートも使わない理由

- **実装は1つで足りる。** 全面描画も「1タイル（offsetY=0, 高さ=全体）」の特殊ケースとして同じ `LGFXVirtualCanvas` で表現できる。full 用／タイル用を別実装にする必要がない → 抽象基底・virtual 不要。
- **コードサイズが予測可能。** virtual を使わないので vtable 無し。テンプレートにしないので draw 関数や分割ループが型ごとに複製されない。forwarding メソッド十数個＋ループのみで有界。
- **IDE 補完が効き、安全。** 具象クラスに対応メソッドだけを宣言するので、補完候補にはサポート済みメソッドだけが出る。未対応メソッドを呼べばコンパイルエラー＝安全側に倒れる。
- **失うものはほぼ無い。** virtual 基底なら「生 LovyanGFX& も具象も多態で受ける」が将来できるが、§6.1 の通り LovyanGFX が非 virtual な以上それは元々不可能。将来ポリモーフィズムが必要になったら、この具象クラスからインタフェースを後付け抽出すればよい（§17）。

## 7. LGFXVirtualCanvas（描画面）の責務

`LGFXVirtualCanvas` は内部タイル sprite をラップし、ユーザから見える仮想座標を実タイル座標へ変換する。

主な責務：

- 仮想 Y 座標からタイルオフセットを差し引く（`y -= offsetY`）。
- タイル外描画は内部 sprite の clip により自動的に消える（§12）。
- ユーザに offsetY を見せない。
- 可能な限り通常の LGFX / M5GFX Canvas と同じメソッド名・引数に見せる。

`LGFXVirtualScreen`（マネージャ）の責務：

- 実パネル（`LovyanGFX&`）を保持する。
- 分割設定（メモリ上限／分割数／タイル高）を保持する。
- `begin()`／初回 `render()` でタイル sprite を確保する。
- `render()` でタイルごとに `LGFXVirtualCanvas` を生成し、draw を再実行し、実パネルへ push する。

### 7.1 LGFXVirtualSprite（部分描画／サブ領域）

`LGFXVirtualSprite` は「**任意位置・任意サイズのサブ領域**」を描く版。通常の `LGFX_Sprite` と同じ感覚だが、内部が縦タイル分割なので小バッファで済む。動的に書き換わる一部分だけを更新する用途（例：320×240 画面の中の固定ビューポート、動くアイコン）に使う。

- **座標系はそのスプライトのローカル**（0,0 = スプライト左上）。全画面仮想座標ではない。`width()/height()` はスプライトのサイズを返す。
- **サイズは構成（begin 前）で確定**。`LGFXVirtualSprite spr(panel, w, h, x = 0, y = 0)`。バッファは幅 `w`・高さ `tileH`（メモリ予算/分割設定から算出）で確保。
- **配置位置は可変**。`render(draw)` は現在位置、`render(draw, x, y)` はそこへ描画しつつ現在位置を更新（`setPosition` でも変更可）。位置変更で再確保はしない。
- **転送は全部ライブラリ側**。push の前にパネルの clip 矩形を `(x, y, w, h)` に設定するので、**最終端数タイルの余剰行も画面端のはみ出しも自動でクリップ**され、ユーザは何も減算しなくてよい（LovyanGFX の push は転送先パネルの clip を尊重することを確認済み）。
- `LGFXVirtualScreen` は実質「サイズ=画面・位置=(0,0)」の `LGFXVirtualSprite` に相当し、両者は内部エンジン（`LGFXVirtualTiledBase`）を共有する。
- メモリ予算・auto-clear・確保失敗のフォールバック無し等の方針は全画面版と共通（§10/§11）。

```cpp
// (40,0) に置く 240x240 のタイル分割スプライト（ローカル座標）
LGFXVirtualSprite view(lcd, 240, 240, 40, 0);
view.setMemoryLimit(16 * 1024);
view.begin();
view.render(drawView);            // 現在位置に描画
// 動かす場合：
view.render(drawIcon, x, y);      // (x,y) へ描画＋現在位置更新
```

## 8. API ラップ方針

最終的には主要描画メソッドを `LGFXVirtualCanvas` 上に個別ラップする。対応メソッドのみ存在し、未対応メソッドの呼び出しはコンパイルエラーになる（段階的に対応メソッドを増やす運用）。

優先実装するメソッド：

- `fillScreen`
- `drawPixel`
- `drawLine`
- `drawFastHLine`
- `drawFastVLine`
- `fillRect`
- `drawRect`
- `drawCircle`
- `fillCircle`
- `drawString`
- `drawCentreString`
- `drawRightString`
- `setCursor`
- `print`
- `println`
- `printf`
- `pushImage`

color を取るメソッドは color 引数の型をテンプレートで保存し、内部 sprite へそのまま転送する（型保存のための薄い sugar。§5.4）。文字列を取るメソッド（`drawString` 系）は string 型（`const char*` / `String`）と font 引数（`uint8_t` / `IFont*`）をテンプレートで受け、`print` / `println` は数値・基数指定等 Arduino Print の全 overload を generic に転送する。

> 実装状況：**上記の優先メソッドは v0.1 時点ですべて実装済み**（parity / pushimage テストで検証）。`setTextColor` / `setTextSize` / `setTextDatum` / `setCursor` / `getCursorX/Y`（仮想座標で返す）も提供。

## 9. 特に注意する API

### 9.1 テキスト描画

以下は内部状態を持つため注意する。

- `setCursor`
- `print`
- `println`
- `printf`
- `drawString`
- `setTextDatum`

カーソル位置の Y 座標も仮想座標として扱う（`setCursor` で offsetY を差し引く）。`print` / `println` / `printf` によるカーソル前進は draw 再実行ごとに決定的に再現され、各タイルで clip されるため、結果としてタイル境界を跨ぐテキストも組み上がる（§12.1 の近傍依存制約は除く）。

ただし、**内部 sprite の高さ（tileH）に依存する挙動**——テキストの自動スクロール（カーソルが下端に達した時の挙動）や下端での折り返し等——は、仮想全画面高 `height` ではなく `tileH` を基準に動くため、全面描画と一致させられない可能性がある。これらは**実装・実験で判断し、安全に提供できない関数は非対応メソッドとする**（呼ぶとコンパイルエラー＝§8）。基本の `setCursor` / `print` / `println` / `printf` / `drawString`（仮想 Y オフセット適用）は提供する方針。`getCursorY` 等カーソルを読む API は offsetY を足し戻して仮想座標で返す。

### 9.2 pushImage

`pushImage` はタイル境界を跨ぐ可能性があるため、専用クリッピングが必要。

考慮するケース：

- 完全にタイル内
- 上にはみ出す
- 下にはみ出す
- タイル境界を跨ぐ
- 画面外
- 最終タイルの端数高さ

**実験結果（対応）**：`tests/pushimage` で split=1 と split=2/3/5/7 が pixel 一致することを確認。上記ケース（上下はみ出し・境界跨ぎ・画面外）はすべて内部 sprite の per-pixel clip だけで全面描画と一致したため、**`y -= offsetY` 転送のみで対応**する。LGFX の `pushImage` は負の dest y のときソース側へオフセットして正しくクリップするため、専用クリッピングは不要だった。透過色付き variant・パレット深度・回転/ズーム系は今後検証する。

## 10. 分割方式・メモリ確保

画面を縦方向に分割する。指定方法は「メモリ上限」を主役とし、分割数・タイル高も併存させる。

### 10.1 指定方法と優先順位

`begin()`／初回 `render()` 時に、以下の優先順位でタイル高 `tileH` と分割数 `N` を決める。各設定値は「未指定」を表す既定値（0）を持ち、0 のものは優先順位の判定対象外として次へフォールスルーする。

1. `setMemoryLimit(bytes)` が `bytes > 0`（**既定 0 = 指定なし**、バイト単位）→ `tileH = floor(bytes / (width * bytesPerPixel))`、`N = ceil(height / tileH)`。← **主役**
   - 上限はあくまで「上限」。`tileH` は画面高でクランプ（`tileH > height` なら 1 タイル）。
   - `bytes` が 1 行分（`width * bytesPerPixel`）にも満たず `tileH < 1` になる場合は**要求を満たせないので確保失敗扱い**（§10.2。勝手に丸めない）。
2. 分割数 `k`（コンストラクタ引数 or `setSplitCount(k)`、`k > 0`）→ `tileH = ceil(height / k)`、`N = k`。
3. `setTileHeight(h)` が `h > 0` → `tileH = h`、`N = ceil(height / h)`。
4. どれも未指定（コンストラクタ省略＝分割数 0＝auto）→ **デフォルト3分割**。

`width` / `bytesPerPixel` / `height` は実パネル（`lcd.init()` 済み）から読む。

### 10.2 確保のタイミング（遅延確保＋任意 begin）

- **コンストラクタ**：`LovyanGFX&` 参照と設定を保持するだけ。確保しない（この時点ではまだ `lcd.init()` 前で画面サイズ不明でもよい）。
- **設定メソッド**（`setMemoryLimit` / `setSplitCount` / `setTileHeight`）：確保前に呼ぶ。各既定値は 0（未指定）。
- **`bool begin()`**：任意。実パネルからサイズ・色深度を読み、§10.1 に従って `tileH` を決め、**幅 × tileH の再利用タイル sprite を1枚確保**する。成功で `true`、失敗で `false` を返す。
- **ガードレール**：`begin()` 未呼びのまま `render()` が来たら、その場で `begin()` 相当の確保を試みる。
- 確保後に設定を変えた場合は再 `begin()` で作り直す。

### 10.3 確保失敗時の扱い（フォールバックしない）

確保に失敗するのは主に次の場合：要求 `tileH` 分の sprite を RAM が確保できない／メモリ上限が 1 行にも満たず `tileH < 1`。

このとき**勝手に分割数を増やす・タイルを小さくするなどのフォールバックは行わない**。要求が通らなかったことを呼び出し側が確実に検知できるようにする。

- `begin()` は `false` を返す（`setup()` での確保結果は必ずチェックすることを推奨）。
- 確保できていない状態（`isReady() == false`）では `render()` は**何も描画せず** `false` を返す。中途半端な描画やゴミ表示はしない。
- `render()` 自身も `bool` を返す（描画できたら `true`、未確保なら `false`）。
- 失敗理由はデバッグ用にログ出力してよい（必須ではない）。

```cpp
void setup() {
    lcd.init();
    screen.setMemoryLimit(20 * 1024);
    if (!screen.begin()) {
        // 確保失敗。フォールバックせず、ここで気づける
        Serial.println("LGFXVirtualScreen: alloc failed");
    }
}
```

状態取得：`bool isReady() const;`（タイルバッファ確保済みか）。

### 10.4 タイル描画ループ

各タイル `i`（`0..N-1`、`offsetY = i * tileH`）について：

1. 再利用タイル sprite を**背景色でクリアする**（auto-clear。§11）。`setAutoClear(false)` 時はスキップ。
2. `LGFXVirtualCanvas`（sprite, offsetY）を生成する。
3. 描画 callback を呼ぶ。
4. タイル内容を実パネルへ `pushSprite(0, offsetY)` で転送する。最終タイルが端数高さの場合、sprite 下端の余剰行はパネル側 clip で捨てられる。

ループ全体のパネル転送は1回の `startWrite()`／`endWrite()` で囲み、N 回の転送を1トランザクションに束ねる（実機 SPI の設定/CS トグル削減。タイルへの描画はメモリ操作でバス転送が無いため draw 側の囲いは不要。バス無しの host では no-op）。`LGFXVirtualSprite` の場合は転送先が `(x, y)`、push 前にパネル clip を `(x, y, w, h)` に設定する（§7.1）。

### 10.5 バッファ再利用・DMA・ダブルバッファ

内蔵RAM（DMA 可能）タイルの `pushSprite` は、タイルバッファを直接読む**非同期** SPI-DMA を起動し、転送完了を待たずに return する。ループ外側の `startWrite`／`endWrite` の内側ではタイルごとのバス待ちが入らない（push ごとのネストした `startWrite`／`endWrite` は最外でしかフラッシュしない）。したがって**単一**のタイルバッファを再利用すると、前タイルの DMA が読み出している最中に次タイルの clear/draw がバッファを上書きし、転送中のタイルが壊れる。

これを2つのモードで解決する：

- **単一バッファ（既定）**。各 `pushSprite` の後にループが `waitDMA()` を呼び、バッファ再利用前に転送を完了させる。正しく、メモリ最小だが、描画と転送が**直列**になる（frame ≈ Σ描画 + Σ転送）。`split=1` は追加待ち不要（push 1回、最後の `endWrite` がフラッシュ）。PSRAM タイルは本質的に安全：LGFX は SPIRAM sprite で DMA を無効化するため転送は同期（安全だが低速）。
- **ダブルバッファ（`setDoubleBuffer(true)`、opt-in）**。タイル sprite を2枚確保して ping-pong する（`i & 1`）。タイル `i` を一方のバッファから（非同期 DMA で）転送しつつ、タイル `i+1` を他方へ描画する。1本の SPI バス上では連続転送がバス自身により直列化されるため、タイル `i` で再利用するバッファ（最後に触れたのはタイル `i-2`）は in-flight DMA が無いことが保証され、ループ内待ちは不要で、CPU 描画と SPI 転送が重なる（転送律速なら frame ≈ max(描画, 転送)）。タイルバッファ 2 倍を要する。`setMemoryLimit` は各バッファ個別に効く。確保は all-or-nothing：2枚目が確保できなければ `begin()`／`render()` は失敗（フォールバックしない。§10.3）。

両モードは**ピクセル単位で同一**の出力になる（parity テストのダブルバッファケースで検証）。バス無しの host では `waitDMA()` は no-op で、両モードの描画結果は同一。

## 11. タイル初期化（auto-clear）と fillScreen

### 11.1 auto-clear（タイルの初期状態）

タイル sprite は全タイル・全フレームで1枚を再利用するため、draw 呼び出し前の sprite には前タイル／前フレームの残骸が残る。これを放置すると、未描画画素が「split=1（全面）」と「split=N（分割）」で食い違い、parity が壊れ実機ではゴーストになる。

そこで**各タイルは draw を呼ぶ前に背景色でクリアする（auto-clear、既定 ON）**。

- 背景色の既定は黒。`setBackgroundColor(color)` で変更できる。
- これにより**未描画画素は常に背景色で決定的**になり、draw が全画素を塗らなくても分割数に依らず結果が一致する。
- 全画面 framebuffer を持たない以上「前フレーム内容の保持（部分更新）」は原理的に不可能であり、毎フレーム全描画が前提。auto-clear はその前提を仕様として明文化したもの。
- ユーザが常に `fillScreen` で全面を塗る場合、auto-clear は二重塗りになる。これを避けたい上級者は `setAutoClear(false)` で無効化できる（無効化時、未描画画素は不定）。

### 11.2 fillScreen の扱い

`fillScreen(color)` は仮想全画面塗りつぶしとして扱う。

各タイル描画時には現在のタイル sprite 全体を塗りつぶす（offsetY 非依存）。端数の最終タイルでも sprite 全体を塗り、パネルへ push する際に余剰行が clip されるため、結果として仮想全画面が塗られる。auto-clear が ON の場合、draw 冒頭の `fillScreen(color)` は背景色クリアの上書きとなる（color が背景色と同じなら実質同じ操作）。

## 12. クリッピング

すべての描画はタイル範囲外に出ても安全であること。内部 sprite の per-pixel clip により、タイル範囲外（負座標・`tileH` 以上）への描画は自動的に消えるので、ライブラリ側の特別な処理なしに安全が確保される（`pushImage` のみ専用配慮が要る。§9.2）。

特に確認するケース：

- `y = tileHeight - 1`
- `y = tileHeight`
- `y = tileHeight + 1`
- 最終タイルの高さが通常タイルより小さいケース
- 画面外の負座標
- 画面下端を超える描画

### 12.1 既知の制約：タイル境界を跨ぐ近傍依存描画

draw 関数はタイルごとに再実行され、各回が sprite の clip で切り取られて組み上がる。このため、**ある画素の出力が「タイル境界をまたいだ近傍画素」に依存する描画**（アンチエイリアス、ぼかし、近傍参照フィルタ等）は、分割描画と全面描画で一致しない場合がある。LovyanGFX の既定プリミティブはアンチエイリアス無しなので通常は問題にならないが、AA 付き描画を使う場合はこの制約に留意する。

## 13. テスト方針・計画

GitHub Actions 上のヘッドレス描画環境でテストする。ホストは `lang-ship:host` の `mode=lgfx` プロファイルを用い、`pytest-embedded-arduino-cli` の `dut` フィクスチャでシリアル出力を待ち受け、sketch が `gfx.createPng()` で出力した PNG を Pillow で pixel 比較する。

> テストの**実行方法・ディレクトリ構成・現状**は [tests/README](tests/README.md)（EN+JA）を参照。本節は設計としての**テスト方針と計画**を定める。

### 13.1 2層構成と配分

LGFXVirtualCanvas のヘッダは gfx 中立で、対応エントリポイントは **LovyanGFX / M5GFX / M5Unified** の3つ。ただし **M5GFX / M5Unified の描画エンジンは内部的に LovyanGFX** であり、タイル分割ロジックの正当性は3つで本質的に同じ。よってテストを2層に分ける。

- **Tier 1（機能・正当性）**: ライブラリのロジックを **LovyanGFX 1本で網羅的に**検証する（§13.4）。
- **Tier 2（クロスライブラリ ビルド＋最小描画）**: 「ヘッダが各エントリポイントで**コンパイルでき、最小描画が parity を満たす**」ことを **3ライブラリ**で担保する。`split=1` と `split=3` の一致＋ PNG 生成を確認するだけの最小内容で、smoke（ビルド確認）の役割も兼ねる。共有最小シーンは `tests/common_libs/` に置き3テストで使い回す。
- **（任意）Tier 3**: `esp32:esp32:esp32` の compile-only を1本置き、host で出ないターゲット依存のビルド破綻を早期検出する。実描画の検証は host に委ねる。

**配分の根拠**：3ライブラリで全ケースを回しても描画エンジンが同一なため**新しいロジック網羅は得られず**、CI 時間とライブラリ取得だけが増える。クロスで守りたいのは「各 include 順序・各ライブラリの型で**コンパイル＆描画できる**」ことであり、それは Tier 2 で十分。

### 13.2 比較方法（同一 draw 関数を分割数だけ変えて比較）

draw 関数は `LGFXVirtualCanvas&` を受けるため、「通常 Canvas への全面描画」も `LGFXVirtualScreen` の **1分割（split=1, offsetY=0）** で表現する。1分割時は forwarding が素通り（offsetY=0）なので、これが全面描画の基準になる。

- 基準：`split = 1` で描画した PNG
- 検証：`split = 2, 3, 5, …` で描画した PNG

PNG のバイナリ一致ではなく、読み込んだ pixel 配列の一致で比較する。「**分割しても結果が変わらない**」という不変条件を直接検証する。

### 13.3 失敗時 artifacts

失敗時には `full.png`（基準）/ `virtual.png`（分割）/ `diff.png`（差分）を保存する。

### 13.4 Tier 1 ケース一覧（LovyanGFX）

各ケースは複数の split（例 1/2/3/5/7）で pixel 一致を確認する。

| # | ケース | 検証内容 |
|---|---|---|
| T1-1 | parity（総合） | 図形＋テキスト混在シーンが split 不変 |
| T1-2 | 基本図形 | fillRect / drawRect / drawLine / drawPixel / drawFastH/VLine |
| T1-3 | 円 | drawCircle / fillCircle |
| T1-4 | テキスト | drawString / setCursor＋print/println/printf、cursor Y 仮想座標（§9.1） |
| T1-5 | タイル境界 | `y = tileH-1 / tileH / tileH+1` を跨ぐ描画（§12） |
| T1-6 | 端数タイル | 画面高が分割数で割り切れない（例 split=7 で最終30行）（§12） |
| T1-7 | クリッピング安全性 | 画面外・負座標・下端超過でクラッシュせず一致（§12） |
| T1-8 | auto-clear | 未描画画素が背景色で決定的／`setBackgroundColor`／`setAutoClear(false)`（§11） |
| T1-9 | メモリ／確保失敗 | `setMemoryLimit` で tileH 算出、過小指定で `begin()`＝false・`isReady()`＝false・`render()`＝false（フォールバックしない）（§10.3） |
| T1-10 | ランダム fuzz | 乱数シードで図形列を生成、各 split で一致（シードをログ出力し再現可能に） |
| T1-11 | アニメーション | フレーム列を回し、各フレームが split 不変 |
| T1-12 | pushImage | clip のみで split 不変（上下はみ出し・境界跨ぎ・画面外）。実験で確認済み・対応（§9.2） |

### 13.5 共有ルール

- 1テスト = 1ディレクトリ（`<name>.ino` / `sketch.yaml` / `test_<name>.py`）。
- 成果物は `output/<name>.png`、`conftest.py` が各テスト前に wipe。
- fuzz はシード固定＋ログ出力で再現可能にする。

## 14. 描画関数分離の推奨

このライブラリでは、描画処理を `loop()` や状態更新処理から分離することを推奨する。

```cpp
void updateState(AppState& state);
void drawScene(LGFXVirtualCanvas& g, AppState& state);
```

これにより以下が容易になる。

- 通常描画
- 分割描画
- headless 描画
- golden image test
- layout test
- animation capture

## 15. View / Model の責務分離

ライブラリは Model を管理しない。Model / State はアプリケーション側が管理する。

```cpp
struct AppState {
    int score;
    int playerX;
    int playerY;
};
```

描画関数は state を参照して View を描画する。

```cpp
void drawScene(LGFXVirtualCanvas& g, AppState& state);
```

## 16. グローバル状態の扱い

グローバル状態を参照する描画関数も許容する。

```cpp
AppState state;

void drawScene(LGFXVirtualCanvas& g) {
    g.drawNumber(state.score, 10, 10);
}
```

ただし、テスト、複数画面、プレビュー、再利用性を考える場合は context を渡す方式（`render(draw, state)`）を推奨する。

## 17. レイアウトライブラリとの関係

将来的にレイアウト系ライブラリを作る場合、LGFXVirtualCanvas を先に土台として用意する。レイアウトライブラリは分割描画を意識しない。

```cpp
root.layout({0, 0, 320, 240});
root.draw(g);
```

描画先 `g` が `LGFXVirtualCanvas`（仮想キャンバス）として渡る。もし将来「通常 Canvas と仮想 Canvas を多態で差し替えたい」需要が出たら、その時点で `LGFXVirtualCanvas` からインタフェースを抽出する（YAGNI。今は具象1本）。

## 18. 将来構想

LGFXVirtualCanvas の上に以下を追加できる設計にする。

- LGFXLayout
- LGFXWidgets
- LGFXView
- layout preview
- component 単位の描画テスト
- headless screenshot test
- animation capture
- 複数 `LGFXVirtualScreen` 間でのタイル sprite 共有（最大横幅で確保した sprite を、同幅以下の screen で使い回すオプション。RAM 節約）

## 19. 非目標

初期段階では以下は必須としない。

- 完全な LGFX / M5GFX API 全網羅
- 任意方向のタイル分割（縦分割のみ）
- 部分更新最適化
- 描画命令の完全記録
- retained mode UI framework
- 自動 layout engine
- キャプチャ付きラムダ / `std::function` の受け入れ（関数ポインタのみ）
- 複数 `LGFXVirtualScreen` 間でのタイル sprite 共有（初期は各 screen が専有。共有は将来のオプション。§18）
- 確保失敗時のフォールバック（分割数の自動増加など。失敗は失敗として返す。§10.3）

## 20. 最小実用版のゴール

最小実用版では以下を満たす。

- 画面を縦方向に分割して描画できる。
- ユーザーは offset を意識しない。
- `LGFXVirtualScreen screen(lcd);` の最小記述で動く（遅延確保・デフォルト3分割）。
- `setMemoryLimit()` で RAM 上限を指定できる。
- `render(draw)` が使える。
- `render(draw, state)` が使える。
- 基本図形と文字描画ができる。
- 分割数を変えても結果が一致する（全面描画＝split:1 と分割描画の PNG 比較テストが通る）。
- GitHub Actions でテストできる。
