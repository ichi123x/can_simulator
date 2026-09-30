# Data Model: 006

## FrameDefinition（`CanSim/CanTypes.h`、既存に追加）
| 項目 | 型 | 検証 |
|---|---|---|
| Name | std::string（UTF-8） | 0〜32 文字（UI で UTF-16 文字数） |
| CanId | unsigned | 0x000〜0x7FF、プロジェクト内で一意 |
| Dlc | unsigned | 0〜8 |
| Data | vector<uint8_t> | 常に Dlc 個の 0xFF（利用者は指定しない） |
| PeriodUs | long long | 1,000〜3,600,000,000（UI では 1〜3,600,000 ms） |
| InitialOffsetUs | long long | 0 以上かつ PeriodUs 未満（UI では ms） |

定数: `kMaxFramesPerNode = 100`。通し番号はノードの `Frames` の添字 + 1（保存しない）。

## FrameEditor（`CanSim/FrameEditor.h`、ASCII のみ）
- `struct FrameInput { std::string Name; std::string IdText; std::string DlcText; std::string PeriodMsText; std::string OffsetMsText; int NameLength; }`
  （`NameLength` は UI が数えた文字数）
- `enum class FrameField { Name, Id, Dlc, Period, Offset }` と `enum class FrameProblem { Empty, NotANumber, OutOfRange, Duplicate, TooLong, OffsetNotBelowPeriod, ListFull }`
- `struct FrameIssue { FrameField Field; FrameProblem Problem; }`
- `std::vector<FrameIssue> ValidateFrame(const Network& defs, int nodeId, int editIndex /* -1 = add */, const FrameInput& in, FrameDefinition& out)`
  — 問題がなければ `out` に確定値（Data = 0xFF × Dlc）を入れる。
- `std::vector<std::uint8_t> MakeFrameData(unsigned dlc)`
- `Node& EnsureNode(Network& defs, int nodeId)` — なければ `ECU<n>` の既定ノードを NodeId 昇順の位置に作る。
- `void AddFrame(Network&, int nodeId, const FrameDefinition&)` / `void ReplaceFrame(Network&, int nodeId, int index, const FrameDefinition&)` / `void DeleteFrame(Network&, int nodeId, int index)`

## 送信要求のスケジュール（`SimulationEngine`）
- 旧: `Run` 開始時に全フレームの最初の TxRequest を `InitialOffsetUs` に積む。
- 新: `HandleSyncComplete` で、そのノードの全フレームの最初の TxRequest を `NormalEntryUs + InitialOffsetUs` に積む
  （シミュレーション時間を超えるものは積まない）。
