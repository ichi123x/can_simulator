# cansim_runner — GUI を介さない検証ツール

`MFCApplication1/CanSim/` のシミュレーションコアは MFC に依存しないプレーン C++ なので、MFC
アプリをビルド・操作しなくても `cl.exe` だけでコンパイル・実行できます。ここに置いてあるのは、
その性質を利用した2つの小さなコマンドラインツールです。

- `runner.exe` — プロジェクトファイル（`.canprj`）を1つ実行し、`.log.csv` / `.summary.csv` / `.startup.csv` を
  出力する（`MFCApplication1Dlg` の「シミュレーション → 開始」と同じ処理。ECU 数はプロジェクトの `EcuCount`）
- `checks.exe` — 機能 001〜011・014・015・017・018 の成功基準（GUI 固有のものを除く）を自動で検証する

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
`quickstart.md`（`specs/001-can-simulation-core/` 〜 `specs/018-frame-flow-animation/`）を参照
してください。機能 014 の項目は、開始・中断・終了の有効／無効の表（`GetSimCommandStates`）、ツールバーのボタンの
範囲と当たり判定（DPI 96/120/144）、再開後の時刻（`ResumedSimTimeUs`）です。機能 015 の項目は、スピードスライダーの目盛りの位置・数字の重なり・つまみの位置（DPI 96/120/144）、一番近い目盛りへの丸め（`SpeedAtX`）、当たり判定（`IsOnSpeedSlider`）、スピード変更時の動作（`GetSpeedChangeAction`）です。機能 017 の項目は、ツールバーの並び（ボタン・チェックボックス・スライダー・シミュレーション時間が重ならない、DPI 96/120/144）、チェックボックスの当たり判定（`IsOnFrameDisplayToggle`）、時刻の 3 けた区切り（`FormatSimTimeUs`）です。機能 018 の項目は、流れのエフェクトの位相（`FlowPhase`）、道筋の端点・通る場所・進む向き（`FlowPointAt`）、粒の数と受信しない ECU を通らないこと・送り合う 2 つの ECU でも粒が出ること（`ComputeFlowEffect`、`ActiveFrames::Flows`）、光の強さの範囲、最悪の場合（送信 8 × 受信 8）の計算時間です。

第3引数に基準 CSV のディレクトリを渡すと、サンプル（`sample_*.canprj`）の結果が基準
の結果と一致すること（004 SC-009）も検証します。機能 006 でデータ（DLC 分の 0xFF）と送信要求の起点（通常動作開始時刻）が
変わり結果が変わったため、基準は 006 以降のビルドで作ったものを使ってください。基準ディレクトリには `sample_<名前>.log.csv` /
`.summary.csv` / `.startup.csv`（各サンプルを 10 秒実行したもの）を置きます。
