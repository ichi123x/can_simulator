---

description: "Task list for feature 025-analysis-log-menus"
---

# Tasks: 解析・ログ表示メニューの追加とシミュレーションログの表示

**Input**: `/specs/025-analysis-log-menus/` の spec.md（Q1 = A、Q2 = A）・plan.md・research.md（決定1〜7）・data-model.md・contracts/ui-log-viewer.md・quickstart.md

**Tests**: spec はテストの作成を求めていない。行の索引の検証は、既存の慣習どおり `checks.cpp` に項目として足す（T011）。

## Phase 1: Setup

- [X] T001 [P] `MFCApplication1/Resource.h` と `MFCApplication1/MFCApplication1.rc` にリソースを足す（contracts の文言どおり）。
  - `Resource.h`（UTF-8 BOM 付きのまま。`New-Object Text.UTF8Encoding($true)` で読み書きする）:
    - `#define IDD_LOG_VIEW 133	// feature 025`（`_APS_NEXT_RESOURCE_VALUE` を 134）
    - `#define ID_LOG_VIEW_SIMLOG 32780	// feature 025`、`#define ID_ANALYSIS_PLACEHOLDER 32781`、`#define ID_LOG_COPY 32782`、`#define ID_LOG_SELECT_ALL 32783`（`_APS_NEXT_COMMAND_VALUE` を 32784）
    - 文字列 `IDS_LOG_NONE_NOPROJECT 255`、`IDS_LOG_NONE 256`、`IDS_LOG_READ_ERROR 257`、`IDS_LOG_VIEW_TITLE 258`、`IDS_LOG_COPY_TOO_LARGE 259`、`IDS_LOG_MENU_COPY 260`、`IDS_LOG_MENU_SELECT_ALL 261`（`_APS_NEXT_SYMED_VALUE` を 262）
  - `.rc`（UTF-16LE・BOM 付き・CRLF。`[IO.File]::ReadAllText(path, [Text.Encoding]::Unicode)` と `New-Object Text.UnicodeEncoding($false,$true)` で読み書きし、書いた後に読み直す）:
    - `IDR_MAINMENU` の「シミュレーション(&S)」の `END` の後に:
      `POPUP "解析(&A)"` / `MENUITEM "準備中", ID_ANALYSIS_PLACEHOLDER, GRAYED` と、`POPUP "ログ表示(&L)"` / `MENUITEM "シミュレーションログ(&L)...", ID_LOG_VIEW_SIMLOG`
    - ダイアログ `IDD_LOG_VIEW DIALOGEX 0, 0, 600, 400`、`STYLE DS_SHELLFONT | WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MAXIMIZEBOX | WS_MINIMIZEBOX`、
      `CAPTION "シミュレーションログ"`、`FONT 9, "MS UI Gothic"`、コントロールなし（中身は子ウィンドウを作る）
    - 文字列テーブル（`IDS_PROJECT_PATH_LABEL` の次）:
      - `IDS_LOG_NONE_NOPROJECT "プロジェクトファイルがないため、シミュレーションログはありません。\nプロジェクトを保存または読み込んでから「開始」でシミュレーションを実行すると、ログが作られます。"`
      - `IDS_LOG_NONE "プロジェクト「%s」のシミュレーションログはありません。\n「開始」でシミュレーションを実行すると、ログが作られます。"`
      - `IDS_LOG_READ_ERROR "シミュレーションログを読み込めませんでした。\n%s"`
      - `IDS_LOG_VIEW_TITLE "シミュレーションログ - %s"`
      - `IDS_LOG_COPY_TOO_LARGE "選択範囲が大きすぎるため、コピーできません（16 MB まで）。"`
      - `IDS_LOG_MENU_COPY "コピー(&C)"`、`IDS_LOG_MENU_SELECT_ALL "すべて選択(&A)"`
  - 書いた後に `git diff` で、追加した行以外（BOM・日本語のコメント）が変わっていないことを確かめる。
- [X] T002 [P] 新規ファイルを `MFCApplication1/MFCApplication1.vcxproj` と `MFCApplication1/MFCApplication1.vcxproj.filters` に登録する。
  - `ClInclude`: `LogViewDlg.h`、`CanSim\LogFileIndex.h`。`ClCompile`: `LogViewDlg.cpp`、`CanSim\LogFileIndex.cpp`。
  - filters は既存の `FrameEditDlg.h/.cpp`・`CanSim\FlowEffect.h/.cpp` と同じフィルター（ヘッダー ファイル／ソース ファイル）に入れる。filters のエンコーディング（BOM の有無）を保つ。

