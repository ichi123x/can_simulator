---

description: "Task list for feature 027-log-clear"
---

# Tasks: ログ表示メニューへのログクリア（ログファイルの削除）の追加

**Input**: `/specs/027-log-clear/` の spec.md・plan.md（流れ）・research.md・data-model.md・contracts/ui-log-clear.md・quickstart.md

**Tests**: spec はテストの作成を求めていない。CanSim に足す関数の検証は、既存の慣習どおり `checks.cpp` に項目として足す（T007）。

## Phase 1: Setup

- [X] T001 [P] `MFCApplication1/Resource.h` と `MFCApplication1/MFCApplication1.rc` にリソースを足す。
  - `Resource.h`:
    - `#define IDS_LOG_CLEAR_CONFIRM 262	// feature 027`
    - `#define IDS_LOG_CLEAR_DONE 263`
    - `#define IDS_LOG_CLEAR_FAILED 264`
    - `#define ID_LOG_CLEAR 32784	// feature 027`
    - `_APS_NEXT_SYMED_VALUE` を 265、`_APS_NEXT_COMMAND_VALUE` を 32785 にする。
  - `.rc` のメニュー: 「ログ表示(&L)」の `MENUITEM "シミュレーションログ(&L)...", ID_LOG_VIEW_SIMLOG` の次の行に `MENUITEM "ログクリア(&C)...", ID_LOG_CLEAR` を足す。
  - `.rc` の文字列テーブル（`IDS_LOG_NONE` などの近く）。%s はファイル名を改行区切りで並べたもの:
    - `IDS_LOG_CLEAR_CONFIRM "次のログファイルを削除します。削除したファイルは元に戻せません。\n\n%s\n\nよろしいですか？"`
    - `IDS_LOG_CLEAR_DONE "ログファイルを削除しました。\n\n%s"`
    - `IDS_LOG_CLEAR_FAILED "次のファイルは削除できませんでした。ほかのアプリで開いていないか、読み取り専用でないかを確かめてください。\n\n%s"`
  - `.rc` の扱い: UTF-16LE・BOM 付き・CRLF を保つ。PowerShell でエンコーディングを明示して読み書きし、日本語は `[char]` のコードで組み立てる。書いた後に読み直して、文字化けが無いことを確かめる（CLAUDE.md のビルド節）。

## Phase 2: Foundational（すべてのストーリーの前提）

- [X] T002 `MFCApplication1/CanSim/LogFileIndex.h/.cpp` に結果ファイルの関数を足す（contracts の「CanSim」）。ASCII のみ、MFC・`windows.h` に依存しない。
  - `std::vector<std::wstring> ResultFilePaths(const std::wstring& projectPath)`: `{ LogPathFor(p), p + L".summary.csv", p + L".startup.csv" }` を、この順で返す。
  - `std::vector<std::wstring> ExistingResultFiles(const std::wstring& projectPath)`:
    - `projectPath` が空なら空を返す。
    - それ以外は、上の 3 つのうち `_wstat64` が成功し、かつ `(st_mode & _S_IFMT) == _S_IFREG` のもの（通常のファイル）を、順を保って返す。
  - `struct LogClearResult { std::vector<std::wstring> Deleted; std::vector<std::wstring> Failed; };`
  - `LogClearResult DeleteResultFiles(const std::vector<std::wstring>& paths)`:
    - 各パスに `_wremove` を呼ぶ。0 なら `Deleted`、それ以外なら `Failed` に入れる（入力の順）。
    - 読み取り専用などの属性は変えない（research R2）。
  - インクルードは `<sys/stat.h>`・`<sys/types.h>`・`<cstdio>`（`_wremove`）。
- [X] T003 `MFCApplication1/MFCApplication1Dlg.cpp` の `WriteResults` で、`logPath`・`summaryPath`・`startupPath` を `CanSim::ResultFilePaths(std::wstring((LPCTSTR)m_projectPath))` の 0・1・2 番目から取る。書く名前と消す名前を 1 か所で決めるため（依存: T002）。

**Checkpoint**: ビルドが通り、checks の既存の項目（004 SC-009 の CSV の一致を含む）が変わらず通る。

## Phase 3: User Story 1 - 現在のプロジェクトのログファイルを削除する (Priority: P1) 🎯 MVP

**Goal**: 「ログ表示 → ログクリア」で、確認の後に 3 つの結果ファイルを削除し、ログ表示エリアを空にする。

**Independent Test**: quickstart.md の手順 1・4〜7・10・11。

