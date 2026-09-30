---

description: "Task list for feature 007-run-menu-lock"
---

# Tasks: 実行中のメニュー制限と、インターミッション後のバスアイドル

**Input**: Design documents from `/specs/007-run-menu-lock/`

**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/, quickstart.md（すべて存在）。機能 004〜006 の実装が完了していること。

**Tests**: spec.md は自動テストを要求していない。plan.md の方針に従い、バスの時間配分（US2）は `tools/cansim_runner/checks.cpp` に
検証を追加し、メニューの状態（US1）は quickstart の GUI 手順で確認する。

**Organization**: spec.md のユーザーストーリー（US1・US2）ごと。新規ファイル・共通の前提はないため、Setup / Foundational はない。

## Format: `[ID] [P?] [Story] Description`

---

## Phase 1: User Story 1 - 実行中はファイル・編集メニューを操作できない (Priority: P1) 🎯 MVP

**Goal**: 実行中は「ファイル」「編集」をトップレベルごとグレーにして開けなくし、停止後に戻す

**Independent Test**: スピード 0・10 秒で開始し、実行中に「ファイル」「編集」が開かず、完了・終了後に開ける（quickstart 手順 2〜3）

- [X] T001 [US1] `MFCApplication1/MFCApplication1Dlg.h/.cpp` に `void UpdateTopMenus()` を追加する: `GetMenu()` の位置 0（ファイル）と
  位置 1（編集）を、実行中は `MF_BYPOSITION | MF_GRAYED`、停止中は `MF_BYPOSITION | MF_ENABLED` にし、`DrawMenuBar()` で再描画する
  （research.md 決定1）。「シミュレーション」（位置 2）は変更しない（spec FR-001〜FR-003）
- [X] T002 [US1] `MFCApplication1/MFCApplication1Dlg.cpp` で、`OnSimStart` が `m_runState = RunState::Running` にした直後と、`EndRun`（完了・
  終了・✖ の共通経路）で Idle に戻した直後に `UpdateTopMenus()` を呼ぶ（spec FR-002）（依存: T001）
- [X] T003 [US1] `MFCApplication1/MFCApplication1Dlg.cpp` の `OnEditFrameDisplay` と `OnFileSaveProject` の冒頭に「実行中なら何もしない」を追加し、
  `OnEditFrameDisplay` の「実行中も切替可能」のコメントを、実行中は切り替えられない旨に直す（spec FR-004、research.md 決定2）（依存: T001）
- [X] T004 [US1] 実機（Debug|x64、一時フォルダのサンプル）で quickstart 手順 1〜4 を確認する: 停止中は 3 メニューが開ける。スピード 0 で
  実行中は「ファイル」「編集」がグレーで、`SC_KEYMENU`（Alt+F / Alt+E 相当）で開かない。「シミュレーション」は開け、「終了」だけ選べる。
  完了後・途中終了後に「ファイル」「編集」が開ける。フレーム表示 ON の強調は実行中も続く（SC-001、SC-002）（依存: T002, T003）
  【結果】停止中は Alt+F でメニューが開く。実行中は「ファイル」「編集」がグレー表示で、`SC_KEYMENU`（Alt+F / Alt+E 相当）を送っても
  ポップアップは開かない。「シミュレーション」は開け、「開始」はグレー・「終了」は選択可。完了後・途中終了後はどちらも開ける。
  フレーム表示 ON の強調は実行中も継続。マウスのクリックは自動操作では確認していない（グレーのトップレベル項目は Windows の
  標準動作で開かない）。

**Checkpoint**: SC-001・SC-002 を満たす

---

## Phase 2: User Story 2 - フレームの後に必ずインターミッションを置いてからバスアイドルになる (Priority: P2)

**Goal**: 実装済みの動作（Doc FR-021・FR-022）を全通信速度で検証する

**Independent Test**: checks.exe の 007 の項目が PASS（quickstart 手順 1）

- [X] T005 [P] [US2] `tools/cansim_runner/checks.cpp` に `CheckFeature007()` を追加する（research.md 決定3）: `sample_collision.canprj` を
  `BitRateBps` = 125000 / 250000 / 500000 / 1000000 に置き換えて 1 秒実行し、ログを Tx_Start 順の 1 フレーム 1 件にまとめて、
  連続する 2 フレームについて (a) 後の Tx_Request ≤ 前の Tx_End + 3 ビット時間 なら Tx_Start − 前の Tx_End = 3 ビット時間
  （24 / 12 / 6 / 3 µs）、(b) それ以外も Tx_Start − 前の Tx_End ≥ 3 ビット時間、(c) Tx_End − Tx_Start = `ComputeFrameTiming(CanId, Dlc,
  0xFF データ, 通信速度)` の伝送時間（インターミッションを含まない）、を確かめる。(a) に当たる組が各速度で 1 件以上あることも確認する
  （SC-003、SC-004）
  【結果】4 速度とも PASS（各速度 200 フレーム中、送信待ちが続く組 100 件で間隔 = 3 ビット時間、伝送時間はビット数 × ビット時間と
  一致）。エンジンの変更は不要だった。

**Checkpoint**: SC-003・SC-004 を満たす

---

## Phase 3: Polish & Cross-Cutting Concerns

- [X] T006 [P] `CLAUDE.md` を更新する: 「実行中はセッティング・読込み・開始を受け付けない」を「実行中は『ファイル』『編集』メニューごと
  選べない（トップレベルをグレー、`UpdateTopMenus`）」に、「フレーム表示は強調を描くかどうかだけを決める」の説明から実行中の切替の
  記述を外す。インターミッション（EOF 後 3 ビット）を経てバスアイドルになること、画面には表示しないことを 1 行で追記（憲章 V）
- [X] T007 [P] `specs/005-simulation-speed/contracts/ui-speed-replay.md` の冒頭に、007 で「実行中のフレーム表示切替」が廃止されたことと
  `specs/007-run-menu-lock/contracts/ui-menu-state.md` への参照を追記する（原文は書き換えない）
- [X] T008 Debug/Release × x64/x86 の全 4 構成で `msbuild MFCApplication1\MFCApplication1.sln` が警告なしで通り、`checks.exe` の全項目が
  PASS であることを確認する（憲章 II）（依存: T001〜T005）

---

## Dependencies & Execution Order

- **US1**: T001 → T002・T003 → T004
- **US2**: T005 は US1 と独立（別ファイル）
- **Polish**: T006・T007 は並行可、T008 は実装完了後

### Parallel Opportunities

- T005（checks.cpp）は US1（ダイアログ）と並行可
- T006・T007（文書）は並行可

## Implementation Strategy

1. **MVP = US1**（T001〜T004）: 実行中のメニュー制限。
2. US2（T005）で実装済みの動作を検証。
3. Polish で文書と 4 構成ビルド。

## Notes

- エンジン（`SimulationEngine`）は変更しない。T005 が失敗した場合は、実装の不一致として報告し、修正方針を相談する。
