// 機能 010（ECU のダブルクリックで編集）の仕様テスト
// 根拠: specs/010-ecu-doubleclick-edit/spec.md、BusDiagramLayout.h（ScaleToDpi・FindEcuAt）。
// ウィンドウの開閉・切り替えの待ち・前面に出す動作は画面側なのでテストせず、当たり判定を確かめる。

#include <gtest/gtest.h>

#include "BusDiagramLayout.h"

using namespace CanSim;

// ---------------------------------------------------------------------------
// ScaleToDpi = (px × dpi + 48) / 96（MulDiv と同じ丸め）。
// 根拠: BusDiagramLayout.h（ScaleToDpi のコメント）。
// ---------------------------------------------------------------------------
TEST(F010Spec, F010_FR004_ScaleToDpi_RoundsLikeMulDiv)
{
    RecordProperty("spec", "010:FR-004");
    EXPECT_EQ(100, ScaleToDpi(100, 96));
    EXPECT_EQ(125, ScaleToDpi(100, 120));
    EXPECT_EQ(150, ScaleToDpi(100, 144));
    EXPECT_EQ(126, ScaleToDpi(101, 120)) << "(12120 + 48) / 96 = 126.75 -> 126";
    EXPECT_EQ(2, ScaleToDpi(1, 144)) << "(144 + 48) / 96 = 2";
    EXPECT_EQ(0, ScaleToDpi(0, 144));
}

// ---------------------------------------------------------------------------
// FR-004 / SC-005: 判定の範囲は描いた箱と一致する（換算後の [Left, Right) × [Top, Bottom)）。
//   100%・125%・150% で、箱の縁の内側 1 px は当たり、外側は外れ。
// 根拠: spec.md FR-004、SC-005、BusDiagramLayout.h（FindEcuAt）。
//   ECU 1 の箱（96 dpi）: 102〜198 × 20〜68
//   120 dpi: Left (12240+48)/96 = 128、Top (2400+48)/96 = 25、Right (23760+48)/96 = 248、Bottom (8160+48)/96 = 85
//   144 dpi: Left 153、Top 30、Right 297、Bottom 102
// ---------------------------------------------------------------------------
struct EdgeCase
{
    int Dpi;
    int Left;
    int Top;
    int Right;
    int Bottom;
};

class EcuHitEdgeTable : public ::testing::TestWithParam<EdgeCase>
{
};

TEST_P(EcuHitEdgeTable, F010_SC005_BoxEdges_InsideHitOutsideMiss)
{
    RecordProperty("spec", "010:SC-005");
    const EdgeCase& c = GetParam();
    const BusDiagramLayout l = ComputeBusDiagramLayout(16);
    const int midX = (c.Left + c.Right) / 2;
    const int midY = (c.Top + c.Bottom) / 2;
    EXPECT_EQ(1, FindEcuAt(l, c.Left, c.Top, c.Dpi)) << "左上の角（縁の内側）";
    EXPECT_EQ(1, FindEcuAt(l, c.Right - 1, c.Bottom - 1, c.Dpi)) << "右下の角（縁の内側）";
    EXPECT_EQ(0, FindEcuAt(l, c.Left - 1, midY, c.Dpi)) << "左の外";
    EXPECT_EQ(0, FindEcuAt(l, c.Right, midY, c.Dpi)) << "右の外";
    EXPECT_EQ(0, FindEcuAt(l, midX, c.Top - 1, c.Dpi)) << "上の外";
    EXPECT_EQ(0, FindEcuAt(l, midX, c.Bottom, c.Dpi)) << "下の外";
}

INSTANTIATE_TEST_SUITE_P(Dpi, EcuHitEdgeTable,
                         ::testing::Values(EdgeCase{96, 102, 20, 198, 68}, EdgeCase{120, 128, 25, 248, 85},
                                           EdgeCase{144, 153, 30, 297, 102}),
                         [](const ::testing::TestParamInfo<EdgeCase>& info) {
                             return "Dpi" + std::to_string(info.param.Dpi);
                         });

// ---------------------------------------------------------------------------
// SC-002: ECU 数 1〜16 のそれぞれで、表示されているすべての箱の中央が、その ECU になる。
// FR-005 / SC-003: バス線・接続線・「CAN BUS」の文字・余白は 0（どの ECU でもない）。表示されていない ECU の位置も 0。
// 根拠: spec.md SC-002、SC-003、FR-005。
// ---------------------------------------------------------------------------
TEST(F010Spec, F010_SC002_EveryShownBox_HitsItsEcu)
{
    RecordProperty("spec", "010:SC-002");
    for (int n = 1; n <= 16; ++n)
    {
        const BusDiagramLayout l = ComputeBusDiagramLayout(n);
        for (const EcuPlacement& e : l.Ecus)
        {
            const int x = (e.Box.Left + e.Box.Right) / 2;
            const int y = (e.Box.Top + e.Box.Bottom) / 2;
            EXPECT_EQ(e.Number, FindEcuAt(l, x, y, 96)) << "ECU 数 " << n << " の ECU " << e.Number;
        }
    }
}

TEST(F010Spec, F010_SC003_OutsideBoxes_NoEcu)
{
    RecordProperty("spec", "010:SC-003");
    const BusDiagramLayout l = ComputeBusDiagramLayout(16);
    EXPECT_EQ(0, FindEcuAt(l, 500, 110, 96)) << "バス線";
    EXPECT_EQ(0, FindEcuAt(l, 150, 90, 96)) << "ECU 1 の接続線";
    EXPECT_EQ(0, FindEcuAt(l, 40, 95, 96)) << "「CAN BUS」の文字のあたり";
    EXPECT_EQ(0, FindEcuAt(l, 1100, 30, 96)) << "右の余白";
    EXPECT_EQ(0, FindEcuAt(l, 500, 300, 96)) << "構成図の下（ログ表示エリア）";

    const BusDiagramLayout two = ComputeBusDiagramLayout(2);
    EXPECT_EQ(0, FindEcuAt(two, 262, 44, 96)) << "ECU 数 2 では ECU 3 の位置（中心 262）は何もない";
}
