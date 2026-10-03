---

description: "Task list for feature 028-analysis-latency-busload"
---

# Tasks: 解析メニューの受信完了時間（表・ヒストグラム）と CAN バス負荷

**Input**: `/specs/028-analysis-latency-busload/` の spec.md・plan.md（計算の決まり）・research.md・data-model.md・contracts/ui-analysis.md・quickstart.md

**Tests**: spec はテストの作成を求めていない。CanSim の計算の検証は、既存の慣習どおり `checks.cpp` に項目として足す（T013）。

## Phase 1: Setup

- [X] T001 [P] `MFCApplication1/Resource.h` と `MFCApplication1/MFCApplication1.rc` にリソースを足す（contracts の「リソース」）。
  - `Resource.h`:
    - `IDD_ANALYSIS_LATENCY 134`・`IDD_ANALYSIS_BUSLOAD 135`
    - `IDC_LIST_ANALYSIS 1027`・`IDC_STATIC_ANALYSIS 1028`・`IDC_GRAPH_ANALYSIS 1029`
    - `ID_ANALYSIS_LATENCY 32785`・`ID_ANALYSIS_BUSLOAD 32786`
    - `IDS_LAT_TITLE 265`〜`IDS_ANALYSIS_NO_DATA 276`（12 個、contracts の順）
    - `ID_ANALYSIS_PLACEHOLDER` は消す。
    - `_APS_NEXT_*` は、RESOURCE 136・CONTROL 1030・SYMED 277・COMMAND 32787 にする。
  - `.rc` のメニュー: 「解析(&A)」の `MENUITEM "準備中", ID_ANALYSIS_PLACEHOLDER, GRAYED` を次の 2 行に置き換える。
    - `MENUITEM "受信完了時間(&R)...", ID_ANALYSIS_LATENCY`
    - `MENUITEM "CAN バス負荷(&B)...", ID_ANALYSIS_BUSLOAD`
  - `.rc` のダイアログ 2 つ: `IDD_LOG_VIEW` と同じスタイル（`WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME`、フォントも同じ）にする。
    - `IDD_ANALYSIS_LATENCY`: 520 × 360 DLU。次の 3 つのコントロールを置く。
      - `IDC_LIST_ANALYSIS`（`SysListView32`、`LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS | WS_BORDER | WS_TABSTOP`）
      - `IDC_STATIC_ANALYSIS`（`LTEXT`）
      - `IDC_GRAPH_ANALYSIS`（`LTEXT` の置き場。実行時に `CAnalysisGraph` で置き換える）
      - 位置は仮でよい（`OnSize` で並べ直す）。
    - `IDD_ANALYSIS_BUSLOAD`: 560 × 380 DLU。同じ 3 つのコントロールを置く。
  - `.rc` の文字列テーブル（`IDS_LOG_CLEAR_FAILED` の次）に、contracts の 12 個を足す。µs は U+00B5。
    - `IDS_LAT_COLUMNS`: `"送信 ECU|受信 ECU|件数|最小 (µs)|平均 (µs)|最大 (µs)|95% (µs)"`
    - `IDS_BUS_COLUMNS`: `"ECU|送信フレーム数|使用率 (%)"`
    - `IDS_LAT_NOTE`: `"送信要求 → 受信完了の時間（再送は届いた 1 回を 1 件）。破棄（OVERWRITE）%d 件は含みません。"`
    - `IDS_BUS_SUMMARY`: `"平均負荷 %s%%　最大負荷 %s%%（%s〜%s ms）　範囲 0〜%s ms（10 ms ごと）"`
    - そのほか: `IDS_LAT_TITLE "受信完了時間 - %s"`、`IDS_LAT_ALL "全体"`、`IDS_LAT_AXIS_X "時間 (µs)"`、`IDS_LAT_AXIS_Y "件数"`、
      `IDS_BUS_TITLE "CAN バス負荷 - %s"`、`IDS_BUS_AXIS_X "時刻 (ms)"`、`IDS_BUS_AXIS_Y "負荷 (%)"`、`IDS_ANALYSIS_NO_DATA "データがありません"`
  - `.rc` の扱い: UTF-16LE・BOM 付き・CRLF を保つ。PowerShell でエンコーディングを明示して読み書きし、日本語は `[char]` のコードで組み立てる。書いた後に読み直して確かめる。
