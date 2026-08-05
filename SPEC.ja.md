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
- **分割の指定は「メモリ上限」が主役**。`setMemoryLimit(bytes)`（バイト単位・既定 0＝指定なし）を最優先、次に分割数、未指定なら 1タイル ≈ 19 KB の組み込みデフォルト予算（`DEFAULT_TILE_BYTES`）。**タイルが 2 枚以上になる面では自動でダブルバッファを有効化**する（`setDoubleBuffer` で上書き可）。
- **クリッピングは sprite 標準の per-pixel clip で自動的に安全**（範囲外描画は消えるだけ）。
- **各タイルは draw 前に背景色でクリアする（auto-clear、既定 ON）**。背景色は設定可（既定: 黒）。未描画画素は背景色で決定的になり、分割数に依らず結果が一致する。`setAutoClear(false)` で無効化できる。
- **マネージャは2種**：`LGFXVirtualScreen`（全画面）と `LGFXVirtualSprite`（任意位置・サイズのサブ領域＝内部分割スプライト。ローカル座標）。両者は内部のタイル分割エンジンを共有し、draw に同じ `LGFXVirtualCanvas` を渡す。違いは転送先（全画面か、置いた矩形か）だけ。詳細は §7.1。
- **分割軸を選択できる。** `setSplitAxis(LGFXVirtualSplitAxis::Rows | ::Columns)`。既定は `Rows`（横帯を上から下へ）。`Columns` は高さいっぱいの縦帯に切り左から右へ転送し、そのように消費される面（長尺帯）向け。変わるのはタイル形状と転送順だけで、コールバックは従来どおり面全体の座標で描き、出力も同一。§10.8 参照。
- **タイルバッファを PSRAM に置ける。** `setUsePsram(true)`（既定 OFF）。大画面でタイル数を大幅に減らす代わりにメモリが遅くなる。フォールバックを許容する唯一の箇所（PSRAM が使えなければ内蔵RAM）であり、変わるのは速度だけだから。§10.9 参照。
- **差分転送は任意機能で既定は無効**。`setDiffMode(LGFXVirtualDiffMode::Tile)` で、前フレームから変化していないタイルの転送を省略する。粒度はタイル単位のみ（タイル内分割は将来の拡張）。削減できるのは転送のみで、描画コストは減らずハッシュ計算分が増える。詳細は §21。

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
- `fillRoundRect`
- `drawRoundRect`
- `drawCircle`
- `fillCircle`
- `drawEllipse`
- `fillEllipse`
- `drawTriangle`
- `fillTriangle`
- `drawString`
- `drawCentreString`
- `drawRightString`
- `setFont`
- `setTextFont`
- `setCursor`
- `print`
- `println`
- `printf`
- `pushImage`

color を取るメソッドは color 引数の型をテンプレートで保存し、内部 sprite へそのまま転送する（型保存のための薄い sugar。§5.4）。文字列を取るメソッド（`drawString` 系）は string 型（`const char*` / `String`）と font 引数（`uint8_t` / `IFont*`）をテンプレートで受け、`print` / `println` は数値・基数指定等 Arduino Print の全 overload を generic に転送する。

> 実装状況：**上記の優先メソッドはすべて実装済み**（parity / pushimage テストで検証）。`setTextColor` / `setTextSize` / `setTextDatum` / `setCursor` / `getCursorX/Y`（仮想座標で返す）も提供。

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
4. どれも未指定（コンストラクタ省略＝分割数 0＝auto）→ **デフォルトのタイル予算 `DEFAULT_TILE_BYTES`（≈ 19 KB、= 16bpp で 320×30 のタイル相当）** を `setMemoryLimit` と同じ式で適用：`tileH = floor(DEFAULT_TILE_BYTES / (width * bytesPerPixel))`（画面高でクランプ）、`N = ceil(height / tileH)`。これにより分割数が**面サイズに応じてスケール**する（小さい面は1タイル、全画面は数タイル）一方、各タイルは ≈ 19 KB に収まる。この値は Core2 ベンチ（`bench/`）に由来し、測定した全サイズで最適分割を再現する。

`width` / `bytesPerPixel` / `height` は実パネル（`lcd.init()` 済み）から読む。

`tileH` / `N` 確定後に**ダブルバッファのモード**を決める（§10.5）。既定の *auto* 状態では `N ≥ 2` で ON、1タイルなら OFF。

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

これを2つのモードで解決する。どちらが使われるかは**ダブルバッファのモード**（既定 *auto*、後述）で決まる：

- **単一バッファ**。各 `pushSprite` の後にループが `waitDMA()` を呼び、バッファ再利用前に転送を完了させる。正しく、メモリ最小だが、描画と転送が**直列**になる（frame ≈ Σ描画 + Σ転送）。`split=1` は追加待ち不要（push 1回、最後の `endWrite` がフラッシュ）。PSRAM タイルは本質的に安全：LGFX は SPIRAM sprite で DMA を無効化するため転送は同期（安全だが低速）。
- **ダブルバッファ**。タイル sprite を2枚確保して ping-pong する（`i & 1`）。タイル `i` を一方のバッファから（非同期 DMA で）転送しつつ、タイル `i+1` を他方へ描画する。1本の SPI バス上では連続転送がバス自身により直列化されるため、タイル `i` で再利用するバッファ（最後に触れたのはタイル `i-2`）は in-flight DMA が無いことが保証され、ループ内待ちは不要で、CPU 描画と SPI 転送が重なる（転送律速なら frame ≈ max(描画, 転送)）。タイルバッファ 2 倍を要する。`setMemoryLimit` は各バッファ個別に効く。確保は all-or-nothing：2枚目が確保できなければ `begin()`／`render()` は失敗（フォールバックしない。§10.3）。

