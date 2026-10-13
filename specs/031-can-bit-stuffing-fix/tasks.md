---
description: "Task list for feature 031: count bit stuffing as CAN 2.0A specifies"
---

# Tasks: ビットスタッフィングを CAN 2.0A の規格どおりに数える

**Input**: Design documents from `specs/031-can-bit-stuffing-fix/`

**Prerequisites**: plan.md、spec.md、research.md、data-model.md、contracts/frame-timing.md、quickstart.md

**Tests**: 憲章 III（自動テスト必須・テストファースト）により、テストを先に書き、今の実装で失敗することを確かめてから直す。

**Organization**: タスクはユーザーストーリーごとにまとめる。ブランチは `ver.0.9`。

## Format: `[ID] [P?] [Story] Description`

- **[P]**: 並行してできる（別のファイルで、終わっていないタスクに依存しない）
- **[Story]**: ユーザーストーリー（US1・US2・US3）
- 説明にファイルのパスを書く

---

## Phase 1: Setup

**Purpose**: 変更の前の状態を記録する（US3 の比較と、テストファーストの確認に使う）

- [X] T001 本機能の前のビルドで、`tools/cansim_runner/runner.cpp` を CLAUDE.md の手順でビルドし、`MFCApplication1/CanSim/SampleConfigs/` の `sample_collision.canprj`・`sample_vehicle_2ecu.canprj`・`sample_vehicle_16ecu.canprj` をそれぞれ 5 秒実行して、フレームログ・集計・起動記録を `build/031-before/` に保存する（Git に入れない）。ビルドした `runner.exe` も `build/031-before/` に残し、Release 相当（`/O2`）でビルドしたその `runner.exe` で `sample_vehicle_16ecu.canprj` を 600 秒実行したときの計算の時間を 3 回測って記録する（T022 で使う。plan の Performance Goals）
- [X] T002 本機能の前の状態で `cmake -S . -B build`、`cmake --build build --config Release`、`ctest --test-dir build -C Release --output-on-failure` を実行し、全件成功（CLAUDE.md の「全 26 件」）であることを記録する。`build/` が別の構成（ver.0.7 の CMake）のキャッシュを持っていてエラーになるときは、`build/` を消して作り直す

**Checkpoint**: 前の結果がそろっている

---

## Phase 2: Foundational

本機能には、すべてのストーリーの前に必要な共通の土台は無い（`FrameTiming.cpp` の修正は US1 に含め、US2 はその結果を使う）。

---

## Phase 3: User Story 1 - シミュレーションの伝送時間が規格どおりになる (Priority: P1) 🎯 MVP

**Goal**: `ComputeFrameTiming` がスタッフビットを規格どおりに数え、シミュレーションの伝送時間がドメイン仕様書の例（0x080 = 123 ビット・246 µs）と一致する

**Independent Test**: `ctest --test-dir build -C Release -R Feature031` が全件成功し、`sample_collision` のフレームログで 0x080・0x100 の伝送時間が 246 µs、0x100 の送信開始が 0x080 の送信完了の 6 µs 後になる

### Tests for User Story 1（先に書いて、失敗を確かめる）

