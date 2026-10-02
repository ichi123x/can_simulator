---

description: "Task list for feature 024-project-path-title"
---

# Tasks: プロジェクトファイル名の表示と関連付けからの起動

**Input**: `/specs/024-project-path-title/` の spec.md・plan.md・research.md（決定1〜7）・data-model.md・contracts/ui-window-title-and-startup.md・quickstart.md

**Tests**: spec はテストの作成を求めていない。純粋関数の検証は、既存の慣習どおり `checks.cpp` に項目として足す（T008）。

## Phase 1: Setup

- [X] T001 [P] `MFCApplication1/Resource.h` と `MFCApplication1/MFCApplication1.rc` にラベルの文字列を足す。
  - `Resource.h`（UTF-8 BOM 付きのまま。PowerShell で `New-Object Text.UTF8Encoding($true)` を指定して読み書きする）:
    `#define IDS_PROJECT_PATH_LABEL 254	// feature 024` を `IDS_SAVE_READONLY` の次に足し、`_APS_NEXT_SYMED_VALUE` を 255 にする。
  - `.rc` の文字列テーブル（`IDS_SAVE_READONLY` の次）に `IDS_PROJECT_PATH_LABEL "プロジェクトファイル名："`（全角のコロン）を足す。
    UTF-16LE・BOM 付き・CRLF を保つため、`[IO.File]::ReadAllText(path, [Text.Encoding]::Unicode)` と `New-Object Text.UnicodeEncoding($false,$true)` で読み書きし、書いた後に読み直して文字化けが無いことを確かめる。
  - 書いた後に `git diff` で、追加した行以外が変わっていないこと（特に先頭の BOM と日本語のコメント）を確かめる。

## Phase 2: Foundational（すべてのストーリーの前提）

- [X] T002 [P] `MFCApplication1/CanSim/ProjectFile.h/.cpp` に純粋関数を 3 つ足す（contracts の「CanSim に足す・変えるもの」）。ASCII のみ。`ProjectFile.h` に `#include <vector>` を足す。
  - `std::wstring ComposeWindowTitle(const std::wstring& appTitle, const std::wstring& label, const std::wstring& projectPath)`:
    `appTitle + L"    " + label + projectPath`（区切りは半角空白 4 つ。パスが空ならラベルで終わる。research 決定2）。
  - `std::wstring FindStartupProjectArg(const std::vector<std::wstring>& args)`:
    `args[0]` はプログラムなので除き、その後の最初の空でない引数を返す。無ければ空文字列（2 つ目以降は無視。research 決定4）。
  - `bool HasProjectExtension(const std::wstring& path)`: 末尾が `.canprj`（大文字小文字を区別しない、ASCII の範囲で比較）なら true。`.canprj` だけ・空文字列は、末尾が一致すれば true／しなければ false（今のダイアログの `HasProjectExtension` と同じ判定。research 決定7）。
- [X] T003 `MFCApplication1/MFCApplication1Dlg.h/.cpp` で、現在のプロジェクトファイルの変更と読込みの処理を 1 か所にまとめる（依存: T001, T002）。動作はまだ変えない（タイトル以外）。
  - メンバー `CString m_baseTitle;`（`.rc` のキャプション）を足す。`OnInitDialog` の `SetIcon` の後で `GetWindowText(m_baseTitle)` する。
  - `void SetProjectPath(const CString& path)`: `m_projectPath = path` にして、`SetWindowText(CanSim::ComposeWindowTitle(m_baseTitle, LoadRcString(IDS_PROJECT_PATH_LABEL), path))` でタイトルを書き直す。
    `m_projectPath` への代入はこの関数だけにする（今の `OnFileLoadProject` と `SaveProjectTo` の代入を置き換える）。
  - `bool LoadProjectFrom(const CString& path)`: 今の `OnFileLoadProject` の、ファイル選択より後の処理（拡張子の検査 → `CanSim::LoadConfig` → エラーなら `IDS_BAD_EXTENSION` / `IDS_LOAD_ERROR` のメッセージで false、
    成功なら `m_settings`・`InvalidateToolbar`・`m_definitions`・`SetProjectPath(path)`・`SetEcuCount` で true）をそのまま移す。
  - `OnFileLoadProject` は `InRun()` の確認・`CloseFrameEditor()`・ファイル選択の後に `LoadProjectFrom(dlg.GetPathName())` を呼ぶだけにする。
  - 無名名前空間の `HasProjectExtension` を消し、`CanSim::HasProjectExtension(std::wstring((LPCTSTR)path))` を使う。

