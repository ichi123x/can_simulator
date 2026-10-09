# Implementation Plan: GoogleTest/CTest による自動テストの導入

**Branch**: `030-googletest-integration` | **Date**: 2026-10-07 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `/specs/030-googletest-integration/spec.md`

## Summary

`MFCApplication1/CanSim/*.cpp` を、ファイルを動かさずに CMake の静的ライブラリ `cansim_core` としてビルドし、`tests/` の GoogleTest 実行ファイル `runTests`（`cansim_core` + `gtest_main`）を CTest に登録する。`tools/cansim_runner/checks.cpp` の `CheckFeatureNNN`（25 機能・`Check()` 204 呼び出し（実行時 214 回））は、機能ごとに `tests/feature_NNN_tests.cpp` へ機械的に移す（`Check(ok, label)` → `EXPECT_TRUE(ok) << label`）。テスト名は `Feature<NNN>.<SC|FR><id>_<内容>`。MSBuild 側（`.sln` / `.vcxproj`）には触れず、`checks.exe` は移行完了まで並存させる。移行は機能単位で段階的に進め、移行台帳（`migration-ledger.md`）で残りを追う。

## Technical Context

**Language/Version**: C++14（`CanSim/` と既存 `checks.exe` と同じ。GoogleTest 1.14 の最低要件も C++14）

**Primary Dependencies**: CMake ≥ 3.22（開発機は 4.4.4）、GoogleTest v1.14.0（`FetchContent` で URL とハッシュを固定して取得）、MSVC v143

**Storage**: ファイル（`SampleConfigs/*.canprj`・基準 CSV の読み込み、ビルドツリー内の一時出力）

**Testing**: GoogleTest + CTest（`gtest_discover_tests`）

**Target Platform**: Windows 10/11（MSVC。`checks.cpp` が `direct.h` / `io.h` を使うためクロスプラットフォームは対象外）

**Project Type**: 既存のデスクトップアプリ（MFC）に、UI 非依存コアのテスト基盤を追加する

**Performance Goals**: 通常の `ctest` は Release で 1 分以内（SC-007）。時間のかかる検証の扱いは research.md R-04

**Constraints**: `.sln` / `.vcxproj` 無変更、4 構成ビルド維持、`CanSim/` のソースは無変更、新規ソースは ASCII のみ（既存規約）、サンプル・基準 CSV を複製しない

**Scale/Scope**: 25 機能・`Check()` 204 呼び出し（実行時 214 回）（`checks.cpp` 2,654 行）→ 25 のテストファイル。`CanSim/` は約 20 の `.cpp`

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| 原則 | 判定 | 根拠 |
|---|---|---|
| I. Spec Kit ワークフロー | 合格 | specify → plan の順で進行。tasks / implement が続く |
| II. 4 構成ビルド・プロジェクト設定 | 合格 | `.sln` / `.vcxproj` は無変更（FR-011）。実装完了時に 4 構成を MSBuild で確認する |
| III. 自動テスト必須・テストファースト・ctest を CLAUDE.md に記載 | 合格 | 本機能がこの原則の実装そのもの。ctest の手順は CLAUDE.md に記載（タスクに含める）。移行は既存検証の移植なので「先に書く」は、移したテストが緑になるのを `checks.exe` と突き合わせて確認する形で満たす。意図的にロジックを壊して赤になることも確認する（SC-004） |
| IV. シンプルさ（YAGNI） | 合格 | 追加するのは CMake 2 ファイルとテストのみ。MFC 側の構造は変えない。`checks.exe` の削除や共通化の作り込みはしない |
| V. ドキュメント同期 | 合格 | CLAUDE.md のビルド・テスト節を同じ作業で更新（タスクに含める）。`tools/cansim_runner/README.md` に移行状況を追記 |
| VI. CAN ドメイン仕様 | 対象外 | ドメインの振る舞いは変えない |

**Gate 結果**: 違反なし（Complexity Tracking は不要）。Phase 1 後の再評価も同じ（設計は上記を変えない）。

## Project Structure

### Documentation (this feature)

```text
specs/030-googletest-integration/
├── plan.md                 # This file
├── research.md             # Phase 0
├── data-model.md           # Phase 1（ビルドターゲット・テスト名・移行台帳の形）
├── quickstart.md           # Phase 1（実行・検証の手順）
├── contracts/
│   ├── build-and-test-interface.md   # CMake ターゲット・ctest の呼び出し・環境の約束
│   └── test-naming.md                # テスト名の規則
├── checklists/requirements.md
└── tasks.md                # /speckit-tasks で作る
```

### Source Code (repository root)

```text
CMakeLists.txt                  # 既存の未追跡の雛形を置き換える: cansim_core、add_subdirectory(tests)
tests/
├── CMakeLists.txt              # runTests、GoogleTest の取得、gtest_discover_tests
├── support/
│   ├── check_support.h         # CHECK(ok, label) = EXPECT_TRUE(ok) << label
│   └── test_helpers.h/.cpp     # checks.cpp 冒頭のヘルパ（RunConfigFile・Join・WriteTextFile 等）と、パス（サンプル・一時・基準）の取得
├── feature_001_tests.cpp       # CheckFeature001 の移行先（以下 NNN ごとに 1 ファイル）
├── ...
├── feature_029_tests.cpp
└── migration-ledger.md         # 移行台帳（checks.cpp の項目 ↔ テスト、対象外・理由）

MFCApplication1/CanSim/         # 無変更（cansim_core の入力）
MFCApplication1/MFCApplication1.sln / .vcxproj   # 無変更
tools/cansim_runner/            # checks.cpp / runner.cpp / pch.h は残す（pch.h は cansim_core も使う）
main.cpp, tests.cpp             # 未追跡の雛形。削除する
```

**Structure Decision**: CMake はリポジトリ直下の `CMakeLists.txt`（ライブラリ）と `tests/CMakeLists.txt`（テスト）の 2 つ。ビルドツリーは `build/`（`.gitignore` 済み）。テスト用の補助は `tests/support/` に分け、機能ごとのファイルはロジックの検証だけを持つ。

## Complexity Tracking

違反なし。
