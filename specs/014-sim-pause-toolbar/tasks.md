---

description: "Task list for feature 014-sim-pause-toolbar"
---

# Tasks: シミュレーションの中断とツールバー（開始・中断・終了）

**Input**: Design documents from `/specs/014-sim-pause-toolbar/`

**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/, quickstart.md（すべて存在）。機能 004・005・007・009・010・012 の実装が完了していること。

**Tests**: spec.md は自動テストのフレームワークを要求していない。新しい純粋関数（`CanSim/SimControls`）は checks.exe に検証項目を加える（憲章 III）。ボタンの見た目・中断と再開の動作は GUI の自動操作で確認する。

**Organization**: spec.md のユーザーストーリーごと。
- US1: ツールバーのボタンで開始・終了する（P1）
- US2: 実行を中断し、再開する（P1）

共通の土台（状態の型・有効／無効の表・ツールバーの座標・リソース）は Phase 2 に置く。

## Format: `[ID] [P?] [Story] Description`

## Path Conventions

- コア（UI 非依存、ASCII のみ）は `MFCApplication1/CanSim/`。
- UI は `MFCApplication1/MFCApplication1Dlg.*`。
- リソースは `MFCApplication1/Resource.h`・`MFCApplication1/MFCApplication1.rc`。`.rc` は UTF-16LE で、PowerShell でエンコーディングを明示して編集する（CLAUDE.md）。
- 検証は `tools/cansim_runner/checks.cpp`。GUI 検証スクリプトはリポジトリ外（scratchpad の `t014/`）。

---

## Phase 1: Setup

- [X] T001 GUI 検証の準備をする（リポジトリ外、scratchpad の `t014/`）。013 の `t013/Gui.cs` を土台にする。
  - アプリの起動、プロジェクトの読込み（ファイルダイアログのファイル名欄 1148、保存は 1001）、メニューのコマンド送出の関数を用意する。
  - ツールバーのボタンをクリックする関数を用意する。座標は `SimButtonRect` と同じ値（research.md 決定3、DPI 換算）の中央を `ClientToScreen` で求める。
  - ボタンの色を読む関数を用意する。箱の中の 1 点の画面の色が、有効 `F0F0F0`・無効 `5F5F62`・押下中 `C8C8C8` のどれかで判定する。ウィンドウ自身に描かせた画像（PrintWindow）で読む（012 の注意）。
  - 「シミュレーション」メニューの項目の有効／無効を読む関数を用意する（`GetMenu` → サブメニュー 2 → `GetMenuState`）。
  - ログ表示エリアの行数を数える方法を用意する。完了・終了メッセージの文言、または `.log.csv` の行数で代える。
  - 一時フォルダに検証用の構成を作る。
    - `sample_appendix_a.canprj` のコピー（スピード 0、10 秒）
    - スピード +10 の構成（`Speed=10`）

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: 状態の型、有効／無効の表、ツールバーの座標と当たり判定、再開後の時刻、リソース。US1・US2 の両方が使う。

- [X] T002 [P] `MFCApplication1/CanSim/SimControls.h/.cpp` を新規作成する（data-model.md、research.md 決定1・3・5）。ASCII のみで、MFC に依存しない。
  - `enum class RunPhase { Idle, Running, Paused };`
  - `struct SimCommandStates { bool Start = false; bool Pause = false; bool Stop = false; };`
  - `SimCommandStates GetSimCommandStates(RunPhase phase, bool editingFrames)` は次の表を返す。
    - Idle: Start = `!editingFrames`、Pause = false、Stop = false
    - Running: Start = false、Pause = true、Stop = true
    - Paused: Start = true、Pause = false、Stop = true
  - 定数（96 dpi）:
    - `kToolbarHeight = 64`、`kSimButtonCount = 3`
    - `kSimButtonLeft = 8`、`kSimButtonPitch = 56`、`kSimButtonTop = 6`
    - `kSimButtonWidth = 48`、`kSimButtonBoxHeight = 36`、`kSimButtonCorner = 8`
    - `kSimButtonLabelTop = 42`、`kSimButtonBottom = 60`、`kSimButtonGlyph = 20`
  - 範囲を返す関数（すべて `ScaleToDpi`（`BusDiagramLayout.h`）で換算した `DiagramRect` を返す）:
    - `SimButtonRect(index, dpi)`: 当たり判定 = 左 `8 + 56*i`、上 6、右 左+48、下 60
    - `SimButtonBoxRect(index, dpi)`: 箱 = 上 6、下 42
    - `SimButtonLabelRect(index, dpi)`: 名前 = 上 42、下 60
  - `int FindSimButtonAt(int x, int y, int dpi)`: 換算後の `[Left, Right) × [Top, Bottom)` に入るボタン番号（0〜2）を返す。どれでもなければ -1。
  - `long long ResumedSimTimeUs(long long baseUs, long long elapsedRealUs, int speed)`:
    - 値は `baseUs + SimTimeAtUs(elapsedRealUs, speed)`（`ReplayClock.h`）。
    - 加算は `LLONG_MAX` で飽和させる。`baseUs` < 0 は 0 として扱う。
