# Contract: ログと集計（008 での変更）

## ログ（`<project>.log.csv`）
```
Timestamp_us,CAN_ID,DLC,Tx_Node,Rx_Node,Tx_Request,Tx_Start,Tx_End,Rx_Start,Rx_End,Rx_Process,Bus_Load_Pct,Status,Retry_Count,Update_Count
```
- 最後の列名を `Overwrite_Count` から `Update_Count` に変更（値の意味は同じ: 送信待ち中のデータ更新回数）。
- `Status` に `OVERWRITE` を追加。

| 行の種類 | Timestamp_us | Rx_Node | Tx_Start / Tx_End | Rx_Start / Rx_End | Rx_Process | Status |
|---|---|---|---|---|---|---|
| 送信バッファ満杯で捨てた送信要求 | 要求時刻 | 空欄 | 空欄 | 空欄 | 空欄 | OVERWRITE |
| 受信バッファ満杯で捨てたフレーム | Rx_End | 受信ノード | 値あり | 値あり | 空欄 | OVERWRITE |
| 通常（従来どおり） | Rx_Process（受信ノードなしは Tx_End） | 受信ノード／空欄 | 値あり | 値あり | 値あり | SUCCESS / ARBITRATION_LOST_RETRY |

## 集計（`<project>.summary.csv`）
- 見出しの末尾に `TxDiscard_Count,RxDiscard_Count` を追加。
- `Node` 行: そのノードの送信側破棄数・受信側破棄数。`ID` 行・`ALL` 行: 空の値。
- OVERWRITE 行は全統計（Count を含む）から除外。
- 破棄が 1 件以上あるノードには、送信した行がなくても `Node` 行を出す。

対応する要件: spec FR-004、FR-006、FR-008〜FR-010。これは機能 001 の `contracts/log-output-format.md` を置き換える部分がある。
