---

description: "Task list for feature 008-frame-buffers"
---

# Tasks: 送信・受信フレームバッファ

**Input**: Design documents from `/specs/008-frame-buffers/`

**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/, quickstart.md（すべて存在）。機能 001〜007 の実装が完了していること。

**Tests**: spec.md は自動テストを要求していない。plan.md の方針に従い、`tools/cansim_runner/checks.cpp` に検証を追加する。

**Organization**: spec.md のユーザーストーリー（US1 送信バッファ、US2 受信バッファ、US3 列名と既存結果）ごと。ログの空欄表現・列名・定数は全ストーリーの前提なので Foundational に置く。

## Format: `[ID] [P?] [Story] Description`

## Path Conventions

変更はすべて `MFCApplication1/CanSim/`（ASCII のみ）と `tools/cansim_runner/`。UI の変更はない。

---

## Phase 1: Setup

- [X] T001 本機能の実装に着手する**前に**、現在のビルドで既存サンプル 5 つ（`sample_appendix_a`・`sample_collision`・`sample_startup`・`sample_startup_latejoin`・`sample_16ecu`）を 10 秒実行した CSV を、基準としてスクラッチ領域（例: `%TEMP%\base007`）に退避する（SC-005 の基準）

---

## Phase 2: Foundational (Blocking Prerequisites)

- [X] T002 `MFCApplication1/CanSim/CanTypes.h` を変更する: 定数 `kTxBufferCount = 16`・`kRxBufferCount = 8` を追加。`PendingFrame::OverwriteCount`と `LogRow::OverwriteCount` を `UpdateCount` に改名。`Node` に `std::deque<long long> RxBuffer`（受信処理時刻、昇順、「最大 8」）、`int TxDiscardCount = 0`、`int RxDiscardCount = 0` を追加。`LogRow` の時刻項目のコメントに「-1 は空欄」を追記（data-model.md）。`OverwriteCount` を参照している `Network.cpp`・`SimulationEngine.cpp`・`Logger.cpp`・`tools/cansim_runner/checks.cpp` を改名に合わせる
- [X] T003 `MFCApplication1/CanSim/Logger.cpp` を変更する: 見出しの最後の列を `Update_Count` にし（Doc FR-051）、時刻項目が負の値のときは空の値を出力する（contracts/log-and-summary.md）（依存: T002）
- [X] T004 `MFCApplication1/CanSim/SimulationEngine.cpp` の `Run` の初期化で、各ノードの `RxBuffer` を空にし、`TxDiscardCount`・`RxDiscardCount`を 0 にする（依存: T002）

**Checkpoint**: ビルドが通り、`checks.exe` の既存項目が PASS（ログの列名だけ変わる）

---

## Phase 3: User Story 1 - 送信バッファの満杯を評価する (Priority: P1) 🎯 MVP

**Goal**: 送信待ち（送信中を含む）を 16 本までにし、超える要求を捨てて OVERWRITE を記録する

**Independent Test**: `sample_buffer_full` で、100 ms ごとに ECU1 の 0x110〜0x113 の要求が捨てられ、送信側 OVERWRITE 行 = 400（quickstart 手順1）