- [X] T003 [P] `MFCApplication1/MFCApplication1.vcxproj` と `MFCApplication1.vcxproj.filters` に `CanSim\SimControls.h`（ClInclude）・`CanSim\SimControls.cpp`（ClCompile）を追加する。既存の CanSim の並びに倣い、filters も同じ分類にする。
- [X] T004 [P] リソースを追加する（research.md 決定8）。
  - `MFCApplication1/Resource.h`:
    - `#define ID_SIM_PAUSE 32778`
    - `IDS_SIM_BTN_START 242`、`IDS_SIM_BTN_PAUSE 243`、`IDS_SIM_BTN_STOP 244`
    - `_APS_NEXT_COMMAND_VALUE` を 32779、`_APS_NEXT_SYMED_VALUE` を 245 にする。
  - `MFCApplication1/MFCApplication1.rc`（UTF-16LE BOM 付き・CRLF を保つ）:
    - `IDR_MAINMENU` の「シミュレーション」を `MENUITEM "開始(&R)", ID_SIM_START` / `MENUITEM "中断(&P)", ID_SIM_PAUSE` / `MENUITEM "終了(&X)", ID_SIM_STOP` の順にする。
    - 文字列テーブルに `IDS_SIM_BTN_START "開始"`、`IDS_SIM_BTN_PAUSE "中断"`、`IDS_SIM_BTN_STOP "終了"` を追加する。
    - 編集後に読み直し、日本語が化けていないことを確かめる。
- [X] T005 `MFCApplication1/MFCApplication1Dlg.h/.cpp` の実行の状態を、`CanSim::RunPhase` に置き換える（research.md 決定6）。（依存: T002）
  - `enum class RunState` と `m_runState` を削除し、`CanSim::RunPhase m_runPhase = CanSim::RunPhase::Idle;` を追加する。`#include "CanSim/SimControls.h"` を足す。
  - `bool InRun() const { return m_runPhase != CanSim::RunPhase::Idle; }` を追加する。
  - 今の `m_runState == RunState::Running` の各箇所を、意味ごとに次のとおり置き換える。
    - `InRun()` にするもの（実行が終わっていない間ずっと）:
      - `DrawBusDiagram` の強調の条件
      - `OnInitMenuPopup` の idleOnly
      - `OnFileLoadProject`、`OnFileSaveProject`、`OnEditSettings`、`OnEditFrames`、`OnEditFrameDisplay` の拒否
      - `OnLButtonDblClk` の拒否
      - `OnTimer` の切り替え待ち（`kSwitchTimerId`）で開かない条件
      - `UpdateTopMenus` のグレーの条件
      - `OnSimStop` の受付（`!InRun()` なら戻る）
    - `m_runPhase == CanSim::RunPhase::Running` にするもの（時計が進んでいる間だけ）: `OnTimer` の再生（`kReplayTimerId`）の条件。
    - `OnSimStart` の先頭: `Running` なら戻る（Paused の扱いは T013 で加える）。
  - `OnSimStart` の終わりで `m_runPhase = Running` にし、`EndRun` で `Idle` にする。
  - この時点では動作を変えない（Paused にはまだならない）。ビルドして、起動・開始・終了・自然終了が今までどおり動くことを確かめる。
