# cansim_runner — GUI を介さない検証ツール

`MFCApplication1/CanSim/` のシミュレーションコアは MFC に依存しないプレーン C++ なので、MFC
アプリをビルド・操作しなくても `cl.exe` だけでコンパイル・実行できます。ここに置いてあるのは、
その性質を利用した2つの小さなコマンドラインツールです。

- `runner.exe` — プロジェクトファイル（`.canprj`）を1つ実行し、`.log.csv` / `.summary.csv` / `.startup.csv` を
  出力する（`MFCApplication1Dlg` の「シミュレーション → 開始」と同じ処理。ECU 数はプロジェクトの `EcuCount`）
- `checks.exe` — 機能 001〜011・014・015・017〜020・022〜028 の成功基準（GUI 固有のものを除く）を自動で検証する

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
`quickstart.md`（`specs/001-can-simulation-core/` 〜 `specs/028-analysis-latency-busload/`）を参照
してください。機能 014 の項目は、開始・中断・終了の有効／無効の表（`GetSimCommandStates`）、ツールバーのボタンの
範囲と当たり判定（DPI 96/120/144）、再開後の時刻（`ResumedSimTimeUs`）です。機能 015 の項目は、スピードスライダーの目盛りの位置・数字の重なり・つまみの位置（DPI 96/120/144）、一番近い目盛りへの丸め（`SpeedAtX`）、当たり判定（`IsOnSpeedSlider`）、スピード変更時の動作（`GetSpeedChangeAction`）です。機能 017 の項目は、ツールバーの並び（ボタン・チェックボックス・スライダー・シミュレーション時間が重ならない、DPI 96/120/144）、チェックボックスの当たり判定（`IsOnFrameDisplayToggle`）、時刻の 3 けた区切り（`FormatSimTimeUs`）です。機能 018 の項目（改訂 1）は、流れのエフェクトの道筋の端点・通る場所・進む向き（`FlowPointAt`）、粒の間隔 16 px と速さ 150 px/秒・数（`FlowOffset` / `ComputeFlowEffect`）、受信しない ECU を通らないこと・送り合う 2 つの ECU でも粒が出ること（`ActiveFrames::Flows`）、光の強さと受信の半周期の遅れ、道筋を 0.3 秒残すこと（`FlowPathHold`）、最悪の場合（送信 8 × 受信 8）の計算時間です。機能 019 の項目は、ボタンとスライダーの半分の大きさ、表示するスライダーの数字（`IsSpeedLabelShown`）、「スピード：」の位置です（並びと数字の重なりは 015・017 の項目も 019 の並びで検証します）。機能 020 の項目は、「イベント＆ログ表示」のラベルが構成図とログの枠の間にあり、枠の左端に揃うことです。機能 022 の項目は、ECUラベル名の文字数・切り詰め（10 文字 + ...）・空白の除去・32 文字の制限、`Label=` の保存と読込みの往復、高さ 48 の ECU の箱が枠の内側にあることです。機能 023 の項目は、保存・読込み・開始・中断・終了の 5 つのボタンの位置（96 dpi で x 8・36・92・120・148）と並び、読込みと開始の間のボタン 1 つ分の空き、当たり判定（`FindToolbarButtonAt`。空きの上は -1）、`SimButtonRect` との対応、有効／無効（`IsToolbarButtonEnabled`）、移動後のスライダーがウィンドウの幅に収まること、改訂 1 のチェックボックスの縦の位置（終了ボタンの箱と中心が揃う）です。機能 024 の項目は、タイトルの組み立て（`ComposeWindowTitle`）、起動時の引数の選び方（`FindStartupProjectArg`）、拡張子の判定（`HasProjectExtension`）です。機能 025 の項目は、ログ表示の行の索引（`LogFileIndex`）で、空ファイル・存在しないファイル、LF／CRLF／最終行の改行なしで同じ行と最長行になること、256 行ごとの索引の境目と両端での切り詰め、索引の境目ちょうどで終わるファイル、sample_16ecu の実際のフレームログの行数と先頭・最終行、100 万行の索引の作成（所要時間を表示）です。機能 026 の項目は、スピードの範囲 -10〜+10（目盛り 21 個、-10 は x 604・+10 は x 864、`SpeedAtX` は両端で止まる）、+10 の数字と「スピード：」が帯に収まること、以前のファイルの `Speed=-13`〜`-11` が -10 として読めること、-14・11 が範囲 -10〜10 を示すエラーになることです（005・015・019 のスピードの項目も新しい範囲で検証します）。機能 027 の項目は、結果ファイルの 3 つのパスとその順（`ResultFilePaths`）、通常のファイルだけを「ある」とすること（同じ名前のフォルダーは含めない、`ExistingResultFiles`）、読み取り専用のファイルは削除に失敗として報告し変えないこと、プロジェクトファイルが残ること（`DeleteResultFiles`。一時フォルダーに作ったファイルで検証）です。機能 028 の項目は、ログの読み込み（列名で探す・空欄・壊れた行と無いファイルの行番号付きのエラー）、受信完了時間の表の並び・件数・最小・平均・最大・P95（最近順位法）の手計算との一致、ヒストグラムのビン（きりの良い幅・10〜50 本・度数の合計）、バス負荷（受信 ECU の数だけ重複する行を 1 回の送信にまとめる・10 ms の境で分ける・平均 = 使用時間 ÷ 範囲・ECU ごとの合計 = 平均）、`NiceTicks` / `MaxPerColumn`、16 ECU の車両サンプル 5 秒での件数・度数・負荷の合計の一致です。

第3引数に基準 CSV のディレクトリを渡すと、サンプル（`sample_*.canprj`）の結果が基準
の結果と一致すること（004 SC-009）も検証します。機能 006 でデータ（DLC 分の 0xFF）と送信要求の起点（通常動作開始時刻）が
変わり結果が変わったため、基準は 006 以降のビルドで作ったものを使ってください。基準ディレクトリには `sample_<名前>.log.csv` /
`.summary.csv` / `.startup.csv`（各サンプルを 10 秒実行したもの）を置きます。