**Checkpoint**: 起動直後からタイトルにラベルが出て、読込み・保存が今までどおり動く。

## Phase 3: User Story 1 - 今どのプロジェクトファイルを使っているかがわかる (Priority: P1) 🎯 MVP

**Goal**: タイトルバーに現在のプロジェクトファイルのフルパスを表示し、読込み・保存の成功のたびに更新する。

**Independent Test**: quickstart.md の手順 1〜5。

- [X] T004 [US1] `MFCApplication1/MFCApplication1Dlg.cpp` の `OnInitDialog` で、`GetWindowText(m_baseTitle)` の直後に `SetProjectPath(CString())` を呼び、起動直後のタイトルを「CAN Bus Simulator    プロジェクトファイル名：」にする（依存: T003。spec US1-1、FR-002）。
- [X] T005 [US1] `MFCApplication1/MFCApplication1Dlg.cpp` の `SaveProjectTo` で、成功時の `m_projectPath = path` を `SetProjectPath(path)` にする（依存: T003）。
  - これで、メニューの「プロジェクト保存」・ツールバーの「保存」・「開始」の保存先確認のすべてでタイトルが更新される（spec US1-3・4、FR-003）。
  - 失敗・キャンセルの経路では `SetProjectPath` を呼ばないことを確かめる（`SaveProjectTo` の失敗は false を返すだけ、`AskSavePath` のキャンセルは `SaveProjectTo` を呼ばない。FR-004）。
- [X] T006 [US1] 読込みのすべての経路でタイトルが更新されることを確かめる（依存: T003）。
  - ツールバーの「読込み」は `ID_FILE_LOAD_PROJECT` → `OnFileLoadProject` → `LoadProjectFrom` で、成功時だけ `SetProjectPath` が呼ばれること。
  - キャンセル（ファイル選択で IDOK 以外）・拡張子違い・`LoadConfig` の失敗では `SetProjectPath` が呼ばれないこと（FR-004）。コードの変更が要らなければ変えない。

**Checkpoint**: US1 の受け入れシナリオ 1〜5 を手で確かめられる。

## Phase 4: User Story 2 - .canprj ファイルから直接アプリを開く (Priority: P1)

**Goal**: 起動時の引数の .canprj を、メニューの読込みと同じ処理で読み込み、絶対パスで表示する。関連付けの登録はしない（FR-008）。

**Independent Test**: quickstart.md の手順 6〜10。

- [X] T007 [US2] 起動時の引数を受け取って読み込む（依存: T003。spec US2、FR-005〜007、research 決定4〜6）。
  - `MFCApplication1/FrameEditDlg.h` の `WM_APP_*` の並びの次に `const UINT WM_APP_LOAD_STARTUP_PROJECT = WM_APP + 4;` を足す
    （既存の `WM_APP + 1〜3` と重ならないように同じ場所で番号を管理する。コメント `// feature 024: main dialog loads the project given on the command line`）。
  - `MFCApplication1/MFCApplication1Dlg.h/.cpp`:
    - メンバー `CString m_startupProject;` と `void SetStartupProject(const CString& path) { m_startupProject = path; }`（public）を足す。
    - `OnInitDialog` の最後（`return TRUE` の前）で、`m_startupProject` が空でなければ `PostMessage(WM_APP_LOAD_STARTUP_PROJECT)`。
    - `afx_msg LRESULT OnLoadStartupProject(WPARAM, LPARAM)` を足し、`ON_MESSAGE(WM_APP_LOAD_STARTUP_PROJECT, …)` をメッセージマップに足す。処理:
      1. `m_startupProject` を取り出して空にする（1 回だけ）。空なら何もしない。
      2. `GetFullPathNameW` で絶対パスにする（失敗したら元の文字列のまま）。続けて `GetLongPathNameW` で 8.3 形式を長い名前にする（失敗したら `GetFullPathNameW` の結果のまま。ファイルが無いときは失敗する）。
         バッファは `MAX_PATH` 固定にせず、1 回目の呼び出しで必要な長さを得てから確保する。
      3. `LoadProjectFrom(fullPath)` を呼ぶ。失敗してもアプリは続ける（メッセージは `LoadProjectFrom` が出す。タイトルのパスは空のまま。FR-006）。
  - `MFCApplication1/MFCApplication1.cpp` の `InitInstance`: `CMFCApplication1Dlg dlg;` の後、`DoModal` の前で、
    `__argc` / `__wargv` を `std::vector<std::wstring>` にして `CanSim::FindStartupProjectArg` を呼び、空でなければ `dlg.SetStartupProject(...)`。
    `#include "CanSim/ProjectFile.h"` と `<vector>`・`<string>` を足す。`ParseCommandLine` / `ProcessShellCommand` は使わない。
  - 関連付けのレジストリへの書き込みは行わない（FR-008）。

