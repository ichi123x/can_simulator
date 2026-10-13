# Implementation Plan: ノードのスタートアップ制御（電源投入〜通常動作移行）

**Branch**: `002-node-startup-control` | **Date**: 2026-09-28 | **Spec**: [spec.md](./spec.md)

**Input**: Feature specification from `/specs/002-node-startup-control/spec.md`

**Note**: This template is filled in by the `/speckit-plan` command; its definition describes the execution workflow.

## Summary

各ノードに「電源オフ／リセット → 初期化 → バス同期検出 → 通常動作」の起動状態遷移を追加し、通常動作に入るまでは調停にも受信にも参加させない。バス同期の成立は、既存のフレーム単位 DES の中で「検出開始以降に始まる、途切れない11ビット時間分のレセシブ期間」として判定する。既存機能 001（`specs/001-can-simulation-core/`）の `CanSim/` に、起動状態のフィールドと3つの新イベント種別、起動記録の CSV 出力を追加する形で実装し、新しい VS プロジェクト・外部ライブラリは導入しない。

## Technical Context

**Language/Version**: C++14相当（機能 001 と同じ。`.vcxproj` に `LanguageStandard` の明示指定なし）

**Primary Dependencies**: MFC（動的リンク、既存設定を継承）、Win32 API、C++ 標準ライブラリのみ。新規の外部ライブラリ・パッケージマネージャは導入しない（機能 001 の research.md 決定1を踏襲）。

**Storage**: 実行中はメモリ内で完結。起動記録はログ・集計サマリと同じ出力先に CSV（UTF-8）で出力する。設定は機能 001 の独自テキスト形式（セクション区切りの `key=value`）にノードごとのキーを2つ追加して読み込む（詳細は research.md / contracts）。

**Testing**: 機能 001 と同じ方針。自動テストフレームワーク・専用テストプロジェクトは導入しない。`CanSim/` は MFC 非依存のプレーン C++ を維持し、`cl.exe` で直接ビルドできる状態を保つ（この性質を使った検証手順は `quickstart.md` を参照）。実装完了後、その検証ツールは `tools/cansim_runner/`としてリポジトリに含めた（`MFCApplication1.sln` には含まれない手動ビルドの開発補助ツール）。

**Target Platform**: Windows 10/11 デスクトップ（Win32 / x64、Debug / Release）

**Project Type**: desktop-app（既存の単一 `MFCApplication1` VS プロジェクトの `CanSim/` を拡張。新しい VS プロジェクトは作成しない）

**Performance Goals**: 機能 001 の暫定目標を据え置き（既定構成で10秒のシミュレーションを1秒未満）。本機能が追加するイベントはノードあたり最大でも数個（電源投入・初期化完了・同期完了）であり、性能特性を変えない。

**Constraints**: 同一構成での再実行時に起動記録とフレームログの両方が完全一致すること（FR-014/SC-005）。ビット単位のシミュレーションは行わず、同期検出もフレーム単位イベントで表現する（憲章 P③）。物理層は模擬しない（憲章 P④）。

**Scale/Scope**: 最大16ノード。追加イベントはノードあたり3種（電源投入・初期化完了・同期完了）＋同期のやり直しに伴う再スケジュール。

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| 原則 | 判定 | 根拠 |
|---|---|---|
| I. Spec Kit ワークフロー準拠 | PASS | `spec.md`（002）を経てから本 `plan.md` を作成している。 |
| II. MFC/C++ ネイティブプラットフォーム規約 | PASS | `.vcxproj` の構成設定は変更せず、新規ソースを全4構成（Debug/Release × Win32(x86)/x64）に追加するのみ。 |
| III. テスト可能性の段階的確保 | PASS | 起動制御ロジックはすべて `CanSim/` 側のプレーン C++ に閉じ、`CMFCApplication1Dlg` には手を入れない（UI 変更なし）。 |
| IV. シンプルさ優先 (YAGNI) | PASS | イベントキューに取り消し機構を追加せず、遅延無効化（research.md 決定1）で対応。起動をスキップする互換モードも設けない（spec.md Assumptions）。 |
| V. ドキュメントとの同期 | PASS（条件付き） | 本実装で (a) CLAUDE.md のアーキテクチャ節（`CanSim/` の新規ファイル）、(b) 機能 001 の `contracts/log-output-format.md` の例示値・`quickstart.md` の期待値（全時刻が11ビット時間ずれる、SC-006）に乖離が生じる。`/speckit-tasks` にこれらの更新タスクを必ず含めること。 |
| VI. CAN バスドメイン仕様準拠 | PASS | 憲章 [P②] が要求する3系統の状態遷移のうち「スタートアップ制御」を実装するものであり、スコープを広げない。同期判定は11ビット連続レセシブというドメイン仕様書 FR-031a の規定に忠実。 |

