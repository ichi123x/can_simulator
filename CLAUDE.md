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
- 正式なテストフレームワーク・テストプロジェクトは導入していません（`MFCApplication1.sln` にテストは含まれません）。代わりに、`CanSim/` が UI 非依存のプレーン C++ である性質を利用した検証ツールを `tools/cansim_runner/` に置いています。GUI を操作せずに機能 001〜011・014・015・017〜020・022〜029 の成功基準（GUI 固有のものを除く）を一括検証できます。

```bat
REM 「x64 Native Tools Command Prompt for VS 2022」等でリポジトリのルートから実行
set OUT=%TEMP%\cansim_tools
mkdir "%OUT%" 2>nul

REM 検証ツールのビルド
cl /nologo /std:c++14 /EHsc /I tools\cansim_runner /I MFCApplication1\CanSim ^
   /Fo"%OUT%\\" /Fe"%OUT%\checks.exe" ^
   tools\cansim_runner\checks.cpp MFCApplication1\CanSim\*.cpp

REM 成功基準の一括検証（終了コード 0 で全パス）。第3引数に基準 CSV のディレクトリを渡すと、サンプルの
REM 結果が変わっていないこと（004 SC-009）も検証する。機能 006 でデータ 0xFF と送信要求の起点が変わった
REM ため、基準は 006 以降のビルドで作ったものを使う（README 参照）
"%OUT%\checks.exe" MFCApplication1\CanSim\SampleConfigs "%OUT%"
```

  単発のシミュレーション実行には同ディレクトリの `runner.cpp` を同様にビルドして使います（`runner.exe <.canprj> <出力接頭辞> [秒数]`）。詳細は [tools/cansim_runner/README.md](tools/cansim_runner/README.md) を参照してください。Lint の仕組みは未整備です。

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

