# Implementation Plan: CAN バスシミュレーション コア機能（構成・調停・性能評価）

**Branch**: `001-can-simulation-core` | **Date**: 2026-09-28 | **Spec**: [spec.md](./spec.md)

**Input**: Feature specification from `/specs/001-can-simulation-core/spec.md`

**Note**: This template is filled in by the `/speckit-plan` command; its definition describes the execution workflow.

## Summary

最大16ノードの CAN 2.0A バスを、ビット単位ではなくフレーム単位のイベント駆動方式（DES）で
模擬する。利用者はネットワーク構成（ノードと各ノードの送信フレーム一覧）を与えて指定時間
シミュレーションを実行すると、送受信・調停（ID優先・自動再送信）の記録がログとして得られ、
そこから総送信レイテンシ・送信待ち遅延・フレーム伝送時間・バス負荷率・ジッターを算出できる。
実装は、既存の `MFCApplication1` ダイアログベース MFC プロジェクト内に、UI（`CMFCApplication1Dlg`）
から独立した C++ シミュレーションコア（ネットワーク／ノード／イベントキュー／ロガー）を追加する
形で行い、新しい依存ライブラリや別 VS プロジェクトは導入しない。

## Technical Context

**Language/Version**: C++14相当（プロジェクトの既定。`.vcxproj` に `LanguageStandard` の明示指定
なし。新規コードでも既定設定のまま追加コンパイル設定を増やさない）

**Primary Dependencies**: MFC（動的リンク、既存設定を継承）、Win32 API、C++ 標準ライブラリのみ。
新規の外部ライブラリ・パッケージマネージャ（vcpkg/NuGet 等）は導入しない（リポジトリに設定が
存在しないため — 詳細は research.md 参照）。

**Storage**: シミュレーション実行中はメモリ内で完結。ログはローカルファイルへ CSV（UTF-8）で
出力する。設定（ネットワーク・ノード・送信フレーム一覧）は、外部ライブラリ不要の軽量な独自
テキスト形式（セクション区切りの `key=value` 形式）でファイルから読み込む（詳細は research.md）。

**Testing**: 本フェーズでは自動テストフレームワーク・専用テストプロジェクトを新規導入しない。
シミュレーションロジックは `CMFCApplication1Dlg`（UI）から独立したクラス群として実装し、将来の
テスト導入を妨げない構造を維持する（憲章 III）。検証は `quickstart.md` の手動シナリオで行う。

**Target Platform**: Windows 10/11 デスクトップ（Win32 / x64、Debug / Release）

**Project Type**: desktop-app（既存の単一 `MFCApplication1` VS プロジェクト内に、UI非依存の
シミュレーションコアをサブフォルダとして追加する。新しい VS プロジェクトは作成しない）

**Performance Goals**: 既定構成（16ノード・500 kbps 相当）で10秒間のシミュレーションを1秒未満
で完了する（暫定目標。ドメイン仕様書の Q-8「実行環境と性能目標」は未確定のため、実行環境確定後
に見直す前提の初期値）

**Constraints**: 同一構成での再実行時にログが完全に一致すること（再現性、FR-007/SC-001）。
イベントはフレーム単位で発生させ、ビット単位のシミュレーションは行わない（憲章 P③）。電気的
物理層は模擬しない（憲章 P④）。

