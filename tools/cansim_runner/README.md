# cansim_runner — GUI を介さない検証ツール

`MFCApplication1/CanSim/` のシミュレーションコアは MFC に依存しないプレーン C++ なので、MFC
アプリをビルド・操作しなくても `cl.exe` だけでコンパイル・実行できます。ここに置いてあるのは、
その性質を利用した2つの小さなコマンドラインツールです。

- `runner.exe` — 設定ファイルを1つ実行し、`.log.csv` / `.summary.csv` / `.startup.csv` を出力する
  （`MFCApplication1Dlg` の実行ハンドラと同じ処理）
- `checks.exe` — 機能 001・002 の成功基準（SC-001〜SC-006 ほか）を自動で検証する

いずれも **正式なテストプロジェクトではありません**。リポジトリのビルド（`MFCApplication1.sln`）
には含まれず、必要なときに手動でビルドして使う開発補助ツールです。

## ビルド

「x64 Native Tools Command Prompt for VS 2022」、または `vcvars64.bat` を読み込んだ環境で、
リポジトリのルートから実行します。出力先はリポジトリ外（一時ディレクトリ）を推奨します。

```bat
set OUT=%TEMP%\cansim_tools
mkdir "%OUT%" 2>nul

cl /nologo /std:c++14 /EHsc /I tools\cansim_runner /I MFCApplication1\CanSim ^
   /Fo"%OUT%\\" /Fe"%OUT%\runner.exe" ^
   tools\cansim_runner\runner.cpp MFCApplication1\CanSim\*.cpp

cl /nologo /std:c++14 /EHsc /I tools\cansim_runner /I MFCApplication1\CanSim ^
   /Fo"%OUT%\\" /Fe"%OUT%\checks.exe" ^
   tools\cansim_runner\checks.cpp MFCApplication1\CanSim\*.cpp
```

`/I tools\cansim_runner` を先に指定するのが重要です。`CanSim/*.cpp` は `#include "pch.h"` を
持っており、ここの空の `pch.h` がそれを満たします（＝コアが MFC に依存していないことの確認にも
なっています）。

## 実行

```bat
REM 1回のシミュレーション（設定ファイル、秒数、出力パスの接頭辞）
"%OUT%\runner.exe" MFCApplication1\CanSim\SampleConfigs\sample_startup.cfg 1 "%OUT%\startup"

REM 成功基準の一括検証（サンプル構成ディレクトリ、一時出力ディレクトリ）
"%OUT%\checks.exe" MFCApplication1\CanSim\SampleConfigs "%OUT%"
```

`checks.exe` はすべてのチェックに通れば終了コード 0 を返します。期待値の根拠は各機能の
`quickstart.md`（`specs/001-can-simulation-core/`、`specs/002-node-startup-control/`）を参照
してください。
