---

description: "Task list for feature 029-tx-wait-jitter-analysis"
---

# Tasks: 解析メニューの送信待ち時間と送信ジッター

**Input**: `/specs/029-tx-wait-jitter-analysis/` の spec.md（Q1 = A、Q2 = A）・plan.md（計算の決まり）・research.md（R1〜R7）・data-model.md・contracts/ui-analysis-wait-jitter.md・quickstart.md

**Tests**: spec はテストの作成を求めていない。CanSim の計算の検証は、既存の慣習どおり `checks.cpp` に項目として足す（T011）。

## Phase 1: Setup

- [X] T001 [P] `MFCApplication1/Resource.h` と `MFCApplication1/MFCApplication1.rc` にリソースを足す（contracts の「リソース」「メニュー」）。
  - `Resource.h`（UTF-8 BOM 付きのまま。`New-Object Text.UTF8Encoding($true)` で読み書きする）:
    - `IDD_ANALYSIS_TXWAIT 136`・`IDD_ANALYSIS_JITTER 137`（`// feature 029`）
    - `ID_ANALYSIS_TXWAIT 32787`・`ID_ANALYSIS_JITTER 32788`
    - `IDS_WAIT_TITLE 278`・`IDS_WAIT_COLUMNS 279`・`IDS_WAIT_NOTE 280`・`IDS_JIT_TITLE 281`・`IDS_JIT_COLUMNS 282`・`IDS_JIT_NOTE 283`・`IDS_JIT_AXIS_Y 284`
    - `_APS_NEXT_RESOURCE_VALUE` 138、`_APS_NEXT_COMMAND_VALUE` 32789、`_APS_NEXT_SYMED_VALUE` 285。
  - `.rc`（UTF-16LE・BOM 付き・CRLF を保つ。`[IO.File]::ReadAllText(path, [Text.Encoding]::Unicode)` と `New-Object Text.UnicodeEncoding($false,$true)` で読み書きし、書いた後に読み直す）:
    - メニュー「解析(&A)」を次の 5 行にする（既存の 2 行の ID は変えない）:
      `MENUITEM "送信待ち時間(&W)...", ID_ANALYSIS_TXWAIT` / `MENUITEM "受信完了時間(&R)...", ID_ANALYSIS_LATENCY` / `MENUITEM "送信ジッター(&J)...", ID_ANALYSIS_JITTER` / `MENUITEM SEPARATOR` / `MENUITEM "CAN バス負荷(&B)...", ID_ANALYSIS_BUSLOAD`
    - ダイアログ `IDD_ANALYSIS_TXWAIT`・`IDD_ANALYSIS_JITTER`: `IDD_ANALYSIS_LATENCY` と同じスタイル・大きさ・フォントで、コントロールは置かない（中身はコードで作る。028 と同じ）。
    - 文字列テーブル（`IDS_BUS_AVERAGE` の次）:
      - `IDS_WAIT_TITLE "送信待ち時間 - %s"`
      - `IDS_WAIT_COLUMNS "CAN ID|送信 ECU|件数|最小 (µs)|平均 (µs)|最大 (µs)|95% (µs)"`（µs は U+00B5）
      - `IDS_WAIT_NOTE "送信要求 → 送信開始の時間（送信 1 回を 1 件。再送は送れた 1 回）。破棄（OVERWRITE）%d 件は含みません。"`
      - `IDS_JIT_TITLE "送信ジッター - %s"`
      - `IDS_JIT_COLUMNS "CAN ID|送信 ECU|周期 (ms)|件数|最小 (µs)|最大 (µs)|標準偏差 (µs)"`
      - `IDS_JIT_NOTE "連続する送信開始の間隔 − 送信要求の間隔（正は遅れ、負は早まり）。標準偏差は件数で割った値です。"`
      - `IDS_JIT_AXIS_Y "ずれ (µs)"`
  - 書いた後に `git diff` で、追加・並べ替えた行以外（BOM・日本語）が変わっていないことを確かめる。
- [X] T002 [P] `MFCApplication1/FrameEditDlg.h` の WM_APP の番号の並びの最後に足す: `const UINT WM_APP_TXWAIT_CLOSED = WM_APP + 8;`・`const UINT WM_APP_JITTER_CLOSED = WM_APP + 9;`（`// Feature 029: sent to the owner when the analysis window has been destroyed.`）。

## Phase 2: Foundational（すべてのストーリーの前提）

