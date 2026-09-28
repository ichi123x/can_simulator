# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## プロジェクトの現状

このリポジトリは Visual Studio の MFC ダイアログベースアプリケーション ウィザードで生成されたプロジェクト（`MFCApplication1`）をベースに、CAN バスシミュレータの最初の機能（`specs/001-can-simulation-core/`: ネットワーク構成・シミュレーション実行・調停/自動再送信・性能指標評価）を実装したものです。シミュレーションのコアロジックは `MFCApplication1/CanSim/` 配下に、UI から独立したプレーン C++ として実装されています（詳細は後述のアーキテクチャ節）。

また `.specify/` 配下に GitHub の Spec Kit（SDD: Spec-Driven Development ツール）が Claude 統合で導入されています。機能追加はこの Spec Kit のワークフロー（`/speckit.specify` → `/speckit.plan` → `/speckit.tasks` → `/speckit.implement`）に沿って進めることが想定されています。プロジェクト固有の憲章は `.specify/memory/constitution.md`（v1.1.0）に確定済みで、CAN 2.0A 準拠・フレーム単位 DES・最大16ノードなどのドメイン仕様上の決定事項と、`Doc/CANバスシミュレータ仕様.md` を一次情報源とする位置付けが記録されています。

## ビルド・実行

Visual Studio (2022, `VCProjectVersion` 17.0, PlatformToolset `v143`) 前提のネイティブ C++/MFC プロジェクトです。ビルドには Visual Studio または `MSBuild` を使用します。

```powershell
# ソリューションのビルド（Developer PowerShell / VS がインストールされた環境で実行）
msbuild MFCApplication1\MFCApplication1.sln /p:Configuration=Debug /p:Platform=x64

# Release ビルド
msbuild MFCApplication1\MFCApplication1.sln /p:Configuration=Release /p:Platform=x64

# Win32（32bit）でビルドする場合、ソリューション構成上のプラットフォーム名は "Win32" ではなく
# "x86" である点に注意（.sln の SolutionConfigurationPlatforms が x86 を定義し、内部的に
# vcxproj の Win32 構成へマッピングされる）
msbuild MFCApplication1\MFCApplication1.sln /p:Configuration=Debug /p:Platform=x86
```

- 対応プラットフォーム: `Win32`（ソリューションレベルの指定名は `x86`）/ `x64`、構成: `Debug` / `Release`。4構成（Debug/Release × Win32/x64）すべてでビルドが通る状態を維持すること（憲章 II）
- 文字セット: Unicode、MFC の使用方法: 動的リンク（`UseOfMfc=Dynamic`）
- `framework.h` で `NOMINMAX` を定義済み。`CanSim/` 配下で `std::min`/`std::max`/`std::numeric_limits<T>::max()` などを使う際は、Windows ヘッダーの `min`/`max` マクロとの衝突を避けるためにこれに依存してよい（新たに `#undef min`/`#undef max` を個別ファイルに書く必要はない）
- `CanSim/` 配下の新規ソースは日本語コメント・文字列リテラルを含めない（ASCII のみ）こと。`.cpp` は PCH（`pch.h`）を経由して `framework.h` を取り込む都合上、BOM なし UTF-8 で保存されるとコンパイラが実行時ロケール（日本語環境では コードページ 932）でソースを解釈し、非 ASCII 文字が文字化けする（警告 C4819）。UI 表示文字列は `MFCApplication1.rc`（UTF-16LE、BOM 付き）側の日本語ラベル・文字列テーブルに閉じ込め、`CanSim/`・`MFCApplication1Dlg.cpp` 側は英語の識別子・メッセージのみを扱う方針としている
- `MFCApplication1.rc` は UTF-16LE（BOM 付き、CRLF）のバイナリに近いテキストファイル。編集する際は Read/Edit ツールでの直接編集を避け、PowerShell の `Get-Content -Encoding Unicode` / `[System.IO.File]::WriteAllText(path, text, (New-Object System.Text.UnicodeEncoding($false,$true)))` のようにエンコーディングを明示して読み書きすること（さもないと文字化けで壊れる）
- 正式なテストフレームワーク・テストプロジェクトは導入していません（`MFCApplication1.sln` にテストは含まれません）。代わりに、`CanSim/` が UI 非依存のプレーン C++ である性質を利用した検証ツールを `tools/cansim_runner/` に置いています。GUI を操作せずに成功基準（SC-001〜SC-006 ほか）を一括検証できます。