- [X] T006 [P] `tools/cansim_runner/checks.cpp` に `CheckFeature014()` を追加し、`main` で `CheckFeature011()` の後に呼ぶ。`#include "SimControls.h"` を足す。（依存: T002）
  - 有効／無効の表: `GetSimCommandStates` の 6 通り（Idle・Running・Paused × editing の有無）が data-model.md の表と一致する。
  - ツールバーの座標（DPI 96/120/144）:
    - 3 つの `SimButtonRect` が `[0, ScaleToDpi(kToolbarHeight))` の中に収まる。
    - 左から順に並び、互いに重ならない。
    - 箱と名前は当たり判定の範囲の中にある。
  - 当たり判定: 各ボタンの換算後の左上の点・右下の 1 px 内側の点で番号が返る。右端・下端ちょうどの点と、ボタンの間の点で -1 が返る。
  - 帯の高さは、構成図・ログ表示エリアのどの座標とも重ならない（ずらし後に、帯の下端 = 構成図の上端になる）。
  - `ResumedSimTimeUs`:
    - `(0, e, s)` == `SimTimeAtUs(e, s)`（s = -13, 0, 5）。
    - スピード 0 で `ResumedSimTimeUs(ResumedSimTimeUs(0, a, 0), b, 0)` == `SimTimeAtUs(a + b, 0)`（5 組）。
    - `(LLONG_MAX - 1, 1000000, 0)` は `LLONG_MAX`（飽和）。

**Checkpoint**: Debug|x64 でビルドが通り、checks.exe の 014 項目と既存項目がすべて PASS。アプリの動作は従来どおり。

---

## Phase 3: User Story 1 - ツールバーのボタンで開始・終了する (Priority: P1) 🎯 MVP

**Goal**: メニューの下の暗い帯に「開始」「中断」「終了」のボタンを描き、ボタンでメニューと同じ開始・終了ができる。有効／無効はメニューと常に同じ。

**Independent Test**: 起動直後に「開始」ボタンだけが有効。「開始」ボタンで実行が始まり、「終了」ボタンでメニューの「終了」と同じ結果になる（quickstart.md 手順 2-1〜2-3・2-8〜2-10）

- [X] T007 [US1] `MFCApplication1/MFCApplication1Dlg.h/.cpp` で、構成図・ログ表示エリアを帯の高さだけ下げる（research.md 決定4）。
  - `int ContentTop() const` を追加する。値は `ScaleToDpi(CanSim::kToolbarHeight, dpi)`。
  - 次の箇所に足す。
    - `GetDiagramRect`: `top = ContentTop()`、`bottom = ContentTop() + 高さ`。
    - `DrawBusDiagram` の `BitBlt` の y: メモリ DC の中は従来どおり原点 (0, 0) で描き、転送先だけずらす。
    - `OnLButtonDblClk` の `FindEcuAt` に渡す y: `point.y - ContentTop()`。
    - `GetLogRect`: `OffsetRect(0, ContentTop())`。スクロールバー・枠・ホイール判定はこれに追従する。
    - `OpenFrameEditor` の `pos.y`: `+ ContentTop()`。
  - `OnInitDialog` で、ウィンドウの高さを `ContentTop()` だけ広げる（`GetWindowRect` → `SetWindowPos(SWP_NOMOVE | SWP_NOZORDER)`）。スクロールバーの `MoveWindow` より前に行う。（spec FR-009）