- [X] T003 `MFCApplication1/CanSim/LogAnalysis.h/.cpp` に、送信のまとめ方と統計の共通化を足す（data-model の Transmission・共通の統計、research R1・R2）。ASCII のみ。
  - `struct Transmission { unsigned CanId = 0; int TxNode = 0; long long TxRequestUs = -1; long long TxStartUs = -1; };`
  - `std::vector<Transmission> CollectTransmissions(const std::vector<LogRecord>& records, long long& overwriteCount)`:
    - 対象: Status が `Overwrite` 以外、`TxRequestUs >= 0`、`TxStartUs >= 0` の行。`Overwrite` の行は `overwriteCount` に数える。
    - `(TxNode, CanId, TxStartUs)` が同じ行は 1 つにする。数百万行でも速いように、対象の行をベクターに集めて `(CanId, TxStartUs, TxNode)` で `std::sort` し、隣と同じものを除く（`std::set` は使わない）。
    - 並び: `(CanId, TxStartUs)` の昇順。
  - 028 の内部関数 `FillStats(LatencyRow&)` を、値の配列から統計を作る関数に分ける: 無名名前空間の `void ComputeStats(std::vector<long long>& values, long long& count, long long& minUs, long long& maxUs, double& meanUs, long long& p95Us)`（`values` を昇順に並べ替え、P95 は `values[ceil(0.95 n) − 1]`、件数 0 なら 0）。
    `FillStats` はこれを呼ぶだけにし、028 の結果を変えない。

**Checkpoint**: checks の 028 の項目がそのまま通る（統計の共通化で結果が変わらない）。

## Phase 3: User Story 1 - CAN ID ごとの送信待ち時間を見る (Priority: P1) 🎯 MVP

**Goal**: 「解析 → 送信待ち時間」で、全体と CAN ID ごとの表とヒストグラムのウィンドウが開き、行の選択でヒストグラムが切り替わる。

**Independent Test**: quickstart.md の手順 1（W のキー）・3。

- [X] T004 [US1] `MFCApplication1/CanSim/LogAnalysis.h/.cpp` に `AnalyzeTxWait` を足す（data-model の TxWaitRow / TxWaitAnalysis。依存: T003）。
  - `struct TxWaitRow { bool All = false; unsigned CanId = 0; int TxNode = 0; long long Count = 0; long long MinUs = 0; long long MaxUs = 0; double MeanUs = 0.0; long long P95Us = 0; std::vector<long long> Values; };`
  - `struct TxWaitAnalysis { std::vector<TxWaitRow> Rows; long long OverwriteCount = 0; };`
  - `TxWaitAnalysis AnalyzeTxWait(const std::vector<LogRecord>& records)`:
    - `CollectTransmissions` の結果から、待ち時間 `TxStartUs − TxRequestUs` を全体と CAN ID ごとに集め、`ComputeStats` で統計にする。
    - `Rows[0]` は全体の行（`All = true`。送信が無くても必ずある）。続いて CAN ID の昇順。`TxNode` はその CAN ID の送信 ECU（CAN ID は全ノードで一意。もし複数なら最初の送信の ECU）。
- [X] T005 [US1] `MFCApplication1/AnalysisDlg.h/.cpp` に `CTxWaitDlg : public CAnalysisDlgBase`（`IDD_ANALYSIS_TXWAIT`）を足す（依存: T001, T002, T004）。028 の `CLatencyDlg` と同じ作り。
  - コンストラクタ `(CWnd* owner, const CString& projectName, const CanSim::TxWaitAnalysis& analysis)`: `CAnalysisDlgBase(IDD_ANALYSIS_TXWAIT, owner, WM_APP_TXWAIT_CLOSED, projectName)`、`m_initialSize96 = CSize(900, 640)`、`m_minSize96 = CSize(520, 400)`。
  - `CreateContent`:
    - タイトルは `IDS_WAIT_TITLE` をプロジェクト名で整形。
    - 一覧は `CreateList(m_list, IDS_WAIT_COLUMNS, { 90, 90, 80, 100, 100, 100, 100 }, 2)`。
      全体の行は 1 列目 `IDS_LAT_ALL`・2 列目は空、ほかは CAN ID `"0x%03X"`・送信 ECU `"ECU %02d"`。件数・最小・最大・95% は整数、平均は小数第 1 位。
    - 説明の行は `IDS_WAIT_NOTE` を `OverwriteCount` で整形。グラフは `m_graph.Create(this, IDC_GRAPH_ANALYSIS)`。
    - 最初は 0 行目を選び、`ShowRow(0)`。
  - `ShowRow(index)`: 件数 0 なら `SetNoData(IDS_ANALYSIS_NO_DATA)`、それ以外は `SetBars(CanSim::MakeHistogram(row.Values), IDS_LAT_AXIS_X, IDS_LAT_AXIS_Y)`。
  - `LVN_ITEMCHANGED` で選ばれた行を `ShowRow`。`LayoutContent` は `CLatencyDlg` と同じ（一覧は上の 40%、説明 1 行、残りがグラフ）。
- [X] T006 [US1] `MFCApplication1/MFCApplication1Dlg.h/.cpp` に送信待ち時間のメニューを足す（依存: T001, T002, T005）。
  - メンバー `CTxWaitDlg* m_txWaitDlg = nullptr;`、ハンドラ `OnAnalysisTxWait`・`OnTxWaitClosed`（`m_txWaitDlg = nullptr`）。`ON_COMMAND(ID_ANALYSIS_TXWAIT, …)`・`ON_MESSAGE(WM_APP_TXWAIT_CLOSED, …)`。
  - `OnAnalysisTxWait` は `OnAnalysisLatency` と同じ流れ: `InRun()` なら return、開いていれば `SetForegroundWindow` して return、`LoadAnalysisRecords` → `AnalyzeTxWait` → `new CTxWaitDlg(this, ProjectFileName(), 結果)` → `Open()`（失敗なら delete）。
  - `CloseResultWindows` に `if (m_txWaitDlg != nullptr) m_txWaitDlg->DestroyWindow();` を足す。
  - `OnInitMenuPopup` に `pPopupMenu->EnableMenuItem(ID_ANALYSIS_TXWAIT, idleOnly);` を足す。

