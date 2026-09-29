# cansim_runner — GUI を介さない検証ツール

`MFCApplication1/CanSim/` のシミュレーションコアは MFC に依存しないプレーン C++ なので、MFC
アプリをビルド・操作しなくても `cl.exe` だけでコンパイル・実行できます。ここに置いてあるのは、
その性質を利用した2つの小さなコマンドラインツールです。

- `runner.exe` — プロジェクトファイル（`.canprj`）を1つ実行し、`.log.csv` / `.summary.csv` / `.startup.csv` を
  出力する（`MFCApplication1Dlg` の「シミュレーション → 開始」と同じ処理。ECU 数はプロジェクトの `EcuCount`）
- `checks.exe` — 機能 001〜004 の成功基準を自動で検証する

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
REM 1回のシミュレーション（プロジェクト、出力パスの接頭辞、[秒数: 省略時はプロジェクトの DurationSec]）
"%OUT%\runner.exe" MFCApplication1\CanSim\SampleConfigs\sample_startup.canprj "%OUT%\startup" 1

REM 成功基準の一括検証（サンプル構成ディレクトリ、一時出力ディレクトリ、[基準 CSV のディレクトリ]）
"%OUT%\checks.exe" MFCApplication1\CanSim\SampleConfigs "%OUT%"
```

`checks.exe` はすべてのチェックに通れば終了コード 0 を返します。期待値の根拠は各機能の
`quickstart.md`（`specs/001-can-simulation-core/` 〜 `specs/004-menu-project-settings/`）を参照
してください。

第3引数に基準 CSV のディレクトリを渡すと、移行後のサンプル（`sample_*.canprj`）の結果が機能 004 以前
の結果と一致すること（004 SC-009）も検証します。基準ディレクトリには `sample_<名前>.log.csv` /
`.summary.csv` / `.startup.csv`（機能 004 着手前のビルドで各サンプルを 10 秒実行したもの）を置きます。
