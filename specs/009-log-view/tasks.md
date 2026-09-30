---

description: "Task list for feature 009-log-view"
---

# Tasks: ログ表示エリア

**Input**: Design documents from `/specs/009-log-view/`

**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/, quickstart.md（すべて存在）。機能 001〜008 の実装が完了していること。

**Tests**: spec.md は自動テストを要求していない。plan.md の方針に従い、`CanSim/LogLines` は `tools/cansim_runner/checks.cpp` で検証し、描画・スクロールは
GUI の自動操作（スクリーンショット）で確認する。

**Organization**: spec.md のユーザーストーリー（US1 時間に沿った表示、US2 スクロールとスライドバー、US3 実行ごとの入れ替え）ごと。行の生成と
ダイアログの領域・描画は全ストーリーの前提なので Foundational に置く。

## Format: `[ID] [P?] [Story] Description`

## Path Conventions

コアは `MFCApplication1/CanSim/`（ASCII のみ）。UI は `MFCApplication1Dlg.*`／`MFCApplication1.rc`。`.rc` は UTF-16LE（BOM 付き、CRLF）のため Read/Edit ツールを
使わず、エンコーディングを明示して編集する（CLAUDE.md）。UI の日本語文字列は `.rc` のみ。

---

## Phase 1: Setup

- [X] T001 `MFCApplication1/CanSim/LogLines.h/.cpp` を空の雛形で作成し（ASCII のみ、`pch.h` 経由）、`MFCApplication1/MFCApplication1.vcxproj` と `.vcxproj.filters` に
  登録する（全 4 構成、フィルタ `CanSim`）

---

## Phase 2: Foundational (Blocking Prerequisites)

- [X] T002 `MFCApplication1/CanSim/LogLines.h/.cpp` を実装する（data-model.md、research.md 決定1）: `enum class LogLineKind { State, Frame }`、
  `struct LogLine { long long TimeUs; LogLineKind Kind; int NodeId; int Index; }`、
  `std::vector<LogLine> BuildLogLines(const std::vector<LogRow>& rows, const Network& endState, long long durationUs)`。状態遷移は各ノードで最大 3 行
  （電源投入 = `PowerOnTimeUs`・遷移後 Initializing、初期化完了 = `InitDoneUs`・BusSyncDetecting、通常動作 = `NormalEntryUs`・Normal。`Index` は `StartupState` の整数値）で、
  「durationUs を超えるもの」と「未到達（-1）」は含めない。フレーム行は `rows` の各行（`TimeUs` = `TimestampUs`、`Index` = `rows` の添字）。並び順は
  「TimeUs 昇順 → 状態遷移が先 → NodeId 昇順 → 元の順（安定ソート）」（依存: T001）
- [X] T003 [P] `tools/cansim_runner/checks.cpp` に `CheckFeature009()` を追加する（`#include "LogLines.h"`）: `sample_startup` を 10 秒実行して (a) 状態遷移が 18 行
  （ノード × 3）で、時刻が起動記録（`PowerOnTimeUs`／`InitDoneUs`／`NormalEntryUs`）と一致、(b) 全体が時刻の昇順、同時刻は状態遷移が先でノード番号の昇順、(c) フレーム行の数 =
  ログの行数、(d) `durationUs` を短く（例: 25 µs）したとき、それを超える状態遷移が含まれない、(e) 通常動作に到達しないノード（電源投入がシミュレーション時間より後）は
  電源投入の行も出ない、(f) `sample_buffer_full` の OVERWRITE 行もフレーム行として含まれる。既存の 001〜008 の項目が PASS のまま（SC-002、SC-008）（依存: T002）
- [X] T004 `MFCApplication1/MFCApplication1.rc` と `MFCApplication1/Resource.h` を更新する（contracts/ui-log-view.md）: `IDD_MFCAPPLICATION1_DIALOG` の高さを、クライアント高さ 560 px に
  なるよう約 345 DLU（幅 720 DLU は変えない）に広げる。スクロールバー `IDC_SCROLL_LOG`（`SBS_VERT | WS_CHILD | WS_VISIBLE`、位置は仮で構わない — コードで配置する）を追加する。
  文字列テーブルに状態名（初期化中／バス同期検出中／通常動作／エラーアクティブ／エラーパッシブ／バスオフ）とフレームの状態名（正常／再送／破棄）を追加する。
  **`.rc` はエンコーディングを明示して編集する**
- [X] T005 `MFCApplication1/MFCApplication1Dlg.h/.cpp` にログ表示エリアの土台を実装する: 定数（左 150・上 260・幅 850・高さ 260 px、スクロールバー幅 17 px、背景
  `RGB(230,230,230)`）を DPI/96 でスケールして矩形を求める `GetLogRect()`、`OnPaint` の `DrawBusDiagram` の後にメモリ DC で灰色の矩形を描く `DrawLogView()`（この時点では
  空）、`OnInitDialog` で `IDC_SCROLL_LOG` を `GetLogRect()` の右端の内側（幅 17 px）に移動する。構成図の位置・大きさは変えない（spec FR-001、FR-002）（依存: T004）

