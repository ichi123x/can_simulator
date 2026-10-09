# カバレッジの保留

テストで実行されていない行のうち、人が「テストしない」と決めた行の一覧。
`.claude/scripts/coverage.ps1` は、ここに載っている行を「保留」として赤い行から分けて表示する。
行番号はソースの変更でずれるので、ファイルとコード（前後の空白を除いた 1 行）で照合する。
コードに `|` があるときは `\|` と書く。同じコードの行が同じファイルに複数あると、すべて保留になる。

追加するのは、人が決めたときだけ（/coverage は候補を出すだけで、自分では追加しない）。

| ファイル | コード | 理由 | 決めた日 |
|---|---|---|---|
| MFCApplication1/CanSim/ConfigLoader.cpp | result.ErrorMessage = "Failed to open configuration file."; | ファイルを開けないという OS 側の失敗で、テストでは起こせない | 2026-10-09 |
| MFCApplication1/CanSim/LogAnalysis.cpp | error = "cannot read the file"; | 読み込みの途中の OS 側の失敗で、テストでは起こせない | 2026-10-09 |
| MFCApplication1/CanSim/LogAnalysis.cpp | return false; | 読み込みの失敗・符号だけの欄で返す。OS の失敗と、アプリが書くログでは起きない入力への守り（このファイルの同じコードの行はすべて保留になる。今の未カバーは読み込みの失敗と符号だけの欄の行だけ） | 2026-10-09 |
| MFCApplication1/CanSim/LogAnalysis.cpp | cells.push_back(std::string()); | 末尾が「,」の行の空の欄。アプリが書くログでは起きない | 2026-10-09 |
| MFCApplication1/CanSim/LogAnalysis.cpp | negative = *b == '-'; | ログの時刻は 0 以上で空欄は -1 として別に扱うので、符号付きの値は来ない | 2026-10-09 |
| MFCApplication1/CanSim/LogAnalysis.cpp | ++b; | 同上（符号の読み飛ばし） | 2026-10-09 |
| MFCApplication1/CanSim/LogFileIndex.cpp | return false; | ファイルの読み込み中の OS 側の失敗（in.bad()）で、テストでは起こせない | 2026-10-09 |
| MFCApplication1/CanSim/LogFileIndex.cpp | return out; | 索引の範囲外・開けない・位置を移せないときの守り。行数の検査の後なので通常は来ない（このファイルの同じコードの行はすべて保留になる） | 2026-10-09 |
| MFCApplication1/CanSim/StartupControl.cpp | return false; | 起動記録の CSV を書けないという OS 側の失敗 | 2026-10-09 |
| MFCApplication1/CanSim/MetricsAggregator.cpp | return 0.0; | 0 件の平均。送信の無いグループは作られないので来ない | 2026-10-09 |
| MFCApplication1/CanSim/MetricsAggregator.cpp | return false; | 集計の CSV を書けないという OS 側の失敗 | 2026-10-09 |
| MFCApplication1/CanSim/Logger.cpp | out.push_back('-'); | 負の数の符号。時刻は 0 以上で、空欄の -1 は別に扱うので来ない | 2026-10-09 |
| MFCApplication1/CanSim/Logger.cpp | file.write(out.data(), static_cast<std::streamsize>(out.size())); | 4 MB ごとの書き出し。内部のバッファの都合で、出力の内容は同じ（このファイルの同じコードの行はすべて保留になる。最後の書き出しはテストで通る） | 2026-10-09 |
| MFCApplication1/CanSim/Logger.cpp | out.clear(); | 同上 | 2026-10-09 |
| MFCApplication1/CanSim/FlowEffect.cpp | return nullptr; | 構成図に無い ECU。呼び出し側が構成図の ECU 数と一致させる約束 | 2026-10-09 |
| MFCApplication1/CanSim/FlowEffect.cpp | elapsedUs = 0; | 負の時刻。エフェクトの時計は 0 から進むので来ない | 2026-10-09 |
| MFCApplication1/CanSim/FlowEffect.cpp | continue; | 構成図に無い ECU・送信と受信が同じ ECU・長さ 0 の道筋。ActiveFrames は送信と受信を別の ECU にする約束（このファイルの同じコードの行はすべて保留になる） | 2026-10-09 |
| MFCApplication1/CanSim/SimControls.cpp | return false; | 5 つのボタンをすべて扱った switch の後で、来ない | 2026-10-09 |
| MFCApplication1/CanSim/SimControls.cpp | baseUs = 0; | 負の中断時刻。時計は 0 から進むので来ない | 2026-10-09 |
| MFCApplication1/CanSim/ReplayClock.cpp | continue; | 終わった送信を飛ばす。送信は重ならないので、先頭を進めた後には来ない | 2026-10-09 |
| MFCApplication1/CanSim/BusDiagramLayout.cpp | r.Bottom = frame.Top; | ラベルとログの枠の丸めの守り。DPI 96/120/144 では起きない | 2026-10-09 |
| MFCApplication1/CanSim/SimulationEngine.cpp | return; | 古い PowerOn/InitDone のイベントを捨てる。状態は前にしか進まず、同じノードに 2 回は来ない（このファイルの同じコードの行はすべて保留になる） | 2026-10-09 |