## Phase 2: Foundational（すべてのストーリーの前提）

- [X] T003 `MFCApplication1/CanSim/LogFileIndex.h/.cpp` を新規作成する（contracts の「CanSim に足すもの」、data-model の規則、research 決定1）。ASCII のみ。`.cpp` は `#include "pch.h"` から始める（他の CanSim と同じ）。
  - `static const int kIndexStride = 256;`
  - `bool Build(const std::wstring& path)`: `std::ifstream(path, std::ios::binary)` で開けなければ false。1 MB ずつ読み、`\n` を数えて行数を数え、`k × 256` 行目の行頭のバイト位置を `m_checkpoints` に記録する（行 0 の位置 0 は必ず入れる）。
    各行の長さ（`\r`・`\n` を除く）の最大を `m_maxLineLength` にする。最後の文字が `\n` でなければ残りも 1 行。空ファイルは 0 行で true。読み込みの途中で失敗したら false にして中身を空にする。
    位置は `long long`（32bit 版でも 2 GB を超えるファイルを扱う。`std::streamoff` で seek する）。
  - `long long LineCount() const`、`int MaxLineLength() const`。
  - `std::vector<std::string> ReadLines(long long first, int count) const`: `[first, first + count)` と `[0, LineCount())` の重なりを返す。
    `m_checkpoints[first / 256]` へ seek し、`first % 256` 行を読み飛ばしてから読む。行末の `\r` を取り除く。ファイルが開けなくなっていたら空を返す。
  - 必要なら `std::wstring LogPathFor(const std::wstring& projectPath)`（`projectPath + L".log.csv"`）も同じヘッダーに置く（research 決定6）。

**Checkpoint**: checks から `LogFileIndex` を呼べる（T011 で検証）。

## Phase 3: User Story 1 - 現在のプロジェクトのシミュレーションログを画面で読む (Priority: P1) 🎯 MVP

**Goal**: 「ログ表示 → シミュレーションログ」で、現在のプロジェクトの `.log.csv` を読み取り専用のテキストで全行表示する。

**Independent Test**: quickstart.md の手順 4〜9・11・12。

- [X] T004 [US1] `MFCApplication1/LogViewDlg.h/.cpp` を新規作成し、自前描画のテキスト表示 `CLogTextView`（`CWnd` 派生）を作る（research 決定2・3、contracts の「ログ表示のウィンドウ」）。
  - `Create(CWnd* parent, const CanSim::LogFileIndex* index)`: `AfxRegisterWndClass(CS_DBLCLKS, 矢印カーソル)` の子ウィンドウ、`WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL`、フォーカスを受ける。
  - フォント: MS Gothic、高さ `-MulDiv(13, dpi, 96)`（ログ表示エリアと同じ）。行の高さ・文字の幅は `GetTextMetrics` の `tmHeight`・`tmAveCharWidth`。
  - 状態: 先頭の行 `m_topLine`（`long long`）、先頭の列 `m_leftCol`、アンカー・キャレット（行・列）、表示行のキャッシュ（直前の範囲と文字列）。
  - `OnPaint`: メモリ DC に白背景、見えている行（`m_topLine` から `clientHeight / lineHeight + 1` 行）を `ReadLines` で読み（キャッシュが同じ範囲なら読まない）、`m_leftCol` から描く。
    選択範囲は `RGB(51,153,255)` の背景・白の文字で描く。`OnEraseBkgnd` は TRUE を返す（ちらつき防止）。
  - スクロール: `UpdateScrollBars`（縦: `nMax = LineCount - 1`、`nPage = 見える行数`。横: `nMax = MaxLineLength`、`nPage = 見える文字数`）を `OnSize` と作成時に呼ぶ。
    `OnVScroll`・`OnHScroll` は `SB_THUMBTRACK`／`SB_THUMBPOSITION` で `GetScrollInfo(SIF_TRACKPOS)` の `nTrackPos` を使う（16bit の `nPos` は使わない）。`OnMouseWheel` は 3 行ずつ。
  - キー（`OnKeyDown`、`OnGetDlgCode` で `DLGC_WANTARROWS | DLGC_WANTCHARS` を返す）: ↑↓←→・PgUp/PgDn・Home/End（行頭・行末）・Ctrl+Home/Ctrl+End でキャレットを動かし、
    見える範囲に入るようにスクロールする。Shift 付きなら選択を広げ、無ければ選択を解く。Ctrl+A で全体、Ctrl+C でコピー。それ以外の文字・Delete・Ctrl+V・Ctrl+X は何もしない（読み取り専用）。
  - マウス: 左ボタンでキャレットとアンカーを置いて `SetCapture`、ドラッグで選択を広げ、表示の外に出たら `SetTimer`（50 ms）で自動スクロール、離して `ReleaseCapture`。
    座標 → (行, 列) は `m_topLine + y / lineHeight`、`m_leftCol + (x + charWidth / 2) / charWidth`（行の長さで切る）。右クリックで `IDS_LOG_MENU_COPY`・`IDS_LOG_MENU_SELECT_ALL` のポップアップ（選択が無ければコピーはグレー）。
  - コピー: 選択範囲の行を `ReadLines` で読み、最初の行は開始列から、最後の行は終了列まで、行の区切りは CRLF で連結して `CF_UNICODETEXT` でクリップボードへ。
    合計が 16 MB（`16 * 1024 * 1024` バイト）を超える見込みなら、読む前に `IDS_LOG_COPY_TOO_LARGE` を出してコピーしない（行数 × (最大行長 + 2) で見積もり、実際の長さでも確かめる）。