- [X] T008 [US1] `MFCApplication1/MFCApplication1Dlg.h/.cpp` にツールバーの描画 `DrawSimToolbar(CDC& dc)` を追加し、`OnPaint` で `DrawBusDiagram` の前に呼ぶ（research.md 決定2・3、contracts/ui-sim-controls.md）。（依存: T004, T007）
  - 帯は `(0, 0)`〜`(クライアント幅, ContentTop())`、色 `RGB(45,45,48)`。メモリ DC に描いてから転送する（ちらつき防止）。
  - 各ボタン i の描き方:
    - 箱は `SimButtonBoxRect` に `RoundRect`（角丸 `ScaleToDpi(kSimButtonCorner)`）。
    - 記号は箱の中央の `ScaleToDpi(kSimButtonGlyph)` 角に描く。
      - 開始: 右向き三角の `Polygon`
      - 中断: 縦の 2 本線。各幅 6、間 6（96 dpi）の `FillSolidRect`
      - 終了: 16 px の正方形
    - 名前は `SimButtonLabelRect` に `DrawText(DT_CENTER | DT_SINGLELINE | DT_VCENTER)`。文字の高さ `ScaleToDpi(12)`、文字列は `IDS_SIM_BTN_*`。
  - 色（research.md 決定3）:
    | 状態 | 箱 | 記号 | 名前 |
    |---|---|---|---|
    | 有効 | `RGB(240,240,240)` | 開始 `RGB(30,160,70)`・中断 `RGB(30,110,200)`・終了 `RGB(210,40,40)` | 白 |
    | 押下中（`m_pressedButton == i` で有効） | `RGB(200,200,200)` | 有効と同じ | 白 |
    | 無効 | `RGB(95,95,98)` | `RGB(140,140,140)` | `RGB(140,140,140)` |
  - 有効／無効は `CanSim::GetSimCommandStates(m_runPhase, IsEditingFrames())` で決める。
  - `CRect GetToolbarRect() const` も追加する（無効化に使う）。
- [X] T009 [US1] `MFCApplication1/MFCApplication1Dlg.h/.cpp` にツールバーのクリックを追加する（research.md 決定2・7）。（依存: T008）
  - `ON_WM_LBUTTONDOWN` / `ON_WM_LBUTTONUP` / `ON_WM_MOUSEMOVE` を追加し、`int m_pressedButton = -1;` を持つ。
  - `OnLButtonDown`:
    - `FindSimButtonAt(point.x, point.y, dpi)` が有効なボタンなら、`m_pressedButton` に入れ、`SetCapture`、ツールバーを無効化する。
    - それ以外は既定の処理。
  - `OnLButtonDblClk`: 先頭で、点がツールバーの帯の中なら `OnLButtonDown` と同じ処理をして戻る（素早い 2 回目のクリック。ECU の判定はしない）。
  - `OnMouseMove`: 押下中に点が押したボタンの外へ出たら、押下の見た目を外す。戻れば付ける（ツールバーを無効化）。
  - `OnLButtonUp`:
    - 押下中なら `ReleaseCapture` する。
    - 離した点が押したボタンと同じで、そのボタンがまだ有効なら、`PostMessage(WM_COMMAND, ID_SIM_START / ID_SIM_PAUSE / ID_SIM_STOP)` を送る。
    - `m_pressedButton = -1` に戻し、ツールバーを無効化する。
  - 捕捉を失ったとき（`WM_CAPTURECHANGED`）も押下を外す。（spec FR-003）
- [X] T010 [US1] `MFCApplication1/MFCApplication1Dlg.h/.cpp` に `UpdateSimControls()` を追加する（research.md 決定7）。（依存: T008）
  - 中身は `UpdateTopMenus()` と、ツールバーの `InvalidateRect(GetToolbarRect(), FALSE)`。
  - 今 `UpdateTopMenus()` を呼んでいる所（`OnSimStart`・`EndRun`）を `UpdateSimControls()` に変える。
  - 編集中が変わる所でも呼ぶ。
    - `OpenFrameEditor` の成功後
    - `OnFrameEditorClosed`
    - `OnLButtonDblClk` で切り替え待ちを始めた後
    - `OnTimer` の切り替え待ちの終わり
    - `CloseFrameEditor`
  - `OnInitMenuPopup` の `ID_SIM_START` / `ID_SIM_PAUSE` / `ID_SIM_STOP` の有効／無効は、`GetSimCommandStates(m_runPhase, IsEditingFrames())` の Start / Pause / Stop にする（FR-004、メニューとボタンが同じ関数）。
  - `ON_COMMAND(ID_SIM_PAUSE, &CMFCApplication1Dlg::OnSimPause)` を追加する。`OnSimPause` はこの時点では空の関数（中身は T012）。