**モード選択。** `setDoubleBuffer(true|false)` で明示指定できる。未指定なら **auto**：解決後のタイル数 `N ≥ 2` で自動的にダブルバッファ、1タイル（`N == 1`、重ねる相手のタイルが無く2枚目が純粋な無駄になる）なら単一バッファ。デフォルトのタイル予算（§10.1、面サイズに応じて `N` が増えつつ各タイルは ≈ 19 KB に保たれる）と組み合わさり、auto は小さい面を**単一バッファの1タイル**（最速・最小RAM）、大きい面を**多数の小さなダブルバッファタイル**（オーバーラップで転送を隠す）に解決する。結果としてタイル合計RAMは面サイズに依らず ≈ 2 × 予算となり、小さなタイル単位の確保なので全画面バッファのような巨大連続確保の失敗を避けられる。この auto 判定は config の*解決*の一部であり、実行時フォールバックではない：解決後の（ダブルを含む）確保に失敗した場合も §10.3 どおり `begin()`／`render()` は失敗し、**黙って単一バッファに落とすことはしない**。

両モードは**ピクセル単位で同一**の出力になる（parity テストのダブルバッファケースで検証）。バス無しの host では `waitDMA()` は no-op で、両モードの描画結果は同一。

### 10.6 なぜ 1タイル ≈ 19 KB なのか / 描画律速 vs 転送律速のトレードオフ

デフォルト予算（§10.1）は、draw コールバックの中身を知らないまま、あらゆる面サイズに対して適切な分割数と auto ダブルバッファ判定（§10.5）を1つの数値で決めなければならない。結論は少し非自明で、**よくある直感と逆になる**ので根拠を残す。

**1フレームの2つのコスト。** ダブルバッファでは、各タイルの描画と各タイルの転送がパイプラインで重なり、フレーム時間はおおよそ：

```
frame ≈ draw₀ + Σ max(drawᵢ₊₁, xferᵢ) + xfer_last   ≈   max(Σ描画, Σ転送)
```

つまり大きい方の総和が支配する：

- **`Σ転送`（SPI転送）は実質フレームあたり一定。** 面の全画素をちょうど1回ずつ送るので、`Σ転送`は面積だけで決まり、分割数には依らない。320×240×16bpp 全画面 ≈ 32 ms（Core2、≈ 31fps ＝ ハードウェア SPI 床）。小さい面はその比例分。
- **`Σ描画`（CPU描画）は一定では*ない* — 分割数とともに増える。** draw コールバックはタイルごとに再実行される（§5.4）。各プリミティブはタイルごとに走査されクリップされる（ラスタライズされるのはタイル内の範囲だけ）。だから*大きなプリミティブ少数*のシーンは `N` に対して緩やかに増えるが、*多数*のプリミティブ（大量の図形・テキスト）のシーンは `Σ描画` が `N` とともに急に増える。

**帰結（ここが直感と逆）。** 分割数を増やしても `Σ描画` は決して減らない。増えるのは冗長な per-プリミティブ処理とタイルごとのオーバーヘッド（`fillScreen`・`pushSprite`・`waitDMA`）だけ。したがって：

- **転送律速（`Σ描画 < Σ転送`）。** 転送が床で、描画はオーバーラップに隠れる。小さいタイルが多いほど細かく重なり（かつダブルバッファでも合計RAMはむしろ*少ない*）→ **`N` を増やすと速くなる**（転送が完全に隠れるまで）。
- **描画律速（`Σ描画 > Σ転送`）。** 転送はすでに描画の裏に隠れており、タイルを増やすと冗長再描画が積み上がるだけ。→ **少なく大きいタイルが良い** ── 理想は1タイル（描画1パス、重ねる相手も不要）。**`N` を増やすほど遅くなる。**

つまり「2枚で描画と転送を重ねるのが最速」が成り立つのは*転送律速のときだけ*。描画がボトルネックなら、できる最善は描画1パス（`N=1` の `Σ描画`）であり、それは単一バッファで既に達成できる。最適な `N` は、オーバーラップ利得（転送床で頭打ち）と 冗長描画＋オーバーヘッド（`N` とともに増加）の内点での釣り合いで、どこに来るかは**シーンの描画コスト**次第。

**19 KB の出どころ。** 19,200 B ＝ 16bpp で 320×30 のタイル。30行タイルは ≈ 4 ms で転送でき、典型的なバッファ付き GUI/アニメの描画ならそれより短く描けるので、*大きい*面は解決後の分割で転送律速に留まり SPI 床に乗る。一方*小さい*面は1タイルに解決される（描画律速の領域で、1タイルがちょうど最適）。Core2 ベンチ（`bench/`）では、この1つの値が全測定サイズで**実測最適**の分割を再現する：