- [X] T004 [US1] `MFCApplication1/MFCApplication1Dlg.h/.cpp` にログクリアのハンドラを足す（依存: T001, T002）。
  - 宣言とメッセージマップ:
    - `afx_msg void OnLogClear();   // feature 027` を `OnLogViewSimLog` の次に宣言する。
    - `ON_COMMAND(ID_LOG_CLEAR, &CMFCApplication1Dlg::OnLogClear)` をメッセージマップに足す。
  - `OnInitMenuPopup`: `ID_LOG_VIEW_SIMLOG` の次に `pPopupMenu->EnableMenuItem(ID_LOG_CLEAR, idleOnly); // 027` を足す。
  - 無名名前空間に `CString JoinFileNames(const std::vector<std::wstring>& paths)` を足す。各パスの最後の `\` か `/` より後（ファイル名）を、`\n` でつないで返す。
  - `OnLogClear` の流れ（plan.md の「流れ」）:
    1. `InRun()` なら return する。
    2. `m_projectPath` が空なら return する（通知は T005 で足す）。
    3. `existing = CanSim::ExistingResultFiles(std::wstring((LPCTSTR)m_projectPath))`。空なら return する（通知は T005 で足す）。
    4. 確認を出す。`IDS_LOG_CLEAR_CONFIRM` を `JoinFileNames(existing)` で整形し、`AfxMessageBox(…, MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2)` で出す。`IDYES` でなければ return する。
    5. `CloseLogViewer();` でシミュレーションログのウィンドウを閉じる（FR-007）。データフレーム編集ウィンドウは閉じない。
    6. `result = CanSim::DeleteResultFiles(existing)`。
    7. `!result.Deleted.empty()` なら `ClearLog();` でログ表示エリアの行を消す（FR-008、Q3）。`m_runRows`・強調・シミュレーション時間は変えない。
    8. `result.Failed` が空なら、`IDS_LOG_CLEAR_DONE` を `JoinFileNames(result.Deleted)` で整形し、`MB_OK | MB_ICONINFORMATION` で通知する（FR-004）。
  - `m_projectPath`・`m_settings`・`m_definitions` は変えない（FR-009）。

**Checkpoint**: ログのあるプロジェクトで、確認 → 削除 → 完了の通知 → ログ表示エリアが空、まで動く。

## Phase 4: User Story 2 - 削除できないときに理由がわかる (Priority: P2)

**Goal**: プロジェクトが無い・ログが無い・削除できない、のそれぞれで理由を通知する。

**Independent Test**: quickstart.md の手順 2・3・8。

- [X] T005 [US2] `MFCApplication1/MFCApplication1Dlg.cpp` の `OnLogClear` に通知を足す（依存: T004）。
  - 手順 2: `AfxMessageBox(LoadRcString(IDS_LOG_NONE_NOPROJECT), MB_OK | MB_ICONINFORMATION)` を出してから return する（025 と同じ）。
  - 手順 3: `IDS_LOG_NONE` を出してから return する。%s はプロジェクトのファイル名で、`OnLogViewSimLog` と同じく最後の `\` / `/` より後を使う（025 と同じ）。確認は出さない。
  - 手順 8: `result.Failed` が空でなければ、`IDS_LOG_CLEAR_FAILED` を `JoinFileNames(result.Failed)` で整形し、`MB_OK | MB_ICONWARNING` で通知する。削除できたものは削除されたまま。手順 7（1 つでも削除できたらログ表示エリアを空にする）は、この場合もそのまま行う。

## Phase 5: Polish & Cross-Cutting

- [X] T006 [P] `tools/cansim_runner/checks.cpp` に `CheckFeature027()` を足し、`main` から呼ぶ（依存: T002）。ファイルは `g_tempDir` の下の `t027_clear.canprj` などで作る。
  - `ResultFilePaths(L"C:\\x\\a.canprj")` が `a.canprj.log.csv`・`.summary.csv`・`.startup.csv` の 3 つを、この順で返す。1 番目は `LogPathFor` と等しい。
  - `ExistingResultFiles(L"")` は空。
  - プロジェクトファイル・`.log.csv`・`.startup.csv` を作り、`.summary.csv` の名前で「フォルダー」を作ったとき:
    - `ExistingResultFiles` は `.log.csv`・`.startup.csv` の 2 つだけを返す（フォルダーは含めない）。
  - そのうち `.startup.csv` を読み取り専用（`_wchmod(path, _S_IREAD)`）にしてから `DeleteResultFiles` を呼んだとき:
    - `Deleted` = `{.log.csv}`、`Failed` = `{.startup.csv}` になる。
    - `.canprj` とフォルダーは残っている。
  - 後片付け: 読み取り専用を外し（`_wchmod(path, _S_IREAD | _S_IWRITE)`）、作ったファイル・フォルダーを消す。
- [X] T007 [P] `CLAUDE.md` と `tools/cansim_runner/README.md` を更新する。
  - `CLAUDE.md`:
    - 「解析・ログ表示メニュー（機能 025）」の節に、027 のログクリアを足す。内容は、メニュー、3 つのファイル、確認（既定はいいえ）、`ClearLog`、`CloseLogViewer`、通知、実行中はグレー。
    - アーキテクチャ節の `LogFileIndex.h/.cpp` の説明に、`ResultFilePaths` / `ExistingResultFiles` / `DeleteResultFiles` を足す。
    - 検証ツールの対象機能の一覧に 027 を足す。
  - `README.md`: 027 の検証項目を足す。
- [X] T008 Debug/Release × x64/x86 の 4 構成をビルドし、次を確かめる（依存: T001〜T007）。
  - 4 構成とも、警告・エラーなしでビルドできる。
  - checks.exe の全項目が PASS する。
  - 変更前（HEAD）のソースの runner で作った基準 CSV と一致する（SC-004）。
  - 起動中のアプリのプロセスは終了させない。exe が使用中でリンクできなければ、ユーザーに閉じてもらうよう頼む。
- [X] T009 quickstart.md の手動確認 1〜11 を行い、結果を tasks.md の末尾に記録する（依存: T008）。
  - Claude ができるのは、画面写しでのメニューの確認だけ。ダイアログを使う手順は、ユーザーに依頼する。

## Dependencies & Execution Order

```text
T001 ─┐
T002 ─┼─> T004 (US1) ─> T005 (US2) ─┐
  └──> T003 ─────────────────────────┼─> T008 ─> T009