**Checkpoint**: `MFCApplication1.exe <パス>` で起動すると、そのプロジェクトが読み込まれ、タイトルに絶対パスが出る。

## Phase 5: Polish & Cross-Cutting

- [X] T008 [P] `tools/cansim_runner/checks.cpp` に `CheckFeature024()` を足し、`main` から呼ぶ（依存: T002）。次を確かめる。
  - `ComposeWindowTitle(L"CAN Bus Simulator", L"LABEL:", L"")` が `L"CAN Bus Simulator    LABEL:"`、パスありで `… LABEL:C:\\work\\a.canprj` になる（ラベルは ASCII の代わりの文字列で検証する。checks も ASCII のみ）。
  - `FindStartupProjectArg`: `{}`・`{exe}` → 空、`{exe, a}` → a、`{exe, L"", a, b}` → a。
  - `HasProjectExtension`: `a.canprj`・`A.CANPRJ`・`C:\\x\\b.CanPrj` → true、`a.canprj.txt`・`README.md`・`canprj`・空 → false。
- [X] T009 [P] `CLAUDE.md` と `tools/cansim_runner/README.md` を更新する。
  - `CLAUDE.md`:
    - 「プロジェクトと実行（機能 004）」の後に機能 024 の段落を足す（タイトルの書式、`SetProjectPath` が `m_projectPath` を変える唯一の箇所であること、`LoadProjectFrom`、起動時の引数 → `WM_APP_LOAD_STARTUP_PROJECT`、関連付けは登録しないこと）。
    - アーキテクチャ節の `ProjectFile.h/.cpp` の説明に 3 つの関数を足し、`MFCApplication1.h / .cpp` の説明に起動時の引数を足す。
    - 検証ツールの対象機能の一覧に 024 を足す。
  - `README.md`: 024 の検証項目を足す。
- [X] T010 Debug/Release × x64/x86 の 4 構成をビルドし、次を確かめる（依存: T001〜T009）。
  - 4 構成とも、警告・エラーなしでビルドできる。
  - checks.exe の全項目が PASS し、サンプルの CSV が基準と一致する（spec SC-005。基準は HEAD のソースの runner で作る）。
  - ユーザーが起動しているアプリのプロセスは終了させない。exe が使用中でビルドできないときは、ユーザーに閉じてもらうよう頼む。
- [X] T011 quickstart.md の手動確認 1〜11 をユーザーに依頼し、結果を tasks.md の末尾に記録する（依存: T010）。
  - 手順 6〜9 のコマンドライン起動は、私（Claude）が自分で起動したプロセスだけを閉じる形で、画面写しで補助してよい。

## Dependencies & Execution Order

```text
T001 ─┐
T002 ─┴─> T003 ─┬─> T004 ─> T005 ─> T006 (US1) ─┐
                └─> T007 (US2) ─────────────────┼─> T010 ─> T011
T002 ─> T008 ───────────────────────────────────┤
T009 ───────────────────────────────────────────┘
```

- **ストーリーの依存**: US1・US2 はどちらも T003（`SetProjectPath`・`LoadProjectFrom`）の後に進められる。US2 の表示は US1 と同じ `SetProjectPath` による。
- **並行できるもの**:
  - T001 と T002（ファイルが別）。
  - T002 の後の T008（checks.cpp）と、T003 以降のダイアログの作業。
  - T009（ドキュメント）は、いつでも。
- T004〜T007 は同じファイル（`MFCApplication1Dlg.cpp`）を触るので、順番に行う。