- [X] T002 [P] `MFCApplication1/MFCApplication1.vcxproj` と `.vcxproj.filters` に、新しいファイルを登録する。`LogViewDlg` と同じ形で並べる。
  - `CanSim\LogAnalysis.h/.cpp`
  - `AnalysisDlg.h/.cpp`
- [X] T003 [P] `MFCApplication1/FrameEditDlg.h` に、WM_APP の番号を足す（既存の番号の並びの最後）。
  - `const UINT WM_APP_LATENCY_CLOSED = WM_APP + 6;`
  - `const UINT WM_APP_BUSLOAD_CLOSED = WM_APP + 7;`

## Phase 2: Foundational（すべてのストーリーの前提）

- [X] T004 `MFCApplication1/CanSim/LogAnalysis.h/.cpp` を新しく作り、ログの読み込みとグラフの補助を置く（data-model の LogRecord・グラフの補助）。
  - 制約: ASCII のみ。MFC・`windows.h` に依存しない（`#include "pch.h"` は既存の CanSim の .cpp と同じく先頭に置く）。
  - `enum class LogStatus { Success, RetrySuccess, Overwrite };`
  - `struct LogRecord { unsigned CanId; int TxNode; int RxNode; long long TxRequestUs, TxStartUs, TxEndUs, RxEndUs; LogStatus Status; };`
    - 空欄の扱い: `RxNode` は 0、時刻は -1。
  - `bool ReadLogRecords(const std::wstring& path, std::vector<LogRecord>& records, std::string& error)`:
    - 1 行目のヘッダーを `,` で分け、`CAN_ID`・`Tx_Node`・`Rx_Node`・`Tx_Request`・`Tx_Start`・`Tx_End`・`Rx_End`・`Status` の列の位置を名前で探す。1 つでも無ければ失敗にする。
    - 2 行目以降: 行末の CR を除き、空行は飛ばす。
    - 値の読み方:
      - `CAN_ID`: `0x` 付きの 16 進
      - ノード・時刻: 10 進の整数。空欄は 0 / -1
      - `Status`: `SUCCESS` / `ARBITRATION_LOST_RETRY` / `OVERWRITE`
    - それ以外の値は失敗にし、`error` に `"line N: ..."` を入れる。
    - ファイルを開けないときも失敗にする。
  - `std::vector<double> NiceTicks(double lo, double hi, int approxCount)`:
    - 刻みは `(hi − lo) / approxCount` を 1・2・5 × 10^m に切り上げた値。
    - `ceil(lo / 刻み) × 刻み` から `hi` 以下までの目盛りを返す。
    - `hi <= lo` なら `{lo}` を返す。
  - `std::vector<double> MaxPerColumn(const std::vector<double>& values, int columns)`:
    - `columns <= 0` または `values.size() <= columns` なら、`values` をそのまま返す。
    - それ以外は、列 c に区間 `[c × n / columns, (c+1) × n / columns)`（整数の割り算）の最大値を入れて返す。
- [X] T005 `MFCApplication1/AnalysisDlg.h/.cpp` を新しく作り、グラフの部品 `CAnalysisGraph : public CWnd` を置く（依存: T002）。
  - 作り方: `CreateOn(CWnd* parent, UINT placeholderId)` で、置き場のコントロールの位置に子ウィンドウを作り、置き場は隠す。
  - 2 つの描き方を持つ。
    - `SetBars(const CanSim::Histogram&, CString xLabel, CString yLabel)`
    - `SetLine(const std::vector<double>& pct, long long sectionUs, double averagePct, CString xLabel, CString yLabel)`
  - `SetNoData(CString text)` で、空の表示（中央に文字）にする。
  - `OnPaint` の描き方:
    - メモリ DC に、白の背景・灰色の枠・軸を描く。
    - 目盛りは `CanSim::NiceTicks` で作り、値の文字（9 pt 程度）を添える。軸の名前も描く。
    - 棒: 青 `RGB(30,110,200)`、縦軸は 0〜最大の度数。
    - 折れ線: 青の 1 px の線、縦軸は 0〜100 固定（20 刻み）。点は `MaxPerColumn(pct, 描画域の幅)` で間引く。
    - 平均: オレンジ `RGB(240,140,0)` の破線と、`"平均 x.x%"` の文字。
  - `OnEraseBkgnd` は TRUE を返す（ちらつき防止）。DPI はデバイスの `LOGPIXELSX` で換算する。

