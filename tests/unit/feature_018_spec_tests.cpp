// feature_018_spec_tests.cpp（tests/unit）
// 機能 018（specs/018-frame-flow-animation/spec.md）の受け入れシナリオと成功基準のうち、画面を使わずに確かめられるもの。
// 粒の位置と ECU の光は FlowEffect.h（ComputeFlowEffect など）、強調の対象の組は ReplayClock.h の ActiveFrameCursor で確かめる。
// 色・重なり・書き換えの速さは画面の確認。

#include "support/spec_test_helpers.h"

#include "BusDiagramLayout.h"
#include "FlowEffect.h"
#include "FrameEvents.h"
#include "ProjectFile.h"
#include "ReplayClock.h"

#include <cmath>

using namespace CanSim;
using namespace spectest;

namespace
{
    const EcuPlacement& Ecu(const BusDiagramLayout& layout, int number)
    {
        return layout.Ecus[static_cast<std::size_t>(number - 1)];
    }

    // 粒が、送信 ECU s → 受信 ECU r の道筋（FlowPointAt の 0〜1）のどこかにあるか
    bool OnPath(const FlowPoint& p, const EcuPlacement& s, const EcuPlacement& r)
    {
        const double len = FlowPathLength(s, r);
        for (double d = 0; d <= len; d += 0.25)
        {
            const FlowPoint q = FlowPointAt(s, r, d / len);
            if (std::fabs(q.X - p.X) < 0.5 && std::fabs(q.Y - p.Y) < 0.5)
                return true;
        }
        return false;
    }

    const FlowGlow* GlowOf(const FlowEffect& e, int ecu)
    {
        for (const FlowGlow& g : e.Glows)
            if (g.Ecu == ecu)
                return &g;
        return nullptr;
    }
}

// 根拠: spec.md US1 受け入れシナリオ 1、FR-001・FR-002、FlowEffect.h（粒は道筋の始まりから offset + k × kFlowSpacing の位置）。
// 送信 ECU 1 → 受信 ECU 4 の組では、粒はすべてその道筋（ECU 1 の箱の縁 → 接続線 → バス → ECU 4 の接続線 → 箱の縁）の上にあり、
// 間隔 16 px で道筋の長さいっぱいに並ぶ。
TEST(F018Spec, F018_US1_1_Particles_AlongSenderToReceiverPath)
{
    RecordProperty("spec", "018:US1-1");
    const BusDiagramLayout layout = ComputeBusDiagramLayout(6);
    const EcuPlacement& s = Ecu(layout, 1);
    const EcuPlacement& r = Ecu(layout, 4);
    const long long elapsed = 50000; // 0.05 秒: offset = 7.5 px
    const FlowEffect e = ComputeFlowEffect(layout, { { 1, 4 } }, elapsed);
    const double len = FlowPathLength(s, r);
    const double offset = FlowOffset(elapsed);
    const std::size_t expectedCount = static_cast<std::size_t>(std::floor((len - offset) / kFlowSpacing)) + 1;
    EXPECT_EQ(e.Particles.size(), expectedCount) << "粒の数が、道筋の長さを 16 px おきに並べた数と違う";
    for (const FlowPoint& p : e.Particles)
        EXPECT_TRUE(OnPath(p, s, r)) << "粒（" << p.X << ", " << p.Y << "）が送信 → 受信の道筋の上にない";
}