**Scale/Scope**: 最大16ノード（憲章 P⑤、FR-001）。ノードあたりの送信フレーム定義数に明確な
上限はないが、動作確認用の既定構成は6ノード・8フレーム（ドメイン仕様書 付録A）を想定する。

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| 原則 | 判定 | 根拠 |
|---|---|---|
| I. Spec Kit ワークフロー準拠 | PASS | `spec.md` を経てから本 `plan.md` を作成している。 |
| II. MFC/C++ ネイティブプラットフォーム規約 | PASS | 既存 `.vcxproj`（VS2022/v143、Win32・x64、Debug・Release、Unicode、MFC動的リンク）をそのまま維持し、新規ソースは同一プロジェクトに追加するのみで設定変更を伴わない。 |
| III. テスト可能性の段階的確保 | PASS | シミュレーションロジック（Network/Node/EventQueue/Logger）を `CMFCApplication1Dlg` のイベントハンドラから分離した独立クラス群として設計する（Project Structure 参照）。 |
| IV. シンプルさ優先 (YAGNI) | PASS | 単一ダイアログ構成を維持し、Document/View への移行や新規 VS プロジェクト・外部ライブラリの追加は行わない。設定・ログ形式も外部依存のない最小構成を選ぶ（research.md）。 |
| V. ドキュメントとの同期 | PASS（条件付き） | 本実装により CLAUDE.md のアーキテクチャ節に実態との乖離が生じるため、`/speckit.tasks` にはドキュメント更新タスクを含めること。 |
| VI. CAN バスドメイン仕様準拠 | PASS | CAN 2.0A・フレーム単位DES・最大16ノード・物理層非模擬という P①〜⑤ の範囲内でのみ設計している（拡張フォーマットやビット単位シミュレーションは導入しない）。 |

違反なし。Complexity Tracking は該当なし。

**Post-Design Re-check**: Phase 1 で生成した `data-model.md` / `contracts/` / `quickstart.md`
は、新規の外部依存・VSプロジェクト・GUIフレームワークの変更を一切含まない（設定ファイル形式は
STLのみで実装、ログはCSV、テスト基盤は導入しない）。上表の判定はすべて据え置きで PASS。

## Project Structure

### Documentation (this feature)

```text
specs/001-can-simulation-core/
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
├── MFCApplication1.h / .cpp           # 既存: CWinApp 派生（変更なし想定）
├── MFCApplication1Dlg.h / .cpp        # 既存: UI。CanSim コアの呼び出しのみ追加（薄いグルーコード）
├── pch.h / pch.cpp, framework.h       # 既存: 変更なし
├── Resource.h, MFCApplication1.rc     # 既存: 実行/ログ出力等のUIコントロールを追加する場合のみ変更
└── CanSim/                            # 新規: UI 非依存のシミュレーションコア（プレーン C++）
    ├── CanTypes.h                     # Frame/Node/Network の値型・列挙（送信状態・スタートアップ状態）
    ├── ConfigLoader.h / .cpp          # 設定ファイル読み込み・バリデーション（FR-001〜005）
    ├── EventQueue.h / .cpp            # DES イベントキュー（時刻・種別・ノード番号順の決定的処理）
    ├── FrameTiming.h / .cpp           # ビットスタッフィングを含む伝送時間計算（FR-008/009）
    ├── Network.h / .cpp               # 調停・自動再送信・受信配送ロジック（FR-010〜015）
    ├── SimulationEngine.h / .cpp      # DES メインループ、ConfigLoader/Network/EventQueue の統合
    ├── Logger.h / .cpp                # ログ行生成・CSV 出力（FR-016/017）
    └── MetricsAggregator.h / .cpp     # 総送信レイテンシ等の集計（FR-018〜020）

MFCApplication1/MFCApplication1.vcxproj / .vcxproj.filters
                                        # 既存プロジェクトファイルに CanSim/ 配下の新規ファイルを
                                        # 全構成（Debug/Release × Win32/x64）に追加
```

**Structure Decision**: 新しい VS プロジェクト／ソリューションは作成せず、既存の単一
`MFCApplication1` プロジェクト内に `CanSim/` フォルダを追加する（Option 1 相当の単純構成）。
`CanSim/` 配下のクラスは `CDialogEx` や MFC の GUI 型に依存しないプレーン C++ とし、
`MFCApplication1Dlg` からは薄いグルーコード（設定ファイル選択・実行ボタン・結果表示）としてのみ
呼び出す。自動テスト用の `tests/` プロジェクトは本フェーズでは追加しない（Technical Context /
Constitution Check 参照）。

## Complexity Tracking

該当なし（Constitution Check に違反がないため記入不要）。