**Checkpoint**: ビルドが通る（ダイアログはまだメニューから開かない）。

## Phase 3: User Story 1 - 受信完了までの時間を表とヒストグラムで見る (Priority: P1) 🎯 MVP

**Goal**: 「解析 → 受信完了時間」で表とヒストグラムのウィンドウが開き、行の選択でヒストグラムが切り替わる。

**Independent Test**: quickstart.md の手順 4・5・7・8。

- [X] T006 [US1] `MFCApplication1/CanSim/LogAnalysis.h/.cpp` に、受信完了時間の集計を足す（data-model の LatencyRow・Histogram。依存: T004）。
  - `struct LatencyRow { int TxNode = 0; int RxNode = 0; long long Count = 0; long long MinUs = 0; long long MaxUs = 0; double MeanUs = 0; long long P95Us = 0; std::vector<long long> Values; };`
  - `struct LatencyAnalysis { std::vector<LatencyRow> Rows; long long OverwriteCount = 0; };`
  - `LatencyAnalysis AnalyzeLatency(const std::vector<LogRecord>& records)`:
    - 対象と時間: `RxNode > 0` かつ Status が Success / RetrySuccess の行。時間は `RxEndUs − TxRequestUs`。
    - `Rows[0]`: 全体の行（TxNode = RxNode = 0）。
    - 続く行: (TxNode, RxNode) の昇順。
    - `Values` は昇順に並べる。
    - 集計: `P95Us = Values[ceil(0.95 × Count) − 1]`。件数 0 の行では、最小・最大・平均・P95 を 0 とする。
    - Status が Overwrite の行は、`OverwriteCount` に数える。
  - `struct Histogram { long long StartUs = 0; long long WidthUs = 1; std::vector<long long> Counts; };`
  - `Histogram MakeHistogram(const std::vector<long long>& sortedValues)`:
    - n = 0: `Counts` は空。
    - max == min: `{min, 1, {n}}`。
    - それ以外:
      - 目安の本数 `k = clamp(ceil(sqrt(n)), 10, 50)`。
      - 幅: `(max − min) / k` を 1・2・5 × 10^m に切り上げ（最小 1）。
      - 始まり: `floor(min / 幅) × 幅`。
      - 本数: `(max − 始まり) / 幅 + 1`。
      - 各値をビン `(v − 始まり) / 幅` に数える。
- [X] T007 [US1] `MFCApplication1/AnalysisDlg.h/.cpp` に、受信完了時間のウィンドウ `CLatencyDlg : public CDialogEx`（`IDD_ANALYSIS_LATENCY`）を足す（依存: T001, T003, T005, T006）。
  - 作りと閉じ方は `LogViewDlg` を参考に、同じにする。
    - コンストラクタ: `(CWnd* owner, CString projectName, CanSim::LatencyAnalysis analysis)`
    - `bool Open()`: `Create` → `ShowWindow`
    - `OnCancel`（Esc・✖）で `DestroyWindow`。`OnOK` は空にする。
    - `PostNcDestroy`: オーナーへ `SendMessage(WM_APP_LATENCY_CLOSED)` を送ってから `delete this`。
  - `OnInitDialog`:
    - タイトルは `IDS_LAT_TITLE` をプロジェクト名で整形したもの。
    - 一覧: 拡張スタイル `LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER`。列は `IDS_LAT_COLUMNS` を `|` で分けたもの（数値の列は右寄せ）。
    - 行: 全体の行は「全体」（`IDS_LAT_ALL`）、それ以外は送信・受信を `"ECU %02d"` で表示する。
    - 数値: 平均は `%.1f`、ほかは整数。
    - 説明の行: `IDS_LAT_NOTE` を `OverwriteCount` で整形する。
    - グラフ: `CAnalysisGraph::CreateOn(this, IDC_GRAPH_ANALYSIS)`。
    - 最初は 0 行目を選ぶ。
  - `LVN_ITEMCHANGED`: 選んだ行の `Values` から `MakeHistogram` を作り、`SetBars(…, IDS_LAT_AXIS_X, IDS_LAT_AXIS_Y)` を呼ぶ。件数 0 なら `SetNoData(IDS_ANALYSIS_NO_DATA)` にする。
  - `OnSize` の並べ方: 余白 8 px。一覧は上の 40%、説明は 1 行の高さ、グラフは残り。最小の大きさは `OnGetMinMaxInfo` で 480 × 360 px にする。