**Checkpoint**: 「送信待ち時間」のウィンドウが開き、表とヒストグラムが出る。

## Phase 4: User Story 2 - CAN ID ごとの送信ジッターを見る (Priority: P1)

**Goal**: 「解析 → 送信ジッター」で、CAN ID ごとの表と、時刻に沿ったずれのグラフのウィンドウが開く。

**Independent Test**: quickstart.md の手順 1（J のキー）・4・5。

- [X] T007 [US2] `MFCApplication1/CanSim/LogAnalysis.h/.cpp` に `AnalyzeJitter` と `MinMaxPerColumn`（点の版）を足す（data-model の JitterPoint / JitterRow / JitterAnalysis / ColumnRange、research R3・R4。依存: T003）。
  - `struct JitterPoint { long long TimeUs = 0; long long DeviationUs = 0; };`
  - `struct JitterRow { unsigned CanId = 0; int TxNode = 0; long long PeriodUs = 0; long long Count = 0; long long MinUs = 0; long long MaxUs = 0; double StdDevUs = 0.0; std::vector<JitterPoint> Points; };`
  - `struct JitterAnalysis { std::vector<JitterRow> Rows; };`
  - `JitterAnalysis AnalyzeJitter(const std::vector<LogRecord>& records)`:
    - `CollectTransmissions` の結果を CAN ID ごとに（送信開始の昇順で）見て、`k ≥ 1` について `DeviationUs = (Start[k] − Start[k−1]) − (Request[k] − Request[k−1])`、`TimeUs = Start[k]`。
    - `PeriodUs` は `Request[k] − Request[k−1]` の最頻値（同数なら小さい方）。送信が 1 回なら `PeriodUs = 0`・`Count = 0`・`Points` は空。
    - `MinUs`・`MaxUs`、`StdDevUs` は母標準偏差（平均を出してから `sqrt(Σ(x − 平均)² / n)` の 2 回の走査）。件数 0 なら 0。
    - `Rows` は CAN ID の昇順。全体の行は無い。
  - `struct ColumnRange { bool Has = false; long long MinUs = 0; long long MaxUs = 0; };`
  - `std::vector<ColumnRange> MinMaxPerColumn(const std::vector<JitterPoint>& points, long long t0, long long t1, int columns)`:
    - `columns <= 0` なら空。それ以外は `columns` 個を返し、各点を列 `(TimeUs − t0) × columns / (t1 − t0 + 1)`（0〜columns−1 に切る）に入れて、列の最小・最大を記録する。
    - 028 の `MaxPerColumn(const std::vector<double>&, int)` とは引数の型で区別される多重定義にする（028 の関数は変えない）。
- [X] T008 [US2] `MFCApplication1/AnalysisDlg.h/.cpp` の `CAnalysisGraph` に `SetDeviation` を足す（research R5、contracts。依存: T007）。
  - `void SetDeviation(const std::vector<CanSim::JitterPoint>& points, const CString& xLabel, const CString& yLabel)`: `Mode::Deviation`（点が無ければ `Mode::None`）にして描き直す。
  - `Draw` の `Mode::Deviation`:
    - 横軸は 0〜最後の点の時刻（ms。`TimeUs / 1000.0`。最後の点が 0 なら 0〜1）、目盛りは既存の x と同じく `NiceTicks`。
    - 縦軸は `lo = min(最小, 0)`・`hi = max(最大, 0)`（両方 0 なら −1〜1）から `NiceTicks(lo, hi, 5)` を作り、範囲を目盛りの端まで広げる（最初の目盛り < lo なら lo を、最後の目盛り < hi なら 1 刻み足す）。値の文字・格子線は既存の y と同じ。
    - ずれ 0 の線を灰色 `RGB(150,150,150)` の 1 px の実線で描く。
    - 点が描画域の幅以下: 各点を `kLineColor` の 3 px（DPI 換算）の四角で描き、`kLineColor` の 1 px の線で順につなぐ。
    - 幅より多い: `CanSim::MinMaxPerColumn(points, 0, 最後の時刻, 描画域の幅)` の各列で、`Has` の列に最小〜最大の縦線を描き、前の `Has` の列の中ほど（`(min+max)/2`）から今の列の中ほどへ線をつなぐ。
  - 既存の `Mode::Bars`・`Mode::Line` の描き方は変えない。
