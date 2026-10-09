// 機能 009（ログ表示エリア）の仕様テスト
// 根拠: specs/009-log-view/spec.md、LogLines.h のコメント。仕様に ID が無いので、
// 受け入れシナリオは US<ストーリー番号>-<シナリオ番号> と呼ぶ。
// 描画・スクロール・追従は画面側なのでテストせず、表示する行の組み立て（BuildLogLines）を確かめる。

#include <gtest/gtest.h>

#include <vector>

#include "LogLines.h"
#include "support/test_helpers.h"

using namespace CanSim;
using namespace TestHelpers;

// ---------------------------------------------------------------------------
// SC-002 / US1-2 / FR-003: 電源投入をずらした 6 ECU では、各 ECU の電源投入・初期化完了・通常動作の 3 つ、
//   計 18 行の状態遷移が時刻の順に出る。
// 根拠: spec.md SC-002、US1 シナリオ 2、FR-003、LogLines.h（Initializing / BusSyncDetecting / Normal）。
//   ECU k（1〜6）: 電源投入 (k−1)×10 ms、初期化 1 ms -> 1000 + (k−1)×10000 で検出、+22 us で通常動作
// ---------------------------------------------------------------------------
TEST(F009Spec, F009_SC002_SixEcus_EighteenStateLinesInOrder)
{
    RecordProperty("spec", "009:SC-002");
    RecordProperty("spec", "009:US1-2");
    Network net;
    net.BitRateBps = 500000;
    for (int k = 1; k <= 6; ++k)
    {
        Node n = MakeNode(k, {});
        n.PowerOnTimeUs = (k - 1) * 10000LL;
        n.InitDurationUs = 1000;
        net.Nodes.push_back(n);
    }
    const RunResult run = RunSim(net, 100000LL);
    const std::vector<LogLine> lines = BuildLogLines(run.Rows, run.After, 100000LL);
    ASSERT_EQ(18u, lines.size()) << "6 ECU × 3 つの状態遷移";
    for (int k = 1; k <= 6; ++k)
    {
        const long long on = (k - 1) * 10000LL;
        const LogLine& a = lines[(k - 1) * 3];
        const LogLine& b = lines[(k - 1) * 3 + 1];
        const LogLine& c = lines[(k - 1) * 3 + 2];
        EXPECT_EQ(k, a.NodeId);
        EXPECT_EQ(LogLineKind::State, a.Kind);
        EXPECT_EQ(static_cast<int>(StartupState::Initializing), a.Index) << "電源投入";
        EXPECT_EQ(on, a.TimeUs);
        EXPECT_EQ(static_cast<int>(StartupState::BusSyncDetecting), b.Index) << "初期化完了";
        EXPECT_EQ(on + 1000, b.TimeUs);
        EXPECT_EQ(static_cast<int>(StartupState::Normal), c.Index) << "通常動作";
        EXPECT_EQ(on + 1022, c.TimeUs);
    }
}

// ---------------------------------------------------------------------------
// FR-004: フレームログの各行（OVERWRITE を含む）を 1 行ずつ出し、Index でその行を指す。
// FR-005 / Edge Cases: 時刻の昇順。同じ時刻は状態遷移が先、次にノード番号の小さい順。
// Edge Cases: シミュレーション時間より後の状態遷移・未到達は出さない。
// 根拠: spec.md FR-004、FR-005、Edge Cases（同じ時刻・電源投入前に終わる ECU）、LogLines.h の並びの決まり。
// ---------------------------------------------------------------------------
TEST(F009Spec, F009_FR005_SameTime_StateFirstThenNodeId)
{
    RecordProperty("spec", "009:FR-005");
    RecordProperty("spec", "009:FR-004");
    // 実行後の状態: ECU 1・2 は 0 で電源投入して 22 us で通常動作、ECU 3 は 50 us に電源投入して初期化中のまま、
    // ECU 4 はシミュレーション時間（1000 us）より後に電源投入（未到達）
    Network end;
    for (int k = 1; k <= 4; ++k) end.Nodes.push_back(MakeNode(k, {}));
    end.Nodes[0].Startup = end.Nodes[1].Startup = StartupState::Normal;
    end.Nodes[0].InitDoneUs = end.Nodes[1].InitDoneUs = 0;
    end.Nodes[0].NormalEntryUs = end.Nodes[1].NormalEntryUs = 22;
    end.Nodes[2].PowerOnTimeUs = 50;
    end.Nodes[2].Startup = StartupState::Initializing;
    end.Nodes[3].PowerOnTimeUs = 5000;

    std::vector<LogRow> rows(3);
    rows[0].TimestampUs = 50;  rows[0].TxNodeId = 2; rows[0].Status = "SUCCESS";
    rows[1].TimestampUs = 50;  rows[1].TxNodeId = 1; rows[1].Status = "SUCCESS";
    rows[2].TimestampUs = 40;  rows[2].TxNodeId = 1; rows[2].Status = "OVERWRITE";

    const std::vector<LogLine> lines = BuildLogLines(rows, end, 1000LL);
    // 期待: 0 のノード 1（電源投入・初期化完了）、0 のノード 2（同）、22 の通常動作（1, 2）、40 のフレーム、
    //       50 の ECU 3 電源投入（状態が先）、50 のフレーム（ノード 1、次にノード 2）
    std::vector<std::pair<long long, int>> got;
    for (const LogLine& l : lines) got.push_back({l.TimeUs, l.Kind == LogLineKind::State ? l.NodeId : 100 + l.Index});
    const std::vector<std::pair<long long, int>> expected = {
        {0, 1}, {0, 1}, {0, 2}, {0, 2}, {22, 1}, {22, 2}, {40, 102}, {50, 3}, {50, 101}, {50, 100}};
    EXPECT_EQ(expected, got) << "(時刻, 状態ならノード番号／フレームなら 100 + 行の番号)";
    for (const LogLine& l : lines) EXPECT_NE(4, l.NodeId) << "シミュレーション時間より後の ECU 4 は出さない";
}