- [X] T005 [US1] 同じファイルに、モードレスのウィンドウ `CLogViewDlg`（`CDialogEx` 派生、`IDD_LOG_VIEW`）を作る（依存: T004）。
  - `CLogViewDlg(CWnd* owner, const CString& logPath)`、`bool Open()`: `m_index.Build(logPath)` を待機カーソル（`CWaitCursor`）の中で行い、失敗なら false（ウィンドウは作らない）。成功なら `Create(IDD_LOG_VIEW, owner)`。
  - `OnInitDialog`: タイトルを `IDS_LOG_VIEW_TITLE` を `logPath` で整形したものにし、大きさを 900 × 600 px（`MulDiv(…, dpi, 96)`）にしてオーナーの中央に置き（画面の作業領域に収める）、
    `CLogTextView` をクライアント全体に作ってフォーカスを渡す。
  - `OnSize` で子ウィンドウをクライアント全体に合わせる。`OnOK` は何もしない（Enter で閉じない）。`OnCancel`（Esc・✖）は `DestroyWindow()`。
  - `PostNcDestroy`: オーナーに `::SendMessage(owner, WM_APP_LOG_VIEWER_CLOSED, 0, 0)` を送ってから `delete this`（機能 010 の `CFrameEditDlg` と同じ作り）。
  - `MFCApplication1/FrameEditDlg.h` の `WM_APP_*` の並びに `const UINT WM_APP_LOG_VIEWER_CLOSED = WM_APP + 5;`（`// Feature 025: sent to the owner when the log viewer has been destroyed.`）を足す。
- [X] T006 [US1] `MFCApplication1/MFCApplication1Dlg.h/.cpp` に「シミュレーションログ」のハンドラを足す（依存: T001, T003, T005）。
  - メンバー `CLogViewDlg* m_logViewer = nullptr;`、`void CloseLogViewer();`（開いていれば `DestroyWindow`。通知で nullptr になる）、`afx_msg void OnLogViewSimLog();`、`afx_msg LRESULT OnLogViewerClosed(WPARAM, LPARAM);`（`m_logViewer = nullptr`）。
  - メッセージマップに `ON_COMMAND(ID_LOG_VIEW_SIMLOG, …)` と `ON_MESSAGE(WM_APP_LOG_VIEWER_CLOSED, …)`。
  - `OnLogViewSimLog`: `InRun()` なら何もしない。開いていれば `SetForegroundWindow` して終わり。
    それ以外はログのパス（`CanSim::LogPathFor` または `m_projectPath + _T(".log.csv")`。`WriteResults` の組み立てと同じ式にし、`WriteResults` もそれを使う）を決めて、US2 の判定（T008）を通った後に
    `new CLogViewDlg(this, logPath)` → `Open()` が false なら `delete` して `IDS_LOG_READ_ERROR`（ログのフルパス、`MB_ICONWARNING`）、true なら `m_logViewer` に入れる。
  - `OnCancel`（メインを閉じる）で `CloseLogViewer()` を呼ぶ。