- [X] T011 [US1] Debug|x64 をビルドし、実機で quickstart.md 手順 2-1〜2-3・2-8（中断を除く「実行中に終了」）・2-9・2-10 を確認する（依存: T001, T009, T010）。
  - 2-1: 起動直後のボタンの色とメニューの状態を確かめる。
    - ボタンの色: 開始 = 有効、中断・終了 = 無効。
    - メニューは 3 項目で、状態がボタンと同じ。
    - 構成図とログ表示エリアの位置: 帯の下端 = 構成図の上端。ログ表示エリアは機能 011 の位置 + 帯の高さ。
    - ECU 01 の箱の中央・箱の外の 1 px のダブルクリックで、開く・開かないが正しい。
  - 2-2: データフレーム編集を開くと「開始」がグレー、閉じると有効。メニューも同じ。
  - 2-3: 「開始」ボタンで実行が始まる。開始 = 無効、中断・終了 = 有効。「ファイル」「編集」はグレー。
  - 実行中に「終了」ボタン → 終了メッセージが出て、待機に戻る（開始だけ有効）。
  - 自然終了 → 待機に戻る。
  - 2-9: スピード +10 → すぐ完了し、待機に戻る。
  - 2-10: ボタンを押したまま外へ出て離すと何も起きない。グレーのボタンを押しても何も起きない。
  - （SC-001、spec US1 受け入れシナリオ 1〜5）

**Checkpoint**: US1 の受け入れシナリオ 1〜5 を満たす

---

## Phase 4: User Story 2 - 実行を中断し、再開する (Priority: P1)

**Goal**: 「中断」で表示と時計が止まり、中断中の「開始」で中断した時点から続く。中断中の「終了」は中断の時刻までの結果を出す。

**Independent Test**: スピード 0 で開始し、3 秒後に中断 → 10 秒待っても表示が変わらない → 「開始」で続きが表示され、最後まで実行した CSV が中断なしと一致する（quickstart.md 手順 2-4〜2-8）

- [X] T012 [US2] `MFCApplication1/MFCApplication1Dlg.h/.cpp` に中断を実装する（research.md 決定5）。（依存: T010）
  - `long long m_simBaseUs = 0;` を追加する。`OnSimStart` の新しい実行の開始で 0 にする。
  - `CurrentSimTimeUs()`:
    - Paused なら `m_curSimUs` を返す。
    - それ以外は `min(CanSim::ResumedSimTimeUs(m_simBaseUs, 経過実時間, m_settings.Speed), 終端)` を返す。
  - `OnSimPause()`（`m_runPhase == Running` のときだけ）は、今の `OnTimer` の再生の更新と同じ処理を 1 回行う。
    - `m_curSimUs = CurrentSimTimeUs()`、`m_active = m_cursor->Collect(m_prevSimUs, m_curSimUs)`、`m_prevSimUs = m_curSimUs`、`ShowLogUpTo(m_curSimUs)`。
    - 終端に達していれば `FinishReplay()` して戻る。
    - そうでなければ `StopReplayTimer()`、`m_runPhase = Paused`、構成図を無効化し、`UpdateSimControls()` を呼ぶ。
  - 共通にできるなら、`OnTimer` の更新部分を `AdvanceReplay()` に切り出して両方から呼ぶ（同じ処理であることを保つ）。
  - 中断中も強調 `m_active` は消さない（`DrawBusDiagram` は `InRun()` で描く。T005）。（spec FR-005、SC-002）
- [X] T013 [US2] `MFCApplication1/MFCApplication1Dlg.cpp` の `OnSimStart` に再開を加える（research.md 決定5・6）。（依存: T012）
  - 先頭で次のとおり分ける。
    - Running なら何もしない。
    - Paused なら `ResumeReplay()` を呼んで戻る。
    - Idle なら今までの新しい実行（編集中の拒否・保存先の確認・`ResetDisplaysForRun`・計算・再生の準備）。
  - `ResumeReplay()`:
    1. `m_simBaseUs = m_curSimUs`、`QueryPerformanceCounter(&m_replayStartQpc)`、`m_prevSimUs = m_curSimUs`。
    2. `m_runPhase = Running`、`m_replayTimer = SetTimer(kReplayTimerId, kRefreshMs, nullptr)`、`UpdateSimControls()`。
    - 計算・表示の初期化・保存先の確認はしない。（spec FR-006）
