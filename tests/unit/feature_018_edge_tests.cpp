// feature_018_edge_tests.cpp（tests/unit）
// 機能 018 の補足のテスト（/coverage）。spec.md の改訂 1（道筋を少し残す）と FlowEffect.h の約束に根拠がある動きを確かめる。

#include "support/spec_test_helpers.h"

#include "BusDiagramLayout.h"
#include "FlowEffect.h"

#include <cmath>

using namespace CanSim;
using namespace spectest;

// 根拠: FlowEffect.h（FlowPointAt は f を 0〜1 に収める）。道筋の外の割合を渡しても、道筋の始まり・終わりの点になる。
TEST(F018Edge, F018_FR001_FlowPointAt_ClampedToPath)
{
    RecordProperty("spec", "018:FR-001");
    const BusDiagramLayout l = ComputeBusDiagramLayout(4);
    const EcuPlacement& s = l.Ecus[0];
    const EcuPlacement& r = l.Ecus[3];
    const FlowPoint start = FlowPointAt(s, r, 0.0);
    const FlowPoint end = FlowPointAt(s, r, 1.0);
    const FlowPoint before = FlowPointAt(s, r, -0.5);
    const FlowPoint after = FlowPointAt(s, r, 1.7);
    EXPECT_DOUBLE_EQ(before.X, start.X) << "0 より小さい割合が始まりの点にならない";
    EXPECT_DOUBLE_EQ(before.Y, start.Y);
    EXPECT_DOUBLE_EQ(after.X, end.X) << "1 より大きい割合が終わりの点にならない";
    EXPECT_DOUBLE_EQ(after.Y, end.Y);
}

// 根拠: spec.md 改訂 1 の D（一度出た「送信 → 受信」の道筋は、最後に見えてから 0.3 秒残す。0.3 秒たつと消える）、FlowEffect.h の
// FlowPathHold、改訂 1（新しい実行の開始で、残っている道筋は消す）。
TEST(F018Edge, F018_REV1_PathHold_KeptFor300msThenGone)
{
    RecordProperty("spec", "018:REV1-D");
    FlowPathHold hold;
    hold.Update({ { 1, 2 }, { 1, 3 } }, 1000000);
    hold.Update({ { 1, 3 } }, 1100000); // (1,3) は 1.1 秒にもう一度見えた
    EXPECT_EQ(hold.Current(1299999), (std::vector<std::pair<int, int>>{ { 1, 2 }, { 1, 3 } })) << "0.3 秒より前に消えた";
    EXPECT_EQ(hold.Current(1300001), (std::vector<std::pair<int, int>>{ { 1, 3 } })) << "(1,2) が最後に見えてから 0.3 秒を過ぎても残る";
    EXPECT_TRUE(hold.Current(1400001).empty()) << "(1,3) が最後に見えてから 0.3 秒を過ぎても残る";
    hold.Update({ { 2, 1 } }, 2000000);
    hold.Clear();
    EXPECT_TRUE(hold.Current(2000000).empty()) << "新しい実行の開始（Clear）で道筋が消えない";
}
