---

description: "Task list for feature 030: GoogleTest/CTest による自動テストの導入"
---

# Tasks: GoogleTest/CTest による自動テストの導入

**Input**: Design documents from `/specs/030-googletest-integration/`

**Prerequisites**: [plan.md](plan.md), [spec.md](spec.md), [research.md](research.md), [data-model.md](data-model.md), [contracts/](contracts/), [quickstart.md](quickstart.md)

**Tests**: 本機能はテスト基盤そのもの。テストの追加（`checks.cpp` からの移行）が成果物であり、各移行タスクが「テスト」を兼ねる。移行したテストは `checks.exe` と同じ入力・同じ期待で、緑になることを確認する（憲章 III）。

**Format**: `- [ ] T### [P?] [US?] 説明（ファイルパス）`。`[P]` = 別ファイルで他の未完了タスクに依存しない。

**共通ルール（全タスク）**: `CanSim/` のソースは変更しない。新規ファイルは ASCII のみ（日本語のコメント・文字列を書かない）。`MFCApplication1.sln` / `.vcxproj` / `.filters` は触らない。テスト名は [contracts/test-naming.md](contracts/test-naming.md) に従う。

## Phase 1: Setup

- [X] T001 `checks.cpp` の機能ごとの項目数（各 `CheckFeatureNNN` 内の `Check(` の数）と、`g_baselineDir` を使う項目の一覧を数え、`tests/migration-ledger.md` を新規作成する（機能 001〜011・014・015・017〜020・022〜029 の 25 行。列: 機能 / checks.cpp の項目数 / 移した項目数 / テストファイル / 状態 / 備考。状態はすべて「未着手」）
- [X] T002 未追跡の雛形 `main.cpp` と `tests.cpp`（リポジトリ直下）を削除する（research R-08）

---

## Phase 2: Foundational（全ストーリーの前提）

**目的**: CMake の骨組みとテスト補助。これが無いと移行できない。

- [X] T003 リポジトリ直下の `CMakeLists.txt` を置き換える: `cmake_minimum_required(VERSION 3.14)`、`project(can_simulator CXX)`、`CMAKE_CXX_STANDARD 14`（拡張なし・必須）、`file(GLOB CANSIM_SOURCES CONFIGURE_DEPENDS MFCApplication1/CanSim/*.cpp)` から `add_library(cansim_core STATIC ...)`、インクルードは `MFCApplication1/CanSim`（PUBLIC）と `tools/cansim_runner`（PRIVATE。空の `pch.h`）、`NOMINMAX` を PUBLIC 定義、MSVC は `/W3 /EHsc`、`enable_testing()` と `add_subdirectory(tests)`（research R-01・R-03）
- [X] T004 `tests/CMakeLists.txt` を新規作成する: `FetchContent` で GoogleTest v1.14.0（URL 固定。取得して SHA256 を求め `URL_HASH SHA256=...` に記録）、`gtest_force_shared_crt ON`、`INSTALL_GTEST OFF`、`FetchContent_MakeAvailable`、`file(GLOB TEST_SOURCES CONFIGURE_DEPENDS feature_*_tests.cpp support/*.cpp)` から `add_executable(runTests ...)`、`target_link_libraries(runTests PRIVATE cansim_core gtest_main)`、コンパイル定義 `CANSIM_SAMPLE_DIR`（`${CMAKE_SOURCE_DIR}/MFCApplication1/CanSim/SampleConfigs`）と `CANSIM_TEST_TMP_DIR`（`${CMAKE_BINARY_DIR}/test_tmp`）、`include(GoogleTest)` と `gtest_discover_tests(runTests DISCOVERY_MODE PRE_TEST)`（research R-02・R-05。`DISCOVERY_MODE` は CMake 3.18 以上。3.14 を最低とする記述のため、`DISCOVERY_MODE` は `if(NOT CMAKE_VERSION VERSION_LESS 3.18)` のときだけ付ける）
- [X] T005 [P] `tests/support/check_support.h` を新規作成する: `#define CHECK(ok, what) EXPECT_TRUE(ok) << (what)`（`<gtest/gtest.h>` を include。`what` は `std::string` でも `const char*` でも受ける）
- [X] T006 `tests/support/test_helpers.h` と `tests/support/test_helpers.cpp` を新規作成する: `checks.cpp` 冒頭（48〜138 行付近）のヘルパ（`Widen`・`Join`・`RunResult`・`RunConfigFile`・`WriteTextFile`・`FindNode`・`RowsEqual` ほか、各機能が共通で使うもの）を名前空間 `cansimtest` に移して複製する（`checks.cpp` 側は変更しない）。`g_sampleDir` / `g_tempDir` は `SampleDir()` / `TempDir()` 関数にして、それぞれ `CANSIM_SAMPLE_DIR` / `CANSIM_TEST_TMP_DIR` を返し、一時ディレクトリが無ければ作る。`BaselineDir()` は環境変数 `CANSIM_BASELINE_DIR`（無ければ空）を返す。基準 CSV が必要なテスト用に `SKIP_IF_NO_BASELINE()` マクロ（空なら `GTEST_SKIP() << "set CANSIM_BASELINE_DIR (see tools/cansim_runner/README.md)"`）を足す（research R-05・R-06）。機能ごとのファイルだけが使う局所ヘルパは、そのファイルに置く