- [X] T008 [US1] `MFCApplication1/MFCApplication1Dlg.h/.cpp` に、受信完了時間のメニューを足す（依存: T001, T003, T007）。
  - メンバー: `CLatencyDlg* m_latencyDlg = nullptr;`
  - ハンドラ: `afx_msg void OnAnalysisLatency();`、`afx_msg LRESULT OnLatencyClosed(WPARAM, LPARAM);`（`m_latencyDlg = nullptr`）。
    - `ON_COMMAND(ID_ANALYSIS_LATENCY, …)` と `ON_MESSAGE(WM_APP_LATENCY_CLOSED, …)` を足す。
  - 共通の下ごしらえ: `bool LoadAnalysisRecords(std::vector<CanSim::LogRecord>& records)`
    - 処理: `m_projectPath` が空なら `IDS_LOG_NONE_NOPROJECT`、ログが無ければ `IDS_LOG_NONE`（プロジェクトのファイル名）を通知して false。
      ここまでは `OnLogViewSimLog` と同じ判定と通知。
    - `CanSim::ReadLogRecords` が失敗したら、`IDS_LOG_READ_ERROR`（ログのパス）を通知して false。
    - 待機カーソル（`CWaitCursor`）を出す。
  - `OnAnalysisLatency` の流れ:
    1. `InRun()` なら return する。
    2. 開いていれば `SetForegroundWindow` して return する。
    3. `LoadAnalysisRecords` → `AnalyzeLatency` → `new CLatencyDlg(this, プロジェクトのファイル名, 結果)` → `Open()`。失敗したら `delete` する。
  - 「準備中」の除去: `OnInitMenuPopup` で `ID_ANALYSIS_PLACEHOLDER` を扱う行があれば消す。代わりに `ID_ANALYSIS_LATENCY` と `ID_ANALYSIS_BUSLOAD` を `idleOnly` にする。
  - `void CloseResultWindows()` を足す（research R7）。
    - 中身: `CloseLogViewer()` を呼び、`m_latencyDlg` と `m_busLoadDlg`（T011 で足す。先に宣言だけしてよい）が nullptr でなければ `DestroyWindow` する。
    - 今 `CloseLogViewer()` を呼んでいる 7 か所と、027 の `OnLogClear` の `CloseLogViewer()` を、すべて `CloseResultWindows()` に置き換える。

**Checkpoint**: 受信完了時間のウィンドウが開き、表とヒストグラムが出る。

## Phase 4: User Story 2 - CAN バスの負荷を見る (Priority: P1)

**Goal**: 「解析 → CAN バス負荷」で、要約・10 ms ごとの折れ線グラフ・ECU ごとの表のウィンドウが開く。

**Independent Test**: quickstart.md の手順 6・7・8・12。

- [X] T009 [US2] `MFCApplication1/CanSim/LogAnalysis.h/.cpp` に、バス負荷の集計を足す（data-model の BusLoadAnalysis。依存: T004）。
  - `struct EcuBusUsage { int Node = 0; long long FrameCount = 0; long long BusyUs = 0; double UsagePct = 0; };`
  - `struct BusLoadAnalysis { long long SectionUs = 10000; long long RangeUs = 10000; std::vector<double> SectionPct; double AveragePct = 0; double MaxPct = 0; int MaxSection = 0; std::vector<EcuBusUsage> Ecus; long long TransmissionCount = 0; };`
  - `BusLoadAnalysis AnalyzeBusLoad(const std::vector<LogRecord>& records)`:
    - 送信 1 回: Status が Overwrite 以外で、`TxStartUs >= 0` かつ `TxEndUs > TxStartUs` の行を、`(TxNode, CanId, TxStartUs)` で重複なくまとめる（`std::set` など）。
    - 範囲: `RangeUs` = 最大の `TxEndUs` を 10000 の倍数に切り上げた値。0 以下なら 10000。
    - 区間: 数は `RangeUs / 10000`。各送信の `[TxStartUs, TxEndUs)` を区間の境で分けて足し、`SectionPct[i] = 使用時間 / 10000 × 100` にする。
    - 平均・最大: `AveragePct = Σ使用時間 / RangeUs × 100`。`MaxPct` と `MaxSection` は最初の最大の区間にする。
    - ECU ごと: `Ecus` は送信のある ECU を番号順に並べ、`UsagePct = BusyUs / RangeUs × 100`。