### 並行の例

```text
# Phase 1・2 の初め
T001 Resource.h / .rc        |  T002 ProjectFile.h/.cpp

# T002 の後
T003 → T004 → T005 → T006 → T007 (MFCApplication1Dlg.cpp, MFCApplication1.cpp)  |  T008 checks.cpp  |  T009 CLAUDE.md / README.md
```

## Implementation Strategy

- **MVP**: T001〜T006（US1: タイトルの表示と更新）。要望の中心で、単独で価値がある。
- **順に足す**: US2（T007: 起動時の引数）→ checks（T008）→ ドキュメント（T009）→ 4 構成のビルドと検証（T010）→ 手動確認（T011）。

## 実装結果（2026-10-02）

- 変更:
  - `CanSim/ProjectFile.h/.cpp`: `ComposeWindowTitle`、`FindStartupProjectArg`、`HasProjectExtension`（ダイアログの無名名前空間から移した）
  - `MFCApplication1Dlg.h/.cpp`: `SetProjectPath`（`m_projectPath` を変える唯一の箇所。タイトルも書き直す）、`LoadProjectFrom`（`OnFileLoadProject` の後半を移した）、
    `m_baseTitle`・`m_startupProject`、`OnLoadStartupProject`（`GetFullPathNameW` → `GetLongPathNameW` → `LoadProjectFrom`）
  - `MFCApplication1.cpp`: `InitInstance` で `__wargv` から起動時のプロジェクトを選んで渡す
  - `FrameEditDlg.h`: `WM_APP_LOAD_STARTUP_PROJECT`（`WM_APP + 4`）
  - `Resource.h`（UTF-8 BOM のまま）・`.rc`（UTF-16LE のまま。1 行 44 文字だけ増えたことを確認）: `IDS_PROJECT_PATH_LABEL`（254）
  - `tools/cansim_runner/checks.cpp`・`README.md`、`CLAUDE.md`
- T006: コードの変更は不要だった。読込みのキャンセルは `LoadProjectFrom` の前に戻り、`SetProjectPath` は読込み・保存の成功時だけ呼ばれる。
- T010（ビルド）: Debug/Release × x64/x86 の 4 構成とも警告・エラーなし（ユーザーのアプリは起動していなかった）。
- checks.exe: 全 184 項目 PASS（024 の 3 項目を追加）。基準 CSV（各サンプル 10 秒）とも一致した。
- 起動の確認（Release x64、自分で起動したプロセスだけを閉じた。サンプルは一時フォルダーへの複製を使用）:
  - 引数なし: タイトル「CAN Bus Simulator    プロジェクトファイル名：」
  - 絶対パスの引数・相対パスの引数: どちらもタイトルが一時フォルダーの `sample_16ecu.canprj` の絶対パスになった
- T011（2026-10-02、ユーザー）: quickstart の手順 1〜11 すべて正常に動作した。

## Phase 6: 改訂 1（ユーザーの指示: ラベルを短くする）

- [X] T012 `MFCApplication1/MFCApplication1.rc` の `IDS_PROJECT_PATH_LABEL` を「プロジェクト名：」に変える（UTF-16LE・BOM 付き・CRLF のまま）。
  CLAUDE.md・plan.md・research.md・data-model.md・contracts・quickstart.md・spec.md（Input を除く）の記述も合わせる。上の T001・T004 と実装結果の記録は当時の文字列のまま残す。
- [X] T013 4 構成のビルドと起動の確認（依存: T012）。

### 改訂 1 の結果（2026-10-02）

- Release x64・Debug x86・Release x86: 警告・エラーなし。Debug x64: 初めはユーザーが Debug 版を起動中で LNK1168（プロセスは終了させていない）。閉じてもらった後にビルドし、警告・エラーなしで通った。
- CanSim は変えていないので checks の結果（184 項目 PASS）は変わらない。
- Release x64 を自分で起動して確認: 引数なし「CAN Bus Simulator    プロジェクト名：」、引数あり「CAN Bus Simulator    プロジェクト名：C:\Users\user\AppData\Local\Temp\cansim024\sample_16ecu.canprj」。
- 手動確認（2026-10-02、ユーザー）: Debug 版のタイトルが「プロジェクト名：」に変わっていることを確認した。
