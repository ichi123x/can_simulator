// feature_003_spec_tests.cpp（tests/unit）
// 機能 003（specs/003-bus-diagram-view/spec.md）の受け入れシナリオと成功基準のうち、画面を使わずに確かめられるもの。
// 構成図の座標（BusDiagramLayout.h の ComputeBusDiagramLayout）で確かめる。色・文字・背景は画面の確認。

#include "support/spec_test_helpers.h"

#include "BusDiagramLayout.h"

using namespace CanSim;
using namespace spectest;

namespace
{
    bool Overlaps(const DiagramRect& a, const DiagramRect& b)
    {
        return a.Left < b.Right && b.Left < a.Right && a.Top < b.Bottom && b.Top < a.Bottom;
    }

    // 接続線がバス線に届いているか（バス側の端がバス線の上端〜下端の範囲にある）
    bool StubReachesBus(const EcuPlacement& e, const DiagramRect& bus)
    {
        return e.LineY2 >= bus.Top && e.LineY2 <= bus.Bottom;
    }
}

// 根拠: spec.md US1 受け入れシナリオ 1、FR-002〜FR-004。ECU 数 16 では、ECU 01〜16 の 16 個すべてがバス線に接続線で結ばれて並ぶ。
TEST(F003Spec, F003_US1_1_SixteenEcus_AllConnectedToBus)
{
    RecordProperty("spec", "003:US1-1");
    const BusDiagramLayout l = ComputeBusDiagramLayout(16);
    ASSERT_EQ(l.Ecus.size(), 16u) << "ECU が 16 個でない";
    EXPECT_LT(l.BusLine.Left, l.BusLine.Right) << "バス線が無い";
    for (int i = 0; i < 16; ++i)
    {
        const EcuPlacement& e = l.Ecus[static_cast<std::size_t>(i)];
        EXPECT_EQ(e.Number, i + 1) << "ECU の番号";
        EXPECT_TRUE(StubReachesBus(e, l.BusLine)) << "ECU " << e.Number << " の接続線がバス線に届いていない";
        EXPECT_TRUE(e.LineY1 == e.Box.Bottom || e.LineY1 == e.Box.Top) << "ECU " << e.Number << " の接続線が箱から出ていない";
    }
}

// 根拠: spec.md US1 受け入れシナリオ 2、FR-005。奇数番号はバス線の上側、偶数番号は下側に交互に並び、番号順に左から右へ進む。
TEST(F003Spec, F003_US1_2_OddAboveEvenBelow_LeftToRight)
{
    RecordProperty("spec", "003:US1-2");
    const BusDiagramLayout l = ComputeBusDiagramLayout(16);
    for (const EcuPlacement& e : l.Ecus)
    {
        if (e.Number % 2 == 1)
            EXPECT_LE(e.Box.Bottom, l.BusLine.Top) << "奇数の ECU " << e.Number << " がバス線の上側にない";
        else
            EXPECT_GE(e.Box.Top, l.BusLine.Bottom) << "偶数の ECU " << e.Number << " がバス線の下側にない";
        EXPECT_EQ(e.OnTop, e.Number % 2 == 1) << "ECU " << e.Number << " の上下";
        if (e.Number > 1)
            EXPECT_GT(e.LineX, l.Ecus[static_cast<std::size_t>(e.Number - 2)].LineX) << "ECU " << e.Number << " が左から順に並んでいない";
    }
}

// 根拠: spec.md US1 受け入れシナリオ 3、FR-003・FR-009。ECU 数 6 では ECU 01〜06 の 6 個だけで、位置は ECU 数 16 のときと同じ。
TEST(F003Spec, F003_US1_3_SixEcus_OnlySixAtSamePositions)
{
    RecordProperty("spec", "003:US1-3");
    const BusDiagramLayout six = ComputeBusDiagramLayout(6);
    const BusDiagramLayout sixteen = ComputeBusDiagramLayout(16);
    ASSERT_EQ(six.Ecus.size(), 6u) << "ECU が 6 個でない";
    for (std::size_t i = 0; i < 6; ++i)
    {
        EXPECT_EQ(six.Ecus[i].Number, static_cast<int>(i) + 1);
        EXPECT_EQ(six.Ecus[i].Box.Left, sixteen.Ecus[i].Box.Left) << "ECU 数で位置が変わった";
        EXPECT_EQ(six.Ecus[i].Box.Top, sixteen.Ecus[i].Box.Top) << "ECU 数で位置が変わった";
    }
}

