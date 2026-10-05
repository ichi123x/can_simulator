# UI Contract: 解析メニューの送信待ち時間・送信ジッター

## メニュー（Q2 = A）

```text
解析(A)
  送信待ち時間(W)...   ID_ANALYSIS_TXWAIT  (32787)   新規
  受信完了時間(R)...   ID_ANALYSIS_LATENCY (32785)   028
  送信ジッター(J)...   ID_ANALYSIS_JITTER  (32788)   新規
  ─────────────────    MENUITEM SEPARATOR
  CAN バス負荷(B)...   ID_ANALYSIS_BUSLOAD (32786)   028
```

- 新しい 2 項目は、`OnInitMenuPopup` で実行中・中断中はグレー（`idleOnly`）。トップレベルの「解析」は 025 の `UpdateTopMenus`（位置 3）のまま。

## メニューを選んだとき（改訂 1。028 の 2 項目も同じ流れ）

```text
InRun() → 何もしない
そのウィンドウが開いている → 前面に出すだけ
RunAnalysis(analyze) が false → 何もしない
  1. プロジェクトなし（IDS_LOG_NONE_NOPROJECT）・ログなし（IDS_LOG_NONE）→ 通知して false（「解析中」より先）
  2. std::async の裏の仕事: ReadLogRecords（進み具合を AnalysisJobState に入れ、中断の要求で止まる）
     → Phase = 1（集計）→ analyze(records)。結果は 0 成功・1 読めない（例外も）・2 中断
  3. 待機カーソルで 500 ms 待ち、終わっていなければ CAnalysisProgressDlg を DoModal（メインは操作できない）
  4. 1 なら IDS_LOG_READ_ERROR を「解析中」を閉じた後に出して false。2 なら何も出さず false（集計中の中断も何も開かない）
→ AnalyzeTxWait / AnalyzeJitter（の結果） → new CTxWaitDlg / CJitterDlg → Open（失敗なら delete）
```

## 解析中のウィンドウ（`CAnalysisProgressDlg`、`IDD_ANALYSIS_PROGRESS`、改訂 1）

- モーダル。キャプション「解析中」、220 × 70 DLU、オーナーの中央に置く。
- 中身: 文字（`IDC_STATIC_PROGRESS`）・進み具合のバー（`IDC_PROGRESS_ANALYSIS`、範囲 0〜1000）・「中断」ボタン（`IDCANCEL`）。
- 50 ms のタイマーで更新する。読み込み中は `IDS_ANALYZING_READ`「ログを読み込んでいます...」と読んだ割合、集計中は `IDS_ANALYZING_CALC`「集計しています...」とバーを満タンにする。
- 裏の仕事が終わったら自分で閉じる（中断を受けていれば `IDCANCEL`、そうでなければ `IDOK`）。
- 「中断」・Esc・✖ は中断を頼み、文字を `IDS_ANALYZING_CANCEL`「中断しています...」にしてボタンを無効にする。仕事が止まるまで閉じない。Enter では何もしない。

## 送信待ち時間のウィンドウ（`CTxWaitDlg`、`IDD_ANALYSIS_TXWAIT`）

- タイトル: `IDS_WAIT_TITLE`「送信待ち時間 - %s」（プロジェクトのファイル名）
- 上: 一覧。列は `IDS_WAIT_COLUMNS`「CAN ID|送信 ECU|件数|最小 (µs)|平均 (µs)|最大 (µs)|95% (µs)」（件数から右寄せ）。
  - 全体の行は CAN ID の欄に「全体」（028 の `IDS_LAT_ALL`）、送信 ECU は空欄。
  - CAN ID は「0x0A0」（3 桁の 16 進、大文字）、送信 ECU は「ECU 03」。平均は小数第 1 位、ほかは整数。
- 中: 説明の 1 行 `IDS_WAIT_NOTE`「送信要求 → 送信開始の時間（送信 1 回を 1 件。再送は送れた 1 回）。破棄（OVERWRITE）%d 件は含みません。」
- 下: ヒストグラム（028 と同じ `SetBars`。軸は `IDS_LAT_AXIS_X`「時間 (µs)」・`IDS_LAT_AXIS_Y`「件数」）。最初は全体の行を選ぶ。件数 0 なら `IDS_ANALYSIS_NO_DATA`。