- [X] T005 [US1] `MFCApplication1/CanSim/Network.h/.cpp` の `EnqueueTxRequest` を変更する: `enum class EnqueueResult { Queued, Updated, Discarded }`を返し、引数に「そのノードのフレームが送信中か」（`bool transmitting`）を加える。同じフレームが送信待ちなら従来どおり更新して`Updated`。そうでなければ、`PendingFrames.size() + (transmitting ? 1 : 0) >= kTxBufferCount` なら何もせず `Discarded`、それ以外は追加して `Queued`（research.md 決定1、Doc FR-092〜FR-092b）（依存: T002）
- [X] T006 [US1] `MFCApplication1/CanSim/SimulationEngine.cpp` の `HandleTxRequest` で、送信中判定（`m_busBusy` かつ `m_currentTx.NodeIndex` がこのノード）を渡して `EnqueueTxRequest` を呼び、`Discarded` のときは、ノードの `TxDiscardCount` を 1 増やし、ログに OVERWRITE 行を 1 行出す（`Timestamp_us` = `Tx_Request` = 要求時刻、`Rx_Node` = -1、`Tx_Start`・`Tx_End`・`Rx_Start`・`Rx_End`・`Rx_Process` = -1、`Bus_Load_Pct` = その時刻の値、`Status` = `OVERWRITE`、`Retry_Count` = `Update_Count` = 0）。`Discarded` のときはバスアイドルのイベントを積まない（research.md 決定3、spec FR-004）。`HandleTxRequest` はロガーを使うため、必要なら `Run` から渡す（依存: T004, T005）【実施メモ】同じ時刻・同じノードの送信要求は、これまで積んだ順に処理されていた（結果には影響しなかった）。どの要求が捨てられるかが処理順で決まるため、Doc FR-092c（ID の大きい 17 個目以降が捨てられる）に合わせて、`EventQueue` の同時刻の並び順の最後に CAN ID を加えた（`Event::CanId`）。調停・更新回数など既存の結果は変わらない（既存サンプルの比較で確認）。送信中の判定は `m_txInProgress`（BusIdle で true、TxComplete=EOF で false）で行い、インターミッション中はバッファを解放済みとした。
- [X] T007 [P] [US1] `MFCApplication1/CanSim/SampleConfigs/sample_buffer_full.canprj` を作成する（research.md 決定6）: `[Project]` EcuCount=3・DurationSec=10・Speed=10。ECU1 に初回オフセット 0・周期 100 ms・DLC 0 のフレーム 20 個（ID 0x100〜0x113、名前 `burst NN`）。ECU2 はフレームなし・受信処理遅延 2000 µs（受信フィルタなし）。ECU3 に周期 1 ms・DLC 0 のフレーム 1 個（ID 0x050）。先頭のコメントで目的（送信・受信バッファの満杯の確認用）と期待値を説明する。`MFCApplication1/MFCApplication1.vcxproj(.filters)` の `None` に登録する
- [X] T008 [US1] `tools/cansim_runner/checks.cpp` に `CheckFeature008()` を追加し、送信側を検証する: `sample_buffer_full` を 10 秒実行し、送信側 OVERWRITE 行（`Tx_Start` が -1）の数 = 400、その CAN ID がすべて 0x110〜0x113、ECU1 の `TxDiscardCount` = 400、OVERWRITE 行の`Tx_Request` が 100 ms の倍数 + ECU1 の通常動作開始時刻、同じ構成を 2 回実行してログが一致（SC-001、SC-006）（依存: T006, T007）

**Checkpoint**: SC-001 を満たす

---

## Phase 4: User Story 2 - 受信バッファの満杯を評価する (Priority: P1)

**Goal**: 受信バッファ 8 本の FIFO を持ち、満杯なら新しいフレームを捨てて OVERWRITE を記録する

**Independent Test**: `sample_buffer_full` の ECU2（受信処理遅延 2 ms）で受信側 OVERWRITE が発生し、集計と一致（quickstart 手順1）

- [X] T009 [US2] `MFCApplication1/CanSim/SimulationEngine.cpp` の `HandleTxComplete` の受信ノードごとの処理を変更する（research.md 決定2）:受信完了時刻 t（`txEndUs`）で、その受信ノードの `RxBuffer` の先頭から `≤ t` の要素を外す（取り出し済み。同時刻は取り出しが先: spec FR-007）。残りが `kRxBufferCount`（8）なら、`RxDiscardCount` を 1 増やし、行の `Status` = `OVERWRITE`、`Rx_Process` = -1、`Timestamp_us` = `Rx_End` とする（spec FR-006）。そうでなければ `t + ReceiveDelayUs` を `RxBuffer` の末尾に足し、従来どおりの行を出す（依存: T004）
- [X] T010 [US2] `tools/cansim_runner/checks.cpp` の `CheckFeature008()` に受信側の検証を追加する: `sample_buffer_full` で、受信側 OVERWRITE 行（`Rx_Node` があり `Rx_Process` が -1）が 1 件以上あり、すべて `Rx_Node` = 2、その数 = ECU2 の `RxDiscardCount`。ログを時刻順にたどって各ノードの受信バッファの占有数が常に 8 以下であることを確認する。既存サンプル（受信処理遅延 0）で受信側 OVERWRITE が 0 件（SC-002、SC-004）（依存: T009, T008）

**Checkpoint**: SC-002・SC-004 を満たす

---

## Phase 5: User Story 3 - ログの列名と既存の結果 (Priority: P2)

**Goal**: 集計に破棄回数を出し、OVERWRITE を統計から除き、既存サンプルの結果が変わらないことを確認する

**Independent Test**: 既存サンプル 5 つが、列名と追加 2 列以外で T001 の基準と一致（quickstart 手順1）