**Checkpoint**: 実行後に「シミュレーションログ」でログが表示され、スクロール・選択・コピーができる。

## Phase 4: User Story 2 - ログが無いことがわかる (Priority: P1)

**Goal**: ログが無いときはウィンドウを開かず、理由を示すメッセージを出す。

**Independent Test**: quickstart.md の手順 2・3。

- [X] T007 [US2] `MFCApplication1/MFCApplication1Dlg.cpp` の `OnLogViewSimLog` で、ウィンドウを作る前に次を判定する（依存: T006。spec FR-005・FR-006、research 決定6）。
  - `m_projectPath` が空 → `AfxMessageBox(LoadRcString(IDS_LOG_NONE_NOPROJECT), MB_OK | MB_ICONINFORMATION)` で終わり。
  - `GetFileAttributes(logPath)` が `INVALID_FILE_ATTRIBUTES` またはフォルダー → `IDS_LOG_NONE` をプロジェクトのファイル名（`m_projectPath` の最後の `\` または `/` より後）で整形して `MB_ICONINFORMATION` で終わり。
  - それ以外は T006 のとおり開く（開けない・読めないときは `IDS_LOG_READ_ERROR`）。
- [X] T008 [US2] `MFCApplication1/MFCApplication1Dlg.cpp` の `OnLogViewSimLog` で、判定の順と、メッセージの後にウィンドウが残らない（`m_logViewer` が nullptr のまま、`new` したものは `delete` 済み）ことを確かめる（依存: T007）。コードの変更が要らなければ変えない。

## Phase 5: User Story 3 - メニューの並びに「解析」「ログ表示」が加わる (Priority: P2)

**Goal**: メニューバーの並びと、実行中・中断中のグレー、表示中のログと現在の状態が食い違わないこと。

**Independent Test**: quickstart.md の手順 1・8・10。

- [X] T009 [US3] `MFCApplication1/MFCApplication1Dlg.cpp` の `UpdateTopMenus` で、位置 0・1 に加えて 3（解析）・4（ログ表示）も `InRun()` ならグレーにする（依存: T001。spec FR-008、research 決定5）。
  - コメントを「File (0), Edit (1), Analysis (3) and Log (4) are greyed as a whole; Simulation (2) stays available」に直す。
  - `OnInitMenuPopup` でも `ID_LOG_VIEW_SIMLOG` を実行中はグレーにする（`idleOnly` と同じ）。`ID_ANALYSIS_PLACEHOLDER` は常に `MF_GRAYED`。
- [X] T010 [US3] ログ表示を閉じる時機を入れる（依存: T006。spec FR-009、research 決定4）。`MFCApplication1/MFCApplication1Dlg.cpp`:
  - `OnSimStart`: 再開（中断中の開始）ではなく新しい実行のときに、保存先の確認より前に `CloseLogViewer()`。
  - `OnFileLoadProject`: `CloseFrameEditor()` の隣で `CloseLogViewer()`（ファイル選択の前）。
  - `OnEditSettings`: `CloseFrameEditor()` の隣で `CloseLogViewer()`。
  - `SetProjectPath`: 新しいパスが今の `m_projectPath` と違う（大文字小文字を区別しない比較）ときに `CloseLogViewer()`（別名で保存・起動時の引数の読込みを含む）。
  - データフレーム編集・スピード・フレーム表示・同じパスへの上書き保存では閉じないことを確かめる。

**Checkpoint**: US3 の受け入れシナリオと FR-008・FR-009 を手で確かめられる。

## Phase 6: Polish & Cross-Cutting

- [X] T011 [P] `tools/cansim_runner/checks.cpp` に `CheckFeature025()` を足し、`main` から呼ぶ（依存: T003）。`#include "LogFileIndex.h"`。一時ディレクトリ（`g_tempDir`）にファイルを作って確かめる。
  - 空ファイル: `Build` が true、行数 0、`ReadLines(0, 10)` が空。存在しないファイル: `Build` が false。
  - LF と CRLF の同じ内容の 1000 行: 行数・各行の内容（`\r` なし）・最長行が一致する。最終行に改行が無い場合も行数が同じ。
  - 索引の境目: 1000 行のファイルで `ReadLines(255, 3)`・`ReadLines(256, 1)`・`ReadLines(998, 10)`（2 行に切られる）・`ReadLines(-5, 3)` 相当が範囲で切られることを確かめる（`first` が負なら 0 から）。
  - サンプルの実行結果: `sample_16ecu.canprj` を 10 秒実行した `.log.csv`（checks が既に出力しているもの、または `Run` + `WriteLog` で作る）で、行数が `std::getline` で数えた数と一致し、先頭行・最終行が一致する。
  - 大きなファイル: 100 万行（各 90 文字程度）の合成ファイルで、`Build` の所要時間を表示し（PASS/FAIL にはしない）、`ReadLines(999990, 20)` が 10 行で最終行が一致することを確かめる。