- `MFCApplication1.h` / `.cpp` — `CWinApp` を継承したアプリケーションクラス `CMFCApplication1App`。`InitInstance()` がエントリーポイントで、メインダイアログを生成・表示する。機能 024 から、コマンドラインの最初の引数（`__wargv` を `CanSim::FindStartupProjectArg` で選ぶ）を `SetStartupProject` でダイアログに渡す（.canprj の関連付けによる起動。関連付けの登録はアプリでは行わない）
- `MFCApplication1Dlg.h` / `.cpp` — メインウィンドウとなる `CDialogEx` 派生クラス `CMFCApplication1Dlg`。メニュー駆動（機能 004）で、ボタン・入力欄などのコントロールは持たない（機能 014 のツールバーはコントロールではなく自前描画）。メニュー「ファイル（プロジェクト読込み／保存）」「編集（セッティング／データフレーム編集）」「シミュレーション（開始／中断／終了）」「解析（送信待ち時間・受信完了時間・送信ジッター・CAN バス負荷、機能 028・029）」「ログ表示（シミュレーションログ、機能 025）」を `ON_COMMAND` で処理し、項目の有効／無効とチェックは `OnInitMenuPopup` で設定する（ダイアログは `ON_UPDATE_COMMAND_UI` を配送しないため）。各ハンドラは `CanSim::LoadConfig` / `SaveProject` / `BuildRunNetwork` → `SimulationEngine::Run` → CSV 出力、`BuildFrameEvents` を呼ぶ薄いグルーコードで、UI 固有のロジック（ダイアログ操作・通知・描画・再生タイマー）以外はここに書かない。Enter/Esc では閉じず（`OnOK` は空、`PreTranslateMessage` で握りつぶす）、閉じるのは ✖ のみ。機能 026 からタイトルバーに Windows 標準の最小化・最大化のボタン（`.rc` の STYLE の `WS_MINIMIZEBOX | WS_MAXIMIZEBOX`）がある。最大化・大きさの変更でも中身（構成図・ログ表示エリア・当たり判定）は左上の固定位置で、余りは白、ツールバーの帯は幅いっぱい。最小化中も再生のタイマーと QPC の時計で実行は進む
- `FrameEditDlg.h` / `.cpp` — 「編集 → データフレーム編集」ウィンドウ `CFrameEditDlg`（`IDD_FRAME_EDIT`、機能 006）。ECU を選び、送信フレーム一覧（No. / フレーム名 / ID / DLC / 初回オフセット / 送信周期）を表示し、追加・編集・削除する。変更はその場でプロジェクトの定義（`m_definitions`）に反映され、保存はプロジェクト保存で行う。実行中は開けない。機能 010 からモードレス（`new` + `Create`、オーナーはメインダイアログ）で、コンストラクタの `initialEcu` で最初に選ぶ ECU を受け取り、`CurrentEcu()` で選択中の ECU を返す。Enter・Esc・「閉じる」・✖ はすべて `DestroyWindow` で、`PostNcDestroy` でオーナーに `WM_APP_FRAME_EDITOR_CLOSED` を送ってから `delete this`。追加・編集の入力画面と削除確認の間はオーナーを無効化する（その間に閉じられたり差し替えられたりしないように）。機能 022 から「ノード:」の右に「ECUラベル名:」の入力欄（`IDC_EDIT_ECU_LABEL`、32 文字まで）があり、入力のたびに選んだ ECU の `Node::Label` へ反映して（`EnsureNode`、前後の空白を除く）、オーナーへ `WM_APP_ECU_LABEL_CHANGED` を送り構成図を描き直させる。機能 013 から、送信フレーム一覧は列見出し・行・行の無い空白のどこをダブルクリックしても「追加」と同じ動作になる（`AddFromList`。追加できない＝ECU なし・100 件のときは `CanAdd()` が偽で何もしない。機能 006 の「行のダブルクリック = 編集」は無くなり、編集は「編集」ボタンから）。行と空白は一覧の `NM_DBLCLK`、列見出しは `CFrameListHeader` でサブクラス化して `WM_LBUTTONDBLCLK` を受け、既定の処理（境目の列幅の自動調整）をせずに `WM_APP_FRAME_LIST_ADD` を編集ウィンドウへ Post する。スクロールバーのダブルクリックは対象外
- `FrameEntryDlg.h` / `.cpp` — 追加・編集の入力画面 `CFrameEntryDlg`（`IDD_FRAME_ENTRY`）。フレーム名・ID（16 進）・DLC・送信周期(ms)・初回オフセット(ms) を入力し、ビット長と伝送時間を表示する。OK で `CanSim::ValidateFrame` を呼び、NG なら「項目: 理由」を示して閉じない
- `SettingsDlg.h` / `.cpp` — 「編集 → セッティング」ダイアログ `CSettingsDlg`（`IDD_SETTINGS`）。ECU 数 0〜16・通信速度 4 種はドロップダウンリスト、シミュレーション時間は 1〜600 秒の整数（範囲外は OK 時に拒否して閉じない）。スピードは機能 015 からここには無く、ツールバーのスライダーで変える
- `LogViewDlg.h` / `.cpp` — 「ログ表示 → シミュレーションログ」のウィンドウ `CLogViewDlg`（`IDD_LOG_VIEW`、機能 025）。モードレスで、作り（`new` + `Open`、✖・Esc で `DestroyWindow`、`PostNcDestroy` でオーナーに `WM_APP_LOG_VIEWER_CLOSED`（`WM_APP + 5`）を送って `delete this`）は編集ウィンドウと同じ。中身は自前描画の読み取り専用のテキスト表示 `CLogTextView`: `CanSim::LogFileIndex` で見えている行だけを読み（数画面分をキャッシュ）、MS Gothic 13 px で描く。縦は行・横は文字のスクロール（つまみのドラッグは `SIF_TRACKPOS`）、ホイール 3 行、矢印・PgUp/PgDn・Home/End・Ctrl+Home/End、ドラッグ／Shift + キーで文字単位の選択、Ctrl+A・Ctrl+C と右クリックの「コピー」「すべて選択」（コピーは 16 MB まで）。編集の操作は無い
- `AnalysisDlg.h` / `.cpp` — 「解析」の 2 つのウィンドウ（機能 028）。`CLatencyDlg`（`IDD_ANALYSIS_LATENCY`: 送信 ECU × 受信 ECU の受信完了時間の表とヒストグラム。行の選択でヒストグラムを切り替える）、`CBusLoadDlg`（`IDD_ANALYSIS_BUSLOAD`: 要約・10 ms ごとの負荷の折れ線・ECU ごとの使用率の表）、共通の `CAnalysisDlgBase`（作りと閉じ方は `CLogViewDlg` と同じ。中身は `OnInitDialog` で作り、`OnSize` で並べ直す）、GDI で棒・折れ線を描く `CAnalysisGraph`。機能 029 で `CTxWaitDlg`（`IDD_ANALYSIS_TXWAIT`: CAN ID ごとの送信待ち時間の表とヒストグラム）と `CJitterDlg`（`IDD_ANALYSIS_JITTER`: CAN ID ごとの送信ジッターの表と、時刻に沿ったずれのグラフ）を追加し、`CAnalysisGraph` にずれの描き方 `SetDeviation`（0 の線・点と折れ線。点が描画域の幅より多いときは `MinMaxPerColumn` で列ごとの最小〜最大の縦線）を足した。一覧・説明の行・グラフの並べ方は `LayoutListNoteGraph` で共通。計算はしない（`CanSim/LogAnalysis` の結果を表示するだけ）。029 改訂 1 で、モーダルの「解析中」ウィンドウ `CAnalysisProgressDlg`（`IDD_ANALYSIS_PROGRESS`: 文字・進み具合のバー・「中断」）と、裏の仕事との共有の状態 `AnalysisJobState`（読んだバイト数・全体・段階（0 読み込み・1 集計）・中断の要求。すべて `std::atomic`）を足した。50 ms のタイマーで表示を更新し、仕事が終わったら自分で閉じる（中断を受けていれば `IDCANCEL`）。「中断」・Esc・✖ は中断を頼んで文字を「中断しています...」にするだけで、仕事が止まるまで閉じない
- `pch.h` / `pch.cpp` — プリコンパイル済みヘッダー（`framework.h` を include）
- `framework.h` — MFC/ATL の共通インクルード（`afxwin.h`, `afxext.h` など）。`NOMINMAX` をここで定義している
- `Resource.h`, `MFCApplication1.rc`, `res/` — リソース定義（ダイアログレイアウト、アイコン、文字列テーブルなど）。ダイアログの見た目を変更する場合は Visual Studio のリソースエディタか `.rc` ファイルを直接編集する（`.rc` は UTF-16LE、上記ビルド節のエンコーディング注意点を参照）
- `targetver.h` — 対象 Windows プラットフォームのバージョン定義
- `CanSim/` — CAN バスシミュレーションのコアロジック。`CDialogEx`/MFC に一切依存しないプレーン C++（`cl.exe` 単体でもコンパイル・リンク可能）:
  - `CanTypes.h` — `Network` / `Node` / `FrameDefinition` / `PendingFrame` / `Event` / `LogRow` などの値型（`specs/001-can-simulation-core/data-model.md` 参照）
  - `EventQueue.h/.cpp` — DES イベントキュー（時刻・イベント種別・ノード順で決定的にソート）
  - `FrameTiming.h/.cpp` — CAN CRC-15 計算とビットスタッフィングを反映した実ビット数からのフレーム伝送時間算出
  - `ConfigLoader.h/.cpp` — プロジェクトファイル（`.canprj`）の読み込み・検証。形式は機能 001 の設定ファイル形式（`specs/001-can-simulation-core/contracts/config-input-format.md`）に `[Project]` セクション（`EcuCount`／`DurationSec`／`FrameDisplay`／`Speed`）を足した後継（`specs/004-menu-project-settings/contracts/project-file-format.md`）。ノード 0 個も許容する。機能 006 以降、フレームは `Name=` を持ち、データは常に DLC 分の 0xFF（`Data=` は読まない・書かない）、1 ノード最大 100 フレーム、`InitialOffsetUs` は `PeriodUs` 未満
  - `ProjectFile.h/.cpp` — `.canprj` の保存（`SaveProject`、コメントは保存しない）と、実行用ネットワークの生成（`BuildRunNetwork`: ECU 番号 1〜`EcuCount` のノードを作り、定義のない ECU はフレームなしで補い、`EcuCount` を超える番号の定義は実行しない）。機能 024 のタイトルの組み立て `ComposeWindowTitle`（タイトル + 半角空白 4 つ + ラベル + パス）、起動時の引数の選び方 `FindStartupProjectArg`（プログラムの次の最初の空でない引数）、拡張子の判定 `HasProjectExtension`（`.canprj`、大文字小文字を区別しない）
  - `FrameEvents.h/.cpp` — フレームログ（1 フレーム × 受信ノードで 1 行）を 1 フレーム 1 件の再生用イベント列に変換する（フレーム表示用）
  - `FrameEditor.h/.cpp` — 送信フレーム一覧の編集（機能 006）。入力検査 `ValidateFrame`（ID 0x000〜0x7FF・全ノードで一意、DLC 0〜8、周期 1〜3,600,000 ms、オフセット < 周期、名前 32 文字、1 ノード 100 件）、`AddFrame` / `ReplaceFrame` / `DeleteFrame`、定義のない ECU を作る `EnsureNode`、データ（DLC 分の 0xFF）を作る `MakeFrameData`
  - `ReplayClock.h/.cpp` — 実行中の時計（機能 005）。`SimTimeAtUs` は実経過時間 × 2^スピードでシミュレーション時刻を求め、`ActiveFrameCursor` は画面の書き換え 1 回分の時刻区間に送信中だったフレームの送信元・受信 ECU を集める（送信を優先）
  - `SimControls.h/.cpp` — シミュレーションの操作（機能 014）。実行の状態 `RunPhase`（Idle / Running / Paused）、状態ごとの開始・中断・終了の有効／無効 `GetSimCommandStates`（メニューとツールバーが共通に使う。Idle の開始は編集中なら無効、Paused の開始は再開）、ツールバー（96 dpi で高さ `kToolbarHeight` = 64）のボタンの範囲 `SimButtonRect` / `SimButtonBoxRect` / `SimButtonLabelRect` と当たり判定 `FindSimButtonAt`、再開後の時刻 `ResumedSimTimeUs`（中断の時刻 + 経過 × 2^スピード、飽和）。機能 015 のスピードスライダーの座標（目盛り -10〜+10 の `SpeedTickX`。機能 026 で -13〜+10 から変更、一番近い目盛りへの丸め `SpeedAtX`、当たり判定 `IsOnSpeedSlider`、`SpeedTrackRect` / `SpeedThumbRect` / `SpeedLabelRect`）と、スピード変更時の動作 `GetSpeedChangeAction`（Running は Rebase、Paused は再開で効く、Running/Paused で +10 は FinishNow）。機能 017 のフレーム表示チェックボックスの範囲（`FrameDisplayBoxRect` / `FrameDisplayLabelRect`、当たり判定 `IsOnFrameDisplayToggle`）、シミュレーション時間の欄（`SimTimeCaptionRect` / `SimTimeValueRect` / `SimTimeUnitRect` / `SimTimeAreaRect`）と 3 けた区切りの `FormatSimTimeUs`。機能 023 のツールバーのボタンの通し番号 `ToolbarButton`（Save・Load・Start・Pause・Stop、`kToolbarButtonCount` = 5）と、その範囲 `ToolbarButtonRect` / `ToolbarButtonBoxRect` / `ToolbarButtonLabelRect`・当たり判定 `FindToolbarButtonAt`・有効／無効 `IsToolbarButtonEnabled`（保存・読込みは Idle のときだけ。編集中でも有効）。`SimButtonRect(i)` などは `ToolbarButtonRect(i + 2)` と同じ値を返す
  - `FlowEffect.h/.cpp` — フレーム表示の流れのエフェクト（機能 018、改訂 1）。書き換え 1 回分に送られたフレームの（送信, 受信）の組 `ActiveFrames::Flows`（`ReplayClock` の `Collect` が強調の集まりと一緒に集める。送信もした ECU も受信先に含む）ごとに、送信 ECU の箱の縁 → 接続線 → バス → 受信 ECU の接続線 → 箱の縁の道筋に、16 px おきの粒を 150 px/秒（96 dpi、実時間）で流す位置（`FlowOffset` / `FlowPointAt`、同じ位置の粒は 1 つにまとめる）と、送信・受信 ECU の光（0.8 秒周期の明滅、受信は半周期遅れ）を `ComputeFlowEffect` が計算する。`FlowPathHold` は道筋を最後に見えてから 0.3 秒残す（点滅を防ぐ）
  - `LogLines.h/.cpp` — ログ表示エリア用の行（機能 009）。`BuildLogLines` が起動の状態遷移（初期化中／バス同期検出中／通常動作。シミュレーション時間を超えるもの・未到達は出さない）と、フレームログの各行（1 フレーム × 受信ノード）を「時刻 → 状態遷移が先 → ノード番号」の順に並べる
  - `LogFileIndex.h/.cpp` — ログファイルの行の索引（機能 025）。`Build` がファイルを 1 回だけ読んで 256 行（`kIndexStride`）ごとの行頭のバイト位置・行数・最長行の文字数を記録し、`ReadLines(first, count)` は近い索引から読む（範囲外は切る、行末の CR は除く、ファイルは読むたびに開いて閉じる）。位置は 64bit。`LogPathFor` はプロジェクトファイルの隣のフレームログ名（`<プロジェクトファイル>.log.csv`、`WriteResults` も使う）。機能 027 の結果ファイルの関数: `ResultFilePaths`（フレームログ・集計・起動記録の 3 つのパス。`WriteResults` もここから取る）、`ExistingResultFiles`（そのうち通常のファイルとしてあるもの）、`DeleteResultFiles`（`_wremove` で削除し、できた・できなかったに分ける。読み取り専用などの属性は変えない）
  - `LogAnalysis.h/.cpp` — フレームログの解析（機能 028）。`ReadLogRecords`（ヘッダーの列名で列を探して読む。読めなければ行番号付きのエラー）、`AnalyzeLatency`（送信 ECU × 受信 ECU ごとの `Rx_End − Tx_Request` の件数・最小・平均・最大・P95）、`MakeHistogram`、`AnalyzeBusLoad`（10 ms の区間・平均・最大・ECU ごとの使用率）、グラフの補助 `NiceTicks` / `MaxPerColumn`。機能 029 の `CollectTransmissions`（ログの行を送信 1 回にまとめる: `(TxNode, CanId, Tx_Start)`、OVERWRITE を除いて数える。並べ替えで重複を除く）、`AnalyzeTxWait`（`Tx_Start − Tx_Request` の全体と CAN ID ごとの統計。統計の計算は 028 と共通の `ComputeStats`）、`AnalyzeJitter`（CAN ID ごとの「送信開始の間隔 − 送信要求の間隔」、周期 = 送信要求の間隔の最頻値、母標準偏差）、`MinMaxPerColumn`（ずれの点を列ごとの最小・最大にまとめる）。029 改訂 1 で `ReadLogRecords` は 4 MB ずつ読み、行・欄の切れ目を `memchr` で探して文字列を作らずに数値を読む（ヘッダー・エラーの内容・結果は以前と同じ）。第 4 引数の `progress(bytesRead, totalBytes)` を 4 MB ごとと最後に呼び、false なら読むのをやめて `error` を `kLogReadCancelled` にする。`CollectTransmissions` は CAN ID ごとの入れ物に分け、並んでいない入れ物だけ並べ替える。600 秒・16 ECU（197 MB・193 万行）で、読み込みは Release 0.7 秒・Debug 3.5 秒（以前は 5.5 秒・62.6 秒）
  - `Network.h/.cpp` — 調停（ID優先）・自動再送信・上書き（FR-011〜014）・受信フィルタ判定
  - `SimulationEngine.h/.cpp` — 上記を統合する DES メインループ本体。フレームバッファ（機能 008、Doc 3.7a）: 送信は「送信待ち + 送信中の 1 本」で 16 本（`kTxBufferCount`）、満杯なら新しい送信要求を捨てて OVERWRITE 行（受信ノード・送受信時刻は空欄）を出す。同じ時刻・同じノードの送信要求は CAN ID の小さい順に処理する（ID の大きいものが捨てられる）。受信は 8 本の FIFO（`kRxBufferCount`、`Node::RxBuffer` に受信処理時刻を持ち、受信完了時に取り出し済みを先に外す）で、満杯なら新しいフレームを捨てて OVERWRITE（受信処理時刻は空欄）。受信処理遅延 0 なら受信側の満杯は起きない
  - `Logger.h/.cpp` — シミュレーションログの CSV 出力（`specs/001-can-simulation-core/contracts/log-output-format.md`、008 で列名 `Update_Count`（旧 `Overwrite_Count`）、Status `OVERWRITE`、負の時刻は空欄）。書き出しは 1 つのバッファに数値を直接書いて 4 MB ごとに書く（行ごとの `ostringstream`・`to_string` をやめた。出力はバイト単位で以前と同じ）。600 秒・16 ECU（193 万行）で Release 13.6 → 0.9 秒、Debug 93 → 7.8 秒。`WriteLogCsv(rows, path)` は `Logger` にコピーせずに書く（メインの `WriteResults` が使う）。メインの `FinishReplay` は書き出しの前に `UpdateWindow` で終わりの状態を描く（書き出しの間に無効化した部分が白いまま残らないように）
  - `MetricsAggregator.h/.cpp` — 総送信レイテンシ／送信待ち遅延／フレーム伝送時間／バス負荷率／ジッターの集計・CSV 出力。OVERWRITE 行は統計から除き、Node 行の末尾に `TxDiscard_Count` / `RxDiscard_Count`（機能 008）
  - `StartupControl.h/.cpp` — ノードの起動制御（電源投入→初期化→バス同期検出→通常動作）。バス同期成立時刻の算出（`max(検出開始, レセシブ期間開始) + 11ビット時間`）と、起動記録の CSV 出力を担う
  - `BusDiagramLayout.h/.cpp` — メインダイアログの CAN バス構成図の座標計算（ECU 数 0〜16 → バス線・ECU 箱・接続線の位置）。96 dpi 基準のピクセル値を返す純粋関数で、描画（GDI）はダイアログ側が行う（`specs/003-bus-diagram-view/`）。DPI 換算 `ScaleToDpi`（`(px * dpi + 48) / 96`）と ECU の当たり判定 `FindEcuAt`（換算後の箱 `[Left, Right) × [Top, Bottom)`、無ければ 0）も持ち、描画と判定で同じ換算を使う（機能 010）
  - `SampleConfigs/` — 動作確認用サンプルプロジェクト（`.canprj`）。`sample_appendix_a` = ドメイン仕様書 付録A 相当、`sample_collision` = 調停確認用の衝突構成（ECU 1・3 が送信。ECU 2 は何も受信しない待機 ECU として明示し、機能 001 の結果を保つ）、`sample_startup` = 電源投入時刻をずらした起動確認用、`sample_startup_latejoin` = 送信中のバスへの後発参加確認用、`sample_16ecu` = 16 ECU の構成図・フレーム表示の確認用、`sample_buffer_full` = 送信・受信バッファの満杯（OVERWRITE）の確認用。`sample_vehicle_2ecu` / `_4ecu` / `_8ecu` / `_16ecu` = 実車の CAN をイメージしたデモ用（500 kbps、日本語の ECUラベル名（エンジン・ブレーキ/ESC・メーターなど、10 文字以内）、実車風のフレーム名・ID の優先度・周期 10〜1000 ms、各 ECU が使うフレームだけを受ける RxFilter（Gateway は全受信）、ずらした初回オフセット、5 秒・スピード -3・フレーム表示 ON。同じ ECU はどのサンプルでも同じ CAN ID で、そのサンプルの中に受信する ECU があるフレームだけを入れる（受信先の無い行を出さない）。バス負荷は約 3〜19%、16 ECU は 5 秒で 4,015 フレーム・調停負けの再送 990 フレーム・破棄 0）。作り直しは `tools/vehicle_samples/gen_vehicle_samples.ps1`、ログの検証は同じフォルダーの `verify_log.ps1`、動作テストの記録は [tools/vehicle_samples/README.md](tools/vehicle_samples/README.md)