| 面サイズ | 解決される分割（19 KB） | ベンチ最適と一致 |
|----------|--------------------------|------------------|
| 64×48    | 1（単一バッファ）        | ✓ |
| 128×96   | 2（ダブル）              | ✓ |
| 160×100  | 2（ダブル）              | ✓ |
| 240×160  | 4（ダブル）              | ✓ |
| 320×240  | 8（ダブル）              | ✓ |

さらにダブルバッファのRAMを面サイズに依らず ≈ 2 × 19 KB ≈ 38 KB に抑え（同時に存在するタイルは2枚だけ）、各確保を小さく保つ ── 全画面バッファが要する ~150 KB の連続確保（300 KB 空きでも失敗する。`bench/` 参照）を避けられる。

**ライブラリ自身が律速を検出して `N` を選べるか?** ジオメトリだけからは無理。draw コールバックは任意のユーザコードで、そのフレームあたりコストは事前に分からず、フレームごとに変わりうる。だからデフォルトは、バッファ付き GUI/アニメが普通そうである「転送律速〜均衡」のワークロード向けに調整した固定ヒューリスティックにしている。**シーンが極端に描画重い**場合（解決後のタイルすら描画律速になるなら）は、*少なく大きい*タイルを選ぶ：`setMemoryLimit()` を大きくするか `setSplitCount()` を小さく固定する（単一バッファが最善のことが多い）。実行時オートチューナ（`drawᵢ`/`xferᵢ` を計測し `N` をヒステリシス付きで適応）は実装可能だが、再確保とフレーム時間のジッタを招くので**デフォルトでは意図的に対象外**とする。

### 10.7 コールバック内の高コスト処理（画像デコード・フラッシュ/PSRAM 読み出し）

§10.6 のとおり、コールバックがタイルごとに再実行されるため `Σ描画` は `N` とともに増える。通常のプリミティブならその増加は緩やかだが、**タイルのクリップ矩形では縮まない固定コスト**をコールバック内で行うと**深刻**になる。その処理は毎タイル丸ごと payされ、`N` にほぼ比例して増える：

- **コールバック内で圧縮画像（PNG / JPEG）をデコードする。** デコーダは1行でも出すために圧縮ストリーム全体を処理する必要がある（DEFLATE / MCU デコードは逐次的）。クリップが捨てるのはタイル外の*出力*画素だけで、デコード自体は減らない。よって全画面 `drawPng` / `drawJpg` をタイル化コールバック内で呼ぶと **毎タイルで再デコード** ── `N` タイル ≈ `N` 回のフルデコード。
- **フラッシュ / SD / ファイルから素材を読む。** タイルごとに再オープン・再読み込み（多くは再デコードも）。フラッシュのレイテンシとファイル I/O を `N` 回払う。
- **PSRAM 上のソース画素をサンプルする。** PSRAM は内蔵RAMよりずっと遅く、ソースをタイルごとに読み直す。（PSRAM 上の*タイル*バッファ自体も DMA が無効化され転送が同期になる ── §10.5 ── ので、コストが重なる。）
- その他クリップで縮まない固定の毎フレーム処理：大きな LUT 構築、レイアウト/採寸パス、センサーポーリング等。

対照的に、**RAM 上**のバッファからの `pushImage` はタイルごとにクリップされる（読んで送るのはタイル分のソース行だけ）。ベンチの `image` シーン（小さな RAM 配列からのタイル状 `pushImage`）が分割しても安いのはこのため。問題になるのは、あくまでクリップで減らせない処理。

**目安：** コールバックのコストがこの種の「クリップで減らない処理」に支配されているなら、分割は性能を ≈ `N`× 劣化させる。auto デフォルトの「小さいタイル多数」はそのワークロードには*不向き*。

**緩和策（優先順）：**

1. **一度だけデコード/読み込み、あとは blit。** 高コストなデコードやフラッシュ読み出しはタイル化コールバックの**外**で一度だけ行い（setup 時、または画像が変わったときだけ）、内蔵RAMの sprite/バッファに展開しておく。per-tile コールバックではそのバッファから `pushImage` / `pushSprite` するだけにする。`pushImage` はタイルごとにクリップされるので、`N`× デコードを **1× デコード + `N`× の安いクリップ済み blit** に変えられる。静的/低頻度更新の画像に推奨。
2. **タイルを少なく大きく。** `setMemoryLimit()` を大きく、または `setSplitCount()` を小さく固定して `N` を小さくする（内蔵RAMとの引き換え）。1タイルならデコードはちょうど1回。
3. **収まる全画面画像はタイル化しない** ── パネルへ直接、または全画面 sprite 1枚に描き、タイル化 Canvas は動的なオーバーレイ層だけに使う。

### 10.8 分割軸（行 / 列）

`setSplitAxis(LGFXVirtualSplitAxis::Rows | ::Columns)` で面を切る向きを選ぶ。**既定は `Rows` で従来どおりの挙動**（`regionW × span` の横帯を上から下へ転送）。`Columns` は `span × regionH` の縦帯に切り、**左から右へ**転送する ── 長尺印刷やスクロールする帯・横長波形のように、列単位で送り出したい対象に自然な順序。

変わるのはタイルの形と転送順だけで、描画コールバックは変わらず面全体の座標を受け取り、出力画像も同一になる（`tests/parity` が全分割数で両軸のピクセル一致を検証する）。

