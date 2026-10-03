# Data Model: 解析（受信完了時間・CAN バス負荷）

ログファイル・プロジェクトファイルの形式は変えない。以下は CanSim（`LogAnalysis.h`）の値型。時刻と時間はすべて µs（`long long`）。

## LogRecord（フレームログの 1 行）

| 項目 | 元の列 | 内容 |
|---|---|---|
| `CanId` | `CAN_ID` | 16 進（`0x080` など） |
| `TxNode` | `Tx_Node` | 1〜16 |
| `RxNode` | `Rx_Node` | 1〜16、空欄は 0 |
| `TxRequestUs` / `TxStartUs` / `TxEndUs` | `Tx_Request` / `Tx_Start` / `Tx_End` | 空欄は -1 |
| `RxEndUs` | `Rx_End` | 空欄は -1 |
| `Status` | `Status` | `Success` / `RetrySuccess`（`ARBITRATION_LOST_RETRY`）/ `Overwrite` / それ以外は読めない |

`ReadLogRecords(path, records, error)`: 1 行目がヘッダーで、上の列が全部そろっていなければ失敗する。2 行目以降の空行は飛ばす。数値にならない欄があれば、行番号付きで失敗にする。

## LatencyRow（受信完了時間の表の 1 行）

| 項目 | 内容 |
|---|---|
| `TxNode` / `RxNode` | 送信 ECU・受信 ECU（全体の行は 0 / 0） |
| `Count` | 件数 |
| `MinUs` / `MaxUs` | 最小・最大 |
| `MeanUs` | 平均（double） |
| `P95Us` | 95 パーセンタイル（最近順位法: 昇順の `ceil(0.95 × Count)` 番目、1 始まり） |
| `Values` | 時間の一覧（昇順。ヒストグラムの元） |

`LatencyAnalysis AnalyzeLatency(records)`:
- `Rows[0]` は全体の行。続けて、受信のあった（送信, 受信）の組み合わせを、送信 ECU・受信 ECU の番号順に並べる。
- `OverwriteCount` は、Status が `Overwrite` の行の数。
- 時間は `RxEndUs − TxRequestUs`。対象は `RxNode > 0` かつ Status が `Success` / `RetrySuccess` の行。
- 件数 0 の全体の行では、最小・平均・最大・P95 を 0 とする。

## Histogram

| 項目 | 内容 |
|---|---|
| `StartUs` | 最初のビンの左端 |
| `WidthUs` | ビンの幅（1・2・5 × 10^m、1 以上） |
| `Counts` | 各ビンの度数（ビン i は `[StartUs + i × WidthUs, StartUs + (i+1) × WidthUs)`） |

`MakeHistogram(values)`（research R4）:
- 規則: 目安の本数 `clamp(ceil(sqrt(n)), 10, 50)`、幅はきりの良い値に切り上げ、始まりは最小値を幅で切り捨てた値。
- 度数の合計は必ず `values` の件数と等しい（SC-002）。
- `n = 0` なら `Counts` は空。

## BusLoadAnalysis

| 項目 | 内容 |
|---|---|
| `SectionUs` | 10,000（10 ms） |
| `RangeUs` | 0 から、最大の `TxEndUs` を含む区間の終わりまで（送信が無ければ 10,000） |
| `SectionPct` | 区間 i の負荷 = 区間内の使用時間 ÷ `SectionUs` × 100 |
| `AveragePct` | 使用時間の合計 ÷ `RangeUs` × 100 |
| `MaxPct` / `MaxSection` | 区間の負荷の最大と、その区間の番号（同じ値なら最初のもの） |
| `Ecus` | 送信 ECU ごとの `{ Node, FrameCount, BusyUs, UsagePct = BusyUs ÷ RangeUs × 100 }`。ECU の番号順。送信の無い ECU は含めない |
| `TransmissionCount` | 送信の回数（重複をまとめた後） |

`AnalyzeBusLoad(records)`:
- 送信 1 回 = Status が `Overwrite` 以外の行を `(TxNode, CanId, TxStartUs)` でまとめたもの。使用時間は `[TxStartUs, TxEndUs)` で、区間をまたぐ分は分けて足す。
- 決まり: `Σ Ecus.UsagePct = AveragePct`、`Σ SectionPct × SectionUs ÷ RangeUs = AveragePct`（浮動小数の誤差を除く。SC-003）。

## グラフの補助

- `std::vector<double> NiceTicks(double lo, double hi, int approxCount)`: `lo` 以上 `hi` 以下の、1・2・5 × 10^m 刻みの目盛り。
- `std::vector<double> MaxPerColumn(const std::vector<double>& values, int columns)`: `values.size() > columns` のとき、各列に入る区間の最大値（列 c は区間 `[c × n / columns, (c+1) × n / columns)`）。そうでなければそのまま返す。
