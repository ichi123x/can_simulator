# Implementation Plan: 送信・受信フレームバッファ

**Branch**: `ver.0.2`（spec ディレクトリ: `008-frame-buffers`） | **Date**: 2026-09-30 | **Spec**: [spec.md](./spec.md)

## Summary

`CanSim` のシミュレーションに、ECU ごとの送信バッファ 16 本と受信バッファ 8 本（FIFO）を加える。送信側は、送信要求時に「送信待ち（`PendingFrames`）＋ 送信中の 1 本」が 16 本なら要求を捨て、OVERWRITE の行をログに出す。受信側は、各受信ノードの「取り出し予定時刻（受信処理時刻）の列」を持ち、受信完了のたびに取り出し済みのものを先に外してから、8 本満杯なら捨ててOVERWRITE にする。新しいイベント種別は追加しない（受信処理遅延がノードごとに一定なので、取り出しの順序と時刻は受信完了時に決まる）。ログの列名を Update_Count に変え、集計の ECU ごとの行に送信側・受信側の破棄回数を足す。

## Technical Context

**Language/Version**: C++14相当（機能 001〜007 と同じ）

**Primary Dependencies**: C++ 標準ライブラリのみ（`std::deque`）。UI の変更はない。

**Storage**: ログ CSV（列名変更、OVERWRITE 行、空欄の時刻）と集計 CSV（列追加）。

**Testing**: `tools/cansim_runner/checks.cpp` に検証を追加。既存サンプル（バッファが満杯にならない）の結果が、列名と追加列を除いて本機能の前と一致することを、基準 CSV（006 以降で作成）との比較で確認する。

**Target Platform**: Windows 10/11（Win32/x64、Debug/Release）

**Project Type**: desktop-app（`CanSim/` のコアのみ変更）

**Performance Goals**: 既存と同等（送信側は 1 回の要求につき本数の比較 1 回、受信側は FIFO の先頭確認のみ）。

**Constraints**: 同一構成で同一結果（憲章 P③）。バッファが満杯にならない構成では、Status・時刻・回数は不変（spec FR-011）。`CanSim/` は ASCII のみ。

**Scale/Scope**: 変更: `CanTypes.h`（定数、`LogRow`・`PendingFrame` の項目名、受信 FIFO、破棄回数）、`Network.cpp`（`EnqueueTxRequest` の結果）、`SimulationEngine.cpp`（送信側の破棄行、受信 FIFO）、`Logger.cpp`（列名・空欄）、`MetricsAggregator.cpp`（OVERWRITE 行の除外、破棄回数の列）、`FrameEvents.cpp`（送信側破棄行の除外）、`checks.cpp`、サンプル 1 つ追加、CLAUDE.md と 001 のログ契約への注記。

## Constitution Check

| 原則 | 判定 | 根拠 |
|---|---|---|
| I. Spec Kit ワークフロー準拠 | PASS | spec.md（008）→ 本 plan.md。 |
| II. MFC/C++ ネイティブ規約 | PASS | 新規ソースなし。4 構成のビルドを確認する。 |
| III. テスト可能性 | PASS | 変更はすべて `CanSim/`。checks.exe で検証。 |
| IV. YAGNI | PASS | 新しいイベント種別を作らず、受信 FIFO は取り出し予定時刻の列で表す。バッファ本数の設定化はしない。 |
| V. ドキュメント同期 | PASS（条件付き） | CLAUDE.md（バッファ、OVERWRITE、Update_Count、集計の列）と、001 の `contracts/log-output-format.md` に後継の注記を入れるタスクを含める。 |
| VI. ドメイン仕様準拠 | PASS | Doc 3.7a（FR-090〜FR-095）と FR-051 を実装する。フレーム単位 DES と決定性を維持。 |

違反なし。Complexity Tracking は該当なし。

## Project Structure

### Documentation (this feature)

```text
specs/008-frame-buffers/
├── plan.md / research.md / data-model.md / quickstart.md
├── contracts/log-and-summary.md   # ログの列・OVERWRITE 行・空欄、集計の追加列
└── tasks.md
```

### Source Code (repository root)

```text
MFCApplication1/CanSim/
├── CanTypes.h           # kTxBufferCount=16, kRxBufferCount=8、UpdateCount への改名、Node の受信 FIFO と破棄回数
├── Network.h/.cpp       # EnqueueTxRequest が Queued / Updated / Discarded を返す（送信中の 1 本を数える）
├── SimulationEngine.cpp # 送信側の破棄行の出力、受信 FIFO（取り出し済みを外す → 満杯なら破棄）
├── Logger.cpp           # 列名 Update_Count、空欄（負の値）の出力
├── MetricsAggregator.*  # OVERWRITE 行を除外、Node 行に TxDiscard_Count / RxDiscard_Count
├── FrameEvents.cpp      # 送信側で捨てた行（Tx_Start なし）を再生から除外
└── SampleConfigs/sample_buffer_full.canprj  # 新規: 送信・受信の満杯を起こす確認用
tools/cansim_runner/checks.cpp
CLAUDE.md
specs/001-can-simulation-core/contracts/log-output-format.md  # 後継の注記
```

**Structure Decision**: 受信バッファは「受信処理時刻の昇順の列」として各ノードに持ち、受信完了時刻 t で `Rx_Process ≤ t` のものを先に外す（spec FR-007）。送信バッファは既存の `PendingFrames` と「送信中の 1 本」で数える。

## Complexity Tracking

該当なし。