違反なし。Complexity Tracking は該当なし。

**残存する憲章上の未対応事項（本機能のスコープ外）**: 憲章 [P②] の3系統目「エラー状態制御の枠」は依然としてどの機能にも起票されていない（`/speckit-analyze` の指摘 C1）。本機能はこれを解消しないため、別途 `/speckit-specify` での起票が必要。

**Post-Design Re-check**: Phase 1 で生成した `data-model.md` / `contracts/` / `quickstart.md` は、新規の外部依存・VSプロジェクト・GUI 変更を一切含まない（追加されるのは `CanSim/` 内の1モジュール、`Node` のフィールド、イベント種別3つ、CSV 出力1種のみ）。上表の判定はすべて据え置きで PASS。

## Project Structure

### Documentation (this feature)

```text
specs/002-node-startup-control/
├── plan.md              # This file (/speckit-plan command output)
├── research.md          # Phase 0 output (/speckit-plan command)
├── data-model.md        # Phase 1 output (/speckit-plan command)
├── quickstart.md        # Phase 1 output (/speckit-plan command)
├── contracts/           # Phase 1 output (/speckit-plan command)
└── tasks.md             # Phase 2 output (/speckit-tasks command - NOT created by /speckit-plan)
```

### Source Code (repository root)

```text
MFCApplication1/
├── MFCApplication1Dlg.h / .cpp        # 変更: 起動記録を書き出すグルーコード数行のみ（新しいコントロール・操作は追加しない）
└── CanSim/
    ├── CanTypes.h                     # 変更: Node に起動設定・起動状態・記録用時刻、EventKind に3種追加
    ├── ConfigLoader.h / .cpp          # 変更: PowerOnTimeUs / InitDurationUs の読み込みと検証
    ├── EventQueue.h / .cpp            # 変更なし（取り消し機構は追加しない — research.md 決定1）
    ├── FrameTiming.h / .cpp           # 変更なし（BitTimeNs を同期判定でも利用）
    ├── Network.h / .cpp               # 変更: 調停候補・受信対象を通常動作ノードに限定（FR-008/009）
    ├── SimulationEngine.h / .cpp      # 変更: 起動イベントの処理、レセシブ期間の追跡と同期完了の再スケジュール
    ├── StartupControl.h / .cpp        # 新規: 同期成立時刻の算出、起動記録の保持と CSV 出力
    ├── Logger.h / .cpp                # 変更なし
    ├── MetricsAggregator.h / .cpp     # 変更なし（送信済みフレームのログのみを集計するため FR-015 は自動的に満たす）
    └── SampleConfigs/
        ├── sample_appendix_a.cfg      # 既存（SC-006 の後方互換確認にも使う）
        ├── sample_collision.cfg       # 既存
        ├── sample_startup.cfg         # 新規: 電源投入時刻をずらした起動確認用構成（US1/SC-001/SC-003）
        └── sample_startup_latejoin.cfg # 新規: 送信中のバスに後から参加する構成（US3/SC-004）
```

**Structure Decision**: 機能 001 の構成をそのまま踏襲し、起動制御に固有のロジック（同期成立時刻の算出、起動記録とその CSV 出力）だけを新しいモジュール `CanSim/StartupControl.h/.cpp` に切り出す。`Node` の状態フィールドは既存の `CanTypes.h` に追加し、DES ループへの組み込みは`SimulationEngine.cpp` の既存イベント分岐を拡張する形で行う。UI（`MFCApplication1Dlg`）には、起動記録を `<設定ファイル名>.startup.csv` として書き出す数行のグルーコードを追加するのみで、新しいコントロール・操作は追加しない（憲章 III: UI にはロジックを持たせない）。

## Complexity Tracking

該当なし（Constitution Check に違反がないため記入不要）。