- [X] T009 [US2] `MFCApplication1/AnalysisDlg.h/.cpp` に `CJitterDlg : public CAnalysisDlgBase`（`IDD_ANALYSIS_JITTER`）を足す（依存: T001, T002, T007, T008）。
  - コンストラクタは `CTxWaitDlg` と同じ形（`WM_APP_JITTER_CLOSED`、900 × 640、最小 520 × 400）。
  - `CreateContent`:
    - タイトルは `IDS_JIT_TITLE`。一覧は `CreateList(m_list, IDS_JIT_COLUMNS, { 90, 90, 90, 80, 100, 100, 110 }, 2)`。
    - 各行: CAN ID `"0x%03X"`、送信 ECU `"ECU %02d"`、周期は ms（`PeriodUs` が 1000 の倍数なら整数、そうでなければ `%.3f`）、件数、最小・最大は整数、標準偏差は `%.1f`。件数 0 なら周期・最小・最大・標準偏差は空欄。
    - 説明の行は `IDS_JIT_NOTE`。グラフは `m_graph.Create(this, IDC_GRAPH_ANALYSIS)`。
    - 行があれば 0 行目を選んで `ShowRow(0)`、無ければ `SetNoData(IDS_ANALYSIS_NO_DATA)`。
  - `ShowRow(index)`: 件数 0 なら `SetNoData`、それ以外は `SetDeviation(row.Points, IDS_BUS_AXIS_X, IDS_JIT_AXIS_Y)`。`LVN_ITEMCHANGED` と `LayoutContent` は `CTxWaitDlg` と同じ。
- [X] T010 [US2] `MFCApplication1/MFCApplication1Dlg.h/.cpp` に送信ジッターのメニューを足す（依存: T006, T009）。
  - メンバー `CJitterDlg* m_jitterDlg = nullptr;`、ハンドラ `OnAnalysisJitter`・`OnJitterClosed`。`ON_COMMAND(ID_ANALYSIS_JITTER, …)`・`ON_MESSAGE(WM_APP_JITTER_CLOSED, …)`。
  - 流れは `OnAnalysisTxWait` と同じ（`AnalyzeJitter` → `new CJitterDlg` → `Open`）。
  - `CloseResultWindows` に `m_jitterDlg` を足し、`OnInitMenuPopup` に `ID_ANALYSIS_JITTER` の `idleOnly` を足す。

**Checkpoint**: 2 つの新しいウィンドウが開き、028 と同じ契機で閉じる。

## Phase 5: User Story 3 - メニューの並びと、解析できないときの通知 (Priority: P2)

**Goal**: メニューの並び・アクセスキーと、028 と同じ通知。

**Independent Test**: quickstart.md の手順 1・2・6・7。

- [X] T011 [US3] `MFCApplication1/MFCApplication1Dlg.cpp` で、次を確かめる（依存: T001, T006, T010）。コードの変更が要らなければ変えない。
  - メニューの並びとアクセスキー（W・R・J・区切り線・B）が `.rc` のとおりで、`UpdateTopMenus` の「解析」（位置 3）のグレーが変わらないこと。
  - 2 つの新しいハンドラが `LoadAnalysisRecords` を通すので、プロジェクトなし・ログなし・読めないの 3 つで 028 と同じ通知が出て、ウィンドウを開かないこと。
  - `CloseResultWindows` が呼ばれる 6 か所（開始・読込み・セッティング・`SetProjectPath` の変更・ログクリア・`OnDestroy`）で、新しい 2 つも閉じること。

## Phase 6: Polish & Cross-Cutting

- [X] T012 [P] `tools/cansim_runner/checks.cpp` に `CheckFeature029()` を足し、`main` から呼ぶ（依存: T003, T004, T007）。ファイルは `g_tempDir` の下に作る。
  - `CollectTransmissions`: 受信 ECU 2 つで同じ送信の行が 2 行・別の送信が 1 行・OVERWRITE が 1 行のログで、送信 2 回・`overwriteCount = 1`・並びが `(CanId, TxStartUs)` 順。
  - `AnalyzeTxWait`（小さなログ）: CAN ID 0x100（待ち 0・10・20）と 0x200（待ち 5）。行の並び 全体 → 0x100 → 0x200、件数・最小・平均・最大・P95 が手計算と一致、全体の件数 4、CAN ID ごとの件数の合計 = 全体。
  - `AnalyzeJitter`（小さなログ、周期 10000 µs）:
    - 送信要求 0・10000・20000・30000、送信開始 0・10050・20000・30120 → ずれ {+50, −50, +120}、周期 10000、件数 3、最小 −50、最大 120、母標準偏差が手計算と一致。点の時刻 = 後の送信開始。
    - 破棄で 1 回抜けた場合（送信要求 0・20000、送信開始 30・20010）→ ずれ −20（抜けた周期をずれにしない）、周期 20000。
    - 送信 1 回だけの CAN ID は件数 0・周期 0。
  - `MinMaxPerColumn`（点の版）: 列ごとに最小・最大が入り、点の無い列は `Has = false`、`columns <= 0` で空。
  - 028 の項目がそのまま通ること（統計の共通化の確認）。
  - 車両サンプル: `sample_vehicle_16ecu.canprj` を 5 秒実行して（028 の項目と同じ書き方）、次を確かめる。
    - 送信待ち時間の全体の件数 = 028 の `AnalyzeBusLoad` の `TransmissionCount`、各行のヒストグラムの度数の合計 = 件数、CAN ID ごとの件数の合計 = 全体（SC-001・SC-002）。
    - `.summary.csv` の `ID` の行の `Jitter_Max_us`・`Jitter_Min_us`・`Jitter_StdDev_us` と、`AnalyzeJitter` の最大・最小・標準偏差が、小数第 2 位で一致する（破棄の無い構成。SC-003）。
