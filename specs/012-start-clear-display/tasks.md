---

description: "Task list for feature 012-start-clear-display"
---

# Tasks: シミュレーション開始時に表示をクリアして先頭から表示する

**Input**: Design documents from `/specs/012-start-clear-display/`

**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/, quickstart.md（すべて存在）。機能 004・005・009・011 の実装が完了していること。

**Tests**: spec.md は自動テストを要求していない。コアの変更はないので checks.exe は既存項目の PASS と CSV の一致を確認する。表示の初期化と順序はGUI の自動操作（画面の色の読み取り、時刻の計測、スクリーンショット）で確認する。

**Organization**: spec.md のユーザーストーリー（US1「開始」するとすぐに表示が空になる P1、US2 新しい実行の行が先頭から順に表示される P1）ごと。US2 は US1 の初期化（先頭行 0・表示 0 行）で満たされるため、コードの変更は US1 だけで、US2 は確認のみ。

## Format: `[ID] [P?] [Story] Description`

## Path Conventions

UI は `MFCApplication1/MFCApplication1Dlg.*`。`CanSim/`・`.rc` の変更はない。GUI 検証スクリプトはリポジトリ外（セッションの scratchpad の `gui.ps1`）。

---

## Phase 1: Setup

- [X] T001 GUI 検証の準備をする（リポジトリ外）:
  - scratchpad の `gui.ps1` に操作 `waitlogblank ms` を追加する。
    - ログ表示エリアの文字の領域の 5 点（96 dpi 基準で (170, 272)・(300, 290)・(500, 330)・(700, 400)・(400, 500)）の画面の色が、すべて背景色 `E6E6E6` になるまで 5 ms 間隔で待つ。
    - 直前の `start` からの経過 ms と、その時点で待機カーソルの計算中か（`timemsg` と同じく完了メッセージがまだ出ていないか）を出力する。
  - 操作 `logblank` を追加する（上の 5 点が背景色かどうかを今すぐ出力）。
  - `start` の前にメインウィンドウを前面に出す（画面の色を読むため）。
  - 一時フォルダの `sample_16ecu.canprj` をコピーして `DurationSec=600`・`Speed=10` にした `long600.canprj` を作る。

---

## Phase 2: User Story 1 - 「開始」するとすぐに表示が空になる (Priority: P1) 🎯 MVP

**Goal**: 計測関連の初期化より前に、イベント表示とログ表示を初期化して画面に反映する

**Independent Test**: `long600.canprj` で一度完了させた後、もう一度「開始」して、計算中にログ表示エリアが空になっている（quickstart.md 手順 2-1〜2-3）

- [X] T002 [US1] `MFCApplication1/MFCApplication1Dlg.h/.cpp` に `void ResetDisplaysForRun()` を追加する（research.md 決定1・3、data-model.md）。
  - 次の順で処理する。
    1. `ClearLog()`: `m_logLines` 空、`m_logShown` 0、`m_logTop` 0（先頭）、スクロールバーを行なし・無効に更新。
    2. `ClearGray()`: `m_grayEcu` 0、グレーのタイマー停止。
    3. `m_active = CanSim::ActiveFrames()`
    4. `InvalidateRect(GetDiagramRect(), FALSE)`、`InvalidateRect(GetLogTextRect(), FALSE)`
    5. `UpdateWindow()`
  - 宣言は `// ---- feature 012 ----` の見出しで 011 の宣言の後に置く。（spec FR-001〜FR-003）
- [X] T003 [US1] `MFCApplication1/MFCApplication1Dlg.cpp` の `OnSimStart` で、今の `ClearLog(); ClearGray();` を `ResetDisplaysForRun();` に置き換える（research.md 決定2、data-model.md「開始」の処理の順序）。
  - 位置は保存先の確定（`AskSavePath`・`SaveProject` の成功）の後で、`CWaitCursor` と `BuildRunNetwork` で始まる計算のブロックの前。
  - 「計測関連の初期化より前に表示を初期化し、画面に反映する（012 FR-005）」旨のコメントを付ける。
  - `m_runRows` を置き換える前に行を消す、という既存の注意（009）は残す。（spec FR-001、FR-005、FR-006）（依存: T002）
- [X] T004 [US1] Debug|x64 をビルドし、実機で quickstart.md 手順 2-1〜2-3・2-6・2-7 を確認する（依存: T001, T003）。
  - 2-3: 2 回目の「開始」から `waitlogblank` が 200 ms 以内に背景色だけになり、その時点では完了メッセージがまだ出ていない（計算中）。スクロールバーは無効。
  - 2-6: ECU のダブルクリック直後に編集ウィンドウを Esc で閉じて「開始」→ 計算中の ECU の点がグレーでない。
  - 2-7: 起動直後の「開始」→ 保存先のキャンセルで、表示が変わらない（ウィンドウ画像の比較）。
  - （SC-001、SC-003、SC-004）