**CAN バス構成図（機能 003）**: `CMFCApplication1Dlg` はクライアント左上に白背景の構成図を描く（`OnPaint` → `DrawBusDiagram`、メモリ DC 経由、DPI/96 でスケール）。表示する ECU 数は `m_ecuCount`（初期値 2）で、`SetEcuCount()` で更新する。変更がキャンセル・失敗した場合は呼ばない約束（003 FR-010）。ダイアログの初期サイズは 16 ECU の図領域とその下のログ表示エリアが収まる大きさ（`.rc` で 720×373 DLU = クライアント高さ約 560 px、メニュー行は含まない。機能 009 で 152 DLU から拡大）。メニュー行より下のクライアント領域は `OnEraseBkgnd` で常に白く塗る（004 FR-004）。

**プロジェクトと実行（機能 004）**: ダイアログは現在のプロジェクト（`m_settings` = ECU 数・シミュレーション時間・フレーム表示、`m_definitions` = 読み込んだ全ノード定義と通信速度、`m_projectPath`）を持つ。起動時は ECU 数 2・500 kbps・10 秒・フレーム表示 ON（機能 021。`ProjectSettings` の既定値で、`FrameDisplay=` の無いファイルも ON）・スピード 0。「開始」はまずシミュレーションを最後まで UI スレッドで同期計算し（上限 600 秒で数秒程度、待機カーソル表示）、その後、実経過時間 × 2^スピードでシミュレーション時刻を進めて、終端に達したら CSV を書く（機能 005。スピード 0 = 実時間、+10 = 待たずに即時完了、-10 = 1/1024。機能 026 で範囲を -10〜+10 にし、以前のファイルの -13〜-11 は -10 として読む（`kLegacyMinSpeed`））。時間の進み方はフレーム表示の ON/OFF に関係せず、フレーム表示は強調を描くかどうかだけを決める（機能 017 からツールバーのチェックボックスでいつでも切り替え、実行中・中断中もその場で反映）。画面は 16 ms のタイマーで書き換え（実測 約 40 回/秒）、直前の書き換えから今回までに送信中だったフレームの送信元 ECU（赤）・受信 ECU（青。機能 016 から。接続線も送信と同じ青の太線）・バス（青）をまとめて強調する。時刻はタイマーの回数ではなく `QueryPerformanceCounter` の経過時間で決める。実行中の「終了」は、その時点のシミュレーション時刻までエンジンを再実行してその時点までの結果を出力する（送信を完了したフレームだけが残る）。実行中は「ファイル」「編集」メニューをトップレベルごとグレーにして開けなくする（`UpdateTopMenus`、機能 007）。操作できるのは「シミュレーション → 終了」と ✖ だけ。バスは各フレームの EOF 後にインターミッション（3 ビット）を経てからアイドルになる（実装済み、画面には表示しない）。