- [X] T013 [P] `CLAUDE.md` と `tools/cansim_runner/README.md` を更新する。
  - `CLAUDE.md`:
    - アーキテクチャ節の `AnalysisDlg.h/.cpp` に `CTxWaitDlg` / `CJitterDlg` と `CAnalysisGraph` のずれの描き方、`CanSim/LogAnalysis.h/.cpp` に `CollectTransmissions` / `AnalyzeTxWait` / `AnalyzeJitter` / `MinMaxPerColumn`（点の版）を足す。メインダイアログの説明の「解析」の項目を 4 つにする。
    - 「解析: 受信完了時間・CAN バス負荷（機能 028）」の節の後に、029 の段落を足す（メニューの並び、送信 1 回のまとめ方、待ち時間・ジッターの定義（送信要求の間隔を引く。破棄が無ければ FR-019 と同じ）、周期の求め方、グラフの間引き、`WM_APP + 8` / `+ 9`、`CloseResultWindows`）。
    - 検証ツールの対象の機能の一覧（022〜028）に 029 を足す。
  - `README.md`: 029 の検証項目を足す。
- [X] T014 Debug/Release × x64/x86 の 4 構成をビルドし、次を確かめる（依存: T001〜T013）。
  - 4 構成とも、警告・エラーなしでビルドできる。
  - checks.exe の全項目が PASS する（028 の項目も変わらず）。
  - 起動中のアプリは終了させない。exe が使用中でリンクできなければ、ユーザーに頼む。
- [X] T015 quickstart.md の手動確認 1〜8 を行い、結果を tasks.md の末尾に記録する（依存: T014）。
  - Claude が行うこと: Windows API でメニューを読み出して並びを確かめる（手順 1）。作業用フォルダーにコピーした 16 ECU の車両サンプルのログで、2 つのウィンドウの画面写しを撮る（Release x64 を別に起動し、`Start-Process -PassThru` の ID だけを閉じる）。
  - ダイアログの操作を伴う手順は、ユーザーに依頼する。

## Dependencies & Execution Order

```text
T001 ─┐
T002 ─┤
T003 ─┼─> T004 ─> T005 ─> T006 (US1) ─┐
      └─> T007 ─> T008 ─> T009 ─> T010 (US2) ─> T011 (US3) ─┐
T003/T004/T007 ─> T012 ──────────────────────────────────────┼─> T014 ─> T015
T013 ────────────────────────────────────────────────────────┘
```

- **ストーリーの依存**:
  - US1・US2 の計算（T004・T007）は T003 の後に並行して作れる。
  - US2 のメニュー（T010）は、同じファイルを触る US1 のメニュー（T006）の後に行う。
  - US3 は確認だけ（メニューの並びは T001 で入る）。
- **並行できるもの**:
  - T001・T002・T003（ファイルが別）。
  - T012（checks）と T013（ドキュメント）は、計算が揃えばいつでも。
- `AnalysisDlg.h/.cpp` を触る T005・T008・T009、`MFCApplication1Dlg.h/.cpp` を触る T006・T010 は、それぞれ順番に行う。

### 並行の例

```text
T001 Resource.h / .rc  |  T002 FrameEditDlg.h  |  T003 LogAnalysis.h/.cpp
T004 → T007 (LogAnalysis)  |  T005 → T008 → T009 (AnalysisDlg)  |  T006 → T010 (MFCApplication1Dlg)
T012 checks.cpp  |  T013 docs
```

## Implementation Strategy

- **MVP**: T001〜T006（US1: 送信待ち時間の表とヒストグラム）。028 の部品をほぼそのまま使うので早く動く。
- **順に足す**: US2（T007〜T010: ジッターの計算・ずれのグラフ・ウィンドウ）→ US3 の確認（T011）→ checks（T012）→ ドキュメント（T013）→ 4 構成のビルドと検証（T014）→ 手動確認（T015）。

## 実装結果（2026-10-05）

- 変更（新しいファイルは無し）:
  - `CanSim/LogAnalysis.h/.cpp`: `Transmission`・`CollectTransmissions`、`TxWaitRow`・`TxWaitAnalysis`・`AnalyzeTxWait`、`JitterPoint`・`JitterRow`・`JitterAnalysis`・`AnalyzeJitter`、`ColumnRange`・`MinMaxPerColumn`（点の版）。統計は 028 の `FillStats` を `ComputeStats` に切り出して共通化
  - `AnalysisDlg.h/.cpp`: `CTxWaitDlg`・`CJitterDlg`、`CAnalysisGraph::SetDeviation`（`Mode::Deviation`）。y 軸を `yLo`〜`yHi` に一般化（棒・折れ線は今までどおり 0 から）。一覧・説明・グラフの並べ方を `LayoutListNoteGraph` に共通化し、`CLatencyDlg` もこれを使う（並びは同じ）
  - `MFCApplication1Dlg.h/.cpp`: `OnAnalysisTxWait`・`OnAnalysisJitter`・`OnTxWaitClosed`・`OnJitterClosed`、`m_txWaitDlg`・`m_jitterDlg`、`CloseResultWindows`・`OnInitMenuPopup`
  - `FrameEditDlg.h`（`WM_APP + 8` / `+ 9`）、`Resource.h`（UTF-8 BOM のまま）・`.rc`（UTF-16LE のまま。メニューの並べ替えと区切り線、ダイアログ 2、文字列 7。読み直して LF の混入なし）
  - `tools/cansim_runner/checks.cpp`・`README.md`、`CLAUDE.md`