- [X] T010 [US2] `MFCApplication1/AnalysisDlg.h/.cpp` に、バス負荷のウィンドウ `CBusLoadDlg : public CDialogEx`（`IDD_ANALYSIS_BUSLOAD`）を足す（依存: T001, T003, T005, T009）。
  - 作りと閉じ方は `CLatencyDlg` と同じ。閉じた通知は `WM_APP_BUSLOAD_CLOSED`。
  - 要約: `IDS_BUS_SUMMARY` で整形する。
    - 平均・最大は `%.1f`。
    - 最大の区間は `MaxSection × 10`〜`(MaxSection + 1) × 10` ms。
    - 範囲は `RangeUs / 1000` ms。
  - グラフ: `SetLine(SectionPct, SectionUs, AveragePct, IDS_BUS_AXIS_X, IDS_BUS_AXIS_Y)`。送信が 0 回なら `SetNoData`。
  - 一覧: 列は `IDS_BUS_COLUMNS`。行は `"ECU %02d"`・送信フレーム数・使用率 `%.1f`。
  - `OnSize` の並べ方: 余白 8 px。要約は 1 行、グラフは残りの 60%、一覧は下。最小の大きさは 520 × 400 px。
- [X] T011 [US2] `MFCApplication1/MFCApplication1Dlg.h/.cpp` に、バス負荷のメニューを足す（依存: T008, T010）。
  - メンバー: `CBusLoadDlg* m_busLoadDlg = nullptr;`
  - ハンドラ: `OnAnalysisBusLoad`、`OnBusLoadClosed`。`ON_COMMAND(ID_ANALYSIS_BUSLOAD)` と `ON_MESSAGE(WM_APP_BUSLOAD_CLOSED)` を足す。
  - 流れは `OnAnalysisLatency` と同じ（`LoadAnalysisRecords` → `AnalyzeBusLoad` → `new CBusLoadDlg` → `Open`）。
  - `CloseResultWindows` で `m_busLoadDlg` も閉じることを確かめる。

**Checkpoint**: 2 つのウィンドウが両方開き、閉じる契機ですべて閉じる。

## Phase 5: User Story 3 - 解析できないときに理由がわかる (Priority: P2)

**Goal**: プロジェクトなし・ログなし・読めないの 3 つで、通知が出てウィンドウは開かない。

**Independent Test**: quickstart.md の手順 2・3・11。

- [X] T012 [US3] `MFCApplication1/MFCApplication1Dlg.cpp` の `LoadAnalysisRecords` が、3 つの場合すべてで通知を出し、false を返すことを確かめる（依存: T008）。
  - 確かめ方: コードを読んで確かめる。読めない場合（ヘッダーの無いファイル・数値でない欄）は T013 の checks で `ReadLogRecords` を確かめる。
  - 足りない経路があれば直す。

## Phase 6: Polish & Cross-Cutting

- [X] T013 [P] `tools/cansim_runner/checks.cpp` に `CheckFeature028()` を足し、`main` から呼ぶ（依存: T004, T006, T009）。ファイルは `g_tempDir` の下に作る。
  - 読み込み（`ReadLogRecords`）:
    - 列の順を入れ替えたヘッダーでも読めること。
    - ヘッダーに `Rx_End` が無い、`Status` が `FOO`、時刻が `abc` の 3 つは失敗し、`error` に行番号が入ること。
    - 空欄は `RxNode = 0`・時刻 -1 になること。
  - 受信完了時間（小さなログを文字列で書く）:
    - 送信 ECU 1 → 受信 ECU 2・3（時間 100・200）。
    - 送信 ECU 2 → 受信 ECU 1。同じ組で 20 件（1〜20 µs）にし、P95 = 19 になること。
    - OVERWRITE を 1 行入れる。
    - 確かめること:
      - 行の並び: 全体 → (1,2) → (1,3) → (2,1)。
      - 件数・最小・最大・平均・P95 が手計算と一致すること。
      - `OverwriteCount = 1`。
  - ヒストグラム（`MakeHistogram`）:
    - 0 件で空になること。
    - 同じ値 5 件で 1 本・度数 5 になること。
    - 1〜1000 の 1000 件で、幅がきりの良い値（1・2・5 × 10^m）、本数が 10〜50、度数の合計が 1000 になること。
  - バス負荷（小さなログを文字列で書く）:
    - 送信 1 回 `[0, 250)` と、受信 ECU 2 つで同じ送信の行が 2 行あるもの `[9900, 10150)`。
    - 確かめること:
      - `TransmissionCount = 2`。
      - `RangeUs = 20000`。
      - `SectionPct = {(250 + 100) / 100, 150 / 100}` = `{3.5, 1.5}`。
      - `AveragePct = 500 / 20000 × 100 = 2.5`。
      - ECU ごとの合計が 2.5 になること。
  - 補助:
    - `NiceTicks(0, 100, 5)` が `{0, 20, 40, 60, 80, 100}` になること。
    - `MaxPerColumn` が各列の最大を取ること（`{1, 5, 2, 8, 3, 1}` を 3 列 → `{5, 8, 3}`）。
  - 車両サンプル: `sample_vehicle_16ecu.canprj` を 5 秒実行して `.log.csv` を書き（既存の 004 の項目と同じ書き方）、次を確かめる。
    - 全体の件数 = 受信 ECU のある非 OVERWRITE の行の数。
    - 各行の度数の合計 = 件数。
    - `Σ UsagePct` と区間の平均が `AveragePct` と 1e-6 以内で一致すること。
