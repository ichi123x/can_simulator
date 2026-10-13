# Contract: シミュレーションログ（出力）

> **機能 008 による変更**: 最後の列名を `Update_Count`（旧 `Overwrite_Count`）に変更。Status に `OVERWRITE`
> （送信・受信バッファが満杯で捨てたフレーム。送信側は受信ノードと送受信時刻、受信側は受信処理時刻が空欄）を追加。
> 集計の末尾に `TxDiscard_Count,RxDiscard_Count`。契約は `specs/008-frame-buffers/contracts/log-and-summary.md` を参照。

`research.md` の決定に基づき、CSV（UTF-8、ヘッダ行あり）を出力形式とする。1フレーム×受信ノードごとに1行、受信ノードがないフレームは送信側の情報のみで1行出力する（FR-017）。

## 列定義（ヘッダ順）

```
Timestamp_us,CAN_ID,DLC,Tx_Node,Rx_Node,Tx_Request,Tx_Start,Tx_End,Rx_Start,Rx_End,Rx_Process,Bus_Load_Pct,Status,Retry_Count,Overwrite_Count
```

| 列 | 型 | 説明 |
|---|---|---|
| `Timestamp_us` | 整数 | 行の代表時刻（受信処理時刻。受信ノードがない場合は送信完了時刻） |
| `CAN_ID` | 16進文字列 | 例: `0x080` |
| `DLC` | 整数 (0〜8) | |
| `Tx_Node` | 整数 | 送信ノード番号 |
| `Rx_Node` | 整数 or 空 | 受信ノードがない行は空欄 |
| `Tx_Request` / `Tx_Start` / `Tx_End` | 整数（µs） | 送信要求／開始／完了時刻 |
| `Rx_Start` / `Rx_End` / `Rx_Process` | 整数（µs）or 空 | 受信ノードがない行は空欄 |
| `Bus_Load_Pct` | 実数（小数第1位まで推奨） | 直近の集計ウィンドウ（既定100ms）のバス負荷率 |
| `Status` | 文字列 | `SUCCESS` または `ARBITRATION_LOST_RETRY` |
| `Retry_Count` | 整数（0以上） | |
| `Overwrite_Count` | 整数（0以上） | |

## 例（ドメイン仕様書のログ例に準拠）

```csv
Timestamp_us,CAN_ID,DLC,Tx_Node,Rx_Node,Tx_Request,Tx_Start,Tx_End,Rx_Start,Rx_End,Rx_Process,Bus_Load_Pct,Status,Retry_Count,Overwrite_Count
10000290,0x080,8,3,1,10000000,10000000,10000250,10000000,10000250,10000290,8.1,SUCCESS,0,0
10000350,0x080,8,3,5,10000000,10000000,10000250,10000000,10000250,10000350,8.1,SUCCESS,0,0
10000604,0x100,8,1,5,10000000,10000256,10000504,10000256,10000504,10000604,8.3,ARBITRATION_LOST_RETRY,1,0
```

> **注記（機能 002 適用後の実挙動）**: 上記はドメイン仕様書 FR-054 のログ例をそのまま引いたもので、
> スタートアップ制御（`specs/002-node-startup-control/`）を考慮していない。実際の実行では、各ノードが
> 通常動作に入るまでにバス同期時間（11ビット時間＝500 kbps で 22 µs）を要するため、起動直後に
> バスが連続して埋まっている区間の送受信時刻はその分だけ後ろにずれる（バスが一度空いて以降は
> 送信要求時刻どおりに戻る）。列構成・`Status` / `Retry_Count` / `Overwrite_Count` の意味は変わらない。

## 集計サマリ出力（FR-018〜020）

シミュレーション終了時、ログとは別に ID ごと・ノードごと・全体の集計（総送信レイテンシ／送信待ち遅延／フレーム伝送時間／バス負荷率／ジッター の最小・最大・平均・標準偏差）を出力する。ファイル形式・列構成はログと同様に CSV とし、具体的なファイル名・配置場所は`/speckit.tasks` での実装時に決定する。

## 対応する要件

FR-016, FR-017, FR-018, FR-019, FR-020
