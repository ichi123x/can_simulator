// feature_009_edge_tests.cpp（tests/unit）
// 機能 009 の補足のテスト（/coverage）。受け入れシナリオ・成功基準とは別に、spec.md の FR と LogLines.h の約束に根拠がある動きを確かめる。

#include "support/spec_test_helpers.h"

#include "LogLines.h"

using namespace CanSim;
using namespace spectest;

// 根拠: spec.md FR-005、LogLines.h（同じ時刻では状態遷移が先）。ノード 3 の電源投入（268 µs）と、ノード 1 の 0x100 の受信の行
// （22〜268 µs に送信、受信処理時刻 268 µs）が同じ時刻のとき、ノード番号の大きいノード 3 の状態遷移の行が先に来る。
TEST(F009Edge, F009_FR005_SameTime_StateBeforeFrame)
{
    RecordProperty("spec", "009:FR-005");
    Network net;
    net.BitRateBps = 500000;
    net.Nodes.push_back(MakeNode(1, { MakeFrame(0x100, 8, 100000) }));
    net.Nodes.push_back(MakeNode(2));
    Node late = MakeNode(3);
    late.PowerOnTimeUs = 268;
    net.Nodes.push_back(late);
    Network after;
    const std::vector<LogRow> rows = RunSim(net, 1000LL, &after);
    const std::vector<LogLine> lines = BuildLogLines(rows, after, 1000LL);
    int stateAt268 = -1, frameAt268 = -1;
    for (std::size_t i = 0; i < lines.size(); ++i)
    {
        if (lines[i].TimeUs != 268)
            continue;
        if (lines[i].Kind == LogLineKind::State && stateAt268 < 0)
            stateAt268 = static_cast<int>(i);
        if (lines[i].Kind == LogLineKind::Frame && frameAt268 < 0)
            frameAt268 = static_cast<int>(i);
    }
    ASSERT_GE(stateAt268, 0) << "前提: 268 µs の状態遷移の行が無い";
    ASSERT_GE(frameAt268, 0) << "前提: 268 µs のフレームの行が無い";
    EXPECT_LT(stateAt268, frameAt268) << "同じ時刻でフレームの行が状態遷移の行より先";
}
