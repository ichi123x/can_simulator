// 機能 020（領域のラベル）の仕様テスト
// 根拠: specs/020-area-labels/spec.md（改訂: ラベルはログ表示エリアの上の「イベント＆ログ表示」だけ）、
//        BusDiagramLayout.h（ScaleLogLabelRect: 左端はログの枠と同じ）。
// 文字の描画は画面側なのでテストせず、ラベルの位置を確かめる。

#include <gtest/gtest.h>

#include "BusDiagramLayout.h"

using namespace CanSim;

// ---------------------------------------------------------------------------
// FR-002 / FR-003 / SC-001: ラベルはログ表示エリアの枠のすぐ上の左端にあり、どの DPI・ECU 数でも、
//   ログの枠・構成図の ECU の箱・構成図の枠と重ならない。
// 根拠: spec.md FR-002、FR-003、SC-001、BusDiagramLayout.h。
// ---------------------------------------------------------------------------
TEST(F020Spec, F020_SC001_LogLabel_AboveFrameLeftAlignedNoOverlap)
{
    RecordProperty("spec", "020:FR-002");
    RecordProperty("spec", "020:SC-001");
    for (int dpi : {96, 120, 144})
    {
        const DiagramRect label = ScaleLogLabelRect(dpi);
        const DiagramRect frame = ScaleLogFrameRect(dpi);
        EXPECT_EQ(frame.Left, label.Left) << dpi << " dpi: 左端がログの枠とそろっていない";
        EXPECT_LE(label.Bottom, frame.Top) << dpi << " dpi: ラベルがログの枠に重なる";
        EXPECT_LT(label.Left, label.Right);
        EXPECT_LT(label.Top, label.Bottom);
        EXPECT_GE(label.Top, ScaleToDpi(212, dpi)) << dpi << " dpi: ラベルが構成図の枠の中に入る";
        for (int n : {0, 16})
            for (const EcuPlacement& e : ComputeBusDiagramLayout(n).Ecus)
                EXPECT_GE(label.Top, ScaleToDpi(e.Box.Bottom, dpi)) << dpi << " dpi: ECU " << e.Number << " と重なる";
    }
}

// 96 dpi の位置（BusDiagramLayout.h の kLogLabelTop/Right/Bottom）: 147〜300 × 236〜254
TEST(F020Spec, F020_FR002_LogLabelAt96_Position)
{
    RecordProperty("spec", "020:FR-002");
    const DiagramRect label = ScaleLogLabelRect(96);
    EXPECT_EQ(147, label.Left) << "ログの枠の左端";
    EXPECT_EQ(236, label.Top);
    EXPECT_EQ(300, label.Right);
    EXPECT_EQ(254, label.Bottom);
}
