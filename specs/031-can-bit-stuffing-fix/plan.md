# Implementation Plan: ビットスタッフィングを CAN 2.0A の規格どおりに数える

**Branch**: `ver.0.9`（仕様の連番 031） | **Date**: 2026-10-13 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/031-can-bit-stuffing-fix/spec.md`

## Summary

`CanSim::ComputeFrameTiming`（`FrameTiming.cpp`）のスタッフビットの数え方を、規格どおりに直す。SOF〜CRC のビット列から、スタッフビットを実際に差し込んだビット列（計算用のフレームバッファ）を作り、その列の上で同じ値の 5 連続を調べる。差し込んだビットは次の連続の 1 個目に数える。総ビット数・伝送時間・インターミッション・CRC の求め方は変えない。シミュレーションのエンジンは、送信のたびに計算するのをやめ、実行の開始時にフレームの定義ごとに 1 回計算した値を使う（FR-006）。入力画面は今までどおり `ComputeFrameTiming` を呼ぶので、表示も規格どおりになる。伝送時間が変わるので、機能 030 のテストのうち値に依存する期待値、サンプルの結果を記録した文書（`tools/vehicle_samples/README.md`・`README.md`）の数値を作り直す。

## Technical Context

**Language/Version**: C++14（MSVC v143、Visual Studio 2022）

**Primary Dependencies**: なし（`CanSim/` は標準ライブラリのみ）。テストは GoogleTest 1.14.0（機能 030、CMake の FetchContent）

**Storage**: なし（結果の CSV の形式は変えない）

**Testing**: GoogleTest + CTest（`tests/feature_NNN_tests.cpp`、`runTests`）。新しいテストは `tests/feature_031_tests.cpp`。手計算の期待値は、製品のコードとは別に書いた Python のスクリプトで求めた値をリテラルで書く

**Target Platform**: Windows（Win32 / x64、Debug / Release の 4 構成）

**Project Type**: デスクトップアプリ（MFC のダイアログ）と、UI に依存しないシミュレーションのライブラリ（`CanSim/`）

**Performance Goals**: 遅くしない。伝送時間を実行の開始時に 1 回計算するので、送信のたびの CRC・スタッフの計算が無くなる（600 秒・16 ECU で計算の時間が増えないこと）

**Constraints**: `CanSim/` のソースは ASCII のみ（CLAUDE.md）。`ComputeFrameTiming` の宣言（引数・戻り値の型）は変えない（入力画面・テスト・`checks.exe` が使っている）

**Scale/Scope**: 製品のコードは `FrameTiming.cpp`（スタッフの数え方）と `SimulationEngine.h/.cpp`（伝送時間の事前計算）の 2 か所。テストの追加 1 ファイル、期待値の見直し、文書の数値の更新

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| 原則 | 判定 | 理由 |
|---|---|---|
| I. Spec Kit ワークフロー準拠 | PASS | spec.md → plan.md → tasks.md → implement の順で進める |
| II. MFC/C++ ネイティブプラットフォーム規約 | PASS | `CanSim/` の C++14 だけの変更。4 構成のビルドを確認する |
| III. 自動テスト必須（テストファースト） | PASS | 規格の例・全 ID × DLC の照合のテストを先に書き、今の実装で失敗することを確かめてから直す。テスト名に機能番号と SC の ID を付ける |
| IV. シンプルさ優先 | PASS | 公開する型・関数は増やさない（ビット列を返す関数は追加しない。research R2）。事前計算はエンジンの中に閉じる |
| V. ドキュメントとの同期 | PASS | CLAUDE.md の FrameTiming の説明、サンプルの数値を記録した文書を同じ作業で更新する |
| VI. CAN バスドメイン仕様準拠 | PASS | ドメイン仕様書 FR-021 とログの例（0x080 = 123 ビット）に合わせる修正。P①〜P⑤・スコープ境界は変えない |

Post-design re-check（Phase 1 の後）: 変更なし、すべて PASS。

## Project Structure

### Documentation (this feature)

```text
specs/031-can-bit-stuffing-fix/
├── plan.md              # この計画
├── research.md          # Phase 0: 数え方・事前計算・影響範囲の決定
├── data-model.md        # Phase 1: フレームのビット列と伝送時間
├── quickstart.md        # Phase 1: 確認の手順
├── contracts/
│   └── frame-timing.md  # ComputeFrameTiming の結果の約束
├── checklists/
│   └── requirements.md  # 仕様の品質チェック（/speckit-specify）
└── tasks.md             # /speckit-tasks で作る
```

### Source Code (repository root)

```text
MFCApplication1/CanSim/
├── FrameTiming.h         # 宣言は変えない。コメントにスタッフの数え方を書く
├── FrameTiming.cpp       # スタッフビットを差し込んだ列を作って数える（R1）
├── SimulationEngine.h    # フレームの定義ごとの伝送時間の表を持つ（R3）
└── SimulationEngine.cpp  # 実行の開始時に表を作り、送信開始で表を引く

tests/
└── feature_031_tests.cpp # 新規: SC-001〜SC-006 のテスト

tools/vehicle_samples/README.md   # サンプルの結果の数値を更新（R5）
README.md                         # 再送フレーム数などの数値を更新（R5）
CLAUDE.md                         # FrameTiming と SampleConfigs の説明を更新
```

**Structure Decision**: 既存の構成（`MFCApplication1/CanSim/` の UI 非依存のライブラリ、`tests/` の GoogleTest）のまま。新しいディレクトリ・プロジェクトは作らない。

## Complexity Tracking

憲章の違反は無いので、記載なし。