- [X] T003 [US1] `tests/feature_031_tests.cpp` を新しく作り、`TEST(Feature031, SC001_...)` として contracts/frame-timing.md の 5 例（0x000・DLC 0 = 50 / 6 / 100000 ns、0x7FF・DLC 0 = 47 / 3 / 94000、0x555・DLC 1・0xAA = 54 / 2 / 108000、0x123・DLC 8・01〜08 = 116 / 8 / 232000、0x080・DLC 8・FF × 8 = 123 / 15 / 246000）の `TotalBits`・`StuffBitCount`・`TxDurationNs` を 500 kbps でリテラルと比べる。期待値は research R6 のとおり製品とは別の計算で求めた値。コメントは日本語、根拠（spec SC-001、ドメイン仕様書 FR-021 とログの例）を書く
- [X] T004 [US1] 同じファイルに `TEST(Feature031, SC003_SC004_...)` を足す。テストの中に規格どおりの参照の数え方（元の列を先頭から読み、直前に出力したビットの値と連続の数を持ち、5 連続で反対の値を出力して連続を 1 に戻す。research R6）を書き、ID 0x000〜0x7FF × DLC 0〜8（データは `MakeFrameData(dlc)`）のすべてで `StuffBitCount` が一致すること（SC-003）、`StuffBitCount` が ⌊(34 + 8 × DLC − 1) / 4⌋ 以下であること（SC-004）、`TotalBits` = 34 + 8 × DLC + `StuffBitCount` + 10 であることを確かめる。CRC シーケンスの最後のビットで 5 連続になり、CRC デリミタの前にスタッフビットが入る場合（spec の Edge Cases）もこの照合に含まれる。参照の実装でそうなる (ID, DLC) を照合の中で 1 つ以上数え、0 件ならテストを失敗にする（この場合を確かめずに通らないように）。コメントに根拠を書く。不一致は最初の数件の ID・DLC・値をメッセージに出す
- [X] T005 [US1] 同じファイルに `TEST(Feature031, SC005_...)` を足す。0x080・DLC 8・FF × 8 で、125k / 250k / 500k / 1M bps の `BitTimeNs` が 8000 / 4000 / 2000 / 1000、`TxDurationNs` が 123 × `BitTimeNs`、`IntermissionNs` が 3 × `BitTimeNs` であることを確かめる
- [X] T006 [US1] 同じファイルに `TEST(Feature031, SC002_...)` と `TEST(Feature031, SC006_...)` を足す。`sample_collision.canprj` を `CANSIM_SAMPLE_DIR` から読み（`tests/support/test_helpers.h` の既存の読み方に合わせる）、`BuildRunNetwork` → `SimulationEngine` で 0.1 秒実行し、0x080 と 0x100 のすべての送信で `TxEndUs − TxStartUs` = 246、同じ要求時刻の 0x100 の `TxStartUs` = 0x080 の `TxEndUs` + 6 であること（SC-002）、同じ構成を 2 回実行してフレームログの全行が一致すること（SC-006）を確かめる
- [X] T007 [US1] `cmake --build build --config Release` と `ctest --test-dir build -C Release -R Feature031 --output-on-failure` を実行し、0x000・0x555・0x080 の例、全 ID の照合（SC-003）、SC-002 が失敗し、0x7FF・0x123 の例、SC-004・SC-005・SC-006 が通ることを確かめて、結果を本ファイルの末尾の「実装の記録」に書く

### Implementation for User Story 1

- [X] T008 [US1] `MFCApplication1/CanSim/FrameTiming.cpp` の `ComputeFrameTiming` で、スタッフビットの数え方を research R1 のとおりに直す: SOF〜CRC シーケンスの元のビット列を先頭から読み、差し込んだビット列（`std::vector<int>`）に書き足し、書き足した列の末尾で同じ値が 5 個続いたら反対の値を 1 つ書き足して連続の数を 1 に戻す。`StuffBitCount` = 差し込んだ列の長さ − 元の列の長さ、`TotalBits` = 差し込んだ列の長さ + 10。CRC・`BitTimeNs`・`TxDurationNs`・`IntermissionNs` の求め方は変えない。ソースは ASCII のみ（CLAUDE.md）
- [X] T009 [US1] `MFCApplication1/CanSim/FrameTiming.h` のコメントに、スタッフビットの数え方（差し込んだビットも次の連続の 1 個目に数える、SOF〜CRC シーケンスが対象、末尾の 10 ビットはスタッフしない）を英語で書く。宣言は変えない
- [X] T010 [US1] `MFCApplication1/CanSim/SimulationEngine.h/.cpp` に、data-model.md「伝送時間の表」を足す: コンストラクタで全ノードの全フレームについて `ComputeFrameTiming(CanId, Dlc, 定義の Data, BitRateBps)` を 1 回ずつ呼んで表（ノードの位置 × フレームの位置）に持ち、`SimulationEngine.cpp` 160 行目付近の送信開始での `ComputeFrameTiming` の呼び出しを、表を引く形に置き換える。「表の値は、送信のたびに呼んだ結果と同じ」こと
- [X] T011 [US1] `ctest --test-dir build -C Release --output-on-failure` で全件（既存の機能 030 のテストと `Feature031`）が成功することを確かめる。失敗した既存のテストがあれば、伝送時間の値に依存する期待値かを調べ（research R4）、そうであれば規格どおりの値に直して、どのテストのどの期待値を直したかを「実装の記録」に書く。そうでなければ実装を直す
- [X] T012 [US1] `ctest --test-dir build -C Debug -LE slow --output-on-failure` も全件成功することを確かめる

