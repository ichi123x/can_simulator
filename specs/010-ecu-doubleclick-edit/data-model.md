# Data Model: 010 ECU ブロックのダブルクリック

新しい保存データはない。既存の型に関数と引数を足し、メインダイアログに編集ウィンドウの状態を 2 つ持たせる。

## ECU ブロック（既存: `CanSim::EcuPlacement`）

| 項目 | 内容 |
|---|---|
| `Number` | ECU 番号 1〜16 |
| `Box` | 96 dpi 基準の箱の矩形（`Left`, `Top`, `Right`, `Bottom`、構成図の左上が原点） |

- 当たり判定の範囲 = `[ScaleToDpi(Left), ScaleToDpi(Right)) × [ScaleToDpi(Top), ScaleToDpi(Bottom))`
- ECU 数 N のとき、`Number` 1〜N の箱だけが存在する（N を超える定義は判定対象外）。

## 追加する関数（`CanSim/BusDiagramLayout.h`）

| 関数 | 入力 | 出力 |
|---|---|---|
| `int ScaleToDpi(int px, int dpi)` | 96 dpi 基準の px（非負）、DPI | `(px * dpi + 48) / 96` |
| `int FindEcuAt(const BusDiagramLayout& layout, int x, int y, int dpi)` | 配置、クライアント座標、DPI | 含まれる箱の `Number`、無ければ 0 |

## データフレーム編集ウィンドウ（既存: `CFrameEditDlg`、モードレス化）

| 追加・変更 | 内容 |
|---|---|
| コンストラクタ引数 `initialEcu`（既定 1） | 開いたときに選ぶ ECU。`1..ecuCount` に収める。ECU 数 0 なら選択なし |
| `int CurrentEcu() const` | 選択欄で選ばれている ECU（表示中の ECU）。無ければ 0 |
| 生成・破棄 | `new` + `Create`（オーナー = メインダイアログ）。閉じる操作は `DestroyWindow`、`PostNcDestroy` でオーナーに通知して `delete this` |
| 入れ子のモーダル中 | 入力画面・削除確認の間、オーナー（メインダイアログ）を無効化する |

## メインダイアログの状態（`CMFCApplication1Dlg` に追加）

| メンバ | 型 | 意味 |
|---|---|---|
| `m_frameEditor` | `CFrameEditDlg*` | 開いている編集ウィンドウ。無ければ `nullptr`（閉じた通知で `nullptr` に戻す） |
| `m_pendingEcu` | `int` | 切り替えの待ちで開く予定の ECU。待ちでなければ 0 |

「編集中」= `m_frameEditor != nullptr || m_pendingEcu != 0`（切り替えの待ちを含む、FR-013・FR-014）。

## 状態遷移

| 状態 | ECU k のダブルクリック | 待ちタイマー（500 ms） | 開始 | 読込み・セッティング | データフレーム編集メニュー | 編集ウィンドウを閉じる |
|---|---|---|---|---|---|---|
| 閉（editor なし、pending 0） | k で開く → 開 | — | 可 | そのまま実行 | ECU 01 で開く → 開 | — |
| 開（editor あり） | k = 表示中: 前面に出す。k ≠ 表示中: 閉じて pending = k、タイマー開始 → 待ち | — | 不可 | 閉じてから実行 → 閉 | 前面に出す | → 閉 |
| 待ち（editor なし、pending ≠ 0） | pending = k（タイマーは延長しない） | pending で開く、pending = 0 → 開 | 不可 | タイマー停止、pending = 0 → 閉、実行 | 何もしない | — |
| 実行中 | 何もしない（機能 007） | — | —（終了のみ） | 不可（機能 007） | 不可（機能 007） | — |
| 開 + 入力画面・削除確認中 | メインウィンドウ無効のため受け付けない | — | 受け付けない | 受け付けない | 受け付けない | — |