- [X] T012 [P] `CLAUDE.md` と `tools/cansim_runner/README.md` を更新する。
  - `CLAUDE.md`:
    - アーキテクチャ節: `LogViewDlg.h/.cpp`（`CLogViewDlg`・`CLogTextView`、モードレス、`WM_APP_LOG_VIEWER_CLOSED`）と `CanSim/LogFileIndex.h/.cpp` を足す。メインダイアログの説明のメニューに「解析」「ログ表示」を足す。
    - 機能 025 の段落: メニューの並び、ログの探し方（`<プロジェクトファイル>.log.csv`）とメッセージ、全行表示の方法（256 行ごとの索引 + 見えている行だけを読む）、閉じる時機、実行中のグレー（`UpdateTopMenus` の位置 0・1・3・4）。
    - 検証ツールの対象機能の一覧に 025 を足す。
  - `README.md`: 025 の検証項目を足す。
- [X] T013 Debug/Release × x64/x86 の 4 構成をビルドし、次を確かめる（依存: T001〜T012）。
  - 4 構成とも、警告・エラーなしでビルドできる。
  - checks.exe の全項目が PASS し、サンプルの CSV が基準と一致する（spec SC-005）。
  - ユーザーが起動しているアプリのプロセスは終了させない。exe が使用中でビルドできないときは、ユーザーに閉じてもらうよう頼む。
- [X] T014 quickstart.md の手動確認 1〜13 をユーザーに依頼し、結果を tasks.md の末尾に記録する（依存: T013）。
  - 確認の前に、自分で起動したプロセスだけを使って、メニューの並びとログ表示のウィンドウが開くことを画面写しで確かめてよい（サンプルは一時フォルダーへの複製を使う）。

## Dependencies & Execution Order

```text
T001 ─┐
T002 ─┤
T003 ─┼─> T004 ─> T005 ─> T006 (US1) ─> T007 ─> T008 (US2) ─┐
      │                       └──────> T009 ─> T010 (US3) ───┼─> T013 ─> T014
T003 ─┴─> T011 ──────────────────────────────────────────────┤
T012 ─────────────────────────────────────────────────────────┘
```

- **ストーリーの依存**: US2 の判定は US1 のハンドラ（T006）の中に入るので、US1 の後。US3 のメニューのグレー（T009）は T001 だけに依存するが、閉じる時機（T010）は `CloseLogViewer`（T006）の後。
- **並行できるもの**:
  - T001・T002・T003（ファイルが別）。
  - T003 の後の T011（checks.cpp）と、T004 以降の表示の作業。
  - T012（ドキュメント）は、いつでも。
- T006〜T010 は同じファイル（`MFCApplication1Dlg.cpp`）を触るので、順番に行う。

### 並行の例

```text
# Phase 1・2 の初め
T001 Resource.h / .rc   |  T002 vcxproj / filters   |  T003 LogFileIndex.h/.cpp

# T003 の後
T004 → T005 (LogViewDlg.h/.cpp) → T006 → T007 → T008 → T009 → T010 (MFCApplication1Dlg.cpp)  |  T011 checks.cpp  |  T012 CLAUDE.md / README.md
```

## Implementation Strategy

- **MVP**: T001〜T006（US1: ログの全行表示）。要望の中心で、単独で価値がある（この時点ではログが無いと読めないメッセージになる）。
- **順に足す**: US2（T007・T008: ログが無い旨）→ US3（T009・T010: メニューのグレーと閉じる時機）→ checks（T011）→ ドキュメント（T012）→ 4 構成のビルドと検証（T013）→ 手動確認（T014）。

## 実装結果（2026-10-02）