**Checkpoint**: シミュレーションの伝送時間が規格どおりになり、自動テストが通る（MVP）

---

## Phase 4: User Story 2 - 入力画面のビット長・伝送時間が規格どおりになる (Priority: P2)

**Goal**: データフレーム編集の入力画面の「ビット長」「伝送時間」が規格どおりの値を表示する

**Independent Test**: quickstart.md §3 の表のとおりに表示される

- [X] T013 [US2] `MFCApplication1/FrameEntryDlg.cpp` が `CanSim::ComputeFrameTiming(id, dlc, CanSim::MakeFrameData(dlc), m_bitRateBps)` の `TotalBits` と `TxDurationNs` をそのまま表示していることを確かめる（コードの変更は無い見込み。変更が要る場合は UI の表示だけを直す）
- [X] T014 [US2] `msbuild MFCApplication1\MFCApplication1.sln` で Debug / Release × x86 / x64 の 4 構成をビルドし、すべて成功することを確かめる（憲章 II）。Release x64 の exe を利用者の手動確認（quickstart.md §3）のために用意する

**Checkpoint**: 入力画面の表示が規格どおり（手動確認は利用者）

---

## Phase 5: User Story 3 - 結果の説明がつく（既存の結果との差がわかる） (Priority: P3)

**Goal**: 本機能の前と後の結果の違いが、伝送時間の差だけで説明できる

**Independent Test**: quickstart.md §4 の比較で、SC-007 のとおりの差だけが出る

- [X] T015 [US3] 本機能の後のビルドで T001 と同じ 3 つのサンプルを実行し、`build/031-after/` に保存する
- [X] T016 [US3] `build/031-before/` と `build/031-after/` の `sample_vehicle_2ecu` のフレームログを比べる短いスクリプト（PowerShell または Python、Git に入れない）で、行数・行の順序・CAN ID・送信 ECU・受信 ECU・状態・再送回数・更新回数・送信要求・送信開始が一致し、送信完了・受信の時刻の差が、その CAN ID のスタッフビットの差（前の実装の値 − 規格どおりの値）× 2 µs に等しく、`Bus_Load_Pct` が前の値以下であることを確かめ（SC-007）、結果を「実装の記録」に書く。`sample_collision`・`sample_vehicle_16ecu` は、行の順序や再送回数が変わり得る（spec の Edge Cases）ので、変わった数（送信フレーム・再送・最長の待ち・バス負荷）だけを記録する。あわせて `sample_vehicle_2ecu` の集計 CSV（`.summary.csv`）も比べ、`TxDuration`・`TotalLatency` の最小・最大・平均が小さくなる（または同じ）こと、`QueueingDelay` と `Jitter_Max_us`・`Jitter_Min_us`・`Jitter_StdDev_us` が変わらないこと、`BusLoad_Avg_Pct` が前の値以下であることを確かめて、差を「実装の記録」に書く（spec US3 シナリオ 2）

**Checkpoint**: 結果の変化の説明がつく

---

## Phase 6: Polish & Cross-Cutting Concerns

