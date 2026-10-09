// 機能 011（ECU のグレー表示と、構成図・ログ表示エリアの枠）の仕様テスト
// 根拠: specs/011-ecu-feedback-frames/spec.md、BusDiagramLayout.h（Frame・ScaleLogViewRect・ScaleLogFrameRect）、
//        CLAUDE.md（枠は構成図の 4 px 内側、ログ表示エリアの 3 px 外側。ログ表示エリアは 96 dpi で 150, 260, 850 × 260）。
// グレー表示の時間・色は画面側なのでテストせず、枠の位置を確かめる。

#include <gtest/gtest.h>

#include "BusDiagramLayout.h"

using namespace CanSim;

namespace
{
    bool Contains(const DiagramRect& outer, const DiagramRect& inner)
    {
        return outer.Left < inner.Left && outer.Top < inner.Top && inner.Right < outer.Right && inner.Bottom < outer.Bottom;
    }
}

// ---------------------------------------------------------------------------
// FR-008 / FR-010: 構成図の枠は構成図の領域の 4 px 内側で、ECU 数によらず同じ位置。
//   16 ECU のすべての箱・バス線・「CAN BUS」の文字（左 14）を中に含む。
// 根拠: spec.md FR-008、FR-010、BusDiagramLayout.h（kDiagramFrameInset = 4）。
// ---------------------------------------------------------------------------
TEST(F011Spec, F011_FR008_DiagramFrame_InsetAndContainsEverything)
{
    RecordProperty("spec", "011:FR-008");
    RecordProperty("spec", "011:FR-010");
    const BusDiagramLayout full = ComputeBusDiagramLayout(16);
    EXPECT_EQ(4, full.Frame.Left);
    EXPECT_EQ(4, full.Frame.Top);
    EXPECT_EQ(1136, full.Frame.Right) << "1140 − 4";
    EXPECT_EQ(212, full.Frame.Bottom) << "216 − 4";
    for (const EcuPlacement& e : full.Ecus) EXPECT_TRUE(Contains(full.Frame, e.Box)) << "ECU " << e.Number;
    EXPECT_TRUE(Contains(full.Frame, full.BusLine)) << "バス線";
    EXPECT_LT(full.Frame.Left, 14) << "「CAN BUS」の文字の左端";
    for (int n : {0, 6})
    {
        const BusDiagramLayout l = ComputeBusDiagramLayout(n);
        EXPECT_EQ(full.Frame.Left, l.Frame.Left) << "ECU 数 " << n;
        EXPECT_EQ(full.Frame.Bottom, l.Frame.Bottom) << "ECU 数 " << n;
    }
}

// ---------------------------------------------------------------------------
// FR-009 / FR-011 / SC-005: ログ表示エリアの枠はエリアの外側を囲む。96 dpi では 3 px 外側。
//   どの拡大率でも、枠はエリアを中に含み、構成図の中身（16 ECU の箱）と重ならない。
// 根拠: spec.md FR-009、FR-011、SC-005、CLAUDE.md（ログ表示エリアの位置）、BusDiagramLayout.h。
// ---------------------------------------------------------------------------
TEST(F011Spec, F011_FR009_LogFrame_Outside3pxAt96)
{
    RecordProperty("spec", "011:FR-009");
    const DiagramRect v = ScaleLogViewRect(96);
    EXPECT_EQ(150, v.Left);
    EXPECT_EQ(260, v.Top);
    EXPECT_EQ(1000, v.Right) << "150 + 850";
    EXPECT_EQ(520, v.Bottom) << "260 + 260";
    const DiagramRect f = ScaleLogFrameRect(96);
    EXPECT_EQ(147, f.Left);
    EXPECT_EQ(257, f.Top);
    EXPECT_EQ(1003, f.Right);
    EXPECT_EQ(523, f.Bottom);
}

TEST(F011Spec, F011_SC005_EachDpi_FramesAroundContentNoOverlap)
{
    RecordProperty("spec", "011:SC-005");
    RecordProperty("spec", "011:FR-011");
    for (int dpi : {96, 120, 144})
    {
        const DiagramRect v = ScaleLogViewRect(dpi);
        const DiagramRect f = ScaleLogFrameRect(dpi);
        EXPECT_TRUE(Contains(f, v)) << dpi << " dpi: ログの枠がエリアを囲んでいない";
        const int lowestBox = ScaleToDpi(200, dpi); // 下段の ECU の箱の下端
        EXPECT_GT(f.Top, lowestBox) << dpi << " dpi: ログの枠が構成図の ECU と重なる";
        EXPECT_GT(f.Top, ScaleToDpi(212, dpi)) << dpi << " dpi: ログの枠が構成図の枠と重なる";
    }
}