**一般化できる部分。** `LGFXVirtualCanvas` は `offsetY` に加えて `offsetX` を持ち、タイル sprite へ転送する前に両方を引く。`width()` はタイル幅ではなく面幅を返す。それ以外の §10 の仕組みは軸に依存しない：§10.1 の「メモリ予算 > 分割数 > タイルスパン」の優先順位は*分割軸方向のスパン*を決めるものとして働き（行なら `setTileHeight`、列なら `setTileWidth`。内部は同一の設定）、端数の最終タイルは従来どおりクリップ矩形が吸収し、差分転送（§21）とダブルバッファ（§10.5）はタイル形状によらずそのまま機能する。

**軸ごとの予算計算。** 行分割では予算を面の行ストライドで割ってタイル高を得る。列分割ではまず `regionH` 行分を賄う必要があるので、1行あたりに残るバイト数をタイル幅に換算する：`tileW = floor(予算 / regionH * 8 / bits)`。行の場合と同様、1画素分のスパンも買えない予算は丸めずに確保失敗（§10.3）として扱う。

**唯一一般化できないのはテキスト。** LovyanGFX が次の2つの挙動を *sprite 自身の幅* で解決しているため：

- **折り返し。** `setTextWrap(true)` は sprite の右端で折り返す。行分割ではタイルが面幅いっぱいなので面の右端と一致し正しいが、列分割ではタイル境界で折り返してしまい出力が分割数に依存する。したがって**列分割では X 方向の折り返しを強制的に無効化する**（タイル sprite を折り返し無効で生成し、`LGFXVirtualCanvas::setTextWrap` も無効のまま保つ）。長い行は折り返さずクリップされる。`setTextScroll` も同様。
- **改行。** `LGFXBase::write` は `'\n'` でカーソルを sprite の `x = 0` に戻すが、列分割ではそれは*タイル*の左端である。そのためカーソル系のテキスト経路（`write`・文字列の `print`・`println`・`printf`・`vprintf`）はすべて小さなヘルパを通し、行送り自体は sprite のフォントメトリクスで正確に進めさせたうえで、カーソルを面の `x = 0` に戻す。行分割では `offsetX` が 0 なので単なる転送となり、挙動は変わらない。

### 10.9 PSRAM 上のタイルバッファ

`setUsePsram(true)`（既定 off）は `LGFX_Sprite::setPsram` を使ってタイル sprite を PSRAM に確保する。想定用途は大画面：内蔵RAM既定のままだとフルHD面は数百タイルに分割され、その全部で描画コールバックが再実行される（§10.6）。遅いメモリと引き換えに `N` を大幅に減らせる選択肢には価値がある。

- **速度のトレードであって無条件の勝ちではない。** PSRAM への描画も、転送のための読み出しも内蔵RAMよりはっきり遅い。巨大な PSRAM タイル1枚が内蔵RAMの小タイル多数に勝つのはコールバックが律速のときだけで、転送律速なら負ける。実測すること（`bench/`）。
- **DMA が効かない。** LovyanGFX は SPIRAM 上の sprite を DMA 無効で push するため転送は同期になり、2枚目のバッファが重ねる相手が存在しない。よって確保されたタイルが PSRAM 上にある場合、*auto* のダブルバッファは **off** に解決される（§10.5）。明示的な `setDoubleBuffer(true)` は従来どおり尊重する。
- **ここではフォールバックを許容する。** PSRAM が無い/空きが足りない場合、LovyanGFX は内蔵RAMに確保し、描画は成功する。これは §10.3 に対する意図的な例外：分割数やタイルサイズのフォールバックと違い、変わるのは速度だけで、出力も呼び出し側が要求したジオメトリも変わらない。逆に PSRAM 非搭載ボードで失敗させると、この設定は可搬なスケッチで使い物にならなくなる。実際にどちらに確保されたかは `tileIsPsram()` が返す（ESP32 以外では常に `false`）。要求したかどうかは `usePsram()`。

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
| T1-13 | 差分転送 | 差分の有無で出力が一致（＝転送最適化に留まる）。未変化フレームで転送 0、変化タイルのみ転送、`invalidate()` の挙動、`LGFXVirtualSprite` の移動で無効化、`Off` は確保 0（§21.9） |
| T1-14 | 分割軸 | 列タイルが全分割数で行タイルと同じ画像になること（単一/ダブルバッファ両方、カーソル系テキストシーンを含む）（§10.8） |
| T1-15 | 列分割の予算 / PSRAM | 列分割ではメモリ予算がタイル*幅*を決めること（最大幅・面の全高・整合するタイル数）。PSRAM 要求で確保が失敗せず、`tileIsPsram()` が実際の確保先を報告すること（§10.8, §10.9） |

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
- タイル単位より細かい差分転送の粒度（走査線単位・格子単位。差分転送自体は §21 で規定し、粒度の細分化は将来の拡張＝§21.8）
- ライブラリ側での更新エリア指定・タイルごとの描画スキップ（差分転送が削減するのは転送のみ。矩形を限って描き直す用途は §7.1 の `LGFXVirtualSprite` で足りる。§21.1）
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
- `LGFXVirtualScreen screen(lcd);` の最小記述で動く（遅延確保・デフォルトは ≈ 19 KB/タイル予算＋auto ダブルバッファ）。
- `setMemoryLimit()` で RAM 上限を指定できる。
- `render(draw)` が使える。
- `render(draw, state)` が使える。
- 基本図形と文字描画ができる。
- 分割数を変えても結果が一致する（全面描画＝split:1 と分割描画の PNG 比較テストが通る）。
- GitHub Actions でテストできる。