// 根拠: spec.md US1 受け入れシナリオ 2・3、FR-003、FlowEffect.h（送信 ECU と受信 ECU に 1 つずつ光。0.8 秒周期の明滅で、
// 受信は半周期遅れ）。送信 ECU には送り出しの光、受信 ECU には届いたことを示す光が付く。
TEST(F018Spec, F018_US1_2_ReceiverGlow_HalfPeriodLate)
{
    RecordProperty("spec", "018:US1-2");
    const BusDiagramLayout layout = ComputeBusDiagramLayout(4);
    const long long t = 123456;
    const FlowEffect e = ComputeFlowEffect(layout, { { 1, 2 }, { 1, 3 } }, t);
    const FlowGlow* g2 = GlowOf(e, 2);
    const FlowGlow* g3 = GlowOf(e, 3);
    ASSERT_NE(g2, nullptr) << "受信 ECU 2 に光が無い";
    ASSERT_NE(g3, nullptr) << "受信 ECU 3 に光が無い";
    EXPECT_FALSE(g2->Sender) << "ECU 2 の光が受信の光でない";
    EXPECT_EQ(GlowOf(e, 4), nullptr) << "関係の無い ECU 4 に光がある";
    // 受信は送信の半周期遅れ: 時刻 t + 0.4 秒の受信の明るさ = 時刻 t の送信の明るさ
    const FlowEffect half = ComputeFlowEffect(layout, { { 1, 2 } }, t + kFlowGlowPeriodUs / 2);
    EXPECT_NEAR(GlowOf(half, 2)->Strength, GlowOf(e, 1)->Strength, 1e-9) << "受信の光が送信の半周期遅れでない";
}

TEST(F018Spec, F018_US1_3_SenderGlow_PeriodicEvery800ms)
{
    RecordProperty("spec", "018:US1-3");
    const BusDiagramLayout layout = ComputeBusDiagramLayout(4);
    const long long t = 123456;
    const FlowEffect e = ComputeFlowEffect(layout, { { 1, 2 } }, t);
    const FlowGlow* g1 = GlowOf(e, 1);
    ASSERT_NE(g1, nullptr) << "送信 ECU 1 に光が無い";
    EXPECT_TRUE(g1->Sender) << "ECU 1 の光が送信の光でない";
    const FlowEffect later = ComputeFlowEffect(layout, { { 1, 2 } }, t + kFlowGlowPeriodUs);
    EXPECT_NEAR(GlowOf(later, 1)->Strength, g1->Strength, 1e-9) << "送信の光が 0.8 秒周期でない";
    const FlowEffect quarter = ComputeFlowEffect(layout, { { 1, 2 } }, t + kFlowGlowPeriodUs / 4);
    EXPECT_GT(std::fabs(GlowOf(quarter, 1)->Strength - g1->Strength), 1e-3) << "送信の光が明滅していない";
}

// 根拠: spec.md US1 受け入れシナリオ 5・SC-002、FR-005。受信しない ECU（sample_collision の ECU 2）とその接続線には、
// 10 秒の実行の間、粒が 1 度も描かれない。書き換え（16 ms）ごとの強調の対象の組から粒を作って確かめる。
TEST(F018Spec, F018_SC002_SampleCollisionEcu2_NeverGetsParticles)
{
    RecordProperty("spec", "018:SC-002");
    const ConfigLoadResult cfg = LoadConfig(SampleConfigPath(L"sample_collision.canprj"));
    ASSERT_TRUE(cfg.Success) << "sample_collision を読めない: " << cfg.ErrorMessage;
    const Network run = BuildRunNetwork(cfg.Project, cfg.Config);
    const std::vector<FrameEvent> events = BuildFrameEvents(RunSim(run, 10000000LL));
    ASSERT_FALSE(events.empty()) << "前提: フレームが無い";
    const BusDiagramLayout layout = ComputeBusDiagramLayout(static_cast<int>(run.Nodes.size()));
    const EcuPlacement& e2 = Ecu(layout, 2);
    const double stubTop = std::min(e2.LineY1, e2.LineY2);
    const double stubBottom = std::max(e2.LineY1, e2.LineY2);
    ActiveFrameCursor cursor(events);
    int windowsWithFlow = 0;
    for (long long t = 0; t < 10000000; t += 16000)
    {
        const ActiveFrames a = cursor.Collect(t, t + 16000);
        for (const auto& f : a.Flows)
            EXPECT_NE(f.second, 2) << t << " µs: ECU 2 が受信の組に入っている";
        if (a.Flows.empty())
            continue;
        ++windowsWithFlow;
        const FlowEffect fx = ComputeFlowEffect(layout, a.Flows, t);
        for (const FlowPoint& p : fx.Particles)
        {
            const bool onEcu2Stub = std::fabs(p.X - e2.LineX) < 0.5 && p.Y > stubTop + 3 && p.Y < stubBottom - 3;
            EXPECT_FALSE(onEcu2Stub) << t << " µs: ECU 2 の接続線に粒がある";
        }
        EXPECT_EQ(GlowOf(fx, 2), nullptr) << t << " µs: ECU 2 に光がある";
    }
    EXPECT_GT(windowsWithFlow, 0) << "前提: 粒の出る書き換えが無い";
}