- [X] T017 [P] `tools/vehicle_samples/verify_log.ps1` で 4 つの車両サンプル（`sample_vehicle_2ecu`・`_4ecu`・`_8ecu`・`_16ecu`、各 5 秒）を確かめ、`tools/vehicle_samples/README.md` の結果の表（送信フレーム・ログの行・調停負けの再送・最長の待ち・バス負荷・破棄）と 16 ECU の詳細（状態ごとの行数など）を新しい値に書き直す。対象のブランチ・コミットの記載も更新する
- [X] T018 [P] `README.md` の「バス使用率（実車デモ構成）」「再送フレーム数（16 ECU・5 秒）」を T017 の値に直す
- [X] T019 [P] `CLAUDE.md` を更新する: `FrameTiming.h/.cpp` の説明に「スタッフビットを差し込んだ列の上で数える（機能 031）」、`SimulationEngine` の説明に「伝送時間はフレームの定義ごとに実行の開始時に計算」、SampleConfigs の車両サンプルの数値（16 ECU の送信フレーム・再送・バス負荷）を T017 の値に
- [X] T020 `tools/cansim_runner/checks.cpp` を CLAUDE.md の手順でビルドして実行し、全項目が通ることを確かめる。基準 CSV（`CANSIM_BASELINE_DIR`）を使う確認のために、本機能の後のビルドで基準を作り直す手順を `tools/cansim_runner/README.md` に追記する（本機能で値が変わるため、031 より前の基準は使えない）
- [X] T021 Release で `ctest --test-dir build -C Release --output-on-failure`、Debug で `ctest --test-dir build -C Debug -LE slow` を実行して全件成功を確かめ、件数（Feature031 を足した後の数）を CLAUDE.md のテストの件数の記載に反映する
- [X] T022 本機能の後のビルドで、T001 と同じ方法（`/O2` の `runner.exe`、`sample_vehicle_16ecu.canprj`、600 秒、3 回）で計算の時間を測り、T001 の値と並べて「実装の記録」に書く。後の方が長くなっていないこと（3 回の中央値で比べる）を確かめ（plan の Performance Goals、FR-006 の事前計算の効果）、長くなっていれば原因を調べる
- [X] T023 quickstart.md の §1〜§5 を順に行い、自動でできる §1・§2・§4・§5 の結果を「実装の記録」に書く。§3（入力画面）は利用者の手動確認として、確認の欄を「実装の記録」に用意する

---

## Dependencies & Execution Order

- **Phase 1（T001・T002）**: 最初に行う。T001 の前の結果は T016 で使う
- **US1（T003〜T012）**: テスト（T003〜T006）→ 失敗の確認（T007）→ 実装（T008〜T010）→ 全件の確認（T011・T012）の順。T003〜T006 は同じファイルなので順に書く。T008 と T010 は別のファイルだが、T010 は T008 の後に確かめる
- **US2（T013・T014）**: US1 の後（同じ `ComputeFrameTiming` の結果を表示するため）
- **US3（T015・T016）**: US1 の後
- **Polish（T017〜T023）**: US1〜US3 の後。T017・T018・T019 は別のファイルなので並行してよい（T018・T019 は T017 の値を使う）

## Parallel Example

```text
# US1 の実装の後、US2 と US3 は並行してよい
T013 [US2] FrameEntryDlg.cpp の表示の確認
T015 [US3] 本機能の後のサンプルの実行

# Polish の文書の更新（T017 の値がそろった後）
T018 [P] README.md
T019 [P] CLAUDE.md
```

## Implementation Strategy

- **MVP**: US1（T001〜T012）。シミュレーションの結果が規格どおりになり、自動テストで確かめられる
- **次に**: US2（4 構成のビルドと入力画面の手動確認）、US3（前後の比較による説明）
- **最後に**: 文書の数値と件数の更新、checks.exe・基準 CSV の手順

## 実装の記録

実施日 2026-10-13、ブランチ `ver.0.9`。