## 21. 差分転送（部分更新最適化）

前フレームから変化していないタイルの転送を省略する任意機能。**既定は無効**であり、有効化しない限りメモリも CPU も一切消費しない。

### 21.1 目標設定と割り切り

FullHD 級のパネルでは全画面分のフレームバッファを持てないため、本ライブラリの方式（小さなタイルバッファでの分割描画）が前提になる。その上で「毎フレーム全画面を転送するのは無駄が大きい。少しでも減れば良い」という**割り切った最適化**として位置づける。厳密な最小差分エンジンは作らない。

- 削減できるのは**転送のみ**。描画コールバックは全タイルで従来どおり走り、さらにハッシュ計算分が増える。描画律速の構成では効果が出ない（§10.6 の描画律速 vs 転送律速がそのまま当てはまる）。
- **細かい更新エリアの指定はライブラリの責務にしない。**「この矩形だけ描き直して全部転送する」は §7.1 の `LGFXVirtualSprite` で既に実現できる（座標がスプライトローカルになるだけ）。差分転送は、そこまで作り込まずに済ませたい場合の横着な代替手段である。
- PSRAM 無しを前提にサイズを見積もる。ハッシュ表がタイルバッファに匹敵するようでは本末転倒。

### 21.2 粒度：タイル単位のみ

差分の単位は**タイル 1 枚**とする。タイル内をさらに分割する粒度（走査線単位・格子単位）は将来の拡張とし（§21.8）、初回リリースには含めない。

タイル単位で十分である理由：

タイル高は内部 RAM 予算で決まるため、大きなパネルでは自動的に小さくなる。1920×1080 / 16bpp では：

| 1 枚あたり予算 | ダブルバッファ合計 | tileH | タイル数 | タイル単位の粒度 |
|---|---|---|---|---|
| 19 KB（既定） | 38 KB | 5 行 | 216 | 画面の 1/216 |
| 64 KB | 128 KB | 17 行 | 64 | 画面の 1/64 |
| 128 KB | 256 KB | 34 行 | 32 | 画面の 1/32 |

**タイルバッファが小さいほどタイル単位の粒度は細かくなる。**つまりフレームバッファを持てない＝本機能が欲しい状況ほど、タイル単位で十分な粒度が得られる。

さらに、**ハッシュ計算コストは粒度に依存しない**（どの粒度でも全画素を 1 回走査する）。タイル単位より細かくして追加で得られるのはスキップ率だけで、コストは変わらない。逆に言えば、タイル単位で元が取れない構成では細かい粒度にしても取れない。タイル単位のみを先に規定するのは、**この機能自体の有効性を最小コストで検証できる**からである。

採算の目安（1920×1080 / 16bpp = 4.1 MB/フレーム）：

- ハッシュ計算：約 13 ms（240 MHz、§21.4）
- 全画面転送：SPI 40 MHz で約 830 ms、USB 高速転送（実効 150 Mbps）で約 220 ms
- → **2 割スキップできればハッシュ計算のコストを 3 倍以上で回収できる**

並列 RGB のように転送が十分速い経路ではハッシュ計算が相対的に重くなる。だからこそ既定は無効とし、効果を実測できる観測 API（§21.3）を用意する。

### 21.3 API

```cpp
/// 差分転送の粒度。将来 Row / Grid / Auto を追加する（§21.8）。
enum class LGFXVirtualDiffMode : uint8_t
{
    Off,   ///< 差分なし（既定）。ハッシュ表を確保しない
    Tile,  ///< タイル単位。前フレームと変化していないタイルの転送を省略する
};

// LGFXVirtualTiledBase（LGFXVirtualScreen / LGFXVirtualSprite 共通）
void setDiffMode(LGFXVirtualDiffMode mode);   // 既定 Off
LGFXVirtualDiffMode diffMode(void) const;

void invalidate(void);                        // §21.5
size_t diffMemoryUsage(void) const;           // ハッシュ表の実バイト数（Off なら 0）

// 観測（直前の render() の結果）
uint32_t diffPushedPixels(void) const;
uint32_t diffTotalPixels(void) const;
```

- 型名は既存のグローバル名（`LGFXVirtualCanvas` / `LGFXVirtualScreen` / `LGFXVirtualSprite`）に合わせて **`LGFXVirtualDiffMode`** とし、他ライブラリとの衝突を避ける。値は `enum class` でスコープされるので `LGFXVirtualDiffMode::Tile` の形で書く。`None` ではなく `Off` を使う（マクロ定義されている環境があり得る／内部の `DBMode { Auto, Off, On }` と語彙が揃う）。
- **`setDiffUpdate(bool)` のような有効/無効フラグは設けない。** bool と粒度 enum を併存させるとスイッチが 2 つになり、「無効かつ Row 粒度」のような説明不能な状態が生まれる。真実は 1 つにする。
- `setDiffMode()` は他の設定 setter と同様に再確保フラグを立て、次回 `render()` で反映する（§10.2）。`Off` にしたらハッシュ表は解放する（既定が本当に無コストであることを守る）。
- ハッシュ表は**内部 RAM に確保する**（毎フレーム全走査するため PSRAM に置く意味が薄い）。確保に失敗した場合は §10.3 の方針どおり `begin()` が `false` を返す。**黙って差分無効に落とすフォールバックはしない。**