- [X] T014 [US2] `MFCApplication1/MFCApplication1Dlg.cpp` の `OnSimStop` を中断中にも受け付けるようにし（`!InRun()` なら戻る。T005 で済み）、停止時刻が中断の時刻になることを確かめる。`CurrentSimTimeUs()` は Paused で `m_curSimUs` を返す（T012）。`EndRun` で `m_simBaseUs` を 0 に戻す。（spec FR-007、SC-004）（依存: T012）
- [X] T015 [US2] Debug|x64 をビルドし、実機で quickstart.md 手順 2-4〜2-8 を確認する（依存: T001, T013, T014）。
  - 2-4: 3 秒後に「中断」ボタンを押す。
    - 押した直後と 10 秒後のウィンドウ画像のログ表示エリアが同じ。
    - ボタンは開始 = 有効、中断 = 無効、終了 = 有効。「ファイル」「編集」はグレー。
  - 2-5: 中断中、ホイールでログを遡れる。ECU のダブルクリックで編集ウィンドウが開かない。
  - 2-6: 「開始」ボタンで再開する。
    - 保存先のダイアログが出ない。
    - 中断前の行が残ったまま続きが出る。
    - 完了後の `.log.csv` が、中断なしで同じ構成を実行したものとバイト単位で一致する。
  - 2-7: 中断・再開を 5 回くり返して完了させる。CSV 3 種が中断なしと一致する（SC-003）。
  - 2-8: 中断 → 「終了」で、終了メッセージの時刻が中断の時刻（ウィンドウのログ表示の最後の行の時刻以上）になる。`.log.csv` の最後の行の時刻はそれを超えない（SC-004）。
  - （SC-002〜SC-005、spec US2 受け入れシナリオ 1〜5）

**Checkpoint**: US2 の受け入れシナリオ 1〜5 を満たす

---

## Phase 5: Polish & Cross-Cutting Concerns

- [X] T016 [P] `CLAUDE.md` を更新する（憲章 V）。
  - アーキテクチャの `CanSim/` の一覧に `SimControls.h/.cpp` を加える。内容は `RunPhase`、`GetSimCommandStates`、ツールバーの座標と `FindSimButtonAt`、`ResumedSimTimeUs`。
  - 「プロジェクトと実行（機能 004）」の段落に、機能 014 を追記する。
    - 実行の状態は待機・実行中・中断中。
    - 「中断」は表示を今の時刻まで進めてから時計を止める。中断中の「開始」は再開。
    - 「終了」は中断の時刻まで。
    - ツールバーは自前描画で、構成図・ログ表示エリアは帯の高さ（`ContentTop()`）だけ下げる。ウィンドウの高さは実行時に広げる。
  - 「`MFCApplication1Dlg.h` / `.cpp`」の説明にある、メニュー「シミュレーション（開始／終了）」を「開始／中断／終了」にする。
  - 「ボタン・入力欄などのコントロールは持たない」に、ツールバーは自前描画である旨を加える。
- [X] T017 [P] `tools/cansim_runner/README.md` の検証範囲の記述を 014 まで広げ、014 の項目（有効／無効の表、ツールバーの座標と当たり判定、再開後の時刻）を加える。
- [X] T018 Debug/Release × x64/x86 の全 4 構成で、次を確認する（依存: T011, T015）。
  - 警告なしでビルドできる。
  - `checks.exe` の全項目が PASS（001〜011・014）。
  - サンプルの CSV が基準と一致する（基準は runner.exe で各サンプルを 10 秒実行したもの。004 SC-009、spec SC-006）。

---

## Dependencies & Execution Order

- **Setup**: T001（GUI 確認の前提。実装と並行可）
- **Foundational**: T002・T003・T004 は並行可 → T005（T002 の後）、T006（T002 の後、T005 と並行可）
- **US1**: T007 → T008 → T009・T010（どちらも T008 の後。同じファイルなので順に）→ T011
- **US2**: T012（T010 の後）→ T013 → T014 → T015
- **Polish**: T016・T017 は並行可、T018 は最後

US2 は US1 の `UpdateSimControls` とメニューの有効／無効（T010）に依存する。中断はメニューからも操作できるため、US1 のツールバー（T008・T009）が無くても US2 の確認自体はメニューで行える。

### Parallel Opportunities

