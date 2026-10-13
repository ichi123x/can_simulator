# Implementation Plan: CAN バス構成図の表示（ダイアログ表示部）

**Branch**: `003-bus-diagram-view` | **Date**: 2026-09-29 | **Spec**: [spec.md](./spec.md)

## Summary

メインダイアログの上部に、白背景の構成図描画領域を追加し、「CAN BUS」見出し・バス線・ECU（奇数=上／偶数=下の千鳥配置）を描画する。ECU 数は現在の ECU 数（機能 004 のセッティング／プロジェクトで決まる値、初期値 2）。ダイアログの初期サイズは、16 ECU が収まる横長（約5:1の図領域が収まる大きさ）に拡げる。ECU 数の入力手段・既存コントロールの削除・ダイアログ全体の白化は機能 004 が担い、本機能は「現在の ECU 数を描く表示部品」に絞る。ECU の座標計算は UI非依存の純粋関数として `CanSim/` に切り出し（憲章 III）、実描画（GDI）のみをダイアログ側に置く。

## Technical Context

**Language/Version**: C++14相当（機能 001/002 と同じ）

**Primary Dependencies**: MFC（動的リンク）、GDI（`CPaintDC`/`CDC`）。新規外部ライブラリなし。

**Storage**: なし（表示のみ。ECU 数はメモリ上の int）

**Testing**: 座標計算 `BusDiagramLayout` を `tools/cansim_runner/checks.cpp` に検証項目として追加（`cl.exe` 単体ビルド可能）。描画・初期サイズの目視確認は `quickstart.md` の手順による。

**Target Platform**: Windows 10/11（Win32/x64、Debug/Release）

**Project Type**: desktop-app（既存 `MFCApplication1` を拡張）

**Performance Goals**: 描画は数十個の矩形・線のみで問題にならない。ちらつき防止にメモリ DC経由の描画を用いる。

**Constraints**: 表示部は常に白（システムテーマ非依存）。ECU の位置・大きさ・間隔は ECU 数に依存しない（FR-009）。`.rc` は UTF-16LE のためエンコーディング明示で編集（CLAUDE.md）。

**Scale/Scope**: ECU 最大16。新規ファイル1組（レイアウト）＋ダイアログ/リソースの変更。

## Constitution Check

| 原則 | 判定 | 根拠 |
|---|---|---|
| I. Spec Kit ワークフロー準拠 | PASS | spec.md → 本 plan.md の順。 |
| II. MFC/C++ ネイティブ規約 | PASS | 4構成ビルド維持。新規ソースを `.vcxproj` に登録するのみ。 |
| III. テスト可能性 | PASS | 座標計算を `CanSim/BusDiagramLayout` に分離。ダイアログは描画と ECU 数の受け渡しのみ。 |
| IV. YAGNI | PASS | 新ダイアログ・Document/View・リサイズ追従・アニメーションは入れない。カスタムコントロールも作らず、既存ダイアログの `OnPaint` に描画する。 |
| V. ドキュメント同期 | PASS（条件付き） | CLAUDE.md のアーキテクチャ節（`BusDiagramLayout`、ダイアログの描画責務、初期サイズ）を更新するタスクを tasks に含めること。 |
| VI. ドメイン仕様準拠 | PASS | 最大16ノード（P⑤）の表示に対応。シミュレーションのロジックには触れない。 |

違反なし。Complexity Tracking は該当なし。

**Post-Design Re-check**: Phase 1 の成果物は新規依存・新規 VS プロジェクトを含まない。全項目 PASSのまま。

## Project Structure

### Documentation (this feature)

```text
specs/003-bus-diagram-view/
├── plan.md
├── research.md
├── data-model.md
├── quickstart.md
├── contracts/ui-layout.md
└── tasks.md            # /speckit-tasks で生成
```

### Source Code (repository root)

```text
MFCApplication1/
├── MFCApplication1.rc            # 変更: ダイアログ寸法を図領域に合わせる（720×152 DLU）。既存コントロールの削除は機能 004（UTF-16LE 注意）
├── MFCApplication1Dlg.h / .cpp   # 変更: OnPaint で構成図描画、m_ecuCount（初期 2）保持と SetEcuCount による更新（値の設定元は機能 004）
├── MFCApplication1.vcxproj(.filters) # 変更: BusDiagramLayout.h/.cpp を登録
└── CanSim/
    └── BusDiagramLayout.h / .cpp # 新規: ECU 数→ECU 矩形・接続線座標の純粋関数（UI 非依存、ASCII のみ）
tools/cansim_runner/checks.cpp    # 変更: レイアウト計算の検証項目を追加
CLAUDE.md                         # 変更: アーキテクチャ節の更新
```

**Structure Decision**: 描画（GDI 呼び出し）は UI 層、幾何計算は `CanSim/` の純粋 C++。表示専用のため `SimulationEngine` には手を入れない。ダイアログの `OnPaint` 内で、構成図領域（ダイアログクライアント座標の固定矩形）に描く。UI ラベル「ECU NN」「CAN BUS」は英字のため `CanSim`側の ASCII 制約とも整合する。

## Complexity Tracking

該当なし。