### 21.4 ハッシュ

**1 タイルあたり 8 バイト。32bit FNV-1a を 2 レーン（偶数ワード / 奇数ワード）に分けて回す。**

- この方式なら**乗算回数は 32bit 版と同じまま状態が 64bit になる**。依存チェーンが 2 本に分かれるので、パイプライン上はむしろ有利。実質 CPU コスト増なしで衝突確率 2^-64 が得られる。
- メモリは 1920×1080 / 既定予算（216 タイル）で 1.7 KB。タイルバッファ 38 KB に対して無害。
- ハッシュ幅は API に現れない（`diffMemoryUsage()` の数値が変わるだけ）ため、**後から変更できる可逆な決定**である。RAM が苦しい場合は 4 バイト（1 レーン）に落とせる。

幅の根拠（失敗モード込み）：

衝突すると「変化したのに転送されない」が起きる。**そのタイルが次に変化するまで古い絵が残り続ける。**アニメーションしている領域は次フレームで自然に治るが、**一度変化してから静止するもの（時計が 08:59→09:00 になり、その後 1 分間静止する等）が最悪ケース**で、これは UI では普通に起こる。

- **2 バイトは採用しない。** 2^-16 = 1/65536。毎秒数百〜数千回比較するため 100 秒に 1 回程度の頻度で発生する。
- **4 バイトは実用上十分。** タイル単位で数十日に 1 回程度。
- **8 バイトはほぼ無料**（上記）。これにより「衝突の自己修復（毎フレーム一定数のタイルを強制転送して巡回する）」という追加機構が不要になる。仕様と実装をひとつ減らせることが決め手である。

関数の選択：

- ESP32 ROM の `crc32_le` はテーブル方式で約 8 cycle/byte。4.1 MB では論外。
- FNV-1a 系（`h = (h ^ w) * 16777619`）は load + xor + mul で約 3 cycle/word。**位置依存**なので内容の入れ替えやシフトも検出できる（単純な XOR や加算和は順序に鈍いため不適）。
- タイル単位ではタイルバッファを完全に逐次走査するので、上記の見積りが素直に出る。
- **24bpp では 1 行のバイト数が 4 の倍数にならない。端数バイトを取りこぼさないこと**（取りこぼすと「変化が検出されない」という最悪の壊れ方をする）。

### 21.5 ハッシュの無効化（自動／手動）

差分転送は「送らない＝パネルが前回の絵を保持している」前提で成立する。前提が崩れたらハッシュを捨てる。

#### 自動で無効化する契機

一般ルールを 1 本にする：

> **再確保フラグが立つ操作（§10.2）では、必ずハッシュも捨てる。**

これで `begin()`・初回 `render()`・tileH / タイル数の変化・`setMemoryLimit()` / `setSplitCount()` / `setTileHeight()` / `setDoubleBuffer()` / `setDiffMode()` の変更が自動的にカバーされる。加えて安全側に倒して次も無効化する：

- `setBackgroundColor()` / `setAutoClear()` の変更
- `LGFXVirtualSprite` の位置変更（移動先の下地は別物）
- **毎 `render()` でパネルの rotation / width / height / colorDepth を前回値と比較し、変化していたら無効化する。** 整数比較が数回だけでコストはほぼゼロであり、「ユーザーがパネルを触った」ケースのうち最も多いパターンを無償で拾える。

`setAutoClear(false)` との併用は問題ない。ハッシュは「これから転送する最終的な画素内容」に対して取るので、バッファに前タイルの残骸が残っていても判定は正しい。

#### 自動では検知できない契機（手動が必要な理由）

- ユーザーが `lcd.fillRect()` 等でパネルへ直接描画した
- 別の `LGFXVirtualScreen` / `LGFXVirtualSprite` が重なって上書きした
- パネルのスリープ・リセット・再初期化で GRAM の内容が失われた
- **USB ディスプレイの再接続やリンク断で受信側が絵を失った**（本機能の主な想定用途で現実的に起こり得る）

#### 手動 API：`invalidate()`

「パネルの現在の内容がもう信用できない」という意味をそのまま表す名前を採る。差分が無効なときは何もしない安全な呼び出しとし、ユーザーがモードを気にせず無条件に呼べる形にする。契約は一文で表せる：

> **このオブジェクト以外が画面に触ったら `invalidate()` を呼ぶ。**

`clearDiffCache()` のほうが字面は正確だが、将来 §21.8 の「描画前の継ぎ目」に手を付けたときも同じ語で通るため `invalidate()` を採用する。矩形指定版 `invalidateRect()` は純粋な追加なので将来の拡張とする。

#### 無効状態の表し方（実装方針）

無効状態を「番兵ハッシュ値」で表してはならない。番兵 S を保存して「S と異なれば転送する」とすると、実際の内容のハッシュが偶然 S になったときに転送されない。しかもそれが起きるのは**ユーザーが明示的に更新を要求した瞬間**であり、筋が悪い。