```bat
REM 「x64 Native Tools Command Prompt for VS 2022」等でリポジトリのルートから実行
set OUT=%TEMP%\cansim_tools
mkdir "%OUT%" 2>nul

REM 検証ツールのビルド
cl /nologo /std:c++14 /EHsc /I tools\cansim_runner /I MFCApplication1\CanSim ^
   /Fo"%OUT%\\" /Fe"%OUT%\checks.exe" ^
   tools\cansim_runner\checks.cpp MFCApplication1\CanSim\*.cpp

REM 成功基準の一括検証（終了コード 0 で全パス）
"%OUT%\checks.exe" MFCApplication1\CanSim\SampleConfigs "%OUT%"
```

  単発のシミュレーション実行には同ディレクトリの `runner.cpp` を同様にビルドして使います。詳細は [tools/cansim_runner/README.md](tools/cansim_runner/README.md) を参照してください。Lint の仕組みは未整備です。

## Spec Kit（SDD）ワークフロー

このリポジトリで新機能を追加する際は、思いつきでコードを書き始めるのではなく、以下の Spec Kit スラッシュコマンドを順番に使うことが期待されています（`.claude/skills/speckit-*` として実装）。

1. `/speckit.constitution` — プロジェクトの憲章（原則・技術方針など）を確定・改定する（`.specify/memory/constitution.md` v1.1.0 として確定済み。原則やドメイン仕様上の決定事項を変更する場合はこのコマンドで改定する）
2. `/speckit.specify` — 自然言語の機能要望から仕様書（`spec.md`）を作成・更新
3. `/speckit.clarify` — 仕様の曖昧な点を対話的に明確化（任意）
4. `/speckit.plan` — 実装計画（`plan.md`）を生成
5. `/speckit.tasks` — 依存関係順のタスクリスト（`tasks.md`）を生成
6. `/speckit.analyze` — spec/plan/tasks 間の整合性チェック（任意）
7. `/speckit.implement` — `tasks.md` に基づき実装を実行
8. `/speckit.converge` — 実装後、仕様に対して未完了の作業がないか突き合わせる（任意）

各機能ディレクトリの成果物（`spec.md`, `plan.md`, `tasks.md`, `research.md` など）は、リポジトリ直下の `specs/<NNN>-<機能名>/`（連番は `.specify/init-options.json` の `feature_numbering: sequential` による 3 桁）に生成されます。`.specify/` 自体はツール本体（スクリプト・テンプレート・憲章）の置き場です。現在の機能コンテキストは `.specify/feature.json`（`feature_directory` キー）または環境変数 `SPECIFY_FEATURE_DIRECTORY` で解決されます（`.specify/scripts/powershell/common.ps1` の `Get-FeaturePathsEnv` 参照）。

PowerShell スクリプト（`.specify/scripts/powershell/*.ps1`）がテンプレート解決・前提条件チェック・新規機能ブランチ作成などを担っています。テンプレートは優先順位付きで解決されます: `overrides/` → `presets/`（`.registry` の優先度順）→ `extensions/` → コアの `.specify/templates/`。

## アーキテクチャ（MFC アプリケーション）

`MFCApplication1/` 配下の構成:

