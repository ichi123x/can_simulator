# Implementation Plan: メニューバー・プロジェクト保存/読込み・設定・フレーム表示・シミュレーション操作

**Branch**: `ver.0.2`（spec ディレクトリ: `004-menu-project-settings`） | **Date**: 2026-09-29 | **Spec**: [spec.md](./spec.md)

## Summary

ダイアログをメニュー駆動に作り替える。既存のコントロールをすべて削除し、メニュー「ファイル／編集／
シミュレーション」、セッティングダイアログ（ECU 数・通信速度・シミュレーション時間）、`.canprj`
プロジェクトの保存・読込み、実行結果のフレーム再生（フレーム表示）を追加する。`.canprj` は機能 001
の設定ファイル（`.cfg`）形式を拡張した後継（`[Project]` セクションの追加）で、読込み・保存・実行用
ネットワークの生成・フレーム再生用イベントの抽出は `CanSim/` の UI 非依存 C++ に置く（憲章 III）。UI は
メニュー処理・セッティング画面・GDI 描画（機能 003 の構成図に強調表示を追加）・タイマー再生のみを持つ。

## Technical Context

**Language/Version**: C++14相当（機能 001〜003 と同じ）

**Primary Dependencies**: MFC（動的リンク）、Win32（`CFileDialog`、メニュー、`SetTimer`）、C++ 標準
ライブラリのみ。新規の外部ライブラリなし。

**Storage**: `.canprj`（UTF-8 のセクション区切り `key=value` テキスト。契約は
`contracts/project-file-format.md`）。結果は従来どおり `<プロジェクトファイル名>.log.csv` などの CSV。

**Testing**: 機能 001〜003 と同じ方針（自動テスト基盤は導入しない）。`CanSim/` の新規モジュール
（`ProjectFile`、`FrameEvents`）は `tools/cansim_runner/checks.cpp` に検証項目を追加し `cl.exe` で
検証する。メニュー・ダイアログ・再生は `quickstart.md` の目視手順で確認する。

**Target Platform**: Windows 10/11（Win32/x64、Debug/Release）

**Project Type**: desktop-app（既存 `MFCApplication1` を拡張。新しい VS プロジェクトは作らない）

**Performance Goals**: 機能 001 の目標を据え置き（10 秒のシミュレーションを 1 秒未満）。フレーム
表示 OFF の「開始」は同期実行で完結し、UI スレッドを占有する時間は 1 秒未満。

**Constraints**: フレーム表示 ON/OFF で出力 CSV が完全一致（FR-015/SC-006）。既存 CSV 出力の内容は
変えない（SC-009）。`CanSim/` は ASCII のみ。UI 文字列は `.rc`（UTF-16LE）のみに置く。

**Scale/Scope**: ECU 最大 16。新規ソース: `ProjectFile`、`FrameEvents`（CanSim）、`SettingsDlg`（UI）。

## Constitution Check

| 原則 | 判定 | 根拠 |
|---|---|---|
| I. Spec Kit ワークフロー準拠 | PASS | spec.md（004）の後に本 plan.md を作成。 |
| II. MFC/C++ ネイティブ規約 | PASS | `.vcxproj` の構成は変えず、新規ソースを 4 構成すべてに登録するのみ。 |
| III. テスト可能性 | PASS | プロジェクト I/O、実行用ネットワーク生成、フレームイベント抽出は `CanSim/` の純粋関数。UI は呼び出しと描画のみ。 |
| IV. YAGNI | PASS | スレッド化・進捗バー・再生速度調整・undo・最近使ったファイルは入れない。既存 `LoadConfig` を再利用して形式を拡張する。 |
| V. ドキュメント同期 | PASS（条件付き） | CLAUDE.md（アーキテクチャ・ビルド・検証コマンド・サンプルの拡張子）と、機能 001 の `contracts/config-input-format.md`（後継の契約への参照）を更新するタスクが必須。 |
| VI. ドメイン仕様準拠 | PASS | 最大 16 ノード（P⑤）、通信速度 4 種（001 FR-002）、再現性（P③）を維持。エラー状態制御には触れない。 |

違反なし。Complexity Tracking は該当なし。

**Post-Design Re-check**: Phase 1 の成果物は新規依存・新規 VS プロジェクトを含まない。全項目 PASS。

## Project Structure

### Documentation (this feature)

```text
specs/004-menu-project-settings/
├── plan.md / research.md / data-model.md / quickstart.md
├── contracts/
│   ├── project-file-format.md   # .canprj の入力/出力形式
│   └── ui-menu-dialogs.md       # メニュー、セッティング画面、再生表示、通知メッセージ
└── tasks.md                     # /speckit-tasks で生成
```

### Source Code (repository root)

```text
MFCApplication1/
├── MFCApplication1.rc / Resource.h     # 変更: 既存コントロール削除、メニュー・セッティング画面・文字列の追加、ダイアログ全体を白（UTF-16LE 注意）
├── MFCApplication1Dlg.h / .cpp         # 変更: メニューコマンド、実行状態、フレーム再生タイマー、強調描画、白背景、Enter/Esc 抑止
├── SettingsDlg.h / .cpp                # 新規: ECU 数・通信速度・シミュレーション時間のセッティング画面
├── MFCApplication1.vcxproj(.filters)   # 変更: 新規ファイルの登録
└── CanSim/
    ├── CanTypes.h                      # 変更: ProjectSettings を追加
    ├── ConfigLoader.h / .cpp           # 変更: [Project] セクションの読込み、ノード 0 個の許容
    ├── ProjectFile.h / .cpp            # 新規: SaveProject、実行用ネットワーク生成 (BuildRunNetwork)
    ├── FrameEvents.h / .cpp            # 新規: LogRow 群 → 再生用フレームイベント列
    ├── BusDiagramLayout.h / .cpp       # 003 で新規（本機能は変更なし）
    └── SampleConfigs/                  # 変更: sample_*.cfg → sample_*.canprj（[Project] セクション付き）
tools/cansim_runner/                    # 変更: runner/checks を .canprj に対応、004 の検証項目を追加
CLAUDE.md                               # 変更: アーキテクチャ・ビルド・検証手順
```

**Structure Decision**: `.canprj` の解析は既存の `ConfigLoader` を拡張して再利用し（形式を
そのまま引き継ぐ）、保存・実行ネットワーク生成・フレームイベント抽出だけを新モジュールに切り出す。
実行は従来どおり `SimulationEngine` を UI スレッドで同期実行する。フレーム再生は結果（`LogRow`）を
先に得てから、タイマーで UI 側が再生する（エンジンに再生用の分岐を入れない）。

## Complexity Tracking

該当なし。