**Checkpoint**: `cmake -S . -B build` が通る（テストソースがまだ無いので `runTests` は T007 で初めてビルドされる）。

---

## Phase 3: User Story 1 - 1 つのコマンドでコアロジックの自動テストを実行する (Priority: P1) 🎯 MVP

**Goal**: 構成 → ビルド → `ctest` で、`CanSim/` のテストが走り、成功・失敗がテスト名（機能番号・SC/FR の ID 入り）で分かる。

**Independent Test**: クリーンな `build/` で [quickstart.md](quickstart.md) の手順 1・2 を行い、全件成功し、ロジックを壊すと失敗する。

- [X] T007 [US1] `tools/cansim_runner/checks.cpp` の `CheckFeature001()`（139〜254 行付近）を `tests/feature_001_tests.cpp` へ移す。`#include "check_support.h"` / `"test_helpers.h"` を使い、`Check(` → `CHECK(`、`g_sampleDir` / `g_tempDir` → `SampleDir()` / `TempDir()` に置換するだけで、入力・期待は変えない。ラベルの `"001 SC-nnn:"` / `"001 FR-nnn:"` ごとに `TEST(Feature001, SC001_<内容>)` / `TEST(Feature001, FR0nn_<内容>)` にまとめる（`CHECK` の数を T001 の項目数と一致させる）
- [X] T008 [US1] 構成・ビルド・実行を通す: `cmake -S . -B build`、`cmake --build build --config Release`、`ctest --test-dir build -C Release --output-on-failure`。`cansim_core` のビルドで警告 C4819 が出ないこと、`ctest -N` の全テスト名が `Feature001.` で始まることを確認する。通らなければ T003〜T006 を直す（`checks.exe` を従来の手順でもビルド・実行し、機能 001 の項目数と一致して全件成功であることを突き合わせる）
- [X] T009 [US1] SC-004 の確認（機能 001 分）: `MFCApplication1/CanSim/FrameTiming.cpp` を一時的に壊して（例: ビットスタッフィングの数え方）再ビルドし、`Feature001` のテストが失敗して項目のラベルが出ることを確認してから、`git checkout -- MFCApplication1/CanSim/FrameTiming.cpp` で元に戻す
- [X] T010 [US1] Debug 構成でも通ることを確認する（`cmake --build build --config Debug` / `ctest --test-dir build -C Debug`）。C4819 などの警告が `cansim_core` / `runTests` で増えていないこと
- [X] T011 [US1] `tests/migration-ledger.md` の機能 001 の行を「移行済み」に更新する（移した項目数・テストファイル）

**Checkpoint**: US1 完了 = MVP。以降の移行は同じ型の繰り返し。

---

## Phase 4: User Story 2 - 既存の検証（checks.cpp）を機能ごとのテストへ移す (Priority: P2)

**Goal**: `checks.cpp` の機能 002〜029 を、機能ごとのテストファイルへ移す。`checks.exe` は残す。

**Independent Test**: 各機能ごとに、移したテストが緑で、`CHECK` の数が台帳の項目数と一致し、`checks.exe` も緑のまま。

