# Data Model: 009

## LogLine（`CanSim/LogLines.h`）
| 項目 | 型 | 内容 |
|---|---|---|
| TimeUs | long long | シミュレーション開始からの時刻（µs） |
| Kind | enum LogLineKind { State, Frame } | 状態遷移／フレーム |
| NodeId | int | State: 遷移したノード。Frame: 送信ノード |
| Index | int | State: 遷移後の `StartupState`（整数）。Frame: `rows` の添字 |

## 関数
`std::vector<LogLine> BuildLogLines(const std::vector<LogRow>& rows, const Network& endState, long long durationUs)`
- 並び順: TimeUs 昇順 → 状態遷移が先 → NodeId 昇順 → 元の順（安定）。
- 状態遷移: ノードごとに最大 3 行（電源投入、初期化完了、通常動作）。`durationUs` を超えるものと、未到達（-1）は含めない。

## ダイアログの状態（UI）
| 項目 | 内容 |
|---|---|
| m_logLines | 実行 1 回ぶんの全行（時刻順） |
| m_logShown | 表示済みの本数（先頭から） |
| m_logTop | 表示エリアの先頭に見えている行の添字 |
| m_logRows | 行の文字を作るための実行結果の `LogRow` 群（既存の `m_runRows`） |
