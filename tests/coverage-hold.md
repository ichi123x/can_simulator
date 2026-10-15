# カバレッジの保留

テストで実行されていない行のうち、人が「テストしない」と決めた行の一覧。
`.claude/scripts/coverage.ps1` は、ここに載っている行を「保留」として赤い行から分けて表示する。
行番号はソースの変更でずれるので、ファイルとコード（前後の空白を除いた 1 行）で照合する。
コードに `|` があるときは `\|` と書く。同じコードの行が同じファイルに複数あると、すべて保留になる。

追加するのは、人が決めたときだけ（/coverage は候補を出すだけで、自分では追加しない）。

| ファイル | コード | 理由 | 決めた日 |
|---|---|---|---|
| MFCApplication1/CanSim/LogFileIndex.cpp | } | 読み取りの失敗（in.bad()）の分岐の閉じ。ディスクの故障など OS 側の失敗で、テストで起こせない | 2026-10-15 |
| MFCApplication1/CanSim/LogFileIndex.cpp | return false; | 索引を作る途中の読み取りの失敗。OS 側の失敗で、テストで起こせない | 2026-10-15 |
| MFCApplication1/CanSim/LogFileIndex.cpp | maxLength = length > 0x7FFFFFFF ? 0x7FFFFFFF : static_cast<int>(length); | 改行で終わらない最後の行が 2^31 文字を超えるときの頭打ち。現実のログで起きない大きさ | 2026-10-15 |
| MFCApplication1/CanSim/LogFileIndex.cpp | return out; | 読み出しの途中で索引の範囲外・シークの失敗。索引を作った後にファイルが変わる OS 側の事情で、テストで起こす意味が薄い | 2026-10-15 |
| MFCApplication1/CanSim/FlowEffect.cpp | return nullptr; | 構成図に無い ECU の番号。呼び出し側（強調の対象の組）は構成図の ECU しか渡さない | 2026-10-15 |
| MFCApplication1/CanSim/FlowEffect.cpp | elapsedUs = 0; | 負のエフェクトの時計。呼び出し側は 0 以上の時間しか渡さない | 2026-10-15 |
| MFCApplication1/CanSim/FlowEffect.cpp | continue; | 構成図に無い ECU・送信と受信が同じ ECU の組・長さ 0 の道筋。呼び出し側がこの組を渡さない | 2026-10-15 |
| MFCApplication1/CanSim/ReplayClock.cpp | continue; | 先頭の位置より後で、すでに終わったフレーム。イベントは時刻順で先頭の位置を先に進めているので来ない | 2026-10-15 |
| MFCApplication1/CanSim/LogAnalysis.cpp | negative = *b == '-'; | 負の数の読み取り。ログは負の時刻を書かない（空欄にする） | 2026-10-15 |
| MFCApplication1/CanSim/LogAnalysis.cpp | ++b; | 符号の読み飛ばし。ログは符号付きの数を書かない | 2026-10-15 |
| MFCApplication1/CanSim/LogAnalysis.cpp | return false; | 符号だけの欄（ログはこの形を出さない）・読み取りの失敗（in.bad()、OS 側の失敗）・2 つ目以降の 4 MB のまとまりでの行の誤り（行の誤りの処理は 1 つ目のまとまりで確認済み） | 2026-10-15 |
| MFCApplication1/CanSim/LogAnalysis.cpp | error = "cannot read the file"; | 読み取りの失敗（in.bad()）。ディスクの故障など OS 側の失敗で、テストで起こせない | 2026-10-15 |
| MFCApplication1/CanSim/LogAnalysis.cpp | } | 4 MB ごとの進み具合の呼び出しの分岐の閉じ。中断の分岐自体は確認済み | 2026-10-15 |
| MFCApplication1/CanSim/LogAnalysis.cpp | continue; | OVERWRITE でないのに時刻の無い行。ログはこの形を出さない | 2026-10-15 |
| MFCApplication1/CanSim/StartupControl.cpp | return false; | 起動記録の CSV を開けない。OS 側の失敗 | 2026-10-15 |
| MFCApplication1/CanSim/Logger.cpp | out.push_back('-'); | 負の数の符号。渡す時刻・数は負にならない（負の時刻は空欄にしてから来る） | 2026-10-15 |
| MFCApplication1/CanSim/Logger.cpp | return false; | ログの CSV を開けない。OS 側の失敗 | 2026-10-15 |
| MFCApplication1/CanSim/MetricsAggregator.cpp | return false; | 集計の CSV を開けない。OS 側の失敗 | 2026-10-15 |
| MFCApplication1/CanSim/BusDiagramLayout.cpp | r.Bottom = frame.Top; | ラベルの下端を枠の上端に合わせる守り。96・120・144 dpi では起きない | 2026-10-15 |
| MFCApplication1/CanSim/SimControls.cpp | return false; | 範囲外のボタン番号。呼び出し側は 0〜4 しか渡さない | 2026-10-15 |
| MFCApplication1/CanSim/SimControls.cpp | baseUs = 0; | 負の中断時刻。呼び出し側は 0 以上の時刻しか渡さない | 2026-10-15 |
| MFCApplication1/CanSim/SimulationEngine.cpp | return; | 2 度目の電源投入・初期化完了のイベントを捨てる守り。同じイベントは 1 度しか積まない | 2026-10-15 |
| MFCApplication1/CanSim/ConfigLoader.cpp | } | catch の閉じ（実行される文が無い行） | 2026-10-15 |