初回リリースは面全体の真偽値 1 個で表す（最も単純）。`invalidateRect()` を導入する段で、1 タイル 1 ビットのビットマップに格上げする（216 タイルでも 27 バイト）。

### 21.6 転送方法

転送は**クリップ矩形を絞ってから `pushSprite()` する**という 1 形式に統一する。

LovyanGFX 1.2.21 で確認済み：`LGFX_Sprite::push_sprite()` → `LGFXBase::pushImage()` はクリップ矩形に合わせて `dw` / `dh` と `param->src_x32` / `src_y` を詰めてから `_panel->writeImage()` を呼ぶ（`LGFXBase.cpp:1425-1445`）。したがって**クリップ矩形を絞れば、実際にパネルへ送られるのは交差部分だけ**であり、`pixelcopy_t` を自前で組む必要はない。

- `startWrite()` / `endWrite()` は参照カウントなので、1 タイルで複数回 push してもバス・トランザクションは 1 本に保たれる（§10.4 の方針を維持）。
- シングルバッファ時の `waitDMA()` は、そのタイルの最後の転送の後に 1 回だけ行う（§10.5）。
- 転送をスキップしたタイルでは `pushSprite()` も `waitDMA()` も行わない。

### 21.7 不変条件

粒度や実装は後から変えてよいが、以下は仕様として固定する。後から変えると既存利用者が壊れる。

1. **`invalidate()` の契約（§21.5）。** 粒度は後から変えても誰も困らないが、「直接描画したら `invalidate()` が必要」を後から追加すると、それまでに書かれた利用者のコードが黙って壊れている状態になる。
2. **有効化 API は粒度 enum 1 本（§21.3）。** 真偽値フラグと併存させない。
3. **差分転送はタイル形状（tileH / タイル数）を変えない。** 将来タイル内を縦方向にまとめる粒度を入れる際に tileH を丸めたくなるが、それを許すと `setMemoryLimit()` / `setSplitCount()` の意味が差分の有無で変わってしまう。**合わない場合は差分側のパラメータを調整する。**
4. **出力画素は一切変わらない。** 差分転送は転送最適化に限る。これにより §13.2 と同じ PNG 一致テストで担保でき、粒度を後から自由に変えられる。

### 21.8 将来の拡張点

タイル描画ループ（§10.4）における継ぎ目は 2 箇所で、互いに直交する。

1. **描画前**：このタイルを描くか／どのクリップで描くか → 将来の「更新エリア指定」「タイルごと描画スキップ」
2. **描画後**：描き終えたタイルのうち何を転送するか → 差分転送（粒度は任意）

**初回リリースは 2 のみに手を付け、1 には触れない。**将来 1 に手を付ける際は、タイルバッファ側に `LGFX_Sprite::setClipRect()` を掛けることで**描画コストも削減できる**点が要点になる（差分転送は原理的に転送しか削減できない）。

2 の実装は、粒度を後から細かくできるよう次の形にしておく：

- 転送の決定結果を**矩形のリスト**として表現する（初回は 0 個か、タイル全体の 1 個のみ）。真偽値にすると細粒度化で書き直しになる。
- ハッシュ状態を**（タイル番号, ブロック番号）の配列**として持つ（初回はブロック数 1）。細粒度化はブロック数が変わるだけになる。

将来の追加候補（いずれも純粋な追加）：

- `LGFXVirtualDiffMode::Row`（全幅走査線単位）／`Grid`（横 n 分割）／`Auto`（tileH から粒度を自動選択）
- 転送矩形の縦マージ・隙間ブリッジ・全面転送閾値といった整形処理
- `invalidateRect()`
- 描画前の継ぎ目（更新エリア指定・タイルごと描画スキップ）
- 転送層にランレングス圧縮が入る場合のチューニング。判断は一貫して「ブロックは大きめ・矩形は少なめ・できれば全幅」になる見込み（行単位エンコードでは部分幅の矩形がランを切る）。ただし圧縮がよく効く内容は元々転送が安く差分の旨味も小さいため、実測で判断する
- 矩形レンダータイル（列方向の分割）。4K / 高ビット深度が要件になるまで不要。タイル枚数は形状に依らず「面積 / メモリ上限」で決まり、全幅バンドが最も連続転送に向くため転送効率では不利になる
- PSRAM 有りを前提とした厳密差分（前フレームの実バッファ保持）

### 21.9 テスト方針

§13 の枠組みに乗せる。

- **出力不変（最重要）**：同じ 2 フレームを差分無効／有効で描画し、2 枚目の PNG が一致すること。ホストパネルはフレームバッファを保持するので、「転送しなかった部分が前フレームの絵のまま正しい」ことがそのまま検証できる。§13.2 の「分割数を変えても結果が一致する」と同じ思想。
- **スキップ量**：同一内容を 2 回描画 → `diffPushedPixels() == 0`。1 タイルだけ変化 → そのタイル分だけ。
- **境界条件**：`height % tileH != 0`、1 タイル構成、`LGFXVirtualSprite::setPosition()` による移動後、色深度 8bpp / 24bpp（§21.4 の端数バイト）。
- **無効化**：パネルへの直接描画後、`invalidate()` なしでは差分が残ること（＝仕様どおりの挙動）と、`invalidate()` 後に復元することの両方。

## 付録: LGFXBase と `LGFXVirtualCanvas` の対応表（表形式）