各タスクの共通手順（T007 と同じ）: 該当する `CheckFeatureNNN()` の本体を `tests/feature_NNN_tests.cpp` へ移す。`Check(` → `CHECK(`、`g_sampleDir`/`g_tempDir`/`g_baselineDir` → `SampleDir()`/`TempDir()`/`BaselineDir()`、基準 CSV が無いと進めない項目は `SKIP_IF_NO_BASELINE()`。`"NNN SC-xxx:"` / `"NNN FR-xxx:"` ごとに `TEST(FeatureNNN, SCxxx_<内容>)` にまとめ、ID を持たない改訂の検証は `Rev<n>_<内容>`。ラベル・入力・期待は変えない。`checks.cpp` 側は変更しない。1 項目あたり 10 秒を超えるものは時間を控えておく（T040 で使う）。

- [X] T012 [P] [US2] 機能 002（`CheckFeature002`、255 行付近）→ `tests/feature_002_tests.cpp`
- [X] T013 [P] [US2] 機能 003（`CheckFeature003`、423 行付近）→ `tests/feature_003_tests.cpp`
- [X] T014 [P] [US2] 機能 004（`CheckFeature004`、561 行付近。基準 CSV を使う SC-009 は `SKIP_IF_NO_BASELINE()`）→ `tests/feature_004_tests.cpp`
- [X] T015 [P] [US2] 機能 005（`CheckFeature005`、748 行付近）→ `tests/feature_005_tests.cpp`
- [X] T016 [P] [US2] 機能 006（`CheckFeature006`、871 行付近。基準 CSV を使う項目あり）→ `tests/feature_006_tests.cpp`
- [X] T017 [P] [US2] 機能 007（`CheckFeature007`、1045 行付近）→ `tests/feature_007_tests.cpp`
- [X] T018 [P] [US2] 機能 008（`CheckFeature008`、1107 行付近）→ `tests/feature_008_tests.cpp`
- [X] T019 [P] [US2] 機能 009（`CheckFeature009`、1261 行付近）→ `tests/feature_009_tests.cpp`
- [X] T020 [P] [US2] 機能 010（`CheckFeature010`、1355 行付近）→ `tests/feature_010_tests.cpp`
- [X] T021 [P] [US2] 機能 011（`CheckFeature011`、1443 行付近）→ `tests/feature_011_tests.cpp`
- [X] T022 [P] [US2] 機能 014（`CheckFeature014`、1501 行付近）→ `tests/feature_014_tests.cpp`
- [X] T023 [P] [US2] 機能 015（`CheckFeature015`、1575 行付近）→ `tests/feature_015_tests.cpp`
- [X] T024 [P] [US2] 機能 017（`CheckFeature017`、1643 行付近）→ `tests/feature_017_tests.cpp`
- [X] T025 [P] [US2] 機能 018（`CheckFeature018`、1687 行付近）→ `tests/feature_018_tests.cpp`
- [X] T026 [P] [US2] 機能 019（`CheckFeature019`、1826 行付近）→ `tests/feature_019_tests.cpp`
- [X] T027 [P] [US2] 機能 020（`CheckFeature020`、1863 行付近）→ `tests/feature_020_tests.cpp`
- [X] T028 [P] [US2] 機能 022（`CheckFeature022`、1877 行付近）→ `tests/feature_022_tests.cpp`
- [X] T029 [P] [US2] 機能 023（`CheckFeature023`、1952 行付近）→ `tests/feature_023_tests.cpp`
- [X] T030 [P] [US2] 機能 024（`CheckFeature024`、2043 行付近）→ `tests/feature_024_tests.cpp`
- [X] T031 [P] [US2] 機能 025（`CheckFeature025`、2070 行付近）→ `tests/feature_025_tests.cpp`
- [X] T032 [P] [US2] 機能 026（`CheckFeature026`、2187 行付近）→ `tests/feature_026_tests.cpp`
- [X] T033 [P] [US2] 機能 027（`CheckFeature027`、2226 行付近）→ `tests/feature_027_tests.cpp`
- [X] T034 [P] [US2] 機能 028（`CheckFeature028`、2268 行付近）→ `tests/feature_028_tests.cpp`
- [X] T035 [P] [US2] 機能 029（`CheckFeature029`、2409 行付近。改訂 1 の大きなログの項目は `Rev1_...`、時間次第で `Feature029Slow`）→ `tests/feature_029_tests.cpp`
- [X] T036 [US2] 全体を再構成・ビルド・実行する（`cmake -S . -B build`、`cmake --build build --config Release`、`ctest --test-dir build -C Release --output-on-failure`）。失敗があれば、`checks.exe` の同じ項目と突き合わせ、移行の写し間違い（検証を弱めていないか）を直す。`ctest -N` の全テスト名が `Feature\d{3}(Slow)?\.` で始まることを確認する
- [X] T037 [US2] `checks.exe` を [CLAUDE.md](../../CLAUDE.md) の手順でビルド・実行し、終了コード 0 であること（SC-006）、`checks.cpp` が未変更であること（`git diff --stat -- tools/cansim_runner/checks.cpp` が空）を確認する
- [X] T038 [US2] 台帳を仕上げる: `tests/migration-ledger.md` の各機能で「移した項目数」を `CHECK(` の数から数えて記入し、`checks.cpp` の項目数との差があれば備考に項目と理由を書く。状態を更新する（SC-002）。基準 CSV が必要で skip になる項目は備考に書く
- [X] T039 [US2] SC-004 の代表確認（機能 006・008・028・029 分）: 各機能に対応する `CanSim/` のロジック（例: `FrameEditor.cpp` の検証、`SimulationEngine.cpp` のバッファ満杯、`LogAnalysis.cpp` の統計）を一時的に壊して該当テストが失敗することを確認し、`git checkout -- <ファイル>` で戻す（機能 001 は T009 で確認済み）
- [X] T040 [US2] 実行時間を測る（`Measure-Command { ctest --test-dir build -C Release }`）。60 秒を超える場合は、10 秒を超えるテストを `Feature<NNN>Slow` スイートに分け、`tests/CMakeLists.txt` で `gtest_discover_tests` の後に `set_tests_properties` でラベル `slow` を付けるか、`ctest -LE slow`（既定）と `-L slow`（別枠）に分ける（research R-04）。収まる場合は何もせず、結果（秒数）を台帳に記録する（SC-007）

