# Data Model: 008

## 定数（`CanSim/CanTypes.h`）
- `kTxBufferCount = 16`（Doc FR-090、FR-092）
- `kRxBufferCount = 8`（Doc FR-090、FR-093）

## PendingFrame（変更）
| 項目 | 変更 |
|---|---|
| UpdateCount | `OverwriteCount` から改名（送信待ち中のデータ更新回数、Doc FR-024） |

## Node（追加）
| 項目 | 型 | 内容 |
|---|---|---|
| RxBuffer | std::deque<long long> | 受信バッファ（FIFO）に入っているフレームの受信処理時刻（µs、昇順）。最大 8 |
| TxDiscardCount | int | 送信バッファ満杯で捨てた送信要求の数 |
| RxDiscardCount | int | 受信バッファ満杯で捨てたフレームの数 |

## LogRow（変更）
| 項目 | 変更 |
|---|---|
| UpdateCount | `OverwriteCount` から改名 |
| 各時刻 | -1 は「空欄」（OVERWRITE 行で使用） |
| Status | `"OVERWRITE"` を追加 |

## EnqueueResult（`Network.h`、新規）
`enum class EnqueueResult { Queued, Updated, Discarded }`

## GroupMetrics（変更）
| 項目 | 型 | 内容 |
|---|---|---|
| HasDiscards | bool | Node 行だけ true |
| TxDiscardCount / RxDiscardCount | int | Node 行の破棄数 |