以下は LovyanGFX 側（`LGFXBase` / `LGFX_Sprite` 等）の主要 API と、`LGFXVirtualCanvas` 側での対応状況を表形式で示したものです。
このリリース時点で、タイル化された仮想 surface から安全に提供できる wrapper は
一通り対応済みとして扱います。具体的には、座標付き draw/write API、テキスト、
画像 decode / push helper、色・状態 helper、tile 内読み戻しです。除外している
関数群は単なる backlog ではなく、設計上そのまま公開しないものです。


| LovyanGFX API 群 | `LGFXVirtualCanvas` / 管理側 マッピング | 対応 | 備考 |
|---|---:|:--:|---|
| ジオメトリ（`width`, `height`） | `LGFXVirtualCanvas` | 対応 | 仮想 surface 全体のサイズを返す |
| 色ユーティリティ（`color332`, `color565`, `color888`, `swap565`, `swap888`, 変換系） | `LGFXVirtualCanvas` static helper | 対応 | LGFX 互換の色変換 helper へ転送 |
| 状態 / palette / pivot / gradient helper | `LGFXVirtualCanvas` | 対応 | 状態は現在の tile sprite が持つ。pivot Y は仮想座標へ変換 |
| 基本図形・拡張図形・座標付き `write*` 描画 | `LGFXVirtualCanvas` | 対応 | public wrapper は必要な箇所で仮想 Y を tile Y に補正 |
| gradient / smooth / wide / spot 描画 | `LGFXVirtualCanvas` | 対応 | sprite clip で tile 外を捨てる。代表ケースは parity test で確認 |
| 画像 push / decode / QR / grayscale / alpha | `LGFXVirtualCanvas` | 対応 | decode helper は使えるが、高コストな decode は原則 callback 外で行うことを推奨 |
| 読み戻し（`readPixel`, `readPixelRGB`, `readPixelValue`, `readRectRGB`, `readRect`） | `LGFXVirtualCanvas` | 対応 | 仮想 Y を tile Y に補正して現在 tile から読む |
| テキスト・メトリクス | `LGFXVirtualCanvas` | 対応 | cursor Y は仮想座標へ相互変換。font metrics は tile へ委譲 |
| window / clip / 転送 / DMA 制御 | `LGFXVirtualTiledBase` | 対応（管理側） | renderRegion がパネル clip、tile flush、DMA wait 順序を管理 |
| 設定系（`setMemoryLimit` / `setSplitCount` / `setTileHeight` / `setBackgroundColor` / `setAutoClear` / `setDoubleBuffer` / `doubleBuffer` / `isReady` / `tileCount` / `tileHeight`） | `LGFXVirtualTiledBase` / `LGFXVirtualScreen` / `LGFXVirtualSprite` | 対応 | タイル解決・確保・auto-clear / double-buffer 方針を管理 |

採用しない関数群：

- 低レベルのストリーミング描画（`writeColor`, `pushBlock`, `writePixels`,
  `writePixelsDMA`, `pushPixels`, `pushPixelsDMA`, `pushColor`, `pushColors`）は、
  呼び出し側が管理する write window / stream cursor に依存し、安全な tile
  単位クリップに必要な仮想座標情報を十分に持たない。`writePixel`,
  `writeFastHLine`, `writeFastVLine`, `writeFillRect`,
  `writeFillRectPreclipped` のような座標付き write API は、仮想 Y を安全に変換
  できるため対応する。`writeFillRectPreclipped` は caller の preclip を信用せず、
  VirtualCanvas 内では clip される `fillRect` 相当として扱う。
- window / clip / transaction 制御（`setWindow`, `startWrite`, `endWrite`,
  `beginTransaction`, `endTransaction`, `initDMA`, `waitDMA`）は tiled manager
  が所有する。callback canvas に公開すると、管理側の clip と DMA 順序保証を
  ユーザーコードが壊せてしまう。
- スクロール・コピー（`scroll`, `copyRect`, scroll-rect 系 API）は、
  source/destination が tile 境界をまたぐと別 tile のピクセルが必要になる。
  callback canvas は現在の tile band しか持たない。
- Sprite 転送 helper（`pushSprite`, `pushRotated`, `pushRotatedWithAA`,
  `pushRotateZoom`, `pushRotateZoomWithAA`, `pushAffine`, `pushAffineWithAA`）は
  `LGFX_Sprite` 自体を別 destination へ転送する API。`LGFXVirtualCanvas` は
  すでに描画先であり、tile 転送は管理側の責務。
- affine 画像 helper（`pushImageAffine`, `pushImageAffineWithAA`,
  `pushGrayscaleImageAffine`）は destination 座標が affine 行列内に埋め込まれる。
  単純な仮想 Y offset 補正ではすべての行列に対して正しくならない。
- 出力・エクスポート（`createPng`, `releasePngMemory`）は現在の tile buffer を
  対象にするため、仮想 surface 全体の出力にはならない。必要なら将来、
  管理側 API として追加する。

注記：
- `対応` は現状の `src/LGFXVirtualCanvas.h` の実装とライブラリ設計に基づく判定です。上記の「採用しない関数群」は単なる実装漏れではなく、タイル化された仮想 surface にそのまま公開すると意味や安全性が崩れるため、対応一覧から意図的に外しています。
