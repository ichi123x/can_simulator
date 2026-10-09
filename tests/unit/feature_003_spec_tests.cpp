// 機能 003（CAN バス構成図）の仕様テスト
// 根拠: specs/003-bus-diagram-view/spec.md、BusDiagramLayout.h のコメント、CLAUDE.md の構成図の説明。
// 仕様に ID が無いので、受け入れシナリオは US<ストーリー番号>-<シナリオ番号> と呼ぶ。
// 座標は 96 dpi の構成図の中の px。描画（色・文字）は画面側なのでテストしない。
// ECU の箱は機能 022 で縦 48 px（上段 20〜68、下段 152〜200）になった。

#include <gtest/gtest.h>

#include "BusDiagramLayout.h"

using namespace CanSim;

namespace
{
    bool Overlap(const DiagramRect& a, const DiagramRect& b)
    {
        return a.Left < b.Right && b.Left < a.Right && a.Top < b.Bottom && b.Top < a.Bottom;
    }
}

// ---------------------------------------------------------------------------
// SC-002 / SC-003 / FR-003 / FR-007: 表示する ECU の数は ECU 数と一致する（0 なら 1 つも無い）。17 以上は 16 に抑える。
// 根拠: spec.md SC-002、SC-003、FR-003、FR-007、BusDiagramLayout.h（ecuCount is clamped to 0..16）。
// ---------------------------------------------------------------------------
TEST(F003Spec, F003_SC002_EcuCount_PlacementsMatchCount)
{
    RecordProperty("spec", "003:SC-002");
    RecordProperty("spec", "003:SC-003");
    for (int n = 0; n <= 16; ++n)
    {
        const BusDiagramLayout l = ComputeBusDiagramLayout(n);
        ASSERT_EQ(static_cast<size_t>(n), l.Ecus.size()) << "ECU 数 " << n;
        for (int i = 0; i < n; ++i) EXPECT_EQ(i + 1, l.Ecus[i].Number) << "番号順";
    }
    EXPECT_EQ(16u, ComputeBusDiagramLayout(17).Ecus.size()) << "17 は 16 に抑える";
    EXPECT_EQ(0u, ComputeBusDiagramLayout(-1).Ecus.size()) << "負は 0 に抑える";
}

// ---------------------------------------------------------------------------
// US1-2 / FR-005 / FR-002: 奇数番号はバスの上、偶数番号は下。バスは 10〜1130 の太い線。
// 根拠: spec.md US1 シナリオ 2、FR-002、FR-005、BusDiagramLayout.h（kBusLeft/kBusRight/kBusY/kBusThickness）、
//        CLAUDE.md（箱は上段 20〜68・下段 152〜200）。
// ---------------------------------------------------------------------------
TEST(F003Spec, F003_US1_2_OddAboveEvenBelow_BusBetween)
{
    RecordProperty("spec", "003:US1-2");
    const BusDiagramLayout l = ComputeBusDiagramLayout(16);
    EXPECT_EQ(10, l.BusLine.Left);
    EXPECT_EQ(1130, l.BusLine.Right);
    EXPECT_EQ(108, l.BusLine.Top) << "y 110 を中心に太さ 4";
    EXPECT_EQ(112, l.BusLine.Bottom);
    for (const EcuPlacement& e : l.Ecus)
    {
        const bool odd = e.Number % 2 == 1;
        EXPECT_EQ(odd, e.OnTop) << "ECU " << e.Number;
        EXPECT_EQ(odd ? 20 : 152, e.Box.Top) << "ECU " << e.Number << " の箱の上端";
        EXPECT_EQ(odd ? 68 : 200, e.Box.Bottom) << "ECU " << e.Number << " の箱の下端";
    }
}