- `MFCApplication1.h` / `.cpp` — `CWinApp` を継承したアプリケーションクラス `CMFCApplication1App`。`InitInstance()` がエントリーポイントで、メインダイアログを生成・表示する
- `MFCApplication1Dlg.h` / `.cpp` — メインウィンドウとなる `CDialogEx` 派生クラス `CMFCApplication1Dlg`。メッセージマップ（`ON_WM_SYSCOMMAND`, `ON_WM_PAINT`, `ON_WM_QUERYDRAGICON` に加え `ON_BN_CLICKED(IDC_BUTTON_BROWSE, ...)` / `ON_BN_CLICKED(IDC_BUTTON_RUN, ...)`）でウィンドウメッセージを処理する。設定ファイル選択・シミュレーション実行ボタンのハンドラは `CanSim::LoadConfig` → `CanSim::SimulationEngine::Run` → `CanSim::Logger::WriteCsv` / `CanSim::ComputeMetrics` + `WriteMetricsCsv` を呼び出す薄いグルーコードで、UI 固有のロジック（ダイアログ操作・結果表示）以外はここに書かない
- `pch.h` / `pch.cpp` — プリコンパイル済みヘッダー（`framework.h` を include）
- `framework.h` — MFC/ATL の共通インクルード（`afxwin.h`, `afxext.h` など）。`NOMINMAX` をここで定義している
- `Resource.h`, `MFCApplication1.rc`, `res/` — リソース定義（ダイアログレイアウト、アイコン、文字列テーブルなど）。ダイアログの見た目を変更する場合は Visual Studio のリソースエディタか `.rc` ファイルを直接編集する（`.rc` は UTF-16LE、上記ビルド節のエンコーディング注意点を参照）
- `targetver.h` — 対象 Windows プラットフォームのバージョン定義
- `CanSim/` — CAN バスシミュレーションのコアロジック。`CDialogEx`/MFC に一切依存しないプレーン C++（`cl.exe` 単体でもコンパイル・リンク可能）:
  - `CanTypes.h` — `Network` / `Node` / `FrameDefinition` / `PendingFrame` / `Event` / `LogRow` などの値型（`specs/001-can-simulation-core/data-model.md` 参照）
  - `EventQueue.h/.cpp` — DES イベントキュー（時刻・イベント種別・ノード順で決定的にソート）
  - `FrameTiming.h/.cpp` — CAN CRC-15 計算とビットスタッフィングを反映した実ビット数からのフレーム伝送時間算出
  - `ConfigLoader.h/.cpp` — `CanSim/SampleConfigs/*.cfg` 形式（`specs/001-can-simulation-core/contracts/config-input-format.md`）の設定ファイル読み込み・検証
  - `Network.h/.cpp` — 調停（ID優先）・自動再送信・上書き（FR-011〜014）・受信フィルタ判定
  - `SimulationEngine.h/.cpp` — 上記を統合する DES メインループ本体
  - `Logger.h/.cpp` — シミュレーションログの CSV 出力（`specs/001-can-simulation-core/contracts/log-output-format.md`）
  - `MetricsAggregator.h/.cpp` — 総送信レイテンシ／送信待ち遅延／フレーム伝送時間／バス負荷率／ジッターの集計・CSV 出力
  - `StartupControl.h/.cpp` — ノードの起動制御（電源投入→初期化→バス同期検出→通常動作）。バス同期成立時刻の算出（`max(検出開始, レセシブ期間開始) + 11ビット時間`）と、起動記録の CSV 出力を担う
  - `SampleConfigs/` — 動作確認用サンプル設定（`sample_appendix_a.cfg` = ドメイン仕様書 付録A 相当、`sample_collision.cfg` = 調停確認用の衝突構成、`sample_startup.cfg` = 電源投入時刻をずらした起動確認用、`sample_startup_latejoin.cfg` = 送信中のバスへの後発参加確認用）

シミュレーション実行時は、読み込んだ設定ファイルのパスを基準に3つの CSV が出力されます: `<設定ファイル名>.log.csv`（フレームログ）、`.summary.csv`（性能指標の集計）、`.startup.csv`（起動記録）。

**起動制御による時刻への影響**: 電源投入時刻・初期化時間が既定値（0）の構成でも、各ノードはバスが空であることを11ビット時間（500 kbps で 22 µs）ぶん観測してから通常動作に入ります。そのため、起動直後にバスが連続して埋まっている区間の送受信時刻はその分だけ後ろにずれます（バスが一度空いて以降は送信要求時刻どおりに戻ります）。送信順序・調停結果・送信回数・ログの行数は影響を受けません。CAN の実挙動に忠実であることを優先しているため、起動を省略する互換モードは意図的に設けていません。

現時点でドキュメント/ビュー アーキテクチャは使用しておらず、単一のダイアログ (`IDD_MFCAPPLICATION1_DIALOG`) がアプリケーションの UI そのものです。新しい CAN シミュレータ機能を追加する際は、ドメインロジックを `CanSim/` に UI 非依存な形で追加し、`MFCApplication1Dlg` からはそれを呼び出すだけのグルーコードを書く、という分離を維持してください（憲章 III）。