- T011: コードの変更は不要だった（メニューは `.rc` のとおり、通知は `LoadAnalysisRecords`、閉じる契機は `CloseResultWindows` の 6 か所で新しい 2 つも閉じる）。
- T014: 4 構成（Debug/Release × x64/x86）とも警告・エラーなしでビルドできた。checks.exe は全 216 項目 PASS（029 の 7 項目を追加。028 の項目も変わらず）。基準 CSV とも一致（SC-005）。
  - 車両サンプル 16 ECU・5 秒: 送信 4,015 回（028 のバス負荷と同じ）。ジッターは全 31 の CAN ID で集計 CSV と小数第 2 位まで一致（SC-003）。ジッターが 0 でないのは 0x128 だけ（±178 µs、標準偏差 112.2）。
- T015 のうち Claude が行ったこと（Release x64 を自分で起動し、`Start-Process -PassThru` の ID だけを閉じた。作業用フォルダーにコピーした 16 ECU の車両サンプルを 5 秒実行したログ）:
  - 手順 1: Windows API でメニューを読み出した。「解析」は [0] 送信待ち時間(&W)（32787）・[1] 受信完了時間(&R)（32785）・[2] 送信ジッター(&J)（32788）・[3] 区切り線・[4] CAN バス負荷(&B)（32786）。
  - 手順 3: 送信待ち時間のウィンドウが約 0.15 秒で開いた（SC-004）。先頭が全体（4,015 件）、続いて CAN ID の小さい順、ヒストグラムと説明の行（破棄 0 件）が出る（画面写し）。
  - 手順 4: 送信ジッターのウィンドウが約 0.1 秒で開いた。表の 0x128 を選ぶと、時刻に沿ったずれ（+178 / −178 µs の繰り返し）、0 の線、±200 の目盛りが出る（画面写し）。
  - 手順 2・5〜8（通知、600 秒のログ、前面に出す・閉じる契機、実行中、既存の項目）はユーザーの手動確認を待つ。

## T015 の手動確認の結果（2026-10-05、ユーザー）

- 手順 2（通知）・6（前面に出す・閉じる契機）・8（既存の項目）: 正常。
- 手順 5（600 秒のログ）: **不合格**。送信待ち時間・受信完了時間・送信ジッターのどれも数秒では終わらず、2 分近くかかる。
  - 計測（16 ECU の車両サンプル 600 秒のログ、約 193 万行・197 MB）: ログの読み込み（`ReadLogRecords`）が大半を占める。
    最適化あり（Release 相当）で読み込み 5.5 秒・各解析 0.05〜0.23 秒、最適化なし（Debug 相当）で読み込み 62.6 秒・送信待ち時間と送信ジッターの計算が各 4.6 秒。
- 手順 7（実行中）: 解析の計算中に、メニューの「解析」がグレーにならない（計算の間は画面が止まる）。
  ユーザーの提案: 「解析中」のウィンドウを出し、そのウィンドウに中断ボタンを付けて、ユーザーが中断できるようにする。
- → 029 改訂 1 で対応する（読み込みの高速化と、解析中のウィンドウ・中断）。

## Phase 7: 改訂 1（読み込みの高速化と、解析中のウィンドウ・中断）

spec.md の「改訂 1」の A・B を行う。

- [X] T016 `MFCApplication1/CanSim/LogAnalysis.h/.cpp` の `ReadLogRecords` を速くし、進み具合と中断を受け取れるようにする（改訂 1 A・B）。ASCII のみ。
  - ファイルを 4 MB ずつ読み、行の切れ目と欄の切れ目を自分で探して、文字列を作らずに数値・状態を読む（行ごとの `std::istringstream`・`std::vector<std::string>` をやめる）。ヘッダーの読み方・エラーの内容（行番号、`too few columns` / `bad CAN_ID` / `bad number` / `bad Status`）・結果は今と同じにする。
  - 第 4 引数に `const std::function<bool(long long bytesRead, long long totalBytes)>& progress = nullptr` を足す。4 MB ごとに呼び、false が返ったら読むのをやめて false を返し、`error` を `kLogReadCancelled`（`"cancelled"`、ヘッダーで公開）にする。
  - `records` はファイルの大きさから行数を見積もって `reserve` する。