- [X] T014 [P] `CLAUDE.md` と `tools/cansim_runner/README.md` を更新する。
  - `CLAUDE.md`:
    - アーキテクチャ節に、次の 2 つを足す。
      - `AnalysisDlg.h/.cpp`: `CLatencyDlg` / `CBusLoadDlg` / `CAnalysisGraph`
      - `CanSim/LogAnalysis.h/.cpp`: 読み込み・集計・ビン・負荷・目盛り・間引き
    - 「解析・ログ表示メニュー（機能 025）」の節: 「解析」は 2 項目になった（準備中は無い）。`CloseResultWindows` のことも書く。
    - 028 の段落を足す。内容は、定義（`Rx_End − Tx_Request`、P95 の最近順位法、ビンの規則、10 ms の区間、範囲、送信の重複のまとめ方、集計 CSV の平均バス負荷率とは別の指標であること）。
    - 検証ツールの対象に 028 を足す。
  - `README.md`: 028 の検証項目を足す。
- [X] T015 Debug/Release × x64/x86 の 4 構成をビルドし、次を確かめる（依存: T001〜T014）。
  - 4 構成とも、警告・エラーなしでビルドできる。
  - checks.exe の全項目が PASS する。
  - HEAD のソースの runner で作った基準 CSV と一致する（SC-005）。
  - 起動中のアプリは終了させない。exe が使用中でリンクできなければ、ユーザーに頼む。
- [X] T016 quickstart.md の手動確認 1〜12 を行い、結果を tasks.md の末尾に記録する（依存: T015）。
  - Claude が行うこと:
    - Windows API でメニューを読み出して確かめる（手順 1）。
    - サンプルのログを使い、2 つのウィンドウの画面写しを撮って表示を確かめる（Release x64 を別に起動し、起動したプロセスだけを閉じる）。
  - ダイアログの操作を伴う手順は、ユーザーに依頼する。

## Dependencies & Execution Order

```text
T001 ─┐
T002 ─┼─> T005 ─┐
T003 ─┘         │
T004 ─┬─> T006 ─┴─> T007 ─> T008 (US1) ─┐
      └─> T009 ─────> T010 ─> T011 (US2) ┼─> T012 (US3) ─┐
T004/T006/T009 ─> T013 ──────────────────┤                ├─> T015 ─> T016
T014 ────────────────────────────────────┴────────────────┘
```

- **ストーリーの依存**:
  - US2 は、US1 で作るメインダイアログの共通部分（`LoadAnalysisRecords`・`CloseResultWindows`）を使う。そのため US1 の T008 の後に T011 を行う。
  - 計算（T009）と画面（T010）は、US1 と並行して作れる。
  - US3 は確認だけ。
- **並行できるもの**:
  - T001・T002・T003・T004（ファイルが別）。
  - T006 と T009（同じ `LogAnalysis` の別の関数。続けて行うのが安全）。
  - T013（checks）と T014（ドキュメント）は、計算が揃えばいつでも。

### 並行の例