- 変更:
  - 新規 `CanSim/LogFileIndex.h/.cpp`（`LogFileIndex`、`LogPathFor`）、`LogViewDlg.h/.cpp`（`CLogTextView`・`CLogViewDlg`）。vcxproj・filters に登録（filters は BOM 付きのまま）
  - `MFCApplication1Dlg.h/.cpp`: `OnLogViewSimLog`・`OnLogViewerClosed`・`CloseLogViewer`・`m_logViewer`、`UpdateTopMenus`（位置 3・4）、`OnInitMenuPopup`、
    閉じる箇所（新しい実行の「開始」・読込み・セッティング・`SetProjectPath` でパスが変わるとき・`OnDestroy`）、`WriteResults` のログ名を `LogPathFor` に
  - `FrameEditDlg.h`: `WM_APP_LOG_VIEWER_CLOSED`（`WM_APP + 5`）
  - `Resource.h`（UTF-8 BOM のまま）・`.rc`（UTF-16LE のまま。メニュー・`IDD_LOG_VIEW`・文字列 7 つ。読み直して確認済み）
  - `tools/cansim_runner/checks.cpp`・`README.md`、`CLAUDE.md`
- T008: コードの変更は不要だった（判定 → メッセージで return、`Open` の失敗は `delete` してからメッセージ。`m_logViewer` は成功時だけ入る）。
- T013（ビルド）: Debug/Release × x64/x86 の 4 構成とも警告・エラーなし。
- checks.exe: 全 190 項目 PASS（025 の 6 項目を追加）。基準 CSV とも一致した。
  - sample_16ecu 10 秒のフレームログ: 120,001 行。100 万行（約 85 MB）の索引の作成: 291 ms（sample_16ecu 600 秒の約 700 万行で約 2 秒の見込み）。
- 起動の確認（Release x64、自分で起動したプロセスだけを閉じた。サンプルは一時フォルダーへの複製）:
  - runner で作ったログがある `sample_appendix_a.canprj` を引数に起動し、`ID_LOG_VIEW_SIMLOG` を送ると、タイトル「シミュレーションログ - <ログのフルパス>」のウィンドウにログの先頭から表示された（画面写しで確認）。
  - ログが無い `sample_16ecu.canprj`: 「プロジェクト「sample_16ecu.canprj」のシミュレーションログはありません。…」。プロジェクトファイルなし: 「プロジェクトファイルがないため、…」。どちらもウィンドウは開かない。
- T014 は未実施（ユーザーの手動確認を待つ）。

## T014 の手動確認の結果（2026-10-02、ユーザー）

- 手順 1・2・5〜13: 正常。
- 手順 3: 初回は「プロジェクトを読み込んだだけ」でメッセージを待っていたため出なかった（メッセージはメニューの「シミュレーションログ」を選んだときに出る仕様）。
  quickstart の手順 3・4 の文言を「読み込んだ後、メニューの『ログ表示 → シミュレーションログ』を選ぶと…」に書き直し、やり直して正常を確認した。
- 手順 4: 初回は同じ理由で「メッセージが出ない」と判断された。書き直した手順（コピーした sample_appendix_a.canprj を読込み → スピード +10 で開始 → ログ表示 → シミュレーションログ → メモ帳の .log.csv と最初の行・最後の行・行数を比較）の 1〜5 をやり直し、すべて正常を確認した。
- 確認の途中で、私（Claude）が再現テストの後片付けに「直近 3 分以内に起動した Debug 版」という条件で終了処理をかけ、ユーザーが起動していた Debug 版も閉じた可能性がある（以後は自分で起動したプロセスの ID だけを閉じる）。

## Phase 7: Convergence

- [X] T015 `MFCApplication1/LogViewDlg.cpp` の `CLogTextView::HitTest` で、表示の左の外（x が負）のときの列を `m_left` に固定せず、`m_left + (x - (m_charWidth - 1)) / m_charWidth`（0 未満は 0）にして、横スクロール中に左へドラッグしたときも自動で左へスクロールするようにする per plan: research 決定3（ドラッグで表示の外に出たら自動でスクロール） (partial)

### Convergence の実装結果（2026-10-02）

- T015: `HitTest` で表示の左の外の列を `m_left` より小さくした。4 構成のビルドは警告・エラーなし、checks.exe は全 194 項目 PASS。
  Release x64 を自分で起動し、ログ表示のウィンドウを幅 300 px にして右端までスクロール（横の位置 98）→ 表示の左の外（x = -30）へドラッグすると、横の位置が 0 まで戻ることを確かめた（閉じたのは自分で起動した ID だけ）。