**Checkpoint**: US1 + US2 完了。25 機能のテストが緑、`checks.exe` も緑。

---

## Phase 5: User Story 3 - 既存の Visual Studio ビルドに影響を与えない (Priority: P3)

**Goal**: アプリのビルド（4 構成）と `.sln` / `.vcxproj` が無変更のまま通る。

**Independent Test**: [quickstart.md](quickstart.md) の手順 5。

- [X] T041 [US3] `git diff --stat -- MFCApplication1/MFCApplication1.sln MFCApplication1/MFCApplication1.vcxproj MFCApplication1/MFCApplication1.vcxproj.filters` が空であることと、`git diff --stat -- MFCApplication1/CanSim` が空であることを確認する（FR-011・FR-012）
- [X] T042 [US3] `msbuild MFCApplication1\MFCApplication1.sln /p:Configuration=<Debug|Release> /p:Platform=<x86|x64>` を 4 構成（Debug/Release × x86/x64）で実行し、すべて成功することを確認する（SC-005。警告が増えていないことも見る）。ビルドの出力先が `build/` と衝突しないことも確認する
- [X] T043 [US3] `git status --short` で、追跡対象外の生成物が `build/`（`.gitignore` 済み）の外に出ていないこと、テストの一時出力が `build/test_tmp` に閉じていること、`SampleConfigs/` がテストで書き換えられていないこと（`git status` に出ない）を確認する（FR-013）

**Checkpoint**: 全ストーリー完了。

---

## Phase 6: Polish & Cross-Cutting

- [X] T044 [P] [CLAUDE.md](../../CLAUDE.md) を更新する（憲章 V）: 「ビルド・実行」節に、構成・ビルド・`ctest` の手順（[contracts/build-and-test-interface.md](contracts/build-and-test-interface.md) と同じコマンド）、`-R Feature006` での絞り込み、`CANSIM_BASELINE_DIR`、オフライン時の `FETCHCONTENT_SOURCE_DIR_GOOGLETEST` を追記する。「正式なテストフレームワークは導入していません」の記述を、GoogleTest/CTest を導入済みで `checks.exe` は移行完了まで並存する旨に直す。アーキテクチャ節の `CanSim/` の先頭に `cansim_core` と `tests/` の説明を足す
- [X] T045 [P] `tools/cansim_runner/README.md` に、移行状況（テストは `tests/`、台帳は `tests/migration-ledger.md`）と、`checks.exe` を削除するのは移行完了後の別作業であることを追記する
- [X] T046 [P] [specs/030-googletest-integration/checklists/requirements.md](checklists/requirements.md) と `spec.md` の `Status` を実装結果に合わせて更新する（`Status: Implemented`）。plan の Constitution Check の「要対応」を「合格」に直す
- [X] T047 `quickstart.md` の手順 1〜7 を通しで実行して結果を確認する（SC-001〜SC-007 をチェック）。クリーンな clone 相当（`build/` を消して）での初回構成も含める