- [X] T011 [US3] `MFCApplication1/CanSim/MetricsAggregator.h/.cpp` を変更する（research.md 決定4、contracts/log-and-summary.md）: `Status` が`OVERWRITE` の行をすべての統計（Count を含む）から除く。`GroupMetrics` に `HasDiscards`・`TxDiscardCount`・`RxDiscardCount` を追加し、Node 行に、送信側 OVERWRITE（`Tx_Start` が -1）の `Tx_Node` 別件数と、受信側 OVERWRITE の `Rx_Node` 別件数を入れる。破棄が 1 件以上あって従来 Node 行がないノードにも Node 行を出す（従来の行は変えない）。CSV の見出しの末尾に `TxDiscard_Count,RxDiscard_Count` を追加し、ID 行・ALL 行は空の値（依存: T006, T009）
- [X] T012 [P] [US3] `MFCApplication1/CanSim/FrameEvents.cpp` の `BuildFrameEvents` で、送信側で捨てた行（`TxStartUs < 0`）を無視する（spec Edge Cases）（依存: T002）
- [X] T013 [US3] `tools/cansim_runner/checks.cpp` の `CheckFeature008()` に、既存サンプル 5 つと T001 の基準の比較を追加する（基準ディレクトリは既存の第 3 引数を使う）: ログは見出しの `Overwrite_Count`→`Update_Count` を置き換えて一致、集計は各行の末尾 2 列を除いて一致、起動記録は完全一致。あわせて `sample_buffer_full` の集計で ECU1 の TxDiscard_Count = 400、ECU2 の RxDiscard_Count = 受信側 OVERWRITE 行数（SC-003、SC-005）。既存の 004 SC-009 の比較も、新しい見出し・列に合わせて同じ置き換えをする（依存: T011, T012）【結果】`sample_buffer_full`（10 秒）: 送信側 OVERWRITE 400 件（すべて ECU1 の 0x110〜0x113、捨てた時点で毎回 16 本使用中）、受信側OVERWRITE 1000 件（すべて ECU2、捨てた時点で毎回 8 本使用中）、集計の ECU1 TxDiscard_Count = 400・ECU2 RxDiscard_Count = 1000。既存サンプル5 つは T001 の基準（`%TEMP%ase007`）と、列名の置き換えと追加 2 列を除いて一致。checks.exe 全項目 PASS。

**Checkpoint**: SC-003・SC-005 を満たす

---

## Phase 6: Polish & Cross-Cutting Concerns

- [X] T014 [P] `CLAUDE.md` を更新する: `CanTypes.h`・`Network`・`SimulationEngine` の説明に送信バッファ 16 本・受信バッファ 8 本（FIFO、受信処理時刻の列で表す）と OVERWRITE を追記。ログの列名 Update_Count、集計の TxDiscard_Count / RxDiscard_Count、サンプル `sample_buffer_full` を追記
- [X] T015 [P] `specs/001-can-simulation-core/contracts/log-output-format.md` の冒頭に、008 による変更（列名 Update_Count、Status OVERWRITE と空欄の時刻、集計の 2 列）と `specs/008-frame-buffers/contracts/log-and-summary.md` への参照を追記する（原文は書き換えない）
- [X] T016 Debug/Release × x64/x86 の全 4 構成でビルドが警告なしで通り、`checks.exe` が T001 の基準で全項目 PASS であることを確認する。GUI で `sample_buffer_full` をフレーム表示 ON・スピード 0 で実行し、例外なく再生・完了することを確認する（quickstart 手順2）（依存: T001〜T015）【結果】全 4 構成をリビルド（`/t:Rebuild`）して警告なし。GUI で `sample_buffer_full`（スピード 0・フレーム表示 ON に変えたコピー）を実行し、約 11 秒で完了、ログ先頭に送信側の OVERWRITE 行（空欄あり）、集計の Node 行に破棄回数が出ることを確認。

---

## Dependencies & Execution Order

- **Setup (T001)** → **Foundational**（T002 → T003・T004）
- **US1**: T005 → T006 → T008（T007 は並行可）
- **US2**: T009（T004 後）→ T010
- **US3**: T011（T006・T009 後）、T012（並行可）→ T013
- **Polish**: T014・T015 並行可、T016 は最後

### Parallel Opportunities

- T007（サンプル）、T012（FrameEvents）、T014・T015（文書）は他と並行可

## Implementation Strategy

1. T001 で基準を退避してから、Foundational（改名・空欄出力）。
2. **MVP = US1**（送信バッファ 16 本と OVERWRITE）。
3. US2（受信 FIFO）→ US3（集計・既存結果の確認）→ Polish。

## Notes

- `CanSim/` の新規コードに日本語を含めない（ASCII のみ）。
- 新しいイベント種別は追加しない（research.md 決定2）。