// ---------------------------------------------------------------------------
// FR-004: 各 ECU は接続線でバスと結ばれる（箱の中央から、箱の縁とバスの縁の間）。
// 根拠: spec.md FR-004、BusDiagramLayout.h（LineX = box center、LineY1 = box side、LineY2 = bus side）。
// ---------------------------------------------------------------------------
TEST(F003Spec, F003_FR004_StubLine_FromBoxEdgeToBusEdge)
{
    RecordProperty("spec", "003:FR-004");
    const BusDiagramLayout l = ComputeBusDiagramLayout(16);
    for (const EcuPlacement& e : l.Ecus)
    {
        EXPECT_EQ((e.Box.Left + e.Box.Right) / 2, e.LineX) << "ECU " << e.Number << " の接続線は箱の中央";
        EXPECT_EQ(e.OnTop ? e.Box.Bottom : e.Box.Top, e.LineY1) << "ECU " << e.Number << " の接続線の箱側";
        EXPECT_EQ(e.OnTop ? l.BusLine.Top : l.BusLine.Bottom, e.LineY2) << "ECU " << e.Number << " の接続線のバス側";
    }
}

// ---------------------------------------------------------------------------
// FR-009 / 機能の Edge Cases: ECU の位置・大きさは ECU 数によらない（左から詰めて同じ間隔）。
//   中心 = 150 + 56 × (n − 1)、幅 96。
// 根拠: spec.md FR-009、BusDiagramLayout.h（kFirstEcuCenterX = 150、kEcuPitch = 56、kEcuBoxWidth = 96）。
// ---------------------------------------------------------------------------
TEST(F003Spec, F003_FR009_PositionIndependentOfCount)
{
    RecordProperty("spec", "003:FR-009");
    const BusDiagramLayout full = ComputeBusDiagramLayout(16);
    for (int n = 1; n <= 16; ++n)
    {
        const BusDiagramLayout l = ComputeBusDiagramLayout(n);
        for (int i = 0; i < n; ++i)
        {
            EXPECT_EQ(full.Ecus[i].Box.Left, l.Ecus[i].Box.Left) << "ECU 数 " << n << " の ECU " << i + 1;
            EXPECT_EQ(full.Ecus[i].Box.Top, l.Ecus[i].Box.Top);
        }
    }
    EXPECT_EQ(102, full.Ecus[0].Box.Left) << "ECU 1 の箱（中心 150、幅 96）";
    EXPECT_EQ(198, full.Ecus[0].Box.Right);
    EXPECT_EQ(942, full.Ecus[15].Box.Left) << "ECU 16 の箱（中心 990）";
    EXPECT_EQ(1038, full.Ecus[15].Box.Right);
}

// ---------------------------------------------------------------------------
// SC-001 / FR-008: 16 ECU がすべて構成図の中に収まり、重ならない。
// FR-011 / SC-006: 「CAN BUS」の文字（右端 118）と ECU の接続線が重ならない。ECU はバスの両端の内側。
// 根拠: spec.md SC-001、SC-006、FR-008、FR-011、BusDiagramLayout.h（kDiagramWidth/Height、kCaptionRight）。
// ---------------------------------------------------------------------------
TEST(F003Spec, F003_SC001_SixteenEcus_InsideAndNoOverlap)
{
    RecordProperty("spec", "003:SC-001");
    RecordProperty("spec", "003:SC-006");
    const BusDiagramLayout l = ComputeBusDiagramLayout(16);
    for (std::size_t i = 0; i < l.Ecus.size(); ++i)
    {
        const DiagramRect& b = l.Ecus[i].Box;
        EXPECT_GE(b.Left, 0);
        EXPECT_LE(b.Right, 1140) << "構成図の幅の中";
        EXPECT_GE(b.Top, 0);
        EXPECT_LE(b.Bottom, 216) << "構成図の高さの中";
        EXPECT_FALSE(Overlap(b, l.BusLine)) << "ECU " << i + 1 << " の箱がバスに重なる";
        EXPECT_GT(l.Ecus[i].LineX, 118) << "ECU " << i + 1 << " の接続線が「CAN BUS」の文字に重なる";
        EXPECT_GT(b.Left, l.BusLine.Left) << "バスの左端の内側";
        EXPECT_LT(b.Right, l.BusLine.Right) << "バスの右端の内側";
        for (std::size_t j = i + 1; j < l.Ecus.size(); ++j)
            EXPECT_FALSE(Overlap(b, l.Ecus[j].Box)) << "ECU " << i + 1 << " と " << j + 1 << " が重なる";
    }
}