T002 ─> T006 ────────────────────────┤
T007 ────────────────────────────────┘
```

- **ストーリーの依存**: US2 は US1 と同じハンドラ `OnLogClear` に通知を足すので、US1 の後に行う。
- **並行できるもの**:
  - T001 と T002。
  - T002 の後の T006（checks.cpp）と、T003〜T005（`MFCApplication1Dlg.cpp`）。
  - T007（ドキュメント）は、いつでも。

### 並行の例

```text
T001 Resource.h / .rc      |  T002 LogFileIndex.h/.cpp
T003 → T004 → T005 (MFCApplication1Dlg.h/.cpp)  |  T006 checks.cpp  |  T007 CLAUDE.md / README.md
```

## Implementation Strategy

- **MVP**: T001〜T004（US1）。確認して削除し、ログ表示エリアを空にする。
- **順に足す**: US2 の通知（T005）→ checks（T006）→ ドキュメント（T007）→ 4 構成のビルドと検証（T008）→ 手動確認（T009）。

## 実装結果（2026-10-03）

- 変更:
  - `CanSim/LogFileIndex.h/.cpp`（`ResultFilePaths`・`ExistingResultFiles`・`LogClearResult`・`DeleteResultFiles`）
  - `MFCApplication1Dlg.h/.cpp`（`OnLogClear`、`JoinFileNames`、メニューの有効／無効、`WriteResults` のパスを `ResultFilePaths` から取る）
  - `Resource.h`・`MFCApplication1.rc`（`ID_LOG_CLEAR`、メニュー項目「ログクリア(&C)...」、`IDS_LOG_CLEAR_CONFIRM` / `DONE` / `FAILED`。.rc は読み直して文字化けが無いことを確認済み）
  - `tools/cansim_runner/checks.cpp`・`README.md`、`CLAUDE.md`
- T005 は T004 と同じ `OnLogClear` の中で、通知まで含めて実装した。
- T008:
  - ビルド: Debug/Release × x64/x86 の 4 構成とも、警告・エラーなしでビルドできた。
  - checks.exe: 全 195 項目 PASS（027 の 6 項目を追加）。
  - HEAD のソースの runner で作った基準 CSV（各サンプル 10 秒）とも一致した（004 SC-009、SC-004）。
- T009（GUI）:
  - 手順 1（メニュー）: Release x64 を別に起動し、Windows API でメニューを読み出して確認した。「ログ表示(&L)」の下に「シミュレーションログ(&L)...」（32780）と「ログクリア(&C)...」（32784）がこの順で並ぶ。
  - 手順 2〜11（確認・通知のダイアログ、削除、ログ表示エリア、ウィンドウを閉じる、読み取り専用、実行中、編集中、再実行）: ユーザーが手動で確認し、正常に動作した（2026-10-03）。
