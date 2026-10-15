// feature_008_edge_tests.cpp（tests/unit）
// 機能 008 の補足のテスト（/coverage）。受け入れシナリオ・成功基準とは別に、spec.md の Edge Cases に根拠がある動きを確かめる。

#include "support/spec_test_helpers.h"

#include "FrameEvents.h"

using namespace CanSim;
using namespace spectest;

// 根拠: spec.md Edge Cases（フレーム表示では、送信側で捨てられた要求は表示しない）。フレーム表示の元のイベント（FrameEvents.h）に、
// 送信側の OVERWRITE は入らない。同時にそろう 20 フレームの最初の 50 ms では、送信された 16 フレームだけがイベントになる。
TEST(F008Edge, F008_EDGE_DroppedRequests_NotInFrameEvents)
{
    RecordProperty("spec", "008:Edge");
    Network net;
    net.BitRateBps = 500000;
    std::vector<FrameDefinition> frames;
    for (unsigned i = 0; i < 20; ++i)
        frames.push_back(MakeFrame(0x100 + i, 8, 100000));
    net.Nodes.push_back(MakeNode(1, frames));
    net.Nodes.push_back(MakeNode(2));
    const std::vector<LogRow> rows = RunSim(net, 50000LL);
    int dropped = 0;
    for (const LogRow& r : rows)
        if (r.Status == kStatusOverwrite)
            ++dropped;
    ASSERT_EQ(dropped, 4) << "前提: 捨てられた要求が 4 件でない";
    const std::vector<FrameEvent> events = BuildFrameEvents(rows);
    EXPECT_EQ(events.size(), 16u) << "送信されたフレームだけのイベントになっていない";
    for (const FrameEvent& e : events)
        EXPECT_LT(e.CanId, 0x110u) << "捨てられた要求（ID 0x110 以上）がイベントになっている";
}