**プロジェクトファイル名の表示と起動時の引数（機能 024）**: タイトルバーは「CAN Bus Simulator␣␣␣␣プロジェクト名：<フルパス>」（基のタイトルは `.rc` のキャプションを `OnInitDialog` で `m_baseTitle` に保持、ラベルは `IDS_PROJECT_PATH_LABEL`、プロジェクトファイルが無いときはラベルで終わる）。`m_projectPath` を変えるのは `SetProjectPath` だけで、そこでタイトルも書き直す（`OnInitDialog` で空、`LoadProjectFrom` と `SaveProjectTo` の成功時。キャンセル・失敗では変わらない）。読込みはメニュー・ツールバー・起動時の引数で共通の `LoadProjectFrom`（拡張子の検査 → `LoadConfig` → エラー表示 or 反映）。起動時の引数は `OnInitDialog` の最後に `WM_APP_LOAD_STARTUP_PROJECT`（`WM_APP + 4`、`FrameEditDlg.h` で番号を管理）を Post し、表示後に `GetFullPathNameW` / `GetLongPathNameW` で絶対パスにしてから読む（失敗しても既定のプロジェクトで続ける）。関連付けの登録はしない（ユーザーが「プログラムから開く」で設定する）

**解析・ログ表示メニュー（機能 025）**: メニューバーは「ファイル・編集・シミュレーション・解析(A)・ログ表示(L)」。「解析」は機能 028 の「受信完了時間」「CAN バス負荷」（025 の「準備中」は無くなった）。「ログ表示 → シミュレーションログ」（`OnLogViewSimLog`）は現在のプロジェクトのフレームログ `LogPathFor(m_projectPath)` を `CLogViewDlg` で開く（`m_logViewer`。開いていれば前面に出すだけ）。プロジェクトファイルが無い → `IDS_LOG_NONE_NOPROJECT`、ログが無い → `IDS_LOG_NONE`（プロジェクトのファイル名）、読めない → `IDS_LOG_READ_ERROR`。表示するのはフレームログだけ（集計・起動記録は出さない）。全行を読めるように、開くときに索引を作って見えている行だけを読む（100 万行で約 0.3 秒）。表示中のログと現在の状態が食い違わないよう、新しい実行の「開始」・読込み・セッティング・`SetProjectPath` でパスが変わるとき・メインを閉じるときに閉じる（機能 028 から `CloseResultWindows` で、解析の 2 つのウィンドウもまとめて閉じる）。実行中・中断中は `UpdateTopMenus` が位置 0・1・3・4 をグレーにする（シミュレーション（2）だけ使える）

