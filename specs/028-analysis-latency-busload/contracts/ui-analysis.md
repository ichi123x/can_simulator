# UI Contract: 解析メニュー（受信完了時間・CAN バス負荷）

## メニュー

```text
解析(A)
  受信完了時間(R)...   ID_ANALYSIS_LATENCY (32785)
  CAN バス負荷(B)...   ID_ANALYSIS_BUSLOAD (32786)
```

- 「準備中」（`ID_ANALYSIS_PLACEHOLDER`）は、メニューと `Resource.h`・ハンドラ・`OnInitMenuPopup` の該当箇所から取り除く。
- 2 つの項目は、`OnInitMenuPopup` で実行中・中断中はグレー（`idleOnly`）にする。トップレベルの「解析」は、025 の `UpdateTopMenus` で実行中グレーのまま。

## メニューを選んだとき（2 つとも同じ流れ）

```text
InRun() → 何もしない
そのウィンドウが開いている → 前面に出すだけ
m_projectPath が空 → IDS_LOG_NONE_NOPROJECT
ログ（LogPathFor）が無い → IDS_LOG_NONE（プロジェクトのファイル名）
ReadLogRecords が失敗 → IDS_LOG_READ_ERROR（ログのパス）
→ 計算（AnalyzeLatency / AnalyzeBusLoad）して、ウィンドウを new + Open（モードレス、オーナーはメイン）
```

## 受信完了時間のウィンドウ（`CLatencyDlg`、`IDD_ANALYSIS_LATENCY`）

- タイトル: `IDS_LAT_TITLE`「受信完了時間 - %s」（プロジェクトのファイル名）
- 上: 一覧 `IDC_LIST_ANALYSIS`。レポート表示・1 行選択・行全体の選択・グリッド線あり。
  - 列: 送信 ECU｜受信 ECU｜件数｜最小 (µs)｜平均 (µs)｜最大 (µs)｜95% (µs)。見出しは `IDS_LAT_COLUMNS` を `|` で分けて使う。
  - 全体の行は「全体」（`IDS_LAT_ALL`）と表示する。ECU は「ECU 03」の形で、ラベル名は付けない。
  - 平均は小数第 1 位まで、ほかは整数で表示する。
- 中: 説明の 1 行 `IDS_LAT_NOTE`。「送信要求 → 受信完了の時間。再送は届いた 1 回を 1 件。破棄（OVERWRITE）%d 件は含まない。」
- 下: ヒストグラム `CAnalysisGraph`。
  - 棒は青で、行の選択に追従する。最初は全体の行を選ぶ。
  - 横軸は「時間 (µs)」（`IDS_LAT_AXIS_X`）、縦軸は「件数」（`IDS_LAT_AXIS_Y`）で、目盛りと値を付ける。
  - 件数 0 なら「データがありません」（`IDS_ANALYSIS_NO_DATA`）を中央に表示する。

## CAN バス負荷のウィンドウ（`CBusLoadDlg`、`IDD_ANALYSIS_BUSLOAD`）

- タイトル: `IDS_BUS_TITLE`「CAN バス負荷 - %s」
- 上: 要約の 1 行 `IDS_BUS_SUMMARY`。「平均負荷 %s%%　最大負荷 %s%%（%s〜%s ms）　範囲 0〜%s ms（10 ms ごと）」
- 中: 折れ線グラフ `CAnalysisGraph`。
  - 線は青。平均負荷はオレンジの破線で描き、値を添える。
  - 横軸は「時刻 (ms)」（`IDS_BUS_AXIS_X`）、縦軸は「負荷 (%)」（`IDS_BUS_AXIS_Y`、0〜100、20 刻み）。
  - 区間の数が幅のピクセル数より多いときは `MaxPerColumn` で間引く。
- 下: 一覧 `IDC_LIST_ANALYSIS`。列は ECU｜送信フレーム数｜使用率 (%)（`IDS_BUS_COLUMNS`）。使用率は小数第 1 位まで表示する。

## ウィンドウの共通の作り（025 の `CLogViewDlg` と同じ）

- 大きさを変えられる（`WS_THICKFRAME`、最小の大きさあり）。中身は `OnSize` で並べ直す。
- ✖・Esc で `DestroyWindow` する。Enter では閉じない。
- `PostNcDestroy` で、オーナーに次のメッセージを送ってから `delete this` する。
  - 受信完了時間: `WM_APP_LATENCY_CLOSED`（`WM_APP + 6`）
  - CAN バス負荷: `WM_APP_BUSLOAD_CLOSED`（`WM_APP + 7`）
  - 番号は `FrameEditDlg.h` で管理する。
- メインは `m_latencyDlg` / `m_busLoadDlg` を持ち、閉じた通知で nullptr にする。
- `CloseResultWindows()` は、ログ・受信完了時間・バス負荷の 3 つのウィンドウを閉じる。呼ぶ場所は次のとおり。
  - 025 で `CloseLogViewer()` を呼んでいた場所すべて
  - 027 のログクリアの「はい」の後

## リソース

| ID | 値 |
|---|---|
| `IDD_ANALYSIS_LATENCY` / `IDD_ANALYSIS_BUSLOAD` | 134 / 135（`_APS_NEXT_RESOURCE_VALUE` は 136） |
| `IDC_LIST_ANALYSIS` / `IDC_STATIC_ANALYSIS` / `IDC_GRAPH_ANALYSIS` | 1027 / 1028 / 1029（`_APS_NEXT_CONTROL_VALUE` は 1030） |
| `ID_ANALYSIS_LATENCY` / `ID_ANALYSIS_BUSLOAD` | 32785 / 32786（`_APS_NEXT_COMMAND_VALUE` は 32787） |
| `IDS_LAT_TITLE`〜`IDS_ANALYSIS_NO_DATA` | 265〜276（`_APS_NEXT_SYMED_VALUE` は 277） |

文字列は次の 12 個。

- 受信完了時間: `IDS_LAT_TITLE` 265、`IDS_LAT_COLUMNS` 266、`IDS_LAT_ALL` 267、`IDS_LAT_NOTE` 268、`IDS_LAT_AXIS_X` 269、`IDS_LAT_AXIS_Y` 270
- CAN バス負荷: `IDS_BUS_TITLE` 271、`IDS_BUS_SUMMARY` 272、`IDS_BUS_COLUMNS` 273、`IDS_BUS_AXIS_X` 274、`IDS_BUS_AXIS_Y` 275
- 共通: `IDS_ANALYSIS_NO_DATA` 276
