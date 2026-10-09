# CAN バスシミュレータ

車載組み込みシステム開発向けの CAN 2.0A 準拠バスシミュレータです。  
MFC/C++ で実装し、Spec-Driven Development (SDD) によって開発しました。

---

## 概要

調停・自動再送信・フレームバッファ管理など、実際の車載 ECU と同じ CAN バスの挙動を忠実に再現します。

**主な機能：**
- 最大 16 ECU（エンジン・ブレーキ/ESC・ゲートウェイ等の実車 ECU 名に対応）
- CAN 2.0A 調停（ID 優先・ビット単位の競合解決）
- 調停負けによる自動再送信
- 送信／受信バッファ管理（満杯時の OVERWRITE 検出）
- ノード起動制御（電源投入 → 初期化 → バス同期検出 → 通常動作）
- 再生スピード調整（×1/1024 ～ ×1024）
- 解析機能：受信完了時間・バス負荷・送信待ち時間・送信ジッター

---

## 機能一覧

| # | 機能 |
|---|------|
| 001 | シミュレーションコア（DES エンジン・調停・自動再送信） |
| 002 | ノード起動制御（バス同期検出） |
| 003 | CAN バス構成図表示（最大 16 ECU） |
| 004 | メニュー駆動のプロジェクト設定（.canprj 保存・読込み） |
| 005 | 再生時計（スピード調整付き） |
| 006 | フレーム定義エディタ（ECU ごと・最大 100 フレーム） |
| 007〜016 | UI 改善（ハイライト・フレーム表示・フロアニメーション等） |
| 017〜024 | ツールバー統合・ECU ラベル・保存/読込みボタン・起動引数 |
| 025〜027 | ログビューア・起動記録 CSV・ログクリア |
| 028 | 解析：受信完了時間・CAN バス負荷グラフ |
| 029 | 解析：送信待ち時間・送信ジッターグラフ |

---

## 性能指標

16 ECU・500 kbps・600 秒シミュレーションでの実測値：

| 項目 | 実測値 |
|------|--------|
| 生成ログ行数 | 193 万行以上 |
| ログ書き込み時間（Release） | 0.9 秒 |
| ログ解析時間（Release） | 0.7 秒 |
| バス使用率（実車デモ構成） | 約 3〜19% |
| 再送フレーム数（16 ECU・5 秒） | 990 フレーム |

---

## 開発手法

**Spec-Driven Development (SDD)** を Claude Code + Spec Kit で実践しました。

```
/speckit.specify   → 自然言語の要望から spec.md を作成
/speckit.plan      → 実装計画（plan.md）を生成
/speckit.tasks     → 検証手順付きタスクリスト（tasks.md）を生成
/speckit.implement → タスクに基づいて実装を実行
/speckit.converge  → 全仕様の充足を確認
```

全 29 機能をこのワークフローで開発しました。  
各 `tasks.md` にはタスクごとの検証結果がインラインで記録されています：

```
T007 … 結果: 合格（8 ファイル、警告 0・エラー 0）
T013 … 結果: 合格（フォルダ選択ダイアログのスクリーンショットで確認）
```

プロジェクト憲章（`.specify/memory/constitution.md` v1.3.0）が開発原則を統制しています：  
CAN 2.0A 準拠・最大 16 ノード・フレーム単位 DES・UI とロジックの分離（`CanSim/` は MFC 非依存の純粋な C++）。

---

## ビルド

Visual Studio 2022・MFC 動的リンク・Unicode。

```powershell
# Debug x64
msbuild MFCApplication1\MFCApplication1.sln /p:Configuration=Debug /p:Platform=x64

# Release x64
msbuild MFCApplication1\MFCApplication1.sln /p:Configuration=Release /p:Platform=x64

# Win32（ソリューションレベルの指定名は "Win32" ではなく "x86"）
msbuild MFCApplication1\MFCApplication1.sln /p:Configuration=Debug /p:Platform=x86
```

Debug/Release × Win32/x64 の全 4 構成でクリーンビルドが通ることを維持しています。