**ログクリア（機能 027）**: 「ログ表示」の「シミュレーションログ」の下に「ログクリア(C)...」（`ID_LOG_CLEAR`、`OnLogClear`。実行中・中断中はグレー）。現在のプロジェクトの結果ファイル 3 つ（`.log.csv`・`.summary.csv`・`.startup.csv`）のうちあるものを、確認（`IDS_LOG_CLEAR_CONFIRM`、ファイル名と元に戻せない旨。はい／いいえ、既定はいいえ）の後に削除する。「はい」なら先に `CloseResultWindows` でシミュレーションログ（と 028 の解析）のウィンドウを閉じ、1 つでも削除できたら `ClearLog` でログ表示エリアの行を消す（構成図の強調・シミュレーション時間・`m_runRows` は変えない）。通知: プロジェクトファイルなし `IDS_LOG_NONE_NOPROJECT`・ファイルなし `IDS_LOG_NONE`（025 と同じ、確認は出さない）・完了 `IDS_LOG_CLEAR_DONE`・削除できないものあり `IDS_LOG_CLEAR_FAILED`（ファイル名）。データフレーム編集ウィンドウ・プロジェクトの内容・`m_projectPath` は変えない

**解析: 受信完了時間・CAN バス負荷（機能 028）**: 「解析」の「受信完了時間(R)...」（`ID_ANALYSIS_LATENCY`、`OnAnalysisLatency`）と「CAN バス負荷(B)...」（`ID_ANALYSIS_BUSLOAD`、`OnAnalysisBusLoad`）。どちらも現在のプロジェクトのフレームログ（`.log.csv`）を `RunAnalysis`（025 と同じ通知: プロジェクトなし・ログなし・読めない。029 改訂 1 から読み込みと計算は裏のスレッド）で読み、`CanSim::LogAnalysis` で計算して、モードレスのウィンドウ（`AnalysisDlg.h/.cpp`。開いていれば前面に出すだけ、実行中・中断中はグレー）に出す。受信完了時間は `Rx_End − Tx_Request`（受信 ECU のある、送れた行。再送は届いた 1 回を 1 件、OVERWRITE は件数だけ）を送信 ECU × 受信 ECU ごと（先頭は全体）に件数・最小・平均・最大・95 パーセンタイル（最近順位法）の表にし、選んだ行のヒストグラム（ビンは `clamp(ceil(sqrt(n)), 10, 50)` 本程度・幅は 1/2/5×10^m）を描く。集計 CSV の「総送信レイテンシ」（`Rx_Process` まで）とは終点が違う。バス負荷は送信 1 回を `(Tx_Node, CAN_ID, Tx_Start)` でまとめ（受信 ECU の数だけ重複する行を 1 回に）、`[Tx_Start, Tx_End)` を使用時間として 10 ms の区間ごとの負荷（折れ線。区間が幅より多ければ列ごとの最大で間引く）・平均（使用時間 ÷ 範囲。範囲は 0 から最後の `Tx_End` を含む区間の終わりまで）・最大・ECU ごとの使用率を出す。集計 CSV の「平均バス負荷率」（送信ごとの直近 100 ms の負荷の平均）とは別の指標。ウィンドウは `CAnalysisDlgBase`（位置・閉じ方・オーナーへの `WM_APP_LATENCY_CLOSED` / `WM_APP_BUSLOAD_CLOSED`（`WM_APP + 6` / `+ 7`））を共通にし、一覧は `CListCtrl`、グラフは自前描画の `CAnalysisGraph`。メインは `m_latencyDlg` / `m_busLoadDlg` を持ち、ログのウィンドウと同じ契機に `CloseResultWindows` で閉じる