// 根拠: spec.md US1 受け入れシナリオ 4、FR-011。「CAN BUS」の見出しは ECU 01 の接続線と重ならず、バス線の左右両端には ECU を
// 置かない余裕がある。
TEST(F003Spec, F003_US1_4_CaptionClearOfStub_MarginsAtBusEnds)
{
    RecordProperty("spec", "003:US1-4");
    const BusDiagramLayout l = ComputeBusDiagramLayout(16);
    EXPECT_LT(kCaptionRight, l.Ecus.front().LineX) << "見出しが ECU 01 の接続線と重なる";
    EXPECT_LT(l.BusLine.Left, l.Ecus.front().Box.Left) << "バス線の左端に余裕が無い";
    EXPECT_GT(l.BusLine.Right, l.Ecus.back().Box.Right) << "バス線の右端に余裕が無い";
}

// 根拠: spec.md US2 受け入れシナリオ 1、FR-008。ECU 数 16 で、16 個すべての ECU が表示領域（構成図の領域）に収まり、
// 隣の ECU と重ならない。文字が切れないことは画面の確認。
TEST(F003Spec, F003_US2_1_SixteenEcus_InsideAreaNoOverlap)
{
    RecordProperty("spec", "003:US2-1");
    const BusDiagramLayout l = ComputeBusDiagramLayout(16);
    for (std::size_t i = 0; i < l.Ecus.size(); ++i)
    {
        const DiagramRect& b = l.Ecus[i].Box;
        EXPECT_GE(b.Left, 0) << "ECU " << i + 1 << " が左にはみ出す";
        EXPECT_GE(b.Top, 0) << "ECU " << i + 1 << " が上にはみ出す";
        EXPECT_LE(b.Right, kDiagramWidth) << "ECU " << i + 1 << " が右にはみ出す";
        EXPECT_LE(b.Bottom, kDiagramHeight) << "ECU " << i + 1 << " が下にはみ出す";
        for (std::size_t j = i + 1; j < l.Ecus.size(); ++j)
            EXPECT_FALSE(Overlaps(b, l.Ecus[j].Box)) << "ECU " << i + 1 << " と " << j + 1 << " が重なる";
    }
}

// 根拠: spec.md US3 受け入れシナリオ 1・SC-003、FR-007。ECU 数 0 では、ECU と接続線は 1 つも無く、バス線だけがある。
TEST(F003Spec, F003_US3_1_ZeroEcus_BusOnly)
{
    RecordProperty("spec", "003:US3-1");
    const BusDiagramLayout l = ComputeBusDiagramLayout(0);
    EXPECT_TRUE(l.Ecus.empty()) << "ECU 数 0 なのに ECU がある";
    EXPECT_LT(l.BusLine.Left, l.BusLine.Right) << "バス線が無い";
}

// 根拠: spec.md SC-002、FR-003。1〜16 の任意の ECU 数で、表示される ECU の個数が ECU 数と一致する。
TEST(F003Spec, F003_SC002_EveryCount_EcuCountMatches)
{
    RecordProperty("spec", "003:SC-002");
    for (int n = 1; n <= 16; ++n)
        EXPECT_EQ(ComputeBusDiagramLayout(n).Ecus.size(), static_cast<std::size_t>(n)) << "ECU 数 " << n;
}

// 根拠: spec.md SC-003、FR-007。ECU 数が 0 のとき、ECU が 1 つも表示されない（範囲外の負の数も 0 として扱う: BusDiagramLayout.h は
// 0〜16 に収める）。
TEST(F003Spec, F003_SC003_ZeroCount_NoEcu)
{
    RecordProperty("spec", "003:SC-003");
    EXPECT_TRUE(ComputeBusDiagramLayout(0).Ecus.empty());
    EXPECT_TRUE(ComputeBusDiagramLayout(-3).Ecus.empty()) << "負の数で ECU がある";
    EXPECT_EQ(ComputeBusDiagramLayout(99).Ecus.size(), 16u) << "17 以上が 16 に収まらない";
}

// 根拠: spec.md SC-006、FR-011。見出し「CAN BUS」と、ECU からバス線に向かう接続線が、ECU 数 1〜16 のいずれでも重ならない。
TEST(F003Spec, F003_SC006_EveryCount_CaptionClearOfAllStubs)
{
    RecordProperty("spec", "003:SC-006");
    for (int n = 1; n <= 16; ++n)
        for (const EcuPlacement& e : ComputeBusDiagramLayout(n).Ecus)
            EXPECT_TRUE(e.LineX < kCaptionLeft || e.LineX > kCaptionRight)
                << "ECU 数 " << n << ": 見出しが ECU " << e.Number << " の接続線と重なる";
}