**Checkpoint**: 起動直後に構成図の下へ薄いグレーの空のエリアとスクロールバー（無効）が見える（SC-001）

---

## Phase 3: User Story 1 - シミュレーション中のイベントとログを、時間の流れに沿って読む (Priority: P1) 🎯 MVP

**Goal**: 状態遷移とフレームのログが、時刻順に上から下へ、再生時計に合わせて追加される

**Independent Test**: `sample_startup` をスピード 0 で実行し、状態遷移とフレームの行が時刻順に現れる（quickstart 手順 2）

- [X] T006 [US1] `MFCApplication1/MFCApplication1Dlg.h/.cpp` に行の保持と書式を実装する: `std::vector<CanSim::LogLine> m_logLines`、`size_t m_logShown`、`size_t m_logTop`。
  `CString FormatLogLine(const CanSim::LogLine&) const`（contracts/ui-log-view.md: 状態は「時刻 us ・ECU NN ・状態名」、フレームは「時刻 us ・ECU 送信元 -> ECU 受信（受信なしは `-`）・0xID・DLC n・状態名
  （正常／再送／破棄）」。文字列は `.rc` から読む。時刻は右詰め）。`DrawLogView()` で、`m_logTop` から見えている行数（矩形の高さ ÷ 行の高さ）ぶんだけ、`m_logShown` の範囲で描く。
  フォントは等幅（`MS Gothic`）、行の高さはフォント高 + 余白を DPI/96 でスケール（依存: T005）
- [X] T007 [US1] `MFCApplication1/MFCApplication1Dlg.cpp` の `OnSimStart` で、計算の直後に `m_logLines = BuildLogLines(m_runRows, m_runEndState, DurationSec × 10^6)`、`m_logShown = 0`、
  `m_logTop = 0` にする（前の実行の行を消す: spec FR-010）。スピード +10（即時）の経路と、`FinishReplay`（自然終了）では、`m_logShown = m_logLines.size()` にして表示を更新する
  （依存: T006）
- [X] T008 [US1] `MFCApplication1/MFCApplication1Dlg.cpp` の `OnTimer` で、`m_curSimUs` を求めた直後に、`TimeUs ≤ m_curSimUs` の行までを `m_logShown` に含め（前進走査）、増えたときだけ
  ログ表示エリアの矩形を `InvalidateRect` する。機能 005 の再生時計と同じ時刻で進めるので、フレーム表示の ON/OFF に関係なく同じ行が同じ時刻に出る（spec FR-005、FR-006）。
  「終了」（`OnSimStop`）で止めたときは、表示済みの行をそのまま残す（依存: T007）

**Checkpoint**: SC-002 を満たす（`sample_startup` の 18 行の状態遷移が時刻どおりに出る）

---

## Phase 4: User Story 2 - 一番下まで来たら1行ずつスクロールし、スライドバーで先頭を確認する (Priority: P1)

**Goal**: 追従スクロールとスライドバーによる遡り

**Independent Test**: 表示エリアより多い行で、最新行が常に一番下、スライドバーで先頭へ遡れる（quickstart 手順 3〜5）

- [X] T009 [US2] `MFCApplication1/MFCApplication1Dlg.h/.cpp` にスクロールバーの同期と追従を実装する: `UpdateLogScroll()` が `SetScrollInfo`（範囲 = `m_logShown`、ページ = 見えている行数、
  位置 = `m_logTop`）を呼び、行が収まるとき（`m_logShown ≤ 見えている行数`）は `EnableScrollBar(SB_CTL, ESB_DISABLE_BOTH)` で無効にする（spec FR-008、US2 AS7）。
  行を追加する直前に「一番下を見ているか」（`m_logTop + visible ≥ 追加前の m_logShown`）を記録し、そうなら追加後に `m_logTop = max(0, m_logShown − visible)` にして 1 行ずつ追従する
  （spec FR-007）。そうでなければ `m_logTop` を動かさない（spec FR-009）。T007・T008 の `m_logShown` の更新をこの経路にまとめる（依存: T008）
- [X] T010 [US2] `MFCApplication1/MFCApplication1Dlg.cpp` に `OnVScroll`（`SB_LINEUP/DOWN`・`SB_PAGEUP/DOWN`・`SB_THUMBTRACK/POSITION`・`SB_TOP/BOTTOM`）と `OnMouseWheel`
  （ログ表示エリア上で 3 行ずつ）を実装し、`m_logTop` を 0〜`m_logShown − visible` に収めて再描画・`UpdateLogScroll()` する。`ON_WM_VSCROLL`／`ON_WM_MOUSEWHEEL` をメッセージマップに
  追加する。操作は実行中も可能（機能 007 の制限の対象外）（依存: T009）