---

## 検証ツール

```bat
REM ビルド
cl /nologo /std:c++14 /EHsc /I tools\cansim_runner /I MFCApplication1\CanSim ^
   /Fo"%TEMP%\cansim\\" /Fe"%TEMP%\cansim\checks.exe" ^
   tools\cansim_runner\checks.cpp MFCApplication1\CanSim\*.cpp

REM 成功基準を一括検証（終了コード 0 で全パス）
"%TEMP%\cansim\checks.exe" MFCApplication1\CanSim\SampleConfigs "%TEMP%\cansim"
```

機能 001〜011・014〜015・017〜020・022〜029 の非 GUI 基準を自動検証します。

---

## アーキテクチャ

```
MFCApplication1/
├── CanSim/              # コアロジック — MFC 非依存の純粋な C++
│   ├── SimulationEngine # DES メインループ
│   ├── Network          # 調停・自動再送信・受信フィルタ
│   ├── EventQueue       # 優先度付きキュー（時刻 + ノード順）
│   ├── FrameTiming      # CAN ビットスタッフィング・CRC-15・フレーム伝送時間
│   ├── LogAnalysis      # 実行後解析（レイテンシ・負荷・ジッター）
│   ├── ReplayClock      # 再生スピード制御（QPC ベース）
│   └── SampleConfigs/   # サンプル .canprj（実車イメージ 2/4/8/16 ECU）
├── MFCApplication1Dlg   # メインダイアログ（薄いグルーコードのみ）
├── FrameEditDlg         # フレーム定義エディタ（モードレス）
├── AnalysisDlg          # 解析ウィンドウ（4 種）
└── LogViewDlg           # シミュレーションログビューア
specs/
├── 001-can-simulation-core/
├── 002-node-startup/
│   ...
└── 029-tx-wait-jitter/
tools/
└── cansim_runner/       # CLI ランナー + バッチ検証ツール
```

---

## 技術スタック

| 項目 | 内容 |
|------|------|
| 言語 | C++14 |
| UI フレームワーク | MFC（Microsoft Foundation Classes） |
| ビルド | MSBuild / Visual Studio 2022 |
| 対応プラットフォーム | Windows（x86 / x64） |
| 開発手法 | Spec-Driven Development（SDD）with Claude Code + Spec Kit |

---

## 作者

組み込みシステムエンジニア（SH・V850・R3000 等の車載・産業機器分野での実務経験 20 年以上）。  
CAN バスの深い知識と SDD による仕様駆動開発の実践を示すポートフォリオプロジェクトとして開発。

---

## メニュー構造

### ファイル(F)

| サブメニュー | ショートカット | 機能概要 |
|-------------|:--------------:|----------|
| プロジェクト読込み | O | .canprj ファイルを開く |
| プロジェクト保存 | S | 現在の設定を .canprj に保存 |

### 編集(E)

| サブメニュー | ショートカット | 機能概要 |
|-------------|:--------------:|----------|
| セッティング | T | ECU 数・通信速度等のプロジェクト設定 |
| データフレーム編集 | F | 各 ECU の CAN フレーム定義を編集 |

### シミュレーション(S)

| サブメニュー | ショートカット | 機能概要 |
|-------------|:--------------:|----------|
| 開始 | R | シミュレーション開始 |
| 中断 | P | 一時停止（再開可能） |
| 終了 | X | シミュレーション終了 |

### 解析(A)

| サブメニュー | ショートカット | 機能概要 |
|-------------|:--------------:|----------|
| 送信待ち時間 | W | アービトレーション待ち時間チャート |
| 受信完了時間 | R | Tx→Rx レイテンシチャート |
| 送信ジッター | J | 送信間隔のばらつきチャート |
| CAN バス負荷 | B | バス占有率チャート |

### ログ表示(L)

| サブメニュー | ショートカット | 機能概要 |
|-------------|:--------------:|----------|
| シミュレーションログ | L | 全フレームのログビューア |
| ログクリア | C | ログを消去 |