- [X] T017 `MFCApplication1/CanSim/LogAnalysis.cpp` の `CollectTransmissions` を、Debug でも速くする（改訂 1 A）。
  - CAN ID（0〜0x7FF）ごとの入れ物に分けてから、入れ物ごとに `(TxStartUs, TxNode)` の順か確かめ、並んでいなければその入れ物だけ並べ替え、隣と同じものを除く（ログは時刻の順で、同じ送信の行は続いて並ぶので、普通は並べ替えが要らない）。0x7FF を超える CAN ID は無い前提だが、あれば最後の入れ物に入れて並べ替える。結果（CAN ID の順・送信開始の順）は今と同じ。
- [X] T018 `MFCApplication1/Resource.h`・`MFCApplication1.rc` に解析中のウィンドウを足す（改訂 1 B）。
  - `IDD_ANALYSIS_PROGRESS 138`、`IDC_PROGRESS_ANALYSIS 1030`・`IDC_STATIC_PROGRESS 1031`、`IDS_ANALYZING_READ 285`・`IDS_ANALYZING_CALC 286`・`IDS_ANALYZING_CANCEL 287`。`_APS_NEXT_RESOURCE_VALUE` 139、`_APS_NEXT_CONTROL_VALUE` 1032、`_APS_NEXT_SYMED_VALUE` 288。
  - ダイアログ `IDD_ANALYSIS_PROGRESS`: 220 × 70 DLU、`DS_MODALFRAME | WS_POPUP | WS_CAPTION`（✖ あり: `WS_SYSMENU`）、キャプション「解析中」、`LTEXT IDC_STATIC_PROGRESS`、`CONTROL "", IDC_PROGRESS_ANALYSIS, "msctls_progress32", WS_BORDER`、`PUSHBUTTON "中断", IDCANCEL`。
  - 文字列: `IDS_ANALYZING_READ "ログを読み込んでいます..."`、`IDS_ANALYZING_CALC "集計しています..."`、`IDS_ANALYZING_CANCEL "中断しています..."`。
  - `.rc` は UTF-16LE・BOM 付き・CRLF のまま。`Resource.h` は UTF-8 BOM 付きのまま。
- [X] T019 `MFCApplication1/AnalysisDlg.h/.cpp` に解析中のウィンドウ `CAnalysisProgressDlg`（モーダル、`IDD_ANALYSIS_PROGRESS`）と、裏の仕事との共有の状態 `AnalysisJobState`（`std::atomic<long long> Done, Total`・`std::atomic<int> Phase`（0 読み込み・1 集計）・`std::atomic<bool> Cancel`）を足す（依存: T016, T018）。
  - コンストラクタ `(CWnd* owner, AnalysisJobState& state, std::future<int>& job)`。`OnInitDialog`: オーナーの中央に置き、バーの範囲 0〜1000、50 ms のタイマー。
  - タイマー: `Phase` が 0 なら `Done / Total` をバーに、文字は `IDS_ANALYZING_READ`。1 ならバーを満タン、文字は `IDS_ANALYZING_CALC`。`job` が終わっていたら `EndDialog(IDOK)`（中断を受けていれば `IDCANCEL`）。
  - `OnCancel`（中断ボタン・Esc・✖）: `Cancel = true`、文字を `IDS_ANALYZING_CANCEL`、ボタンを無効にして、`job` が終わるのをタイマーで待つ（すぐには閉じない）。`OnOK` は何もしない。
- [X] T020 `MFCApplication1/MFCApplication1Dlg.h/.cpp` の解析の流れを、裏の仕事 + 解析中のウィンドウにする（依存: T019）。
  - `LoadAnalysisRecords` を `bool RunAnalysis(const std::function<void(const std::vector<CanSim::LogRecord>&)>& analyze)` に置き換える:
    1. プロジェクトなし・ログなしの通知（今と同じ）。
    2. `std::async(std::launch::async, …)` で、`ReadLogRecords`（進み具合で `Done`・`Total` を更新し、`Cancel` なら false を返す）→ `Phase = 1` → `analyze(records)` を行う。結果は 0 = 成功、1 = 読めない、2 = 中断。
    3. 待機カーソルで 500 ms まで待ち、終わっていなければ `CAnalysisProgressDlg` を `DoModal` する（メインウィンドウは操作できない）。
    4. `get()` の結果が 1（例外も 1）なら `IDS_LOG_READ_ERROR`、2 なら何もしない。0 のときだけ true。
  - 4 つのハンドラ（`OnAnalysisLatency`・`OnAnalysisBusLoad`・`OnAnalysisTxWait`・`OnAnalysisJitter`）は、`RunAnalysis([&](const auto& records) { result = CanSim::AnalyzeXxx(records); })` の形にする（C++14 なので引数の型は明示する）。
- [X] T021 `tools/cansim_runner/checks.cpp` に改訂 1 の項目を足す（依存: T016, T017）。
  - 028・029 の既存の項目がそのまま通る（読み込み・まとめ方の結果が変わらない）。
  - 進み具合: 大きめのログ（例: 10 MB 以上）で `progress` が 2 回以上呼ばれ、最後の `bytesRead == totalBytes`。最初の呼び出しで false を返すと、`ReadLogRecords` が false・`error == kLogReadCancelled`。
  - 行がチャンクの境目をまたいでも正しく読める（4 MB を超えるログで、行数と最終行が `std::getline` で数えた結果と一致）。
  - 600 秒のログ（`sample_vehicle_16ecu` を 600 秒実行）の読み込みと 4 つの計算の時間を表示する（PASS/FAIL にはしない）。