### Phase 1（T001・T002）

- T001: 変更の前のソースで `runner.exe` を `/O2` 付きでビルドし（CLAUDE.md の手順に `/O2` を足した 1 回のビルドを、5 秒の実行と 600 秒の測定の両方に使った）、3 つのサンプルの 5 秒の結果を `build/031-before/` に保存した。`sample_vehicle_16ecu` 600 秒の実行時間（計算と CSV の書き出しを含む）は 3.20・2.85・2.87 秒、中央値 2.87 秒。
- T002: `build/` に ver.0.7 の CMake（GoogleTest 1.15.2）のキャッシュが残っていてリンクエラーになったので、`build/031-*` 以外を消して作り直した。その後、変更の前の Release で 51 件すべて成功（CLAUDE.md の「全 26 件」は ctest の件数と合っていなかったので、T021 で直した）。

### US1（T003〜T012）

- T003〜T006: `tests/feature_031_tests.cpp` に 4 つのテストを追加した（SC-001、SC-003・SC-004、SC-005、SC-002・SC-006）。SC-003 の参照の数え方はテストの中に、製品とは別の書き方で置いた。CRC シーケンスの最後のビットの直後に差し込む場合が照合に含まれることも数えて確かめる。
- T007（変更の前の実行）: SC-001 は 0x000・DLC 0（52 / 8）、0x555・DLC 1（55 / 3）、0x080・DLC 8・FF（127 / 19）が失敗し、0x7FF・DLC 0 と 0x123・DLC 8 は通った。SC-003 は 18,432 通りのうち 14,959 通りが規格どおりの参照と合わなかった（最悪値の超過は 0）。SC-002 は失敗。SC-005 も失敗した（期待値 123 ビットが規格どおりの値のため。tasks.md の当初の見込み「通る」は誤りで、規格どおりの値を期待値にしている以上、変更の前は失敗するのが正しい）。
- T008・T009: `FrameTiming.cpp` のスタッフの数え方を、差し込んだビット列（計算用のフレームバッファ）に書き足しながら数える形に直し、`FrameTiming.h` に数え方のコメントを書いた。宣言は変えていない。
- T010: `SimulationEngine` のコンストラクタでフレームの定義ごとの伝送時間の表（`m_frameTimings`）を作り、送信開始で表を引くようにした。
- T011: Release で、Feature031 の 4 件は通り、既存のテストのうち次の 3 つの期待値が伝送時間に依存していて失敗した。どれも製品とは別の計算で規格どおりの値を求め、その値に直した（research R4）。
  - `tests/feature_001_tests.cpp`: sample_appendix_a の 0x080 の最初の Tx_Start 162 → 158 us（先に送る 0x050・DLC 2・FF が 67 → 65 ビット）。
  - `tests/feature_002_tests.cpp`: sample_startup_latejoin のノード 2 の Normal_Entry 280 → 274 us、ノード 3 の Normal_Entry 580 → 574 us（0x100・DLC 8・FF が 126 → 123 ビット）。
  - 直した後、Release 55 件すべて成功。
- T012: Debug（slow を除く）54 件すべて成功。

### US2（T013・T014）

- T013: `FrameEntryDlg.cpp` は `ComputeFrameTiming(id, dlc, MakeFrameData(dlc), m_bitRateBps)` の `TotalBits` と `TxDurationNs` をそのまま表示している。コードの変更は無し。
- T014: `MFCApplication1.sln` を Debug / Release × x86 / x64 の 4 構成でビルドし、すべて成功（エラー・警告なし）。手動確認用の `MFCApplication1/x64/Release/MFCApplication1.exe` を作った。

### US3（T015・T016）

