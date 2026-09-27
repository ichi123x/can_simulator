# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## プロジェクトの現状

このリポジトリは Visual Studio の MFC ダイアログベースアプリケーション ウィザードで生成された初期状態のプロジェクト（`MFCApplication1`）です。リポジトリ名（`can_simulator`）から CAN バスシミュレータの開発が意図されていますが、現時点でアプリケーション固有のロジックはまだ実装されておらず、`MFCApplication1Dlg.cpp`/`.h` はウィザードが生成した雛形のままです。

また `.specify/` 配下に GitHub の Spec Kit（SDD: Spec-Driven Development ツール）が Claude 統合で導入されています。機能追加はこの Spec Kit のワークフロー（`/speckit.specify` → `/speckit.plan` → `/speckit.tasks` → `/speckit.implement`）に沿って進めることが想定されています。`.specify/memory/constitution.md` はまだプレースホルダのままで、プロジェクト固有の憲章（原則）は未確定です。

## ビルド・実行

Visual Studio (2022, `VCProjectVersion` 17.0, PlatformToolset `v143`) 前提のネイティブ C++/MFC プロジェクトです。ビルドには Visual Studio または `MSBuild` を使用します。

```powershell
# ソリューションのビルド（Developer PowerShell / VS がインストールされた環境で実行）
msbuild MFCApplication1\MFCApplication1.sln /p:Configuration=Debug /p:Platform=x64

# Release ビルド
msbuild MFCApplication1\MFCApplication1.sln /p:Configuration=Release /p:Platform=x64
```

- 対応プラットフォーム: `Win32` / `x64`、構成: `Debug` / `Release`
- 文字セット: Unicode、MFC の使用方法: 動的リンク（`UseOfMfc=Dynamic`）
- 自動テスト・Lint の仕組みは現時点で未整備です（テストフレームワークやテストプロジェクトは存在しません）。テストの仕組みを追加した場合は、このファイルに実行コマンドを追記してください。

## Spec Kit（SDD）ワークフロー

このリポジトリで新機能を追加する際は、思いつきでコードを書き始めるのではなく、以下の Spec Kit スラッシュコマンドを順番に使うことが期待されています（`.claude/skills/speckit-*` として実装）。

1. `/speckit.constitution` — プロジェクトの憲章（原則・技術方針など）を確定させる（`.specify/memory/constitution.md` は未記入のプレースホルダなので、本格的な開発前に埋めるべき）
2. `/speckit.specify` — 自然言語の機能要望から仕様書（`spec.md`）を作成・更新
3. `/speckit.clarify` — 仕様の曖昧な点を対話的に明確化（任意）
4. `/speckit.plan` — 実装計画（`plan.md`）を生成
5. `/speckit.tasks` — 依存関係順のタスクリスト（`tasks.md`）を生成
6. `/speckit.analyze` — spec/plan/tasks 間の整合性チェック（任意）
7. `/speckit.implement` — `tasks.md` に基づき実装を実行
8. `/speckit.converge` — 実装後、仕様に対して未完了の作業がないか突き合わせる（任意）

各機能ディレクトリの成果物（`spec.md`, `plan.md`, `tasks.md`, `research.md` など）は `.specify/` 配下の機能ディレクトリに生成されます。現在の機能コンテキストは `.specify/feature.json`（`feature_directory` キー）または環境変数 `SPECIFY_FEATURE_DIRECTORY` で解決されます（`.specify/scripts/powershell/common.ps1` の `Get-FeaturePathsEnv` 参照）。

PowerShell スクリプト（`.specify/scripts/powershell/*.ps1`）がテンプレート解決・前提条件チェック・新規機能ブランチ作成などを担っています。テンプレートは優先順位付きで解決されます: `overrides/` → `presets/`（`.registry` の優先度順）→ `extensions/` → コアの `.specify/templates/`。

## アーキテクチャ（MFC アプリケーション）

`MFCApplication1/` 配下の構成（すべて Visual Studio ウィザードの標準的なダイアログベース MFC アプリケーション構成）:

- `MFCApplication1.h` / `.cpp` — `CWinApp` を継承したアプリケーションクラス `CMFCApplication1App`。`InitInstance()` がエントリーポイントで、メインダイアログを生成・表示する
- `MFCApplication1Dlg.h` / `.cpp` — メインウィンドウとなる `CDialogEx` 派生クラス `CMFCApplication1Dlg`。メッセージマップ（`ON_WM_SYSCOMMAND`, `ON_WM_PAINT`, `ON_WM_QUERYDRAGICON` など）でウィンドウメッセージを処理する。UI ロジックを追加する場合はここが起点になる
- `pch.h` / `pch.cpp` — プリコンパイル済みヘッダー（`framework.h` を include）
- `framework.h` — MFC/ATL の共通インクルード（`afxwin.h`, `afxext.h` など）
- `Resource.h`, `MFCApplication1.rc`, `res/` — リソース定義（ダイアログレイアウト、アイコン、文字列テーブルなど）。ダイアログの見た目を変更する場合は Visual Studio のリソースエディタか `.rc` ファイルを直接編集する
- `targetver.h` — 対象 Windows プラットフォームのバージョン定義

現時点でドキュメント/ビュー アーキテクチャは使用しておらず、単一のダイアログ (`IDD_MFCAPPLICATION1_DIALOG`) がアプリケーションの UI そのものです。CAN シミュレータとしての機能（CAN フレーム送受信、ペリフェラル通信など）を追加する際は、まずこのダイアログクラスにコントロールとハンドラを追加するところから始まる想定です。