```text
T001 Resource.h / .rc  |  T002 .vcxproj  |  T003 FrameEditDlg.h  |  T004 LogAnalysis.h/.cpp
T005 → T007 → T010 (AnalysisDlg.h/.cpp)  |  T006 → T009 (LogAnalysis)  |  T013 checks.cpp  |  T014 docs
T008 → T011 → T012 (MFCApplication1Dlg.h/.cpp)
```

## Implementation Strategy

- **MVP**: T001〜T008（US1: 受信完了時間の表とヒストグラム）。
- **順に足す**: US2（T009〜T011）→ US3 の確認（T012）→ checks（T013）→ ドキュメント（T014）→ 4 構成のビルドと検証（T015）→ 手動確認（T016）。

## 実装結果（2026-10-03）

- 変更・追加:
  - 新規: `CanSim/LogAnalysis.h/.cpp`、`AnalysisDlg.h/.cpp`（`.vcxproj`・`.filters` に登録）
  - 変更: `MFCApplication1Dlg.h/.cpp`（`OnAnalysisLatency` / `OnAnalysisBusLoad`、`LoadAnalysisRecords`、`ProjectFileName`、`CloseResultWindows` と既存の `CloseLogViewer` 呼び出し 6 か所の置き換え）
  - 変更: `FrameEditDlg.h`（`WM_APP + 6` / `+ 7`）
  - 変更: `Resource.h`・`MFCApplication1.rc`（メニューの置き換え、ダイアログ 2、文字列 13。.rc は読み直して文字化け・LF 混入が無いことを確認済み）
  - 変更: `checks.cpp`・`README.md`、`CLAUDE.md`
- tasks からの変更点:
  - ダイアログの一覧・説明・グラフは、置き場のコントロールを `.rc` に置かず、`OnInitDialog` でコードから作った（025 の `IDD_LOG_VIEW` と同じ作り）。
  - 2 つのウィンドウの共通部分（位置決め・閉じ方・オーナーへの通知・最小の大きさ）は `CAnalysisDlgBase` にまとめた。
  - グラフの平均の文字のために `IDS_BUS_AVERAGE`（277、「平均」）を足した（`_APS_NEXT_SYMED_VALUE` は 278）。
- 画面写しで見つけて直したこと:
  - 縦軸の名前（「件数」「負荷 (%)」）が一番上の目盛りの値と重なっていた。グラフの上の余白を広げ、軸の名前をその中に置いた。
  - 「平均 19.0%」の文字に折れ線が重なっていた。文字を白い背景で描くようにした。
- T015:
  - ビルド: Release x64・Debug x86・Release x86 は、警告・エラーなしでビルドできた。
    Debug x64 は初回、ユーザーが起動中の Debug 版が exe を使っていてリンク（LNK1168）で止まった（プロセスは終了させていない）。アプリを閉じてもらった後にビルドし直し、警告・エラーなしで通った。
  - checks.exe: 全 209 項目 PASS（028 の 9 項目を追加）。HEAD の runner で作った基準 CSV とも一致した（SC-005）。
- T016（GUI、Release x64、作業用フォルダーにコピーした `sample_vehicle_16ecu.canprj` を 5 秒実行したログ）:
  - 手順 1（メニュー）: Windows API で読み出して確認した。「解析(&A)」の下に「受信完了時間(&R)...」（32785）と「CAN バス負荷(&B)...」（32786）だけがあり、「準備中」は無い。
  - 手順 4（受信完了時間）: 起動時の引数でプロジェクトを開き、メニューのコマンドを送って画面写しで確認した。約 0.25 秒で開いた（SC-004）。
    - 表: 先頭が全体（16,080 件）で、送信 ECU × 受信 ECU の行が番号順に並ぶ。
    - ヒストグラム: 目盛り・値・軸の名前がある。
  - 手順 6（CAN バス負荷）: 同じ方法で確認した。約 0.14 秒で開いた。
    - 要約: 平均 19.0%、最大 30.3%（20〜30 ms）、範囲 0〜5000 ms。
    - グラフ: 10 ms ごとの折れ線と、平均の破線がある。
    - 表: ECU ごとの送信フレーム数と使用率がある。
  - 手順 2・3・5・7〜12（通知、行の選択、大きさの変更、前面に出す、閉じる契機、実行中、ログクリアの後、600 秒のログ）: ユーザーが手動で確認し、問題なかった（2026-10-03）。