- T002（CanSim）・T003（vcxproj）・T004（リソース）は別ファイルなので並行可。
- T006（checks）は T005（ダイアログ）と並行可。
- T016（CLAUDE.md）・T017（README）は並行可。
- `MFCApplication1Dlg.*` を変更するタスク（T005・T007〜T010・T012〜T014）は同じファイルなので、並行にしない。

## Implementation Strategy

1. **Foundational**（T002〜T006）: 型・表・座標・リソースを入れても、動作は変わらない。
2. **MVP = US1**（T007〜T011）: ツールバーで開始・終了ができ、メニューに「中断」が並ぶ（中断はまだ何もしない）。
3. **US2**（T012〜T015）: 中断と再開。
4. **Polish**: 文書・4 構成ビルド・checks。

## Notes

- シミュレーションエンジン・CSV・ログの行は変更しない。
- `CanSim/SimControls.*` は ASCII のみ。日本語のボタン名は `.rc` の文字列テーブル。
- `.rc` を編集した後は、日本語の文字列が化けていないことを必ず読み直して確認する。

## 実装結果（2026-10-01）

- 新規: `MFCApplication1/CanSim/SimControls.h/.cpp`（ASCII のみ）。
- 変更:
  - `MFCApplication1Dlg.h/.cpp`、`Resource.h`、`MFCApplication1.rc`（UTF-16LE。日本語を読み直して確認済み）
  - `MFCApplication1.vcxproj(.filters)`
  - `tools/cansim_runner/checks.cpp`・`README.md`、`CLAUDE.md`
- ビルド: Debug/Release × x64/x86 の 4 構成とも、警告・エラーなしでビルドできた。
- checks.exe: 全 140 項目 PASS（014 の 8 項目を追加）。基準 CSV（runner.exe で各サンプルを 10 秒実行したもの）との比較も一致した。
- GUI（Debug|x64、拡大率 125% = 120 dpi）: scratchpad の `t014/gui.ps1` で、全項目が合格した。
  - 2-1:
    - 起動直後のボタンは開始 = 有効、中断・終了 = グレー。メニューも同じだった。
    - 帯の色は `2D2D30`。構成図の枠は帯の 4 px 下にあり、ログ表示エリアも帯の分だけ下がった。
    - ずらした ECU 01 の箱の中央でダブルクリックすると編集ウィンドウが開き、箱の 3 px 上では開かなかった。
  - 2-2: 編集中は「開始」がボタン・メニューともグレー。閉じると有効に戻った。
  - 2-3: 「開始」ボタンで実行すると、開始 = グレー、中断・終了 = 有効になった。「ファイル」「編集」はグレーで、ログは増えた。
  - 2-4: 「中断」ボタンを押した。
    - 開始・終了 = 有効、中断 = グレー。「ファイル」「編集」はグレーのままだった。
    - 0.1 秒後と 10 秒後で、ログ表示エリア・構成図（フレーム表示 ON の強調を含む）の画像が同じだった。
  - 2-5: 中断中の ECU のダブルクリックでは、編集ウィンドウは開かなかった。
  - 2-6: 「開始」で再開すると、保存先を尋ねずにログが続いた。自然終了で完了メッセージが出て、待機に戻った。
    - CSV 3 種は、中断なしの実行とバイト単位で一致した。
  - 2-7: 中断・再開を 5 回くり返しても、CSV 3 種は中断なしと一致した。
  - 2-8: 約 3 秒で中断して 2 秒待ち、「終了」した。
    - 終了メッセージの時刻は 2,933,590 µs（中断の時刻）。
    - `.log.csv` の最大時刻は 2,930,534 µs で、それを超えなかった。終了後は待機に戻った。
  - 2-9: スピード +10 ではすぐ完了し、待機に戻った。
  - 2-10:
    - 押している間は、押下の色（`C8C8C8`）になった。
    - ボタンの外へ出ると、通常の色に戻った。外で離すと何も起きなかった。
    - グレーの中断・終了を押しても、何も起きなかった。
- 自動操作の注意: メッセージボックスを `WM_COMMAND IDOK` の Post だけで閉じようとすると、閉じないことがあった。そのため ID 2 と `WM_CLOSE` も順に試し、閉じたことを確かめてから次の操作に進むようにした。
