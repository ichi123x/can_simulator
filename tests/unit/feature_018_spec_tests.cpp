// 機能 018（フレーム表示の流れのエフェクト、改訂 1）の仕様テスト
// 根拠: specs/018-frame-flow-animation/spec.md、FlowEffect.h のコメント。
// 描画・色・書き換えの回数は画面側なのでテストせず、粒の位置・光・道筋の保持を確かめる。

#include <gtest/gtest.h>

#include <utility>
#include <vector>

#include "FlowEffect.h"

using namespace CanSim;

namespace
{
    const EcuPlacement& Ecu(const BusDiagramLayout& l, int n) { return l.Ecus[static_cast<std::size_t>(n - 1)]; }
}

// ---------------------------------------------------------------------------
// FR-002 / FR-006: 粒の先頭の位置は実時間で連続して進む（150 px/秒、16 px おきのくり返し）。
// 根拠: spec.md FR-002、FR-006、FlowEffect.h（FlowOffset = elapsed × 150 / 1e6 を 16 で割った余り）。
// ---------------------------------------------------------------------------
TEST(F018Spec, F018_FR006_FlowOffset_ContinuousInRealTime)
{
    RecordProperty("spec", "018:FR-006");
    EXPECT_DOUBLE_EQ(0.0, FlowOffset(0));
    EXPECT_DOUBLE_EQ(15.0, FlowOffset(100000)) << "0.1 秒で 15 px";
    EXPECT_DOUBLE_EQ(14.0, FlowOffset(200000)) << "0.2 秒で 30 px -> 16 で割った余り 14";
    EXPECT_NEAR(1.5, FlowOffset(10000), 1e-9) << "0.01 秒で 1.5 px";
}

// ---------------------------------------------------------------------------
// FR-001: 道筋は送信 ECU の箱の縁 -> 接続線 -> バス -> 受信 ECU の接続線 -> 箱の縁。
// 根拠: spec.md FR-001、FlowEffect.h（FlowPathLength・FlowPointAt、f は 0〜1 に抑える）。
//   ECU 1（上、接続線 x 150、箱の下端 68）から ECU 2（下、接続線 x 206、箱の上端 152）
// ---------------------------------------------------------------------------
TEST(F018Spec, F018_FR001_Path_FromSenderBoxToReceiverBox)
{
    RecordProperty("spec", "018:FR-001");
    const BusDiagramLayout l = ComputeBusDiagramLayout(16);
    const EcuPlacement& s = Ecu(l, 1);
    const EcuPlacement& r = Ecu(l, 2);
    const FlowPoint start = FlowPointAt(s, r, 0.0);
    const FlowPoint end = FlowPointAt(s, r, 1.0);
    EXPECT_DOUBLE_EQ(150.0, start.X) << "送信 ECU の接続線";
    EXPECT_DOUBLE_EQ(68.0, start.Y) << "送信 ECU の箱の縁";
    EXPECT_DOUBLE_EQ(206.0, end.X) << "受信 ECU の接続線";
    EXPECT_DOUBLE_EQ(152.0, end.Y) << "受信 ECU の箱の縁";
    const FlowPoint below = FlowPointAt(s, r, -1.0);
    EXPECT_DOUBLE_EQ(start.X, below.X) << "f < 0 は始点";
    EXPECT_DOUBLE_EQ(start.Y, below.Y);
    const FlowPoint above = FlowPointAt(s, r, 2.0);
    EXPECT_DOUBLE_EQ(end.X, above.X) << "f > 1 は終点";
    EXPECT_DOUBLE_EQ(end.Y, above.Y);
    EXPECT_GE(FlowPathLength(s, r), 56.0 + 80.0) << "横 56 px + 2 本の接続線（40 px ずつ）以上";
}

// ---------------------------------------------------------------------------
// FR-003 / FR-005 / SC-002: 光は送信・受信 ECU だけ（送受信の両方をした ECU は送信として 1 つ）。
//   強さは 0.05〜1。粒は強調された道筋の上だけで、受信しない ECU の接続線には描かない。組が無ければ何も無い。
// 根拠: spec.md FR-003、FR-005、SC-002、FlowEffect.h（ComputeFlowEffect）。
// ---------------------------------------------------------------------------
TEST(F018Spec, F018_SC002_ParticlesOnlyOnPathsGlowsOnlyOnEcus)
{
    RecordProperty("spec", "018:SC-002");
    RecordProperty("spec", "018:FR-003");
    const BusDiagramLayout l = ComputeBusDiagramLayout(16);
    const std::vector<std::pair<int, int>> flows = {{1, 2}, {2, 1}};
    for (long long t : {0LL, 37000LL, 410000LL, 799999LL})
    {
        const FlowEffect fx = ComputeFlowEffect(l, flows, t);
        ASSERT_FALSE(fx.Particles.empty()) << t << " us: 粒が無い";
        for (const FlowPoint& p : fx.Particles)
        {
            EXPECT_GE(p.X, 150.0 - 1e-9) << "ECU 1 と 2 の間（ECU 3 の接続線 x 262 には来ない）";
            EXPECT_LE(p.X, 206.0 + 1e-9);
            EXPECT_GE(p.Y, 68.0 - 1e-9);
            EXPECT_LE(p.Y, 152.0 + 1e-9);
        }
        ASSERT_EQ(2u, fx.Glows.size()) << "ECU 1・2 に 1 つずつ（両方とも送信として）";
        for (const FlowGlow& g : fx.Glows)
        {
            EXPECT_TRUE(g.Ecu == 1 || g.Ecu == 2);
            EXPECT_TRUE(g.Sender) << "送受信の両方をした ECU は送信として光る";
            EXPECT_GE(g.Strength, 0.05);
            EXPECT_LE(g.Strength, 1.0);
        }
    }
    const FlowEffect none = ComputeFlowEffect(l, {}, 12345);
    EXPECT_TRUE(none.Particles.empty()) << "組が無ければ粒は無い";
    EXPECT_TRUE(none.Glows.empty()) << "組が無ければ光は無い";
}

// 受信だけの ECU は受信の光（Sender = false）になる。
TEST(F018Spec, F018_FR003_ReceiverOnly_ReceiverGlow)
{
    RecordProperty("spec", "018:FR-003");
    const FlowEffect fx = ComputeFlowEffect(ComputeBusDiagramLayout(4), {{1, 4}}, 0);
    ASSERT_EQ(2u, fx.Glows.size());
    for (const FlowGlow& g : fx.Glows) EXPECT_EQ(g.Ecu == 1, g.Sender) << "ECU " << g.Ecu;
}

// ---------------------------------------------------------------------------
// 改訂 1: 道筋は最後に見えてから 0.3 秒残す（点滅を防ぐ）。Clear で消える。
// 根拠: FlowEffect.h（FlowPathHold、kFlowHoldUs = 300000）、CLAUDE.md（FlowPathHold）。
// ---------------------------------------------------------------------------
TEST(F018Spec, F018_REV1_PathHold_KeptFor300ms)
{
    RecordProperty("spec", "018:REV1");
    FlowPathHold hold;
    hold.Update({{1, 2}}, 1000000);
    hold.Update({{3, 4}}, 1100000);
    EXPECT_EQ((std::vector<std::pair<int, int>>{{1, 2}, {3, 4}}), hold.Current(1200000)) << "0.2 秒後は両方残る";
    EXPECT_EQ((std::vector<std::pair<int, int>>{{3, 4}}), hold.Current(1350000)) << "(1,2) は 0.35 秒前が最後なので消える";
    hold.Clear();
    EXPECT_TRUE(hold.Current(1350000).empty()) << "Clear で消える";
}
