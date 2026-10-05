# Data Model: 解析メニューの送信待ち時間と送信ジッター

すべて `CanSim/LogAnalysis.h`（028）に追加する。入力は 028 の `LogRecord` の列（`ReadLogRecords` の結果）。

## Transmission（送信 1 回）

| 項目 | 型 | 内容 |
|---|---|---|
| `CanId` | `unsigned` | CAN ID |
| `TxNode` | `int` | 送信 ECU |
| `TxRequestUs` | `long long` | 送信要求の時刻 |
| `TxStartUs` | `long long` | 送信開始の時刻 |

`CollectTransmissions(records, overwriteCount)`:
- 対象: Status が `Overwrite` 以外、`TxRequestUs >= 0`、`TxStartUs >= 0` の行。
- `(TxNode, CanId, TxStartUs)` が同じ行は 1 つにする。
- 並び: `(CanId, TxStartUs)` の昇順。
- `Overwrite` の行の数を `overwriteCount` に返す。

## TxWaitRow / TxWaitAnalysis（送信待ち時間）

| 項目 | 型 | 内容 |
|---|---|---|
| `All` | `bool` | 全体の行なら true |
| `CanId` | `unsigned` | CAN ID（全体の行は 0） |
| `TxNode` | `int` | 送信 ECU（全体の行は 0） |
| `Count`・`MinUs`・`MaxUs`・`MeanUs`・`P95Us` | | 028 の `LatencyRow` と同じ統計（件数 0 なら 0） |
| `Values` | `std::vector<long long>` | 待ち時間の昇順の一覧（ヒストグラムの元） |

`TxWaitAnalysis { std::vector<TxWaitRow> Rows; long long OverwriteCount; }`:
- `Rows[0]` は全体の行（送信が無くても必ずある）。続いて CAN ID の昇順。
- 待ち時間 = `TxStartUs − TxRequestUs`。

## JitterPoint / JitterRow / JitterAnalysis（送信ジッター）

| 項目 | 型 | 内容 |
|---|---|---|
| `JitterPoint::TimeUs` | `long long` | 後の送信の送信開始の時刻（グラフの横軸） |
| `JitterPoint::DeviationUs` | `long long` | `(Start[k] − Start[k−1]) − (Request[k] − Request[k−1])`。正は遅れ、負は早まり |
| `JitterRow::CanId`・`TxNode` | | CAN ID・送信 ECU |
| `JitterRow::PeriodUs` | `long long` | 送信要求の間隔の最頻値（同数なら小さい方）。送信が 1 回なら 0 |
| `JitterRow::Count` | `long long` | ずれの件数（送信の回数 − 1。1 回なら 0） |
| `JitterRow::MinUs`・`MaxUs` | `long long` | ずれの最小・最大（件数 0 なら 0、表では空欄） |
| `JitterRow::StdDevUs` | `double` | 母標準偏差（件数 0 なら 0、表では空欄） |
| `JitterRow::Points` | `std::vector<JitterPoint>` | 時刻の昇順 |

`JitterAnalysis { std::vector<JitterRow> Rows; }`: CAN ID の昇順。全体の行は無い（ずれは CAN ID ごとの周期が前提のため）。

## ColumnRange（ずれのグラフの間引き）

| 項目 | 型 | 内容 |
|---|---|---|
| `Has` | `bool` | その列に点があるか |
| `MinUs`・`MaxUs` | `long long` | 列の中のずれの最小・最大 |

`MinMaxPerColumn(points, t0, t1, columns)`: 時刻 `[t0, t1]` を `columns` 等分し、各点を `(TimeUs − t0) × columns / (t1 − t0 + 1)` の列に入れる。`columns <= 0` なら空。

## 共通の統計（028 から切り出す）

`StatsOf(std::vector<long long>& values)` → 件数・最小・最大・平均・P95（値は昇順に並べ替える）。028 の `AnalyzeLatency` と 029 の `AnalyzeTxWait` が使う。028 の結果は変えない。