**解析: 送信待ち時間・送信ジッター（機能 029）**: 「解析」は「送信待ち時間(W)・受信完了時間(R)・送信ジッター(J)・区切り線・CAN バス負荷(B)」の順（`.rc` の並びだけ。コマンド ID は 028 の 2 つを変えずに `ID_ANALYSIS_TXWAIT` 32787・`ID_ANALYSIS_JITTER` 32788 を足した）。ハンドラ `OnAnalysisTxWait` / `OnAnalysisJitter` は 028 と同じ流れ（`RunAnalysis` の通知、開いていれば前面に出すだけ、実行中・中断中はグレー）で、ウィンドウは `m_txWaitDlg` / `m_jitterDlg`、閉じた通知は `WM_APP_TXWAIT_CLOSED` / `WM_APP_JITTER_CLOSED`（`WM_APP + 8` / `+ 9`）、`CloseResultWindows` で一緒に閉じる。どちらもログの行を送信 1 回にまとめてから数える（受信 ECU の数だけの重複を除く。送信の回数は 028 のバス負荷と同じ）。送信待ち時間は `Tx_Start − Tx_Request`（全体と CAN ID ごとの件数・最小・平均・最大・P95、ヒストグラムは 028 と同じ規則）。送信ジッターは CAN ID ごとに `(Start[k] − Start[k−1]) − (Request[k] − Request[k−1])`（正は遅れ、負は早まり）。破棄が無ければ機能 001 の FR-019（間隔 − 周期）と同じ値で、集計 CSV の `Jitter_Max_us` / `Jitter_Min_us` / `Jitter_StdDev_us` と一致する（checks で全 CAN ID を照合）。破棄で送信が抜けても、抜けた周期をずれとして数えない（集計 CSV とは値が違ってよい）。周期はプロジェクトの設定ではなく、ログの送信要求の間隔の最頻値。表の周期は ms、送信が 1 回だけの CAN ID は件数 0 で周期・統計は空欄

**解析中のウィンドウと中断（029 改訂 1）**: 4 つの解析のハンドラは `RunAnalysis(analyze)` を呼ぶ。プロジェクトなし・ログなしを通知したあと、`std::async` の裏の仕事で `ReadLogRecords`（進み具合を `AnalysisJobState` に入れ、中断の要求で止まる）→ 段階を集計に → `analyze(records)` を行う（結果は 0 成功・1 読めない（例外も）・2 中断）。待機カーソルで 500 ms 待っても終わらなければ `CAnalysisProgressDlg` を `DoModal` する（その間メインは操作できないので、解析のメニューを重ねて選ぶことは無い）。読めなければ `IDS_LOG_READ_ERROR`、中断なら何も出さず何も開かない。ハンドラは `RunAnalysis([&analysis](const std::vector<CanSim::LogRecord>& records) { analysis = CanSim::AnalyzeJitter(records); })` の形。小さなログはウィンドウを出さずに開く

**中断とツールバー（機能 014）**: 実行の状態は待機・実行中・中断中（`m_runPhase`、`InRun()` は実行中か中断中）。「中断」（`OnSimPause`）はタイマーの更新と同じ処理（`AdvanceReplay`）で表示を今の時刻まで進めてから時計を止め、強調・ログはそのまま残す。中断中の「開始」は再開（`ResumeReplay`: `m_simBaseUs` = 中断の時刻から同じスピードで続ける。計算・表示の初期化・保存先の確認はしない）、「終了」は中断の時刻までの結果を出す。CSV は中断しても変わらない。中断中も「ファイル」「編集」はグレー、ECU のダブルクリックは無効。メニューの下に暗い帯のツールバー（開始 = 緑の三角・中断 = 青の 2 本線・終了 = 赤の四角、名前は `.rc` の `IDS_SIM_BTN_*`）を `DrawSimToolbar` で描き、押して同じボタンの上で離すと `ID_SIM_*` を Post する（メニューと同じ処理）。有効／無効は `GetSimCommandStates` でメニューと共通、状態や編集中が変わるたびに `UpdateSimControls` で描き直す。構成図・ログ表示エリア・枠・編集ウィンドウの位置・ECU の当たり判定は帯の高さ `ContentTop()` だけ下げ（CanSim の座標はコンテンツ内のまま）、ウィンドウの高さは `OnInitDialog` でその分広げる

**スピードスライダー（機能 015）**: ツールバーの終了ボタンの右に、-10〜+10 の 21 個（機能 026。以前は -13〜+10 の 24 個）の目盛りと数字（今の値は太字・黄色）を持つスライダーを自前描画する（つまみは縦長の棒）。いつでも（待機・実行中・中断中・編集中）操作でき、押した位置・ドラッグ中の位置の一番近い目盛りの値を `SetSpeed` で `m_settings.Speed` に入れる（スピードの保持はここ 1 か所で、プロジェクトに保存される。読込み後はつまみを描き直す）。実行中の変更は、その瞬間の時刻を `m_simBaseUs` にして新しいスピードで続け（時刻は飛ばない・戻らない）、中断中は再開から効く。実行中・中断中に +10 にするとその場で `FinishReplay`（完了。ドラッグの捕捉は先に外す）。高いスピードを通るドラッグでは、+10 に届く前に自然終了することがある

**フレーム表示チェックボックスとシミュレーション時間（機能 017）**: ツールバーは左から 開始・中断・終了のボタン → 「フレーム表示」のチェックボックス → スピードスライダー（当時の -13 の目盛りは 96 dpi で x 320）→ 右端の「シミュレーション時間」（数値は 3 けた区切り + 「μs」）。「編集 → フレーム表示」は無い。チェックボックスはチェック欄・文字のどちらを押しても `ToggleFrameDisplay` で `m_settings.FrameDisplay` を反転し（いつでも。プロジェクトに保存される）、強調の対象 `m_active` は ON/OFF に関係なく集めているので、実行中・中断中もその場で反映される。表示する時刻は `m_displaySimUs`（`SetDisplaySimUs`）: 新しい実行の開始で 0、書き換え・中断で `m_curSimUs`、自然終了・即時完了で終端、「終了」で停止時刻（終了メッセージと同じ値）