- T015: 変更の後の `runner.exe`（`/O2`）で 3 つのサンプルを 5 秒実行し、`build/031-after/` に保存した。
- T016: `sample_vehicle_2ecu`（600 行）: 行数・行の順序・CAN ID・送信 ECU・受信 ECU・状態・再送回数・更新回数・送信要求・送信開始はすべて一致。送信完了・受信完了・受信処理の時刻の差は、すべての行で（前の数え方のスタッフビット − 規格どおりのスタッフビット）× 2 µs と一致。`Bus_Load_Pct` が前より大きくなった行は 0。集計 CSV: すべてのグループで `TxDuration`・`TotalLatency` の最小・最大・平均と `BusLoad_Avg_Pct` は同じか小さく、`QueueingDelay` とジッターの 3 列は一致（ALL: TxDuration 平均 243.17 → 235.92 µs、TotalLatency 平均 243.17 → 235.92 µs、BusLoad_Avg_Pct 2.89 → 2.81%）。
  - `sample_collision`: 送信 1,000・行 1,000・再送 500 は同じ、最長の待ち 260 → 252 µs、バス負荷（使用時間 ÷ 範囲）5.06 → 4.92%。
  - `sample_vehicle_16ecu`: 送信 4,015・行 16,080・再送 990 は同じ、最長の待ち 514 → 500 µs、バス負荷（使用時間 ÷ 範囲）19.04 → 18.56%。

### Polish（T017〜T023）

- T017: 4 つの車両サンプル（5 秒）を変更の後で実行し、`verify_log.ps1` はすべて「no problems found」。送信フレーム・ログの行・再送・破棄・状態ごとの行数（16 ECU: SUCCESS 12,735・ARBITRATION_LOST_RETRY 3,345）は変わらず、最長の待ち 0 / 172 / 500 / 500 µs、集計のバス負荷 2.81 / 7.95 / 14.80 / 18.37%（以前は 0 / 174 / 514 / 514 µs、2.89 / 8.17 / 15.10 / 18.74%）。`tools/vehicle_samples/README.md` の結果の表をこの値にし、更新の経緯を書いた。
- T018: `README.md` のバス使用率を「約 3〜18%」に（再送 990 は変わらない）。
- T019: CLAUDE.md の FrameTiming と SimulationEngine の説明に機能 031 を書き、SampleConfigs のバス負荷を「約 3〜18%」にした。
- T020: `checks.exe` にも T011 と同じ 3 つの期待値があり失敗したので、同じ根拠で 158 / 274 / 574 us に直した。直した後「0 check(s) failed」。`tools/cansim_runner/README.md` に、基準 CSV を 031 以降のビルドで作り直す旨を足した。基準 CSV そのものは作っていない（リポジトリに置かない運用のため）。
- T021: Release 55 件・Debug（slow を除く）54 件すべて成功。CLAUDE.md の件数を「全 55 件」「54 件」に直した。
- T022: 変更の後の `sample_vehicle_16ecu` 600 秒は 2.38・2.30・2.33 秒、中央値 2.33 秒（変更の前 2.87 秒より約 19% 短い）。送信のたびの CRC とスタッフの計算が無くなったため。
- T023: quickstart の §1（自動テスト）・§2（`sample_collision` のログで 0x080 が 22 → 268 us の 246 us、0x100 が 274 → 520 us の 246 us・`ARBITRATION_LOST_RETRY`・Retry_Count 1）・§4（T016）・§5（T017〜T020）を確かめた。§3（入力画面）は利用者の手動確認として下に欄を用意した。

### 手動確認（利用者、Release x64 の `MFCApplication1/x64/Release/MFCApplication1.exe`）

| 入力（500 kbps のプロジェクト） | 期待する表示 | 結果 |
|---|---|---|
| ID 0x080、DLC 8 | ビット長 123 ビット、伝送時間 246 µs | OK（「123 ビット / 246 μs」、2026-10-13） |
| ID 0x7FF、DLC 0 | ビット長 47 ビット、伝送時間 94 µs | OK（「47 ビット / 94 μs」、2026-10-13） |
| ID 0x000、DLC 0 | ビット長 50 ビット、伝送時間 100 µs | OK（「50 ビット / 100 μs」、2026-10-13） |