- [X] T022 4 構成をビルドし、checks.exe の全項目が PASS することを確かめ、600 秒のログで読み込み + 計算の時間を Release 相当・Debug 相当で計る（改訂 1 A の目標: Release で数秒以内、Debug で 20 秒以内）。`CLAUDE.md`・`README.md` に改訂 1 を書く。
- [X] T023 手動確認（依存: T022）: quickstart.md の 5・7・9〜11 を Release x64 で行う。600 秒のログで 4 項目を開き、解析中のウィンドウが出ること・その間メインを操作できないこと・「中断」で止まり何も開かないこと・小さなログではウィンドウが出ないことを確かめる。Claude は自分で起動したプロセスで画面写しと中断の動作を確かめ、ユーザーにも確認を依頼する。

## 改訂 1 の実装結果（2026-10-05）

- T016〜T021: 実装済み。checks.exe 219 項目すべて PASS（サンプルの基準 CSV との一致を含む）。
- T021 の「600 秒のログの時間」は checks には入れず、別の計測ツールで計った（ログを作るのに約 2 分かかるため）。
- 600 秒・`sample_vehicle_16ecu`（197 MB・1,929,600 行）の時間:

  | 構成 | 読み込み | 受信完了時間 | 送信待ち時間 | 送信ジッター | CAN バス負荷 |
  |------|---------:|-------------:|-------------:|-------------:|-------------:|
  | Release（/O2） | 0.71 秒（以前 5.5 秒） | 0.06 秒 | 0.03 秒 | 0.03 秒 | 0.27 秒 |
  | Debug | 3.5 秒（以前 62.6 秒） | 0.9 秒 | 0.26 秒（以前 4.6 秒） | 0.20 秒 | 3.1 秒 |

  参考: 600 秒・`sample_16ecu`（717 MB・7,200,000 行）は Release で読み込み 1.8 秒、計算は 0.05〜0.55 秒。
- 4 構成のビルド: すべて成功（Debug x64 は、ユーザーが起動中のアプリが exe を開いていて一度 LNK1168 になったため、アプリを閉じてもらってから再ビルドした）。
- T023 の Claude の確認（自分で起動した Release x64 のプロセスだけ）:
  - 717 MB のログで「送信待ち時間」→ 約 0.53〜0.56 秒後に「解析中」が出て、その間メインは無効。0.2 秒後に「中断」→ 約 30 ms で閉じ、解析のウィンドウは開かず、メインは有効に戻る。
  - 同じログで「送信ジッター」→ 「解析中」が出たあと約 1.5 秒で閉じ、「送信ジッター - big.canprj」が開く。
  - `sample_appendix_a` の小さなログで「受信完了時間」→ 「解析中」は出ずに「受信完了時間 - small.canprj」が開く。

## T023 ユーザーの手動確認（2026-10-05、Release x64）

- 5: 正常。解析メニューのすべての項目が数秒程度で開く。
- 7: 正常。シミュレーション中は「解析」がグレー。解析中も（Debug x64 で）メインのメニューの「解析」がグレーになっていることを確認。
- 9: 正常。プログレスバーと「中断」ボタンが出る。
- 10: 正常（Debug x64 で確認）。Release では解析が速く「中断」を押す前に終わるため、読み込みが遅い Debug x64 で行った。「解析中」の間に「中断」・✖ のどちらでも中断できる。
- 11: 正常。
- T023 の確認中に見つかった不具合（Release x64）: 実行中にスピードを +10 付近にすると、シミュレーション時間の欄とログ表示エリアが白くなり、反応が重い。原因は、再生が終わった直後の結果の書き出し（`Logger::WriteCsv`、600 秒・16 ECU で Release 13.6 秒・Debug 93 秒）の間、UI スレッドが止まって描き直しが行われないこと。`WriteLogCsv`（1 つのバッファに数値を直接書く、出力はバイト単位で同じ。Release 0.9 秒・Debug 7.8 秒）と、`FinishReplay` で書き出しの前に `UpdateWindow` で直した。checks 219 項目 PASS、4 構成ビルド成功。
- 上の不具合の修正をユーザーが Release x64 で確認（2026-10-05）: 実行中にスピードを +10 付近にしても画面は白くならず、処理が重くなることも無い。

## Phase 8: Convergence

- [X] T024 `contracts/ui-analysis-wait-jitter.md` を改訂 1 の動きに合わせる。メニューを選んだときの流れを `LoadAnalysisRecords` から `RunAnalysis`（裏の仕事、0.5 秒を過ぎたら「解析中」、「中断」なら何も開かない）に直し、リソースの表に `IDD_ANALYSIS_PROGRESS` 138・`IDC_PROGRESS_ANALYSIS` 1030・`IDC_STATIC_PROGRESS` 1031・`IDS_ANALYZING_READ/CALC/CANCEL` 285〜287 と、次の値 139・1032・288・32789 を書く per spec 改訂 1 B (partial)