**流れのエフェクト（機能 018、改訂 1）**: フレーム表示の強調（送信 ECU 赤・受信 ECU 青・青い線とバス）の上に、送信 ECU から受信 ECU へ流れるオレンジの粒（白い縁、16 px おき・150 px/秒）と、ECU の箱の外側のオレンジの光の枠（0.8 秒周期の明滅）を `DrawFlowEffect`（`DrawBusDiagram` の最後）で重ねて描く。動きは実際のビットの進みではなく一定の速さのくり返しで、エフェクトの時計 `m_flowElapsedUs` は再生の書き換え（`AdvanceReplay`）ごとに実時間だけ進む（中断中は書き換えが無いので止まり、再開で続きから。スピードに依存しない。新しい実行で 0）。道筋は書き換え 1 回分の実際の（送信, 受信）の組から作り、`m_flowHold`（`FlowPathHold`）で最後に見えてから 0.3 秒残す。描くのは `InRun() && FrameDisplay` で残っている道筋があるとき。スピード 0 で全 ECU が同じ書き換えの間に送信しても粒は出る

**ツールバーの並びと大きさ（機能 019）**: ツールバーは左から 開始・中断・終了のボタン（箱 24×18、96 dpi。名前は下に 10 px の文字）→ フレーム表示のチェックボックス → シミュレーション時間 → 「スピード：」（`IDS_SPEED_CAPTION`）→ スピードスライダー（目盛り 13 px 間隔、当時の -13 の目盛りは x 520。機能 023 で x 604。機能 026 から一番左は -10 の目盛りで x 604、+10 は x 864。つまみ幅 5）。スライダーの数字は 5 刻み（-10・-5・0・+5・+10。機能 026 で -13 が無くなった）と今の値だけを表示し、今の値から 2 目盛り以内の 5 刻みは隠す（`IsSpeedLabelShown`）。座標はすべて `SimControls.h` の定数で、checks の 014・015・017・019 の項目が並び・重なりを検証する

**保存・読込みボタン（機能 023）**: ツールバーの左端に「保存」「読込み」のボタン（開始などと同じ大きさ。記号は紫のフロッピー・黄土色のフォルダー、名前は `IDS_FILE_BTN_SAVE` / `IDS_FILE_BTN_LOAD`）を置き、ボタン 1 つ分の空きを挟んで開始・中断・終了が続く（96 dpi で箱の左端 8・36・92・120・148）。チェックボックス・シミュレーション時間・「スピード：」・スライダーは 019 の位置から 84 px 右。ダイアログはボタンを `CanSim::ToolbarButton` の通し番号（`m_pressedButton` も同じ）で描き・押し、有効／無効は `IsToolbarButtonEnabled`（保存・読込みは実行中・中断中はグレー）。読込みはメニューと同じ `ID_FILE_LOAD_PROJECT` を Post する（編集ウィンドウは先に閉じる）。保存はツールバー専用の `ID_FILE_SAVE_OVERWRITE`（`OnFileSaveOverwrite`）で、プロジェクトファイル（`m_projectPath`）があれば尋ねずに上書きし、無ければ保存先を尋ねる。メニューの「プロジェクト保存」は従来どおり毎回保存先を尋ねる。書き込みと失敗時のメッセージ・成功時の `m_projectPath` の更新は `SaveProjectTo` に共通化（メニューの保存・ツールバーの保存・「開始」の保存先確認が使う。失敗なら `m_projectPath` は変えない）。改訂 1: 保存先が読み取り専用で失敗したときは、ファイル名と「読み取り専用のため保存できなかった」ことを示す `IDS_SAVE_READONLY`（`GetFileAttributes` で判定。それ以外の失敗は `IDS_SAVE_ERROR`）。フレーム表示のチェックボックスの箱と文字は、ボタンの箱の縦の中心（96 dpi で y 23）に揃える（箱 y 16〜30、文字 13〜33）

**領域のラベル（機能 020）**: ログ表示エリアの枠のすぐ上の左端に「イベント＆ログ表示」（`ScaleLogLabelRect`、左端は枠と同じ換算、`DrawLogFrame` → `DrawAreaLabel`、文字列は `IDS_LOG_VIEW_LABEL`）を 12 px の濃い灰色で描く。構成図の左上にはラベルを置かない（ユーザーの指示で「イベント表示」を外した）。checks 020 がログの枠・構成図と重ならないことを検証する

**ECUラベル名（機能 022）**: ノードに表示用の `Label`（プロジェクトファイルの `[NodeN]` の `Label=`、空なら書かない。古い版は無視する）を持つ。起動記録の CSV に出る `Name` とは別で、自動の「ECUn」も入らない（既定は空）。構成図の ECU の箱は縦 48 px（96 dpi、上段 20〜68・下段 152〜200）で、ラベル名があれば「ECU nn」を上の段、ラベル名（`EcuLabelForDiagram`: 10 文字まで、超えたら先頭 10 文字と「...」）を下の段に 10 px の文字で描く（箱の幅に収まらなければ 7 px まで小さくする）。無ければ「ECU nn」は中央。文字数（`CountUtf8Chars`、全角・半角とも 1）・`TrimEcuLabel`・`LimitEcuLabel` は `FrameEditor.h`

**ログ表示エリア（機能 009）**: 構成図の下に薄いグレー（`RGB(230,230,230)`、96 dpi 基準で左 150・上 260・850×260 px）の領域を描き（`DrawLogView`、等幅フォント MS Gothic）、右端にスクロールバー `IDC_SCROLL_LOG` を置く。「開始」で前回の行を消し、計算結果から `BuildLogLines` で全行を作っておき、再生時計（`m_curSimUs`）が各行の時刻に達したものから順に表示する（フレーム表示の ON/OFF に関係しない。スピード +10 と自然終了では全行、「終了」ではその時点まで）。一番下を見ている間は最新行が一番下に来るよう 1 行ずつ追従し、スクロールバー・ホイール（3 行）で遡っている間は位置を動かさない。一番下へ戻すと追従を再開する。行が収まる間はスクロールバーを無効にする。行は次の「開始」まで残る（読込み・セッティング変更では消さない）。「開始」は、保存先の確定後・計測関連の初期化（実行用ネットワークの生成・計算・再生の準備）の前に `ResetDisplaysForRun` でログ表示（行・先頭位置・スクロールバー）・グレー表示・強調を初期化し、`UpdateWindow` でその場で描き直す（機能 012。計算の間に前回の表示が残らない）。状態名・フレームの状態名（正常／再送／破棄）は `.rc` の文字列テーブル（`IDS_LOG_*`）から読む。