---

## Dependencies & Execution Order

- Phase 1（T001・T002）→ Phase 2（T003〜T006。T005 は並列可）→ Phase 3（US1）→ Phase 4（US2）→ Phase 5（US3）→ Phase 6。
- US3 は他に依存しないが、`CanSim/` と `.vcxproj` が無変更かの確認なので、US1・US2 の後（最後の変更が済んでから）に行う。
- T007 は T003〜T006 に依存。T008〜T011 は T007 の後に順に。
- T012〜T035 は互いに別ファイルで `[P]`。ただし T003〜T006 の完了後、かつ T008（パイプラインが通ること）の後に始める（写しの型が確定してから）。`tests/CMakeLists.txt` は glob なので、ファイルを足しても編集は不要。
- T036〜T040 は T012〜T035 の完了後。T038（台帳）は T036 の後。T040 の結果で `tests/CMakeLists.txt` を変える場合のみ、T036 をやり直す。
- 台帳 `tests/migration-ledger.md` は共有ファイルなので、並列タスク（T012〜T035）では更新せず、T038 でまとめて更新する。

## Parallel Example

```text
# T008 のあと、25 機能のうち任意の数を同時に:
T012 tests/feature_002_tests.cpp   T013 tests/feature_003_tests.cpp   T014 tests/feature_004_tests.cpp
T028 tests/feature_022_tests.cpp   T034 tests/feature_028_tests.cpp   T035 tests/feature_029_tests.cpp
```

## Implementation Strategy

- **MVP = US1（T001〜T011）**: CMake の骨組み、補助、機能 001 の移行で、構成 → ビルド → `ctest` の全体が通る。ここで止めても、憲章 III を満たす基盤として使える。
- **増分**: US2 は機能単位で独立に完了できる。1 機能ごとに「テストが緑 + `checks.exe` が緑」で区切る。
- **US3** は変更を入れるたびに回帰として確認できる（T041〜T043 は軽いので、US2 の途中でも実行してよい）。

## Notes

- 総タスク数: 47（Setup 2・Foundational 4・US1 5・US2 29・US3 3・Polish 4）。
- `checks.exe`（`checks.cpp` / `runner.cpp`）の削除は本機能の範囲外（FR-009）。
- コミットは機能の区切り（US1 完了、US2 の任意の区切り、US3・Polish）でまとめる。

## Implementation notes (2026-10-07)

- All 47 tasks done. The generated test files came from a one-off script, not from hand edits; the script is not kept.
- CMake minimum raised to 3.20 (analysis finding F1): `ctest --test-dir` needs 3.20; later to 3.22 for `gtest_discover_tests(TEST_FILTER)` (the `slow` label of Feature018Slow).
- Helpers (analysis finding C1): everything in `checks.cpp`'s anonymous namespace that is not a `CheckFeatureNNN` went into `tests/support/test_helpers.h` as `inline` functions, so no feature file defines its own copy. `Check()` stays a function (`tests/support/check_support.h`), not a `CHECK` macro, so the bodies are unchanged. `g_sampleDir` / `g_tempDir` / `g_baselineDir` became `SampleDir()` / `TempDir()` / `BaselineDir()`.
- One `TEST` per feature, not per SC/FR: the checks of a feature share local variables, so splitting would have meant rewriting them. Test names list the SC/FR ids found in the labels (T007 and the research R-06 note say so). Splitting further is a separate job.
- The baseline CSV comparison (004 SC-009, 008 SC-005) is not a `GTEST_SKIP`; as in `checks.exe`, it runs only when `CANSIM_BASELINE_DIR` is set, and prints a NOTE otherwise.
- T040: all 25 tests take about 4 s in Release and 27 s in Debug, so no `slow` split.
- Verified: `checks.exe` and `runTests` pass the same 214 checks with identical labels; sabotage of `FrameTiming.cpp`, `FrameEditor.cpp`, `CanTypes.h` (tx buffer count) and `LogAnalysis.cpp` (P95, population variance) makes Features 001, 006, 008, 028 and 029 fail, and the sources were restored with `git checkout`; MSBuild builds all 4 configurations; `.sln` / `.vcxproj` / `CanSim/` have no diff.