// 根拠: spec.md US1 受け入れシナリオ 5、FR-005。受信しない ECU（sample_collision の ECU 2）には粒が行かない。粒の道筋の元になる
// （送信, 受信）の組に、ECU 2 が受信として入らない（実行全体を 1 回の区間として集める）。
TEST(F018Spec, F018_US1_5_NonReceivingEcu_NotInFlowPairs)
{
    RecordProperty("spec", "018:US1-5");
    const ConfigLoadResult cfg = LoadConfig(SampleConfigPath(L"sample_collision.canprj"));
    ASSERT_TRUE(cfg.Success) << cfg.ErrorMessage;
    const std::vector<FrameEvent> events = BuildFrameEvents(RunSim(BuildRunNetwork(cfg.Project, cfg.Config), 1000000LL));
    ActiveFrameCursor cursor(events);
    const ActiveFrames all = cursor.Collect(-1, 1000000);
    ASSERT_FALSE(all.Flows.empty()) << "前提: 組が無い";
    for (const auto& f : all.Flows)
        EXPECT_NE(f.second, 2) << "ECU 2 が受信の組に入っている（送信 " << f.first << "）";
}

// 根拠: spec.md US1 受け入れシナリオ 6、FR-005。強調の対象のフレームが無い（バスが空いている）書き換えでは、エフェクトは描かれない。
TEST(F018Spec, F018_US1_6_NoFlows_NoEffect)
{
    RecordProperty("spec", "018:US1-6");
    const FlowEffect e = ComputeFlowEffect(ComputeBusDiagramLayout(16), {}, 987654);
    EXPECT_TRUE(e.Particles.empty()) << "組が無いのに粒がある";
    EXPECT_TRUE(e.Glows.empty()) << "組が無いのに光がある";
}

// 根拠: spec.md SC-001、FR-002・FR-006（粒の位置は書き換えのたびにやり直さず、実時間で連続して進む。速さは一定）。
// 書き換え（16 ms）ごとに、最初の粒の位置は 150 px/秒 × 0.016 秒 = 2.4 px ずつ道筋の先（受信 ECU の向き）へ進む
// （16 px の間隔で次の粒に入れ替わる所を除く）。画面写しで目に見えることは画面の確認。
TEST(F018Spec, F018_SC001_ParticlesAdvanceTowardReceiver)
{
    RecordProperty("spec", "018:SC-001");
    int advanced = 0;
    for (long long t = 0; t < 1000000; t += 16000)
    {
        const double a = FlowOffset(t);
        const double b = FlowOffset(t + 16000);
        ASSERT_GE(a, 0.0);
        ASSERT_LT(a, static_cast<double>(kFlowSpacing));
        if (b > a)
        {
            EXPECT_NEAR(b - a, 2.4, 1e-6) << t << " µs: 1 回の書き換えで進んだ距離";
            ++advanced;
        }
        else
            EXPECT_NEAR(b + kFlowSpacing - a, 2.4, 1e-6) << t << " µs: 次の粒への入れ替わり";
    }
    EXPECT_GT(advanced, 0);
    // 速さはスピードに関係しない（FlowOffset は実時間だけで決まる）: 1 秒で 150 px = 9 間隔と 6 px
    EXPECT_NEAR(FlowOffset(1000000), 6.0, 1e-6) << "1 秒後の位置";
}