**ECU のダブルクリックで編集（機能 010）**: 構成図の ECU の箱をダブルクリック（`OnLButtonDblClk`）すると、その ECU を選んだデータフレーム編集ウィンドウを開く（実行中・箱の外は何もしない）。編集ウィンドウはモードレスで、メインダイアログは `m_frameEditor`（開いている編集ウィンドウ）と `m_pendingEcu`（切り替えの待ちで開く ECU）を持つ。開いたまま同じ ECU をダブルクリックすると前面に出すだけ、別の ECU なら閉じて 500 ms 後（タイマー ID 2、`kSwitchTimerId`）にその ECU で開き直す（待ちの間のダブルクリックは開く ECU を置き換えるだけで待ちは延ばさない）。開く位置はログ表示エリアの左端・構成図の下端の 8 px 下（ECU を覆わない。画面の作業領域からはみ出す分はずらす）。「編集中」（開いている、または待ち）は「開始」をグレーにし、「プロジェクト読込み」「セッティング」は先に編集ウィンドウを閉じて（待ちも取り消して）から行う。「データフレーム編集」メニューは、開いていれば前面に出すだけ、待ちの間は何もせず、どちらでもなければ ECU 01 で開く。構成図の ECU 数（`m_ecuCount`）を編集ウィンドウの選択欄の ECU 数に使う。

**グレー表示と枠（機能 011）**: 受け付けた ECU のダブルクリック（開く・前面に出す・切り替え・待ちの間の置き換え）のたびに、その ECU を 500 ms グレー（塗り `RGB(205,205,205)`）で描く（`m_grayEcu`、タイマー ID 3、`StartGray` は機能 010 の処理より前に `UpdateWindow` まで行う）。グレーは常に最後の 1 つで、次のダブルクリックで張り直す。ECU 数の変更でその ECU が無くなるとき・「開始」で取り消す（`ClearGray`）。実行中・箱の外・入力画面の表示中はグレーにしない。構成図の領域の 4 px 内側（`BusDiagramLayout::Frame`、`kDiagramFrameInset`）とログ表示エリア（スクロールバー込み）の 3 px 外側（`DrawLogFrame`。ログ表示エリアの位置と枠は `BusDiagramLayout` の `ScaleLogViewRect` / `ScaleLogFrameRect` で求め、checks で検証）に、グレー `RGB(160,160,160)` の 1 px の枠を描く（ウィンドウの大きさ・判定範囲は変えない）。

シミュレーション実行時は、プロジェクトファイルのパスを基準に3つの CSV が出力されます: `<プロジェクトファイル名>.log.csv`（フレームログ）、`.summary.csv`（性能指標の集計）、`.startup.csv`（起動記録）。プロジェクトを一度も読込み・保存していない状態で「開始」すると、先に保存先を尋ねる。

**送信要求の起点（機能 006）**: 各フレームの最初の送信要求は、送信ノードが通常動作に入った時刻 + 初回オフセットで、以後は周期ごと（Doc FR-020a）。`SimulationEngine::HandleSyncComplete` で積む。通常動作前に送信要求は発生しないので、機能 002 の「起動前の要求の保留」は起きない。

**起動制御による時刻への影響**: 電源投入時刻・初期化時間が既定値（0）の構成でも、各ノードはバスが空であることを11ビット時間（500 kbps で 22 µs）ぶん観測してから通常動作に入ります。そのため、起動直後にバスが連続して埋まっている区間の送受信時刻はその分だけ後ろにずれます（バスが一度空いて以降は送信要求時刻どおりに戻ります）。送信順序・調停結果・送信回数・ログの行数は影響を受けません。CAN の実挙動に忠実であることを優先しているため、起動を省略する互換モードは意図的に設けていません。

現時点でドキュメント/ビュー アーキテクチャは使用しておらず、単一のダイアログ (`IDD_MFCAPPLICATION1_DIALOG`) がアプリケーションの UI そのものです。新しい CAN シミュレータ機能を追加する際は、ドメインロジックを `CanSim/` に UI 非依存な形で追加し、`MFCApplication1Dlg` からはそれを呼び出すだけのグルーコードを書く、という分離を維持してください（憲章 III）。

<!-- gtest-setup -->
## テストの実行

- ビルドとテスト: `cmake -S . -B build && cmake --build build --config Debug && ctest --test-dir build -C Debug --output-on-failure -LE slow`
- マージ前の全テスト（Release、slow を含む）: `cmake --build build --config Release && ctest --test-dir build -C Release --output-on-failure`
- 処理時間・速度を測るテストは別のファイル（別の `add_gtest`）にして `slow` を付ける。Debug の時間では合否を決めない
- CMake はテスト専用（製品のビルドは `.sln`）。`CanSim/` は静的ライブラリ `app_core` としてビルドし、`pch.h` は `tests/stubs/pch.h` の空ヘッダーで満たす（MFC に依存しないことの確認も兼ねる）
- 仕様テストは機能ごとに `tests/unit/feature_<NNN>_spec_tests.cpp`（001 は補足の `feature_001_edge_tests.cpp` も）。一時ファイル・構成の組み立て・実行・CSV の分解は `tests/support/test_helpers.h` を使う。仕様と実装が食い違うテストは `GTEST_SKIP() << "仕様と実装の食い違い（人の判断待ち）: ..."` で残し、人が決めるまで消さない
- ラベル: `unit` / `integration` / `acceptance` / `slow`（`tests/CMakeLists.txt` の `add_gtest` で付ける）
- テスト名の決まり: `F<機能番号>_<ID>_<条件>_<結果>`（英数字と _ だけ。日本語にすると ctest で 0 件実行のまま成功になるため、フックが止める）、`RecordProperty("spec", "<機能番号>:<ID>")` で spec.md の ID を記録する
- テストコードのコメントと、失敗時に表示するメッセージは日本語で書く（ソースは UTF-8）
- `/speckit-implement` では、各タスクの後に上のコマンドを実行し、全件成功を確かめること
- 作業の終わりには Stop フック（`.claude/hooks/run-tests.ps1`）が自動でビルドとテストを実行する。失敗していると作業を終えられない

<!-- gtest-coverage -->
## カバレッジ

- 測る: `powershell -NoProfile -ExecutionPolicy Bypass -File .claude/scripts/coverage.ps1`（結果: `build/coverage/uncovered.txt` と `build/coverage/index.html`）
- 赤い行（テストで実行されていない行）を減らすときは `/coverage` を使う。測る → 根拠を探す → テストを書く → 測り直す、をくり返す
- テストは、期待値の根拠が仕様（spec.md・data-model.md・contracts/・ヘッダーのコメント）にある行だけ書き、根拠を各テストのコメントに残す
- 根拠が無い行はテストを書かずに人に確認する。人が「テストしない」と決めた行は `tests/coverage-hold.md` に理由と一緒に載せる
- 赤い行を消すためだけのテストは書かない。100% は目標にしない