## 送信ジッターのウィンドウ（`CJitterDlg`、`IDD_ANALYSIS_JITTER`）

- タイトル: `IDS_JIT_TITLE`「送信ジッター - %s」
- 上: 一覧。列は `IDS_JIT_COLUMNS`「CAN ID|送信 ECU|周期 (ms)|件数|最小 (µs)|最大 (µs)|標準偏差 (µs)」（周期から右寄せ）。
  - 周期は ms で、整数ならそのまま、そうでなければ小数第 3 位まで。標準偏差は小数第 1 位。
  - 件数 0 の行は、周期・最小・最大・標準偏差を空欄にする（送信が 1 回だけ）。
- 中: 説明の 1 行 `IDS_JIT_NOTE`「連続する送信開始の間隔 − 送信要求の間隔（正は遅れ、負は早まり）。標準偏差は件数で割った値です。」
- 下: ずれのグラフ（`SetDeviation`。Q1 = A）。
  - 横軸は時刻（`IDS_BUS_AXIS_X`「時刻 (ms)」）、縦軸はずれ（`IDS_JIT_AXIS_Y`「ずれ (µs)」）。目盛りと値、0 の線。
  - 点（3 px の四角）を折れ線でつなぐ。点が幅より多いときは列ごとの最小〜最大の縦線。
  - 最初は 0 行目（CAN ID の最も小さい行）を選ぶ。件数 0 なら `IDS_ANALYSIS_NO_DATA`。行が無ければ一覧は空でグラフも `IDS_ANALYSIS_NO_DATA`。

## ウィンドウの共通の作り

- `CAnalysisDlgBase` を継承（028）。最初の大きさ 900 × 640、最小 520 × 400 px（96 dpi）。✖・Esc で閉じる。Enter では閉じない。
- `PostNcDestroy` でオーナーに `WM_APP_TXWAIT_CLOSED`（`WM_APP + 8`）・`WM_APP_JITTER_CLOSED`（`WM_APP + 9`）を送る。番号は `FrameEditDlg.h` で管理する。
- メインは `m_txWaitDlg` / `m_jitterDlg` を持ち、閉じた通知で nullptr にする。`CloseResultWindows()` はこの 2 つも閉じる。

## リソース

| ID | 値 |
|---|---|
| `IDD_ANALYSIS_TXWAIT` / `IDD_ANALYSIS_JITTER` | 136 / 137 |
| `IDD_ANALYSIS_PROGRESS`（改訂 1） | 138 |
| `IDC_PROGRESS_ANALYSIS` / `IDC_STATIC_PROGRESS`（改訂 1） | 1030 / 1031 |
| `ID_ANALYSIS_TXWAIT` / `ID_ANALYSIS_JITTER` | 32787 / 32788 |
| `IDS_WAIT_TITLE` / `IDS_WAIT_COLUMNS` / `IDS_WAIT_NOTE` | 278 / 279 / 280 |
| `IDS_JIT_TITLE` / `IDS_JIT_COLUMNS` / `IDS_JIT_NOTE` / `IDS_JIT_AXIS_Y` | 281 / 282 / 283 / 284 |
| `IDS_ANALYZING_READ` / `IDS_ANALYZING_CALC` / `IDS_ANALYZING_CANCEL`（改訂 1） | 285 / 286 / 287 |
| 次の値（`_APS_NEXT_RESOURCE_VALUE` / `_CONTROL_VALUE` / `_SYMED_VALUE` / `_COMMAND_VALUE`） | 139 / 1032 / 288 / 32789 |

ダイアログは 028 と同じく、`.rc` にはスタイルだけを書き、中身（一覧・説明・グラフ）は `OnInitDialog` でコードから作る。解析中のウィンドウ（改訂 1）だけは、文字・バー・ボタンを `.rc` に書く。