- [X] T011 [US2] 実機（Debug|x64、一時フォルダのサンプル）で quickstart 手順 3〜5 を確認する: 行が埋まったあと 1 行ずつスクロールして最新行が一番下、スライドバーを上へ動かすと過去の行が
  見えて 10 行以上追加されても動かない、一番下へ戻すと追従が再開、実行後に一番上へ動かすと最初の行が見える（SC-003〜SC-005）（依存: T010）

**Checkpoint**: SC-003・SC-004・SC-005 を満たす

---

## Phase 5: User Story 3 - 新しい実行を始めるとログが入れ替わる (Priority: P2)

**Goal**: 実行後も残り、次の「開始」で入れ替わる

**Independent Test**: 実行後に先頭まで読み返せる。2 回目の「開始」で空から始まる（quickstart 手順 6・8）

- [X] T012 [US3] `MFCApplication1/MFCApplication1Dlg.cpp` で、`EndRun`（完了・終了・✖）と、プロジェクトの読込み・セッティングの変更では `m_logLines`・`m_logShown`・`m_logTop` を消さないことを
  確認する。`OnSimStart` の最初（計算の前）で消して再描画・`UpdateLogScroll()` する。実行中の ✖ で閉じたときは何も出力しない（既存）（spec FR-010）（依存: T009）
- [X] T013 [US3] 実機で quickstart 手順 6・8 を確認する: スピード +10 で実行して完了時に全行がありスクロールバーで先頭まで読める。もう一度「開始」すると前の行が消えて最初の行が
  一番上に出る。フレーム表示 ON/OFF で表示が変わらない。読込み・セッティング変更後も行が残る（SC-006、spec FR-006）（依存: T012）

**Checkpoint**: SC-006 を満たす

---

## Phase 6: Polish & Cross-Cutting Concerns

- [X] T014 `sample_16ecu`（約 12 万行）をスピード +10 で実行し、スライドバーの操作（つまみのドラッグ、ページ送り、ホイール）の反応と、行の生成時間を確認する。1 操作 0.2 秒未満、
  実行完了までの追加待ちが 1 秒未満であること（SC-007、spec FR-012）。重い場合は、`BuildLogLines` のソートと `DrawLogView` の描画を見直す（依存: T010, T013）
- [X] T015 [P] `CLAUDE.md` を更新する: `LogLines.h/.cpp`（行の生成・並び順）、ダイアログの「ログ表示エリア」（構成図の下の灰色の領域、再生時計に合わせた供給、追従とスライドバー、開始で
  クリア、フレーム表示に依存しない）、ダイアログの高さの変更（憲章 V）
- [X] T016 Debug/Release × x64/x86 の全 4 構成で警告なしのビルドと `checks.exe` の全項目 PASS を確認し、CSV が変わらないこと（SC-008）を、`sample_appendix_a` の CSV の前後比較で確かめる
  （依存: T014）

---

## Dependencies & Execution Order

- **Setup (T001)** → **Foundational**（T002 → T003 は並行可、T004 → T005）
- **US1**: T006 → T007 → T008
- **US2**: T009 → T010 → T011
- **US3**: T012 → T013
- **Polish**: T014 → T016、T015 は並行可

### Parallel Opportunities

- T003（checks.cpp）は T004・T005（UI）と並行可
- T015（文書）は実装と並行可

## Implementation Strategy

1. **MVP = US1**（T001〜T008）: 灰色のエリアに、時刻に合わせて行が追加される。
2. US2 で追従スクロールとスライドバー、US3 で入れ替え、Polish で 12 万行の確認・文書・4 構成ビルド。

## Notes

- `CanSim/` の新規コードに日本語を含めない（ASCII のみ）。
- エンジン（`SimulationEngine`）は変更しない。表示用の行は実行結果から作る。

## 実装結果（2026-09-30）

- T003: `checks.exe` の 009 項目 6 件を含め全項目 PASS（0 failed）。004 SC-009（サンプル 4 件の CSV がベースラインと一致）・008 SC-005 も PASS
- T011: `sample_startup`（スピード 0、10.2 秒で完了）で、行が埋まった後の 1 行ずつの追従、実行中に先頭へ戻すと行が増えても位置が動かないこと、一番下へ戻すと追従再開、実行後に先頭の行が見えることをスクリーンショットで確認
- T013: スピード +10 で完了時に全行があり先頭まで遡れる。プロジェクト読込み・セッティング（OK）の後も行が残る。フレーム表示 ON のプロジェクトで再度「開始」すると前の行が消えて最初の行が一番上に出て、内容は OFF の時と同じ
- T014: `sample_16ecu`（ログ 120,000 行 → 表示行 120,048 行）で `BuildLogLines` は 5 ms（/O2）・10 ms（/Od）。先頭へ移動・ページ送りは即時に反映（1 操作 0.2 秒未満）
- T016: Debug/Release × x64/x86 の 4 構成で警告 0・エラー 0