**Checkpoint**: US1 の受け入れシナリオ 1〜3 を満たす

---

## Phase 3: User Story 2 - 新しい実行の行が先頭から順に表示される (Priority: P1)

**Goal**: 前回のスクロール位置に関係なく、新しい実行の最初の行がログ表示エリアの一番上に出る

**Independent Test**: 前回のログを遡った状態から `sample_appendix_a`（スピード 0）を「開始」し、最初の行が一番上に出る（quickstart.md 手順 2-5）

- [X] T005 [US2] 実機で quickstart.md 手順 2-4・2-5 を確認する（依存: T004）。
  - 2-4: 完了後のログに新しい実行の行が出ている。
  - 2-5: `sample_appendix_a`（スピード 0）で前回のログを途中まで遡った状態から「開始」する。
    - 最初の行が出た時点のウィンドウ画像で、1 行目の位置（96 dpi 基準 y 約 266〜284）に行の文字があり、その上に前回の行が無い。
    - 行がエリアに収まる間の数回のウィンドウ画像で、1 行目が動かない。
    - 同じ手順を 10 回くり返して 10 回とも満たす。
  - コードの変更は不要の見込み（`SetLogShown` の追従は先頭行 0・表示 0 行から始まるため）。満たさない場合は原因を報告する。（SC-002、spec FR-004）

**Checkpoint**: SC-002 を満たす

---

## Phase 4: Polish & Cross-Cutting Concerns

- [X] T006 [P] `CLAUDE.md` の「プロジェクトと実行（機能 004）」または「ログ表示エリア（機能 009）」の段落に追記する（憲章 V）。
  - 「開始」は、保存先の確定後、計算の前に `ResetDisplaysForRun` でログ表示・グレー表示・強調を初期化して、その場で描き直す（機能 012）。
- [X] T007 Debug/Release × x64/x86 の全 4 構成で、次を確認する（依存: T004, T005）。
  - 警告なしでビルドできる。
  - `checks.exe` の全項目が PASS（001〜011）。
  - サンプルの CSV が基準と一致する（004 SC-009、spec SC-005）。

---

## Dependencies & Execution Order

- **Setup**: T001（GUI 確認の前提。実装と並行可）
- **US1**: T002 → T003 → T004
- **US2**: T005（T004 の後）
- **Polish**: T006 は並行可、T007 は最後

### Parallel Opportunities

- T001（検証スクリプト）は T002・T003 と並行可
- T006（文書）は実装と並行可

## Implementation Strategy

1. **MVP = US1**（T001〜T004）: 「開始」で計算の前に表示が空になる。
2. US2 で先頭から表示されることを確認する。
3. Polish で文書・4 構成ビルド。

## Notes

- シミュレーションエンジン・CSV・`.rc`・`CanSim/` は変更しない。
- 完了後・「終了」後の表示（追従・行を残す）は機能 009 のまま。

## 実装結果（2026-10-01）

- checks.exe: 001〜011 の全項目 PASS（サンプルの CSV は基準と一致）。4 構成（Debug/Release × x64/x86）とも警告なし。
- GUI（Debug|x64、拡大率 125%）:
  - 変更前の再現: 16 ECU・30 秒・スピード +10 の構成（計算に約 18〜27 秒）で 2 回目の「開始」をすると、2 秒たってもログ表示エリアに前回の行が残っていた。
  - 2-3: 変更後は、同じ操作で「開始」から 142 ms でログ表示エリアが空になった（完了メッセージが出る前＝計算中）。
  - 2-4: 完了後は新しい実行の行が表示された。
  - 2-6: ECU のダブルクリック直後に編集ウィンドウを閉じて「開始」すると、開始の 60 ms 後にグレーの ECU は無かった。
  - 2-7: 起動直後の「開始」で保存先をキャンセルすると、何も実行されず表示は変わらなかった。
  - 2-5: 前回のログ（スピード +10 で全行表示し 3 行遡った状態）から、スピード -13 の構成を「開始」する手順を 10 回くり返した。
    - 行が出た時点の 1 行目は 10 回とも同じ内容で、前回の 1 行目とは異なった。
    - 行が出始めた瞬間のウィンドウ画像で、1 行目は「0 us ECU 01 初期化中」だった。
    - 14 行を超えた後は、機能 009 のとおり最新行に追従した。
- 自動操作の注意: 画面の色を読む判定は、ほかのウィンドウに隠れると誤る。そのため 2-5 はウィンドウ自身に描かせた画像（PrintWindow）で判定した。
- 作業中に `MFCApplication1/CanSim/SampleConfigs/` の 6 ファイルが作業ツリーから消えていた（9/30 18:07、原因不明）。コミット済みの内容から `git restore` で戻した。
