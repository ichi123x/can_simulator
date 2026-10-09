// 機能 019（ツールバーの並びと大きさ）の仕様テスト
// 根拠: specs/019-toolbar-compact-layout/spec.md、SimControls.h のコメント、CLAUDE.md。
// 範囲は機能 026 で -10〜+10、位置は機能 023 で右へずれた。描画は画面側なのでテストしない。

#include <gtest/gtest.h>

#include <vector>

#include "SimControls.h"

using namespace CanSim;

// ---------------------------------------------------------------------------
// FR-001 / SC-001: 左から ボタン -> チェックボックス -> シミュレーション時間 -> 「スピード：」-> スライダー の順に、
//   重ならずに並び、ツールバーの帯（高さ 64）と構成図の幅（1140）に収まる（DPI 96/120/144）。
// 根拠: spec.md FR-001、SC-001、SimControls.h（kToolbarHeight = 64）、BusDiagramLayout.h（kDiagramWidth = 1140）。
// ---------------------------------------------------------------------------
TEST(F019Spec, F019_SC001_ToolbarOrder_NoOverlapFitsBand)
{
    RecordProperty("spec", "019:FR-001");
    RecordProperty("spec", "019:SC-001");
    for (int dpi : {96, 120, 144})
    {
        const DiagramRect lastButton = ToolbarButtonRect(kToolbarButtonCount - 1, dpi);
        const DiagramRect box = FrameDisplayBoxRect(dpi);
        const DiagramRect label = FrameDisplayLabelRect(dpi);
        const DiagramRect time = SimTimeAreaRect(dpi);
        const DiagramRect caption = SpeedCaptionRect(dpi);
        const DiagramRect left = SpeedLabelRect(-10, dpi);
        const DiagramRect right = SpeedLabelRect(10, dpi);
        EXPECT_LE(lastButton.Right, box.Left) << dpi << " dpi: ボタン -> チェックボックス";
        EXPECT_LE(box.Right, label.Left) << dpi << " dpi: チェック欄 -> 文字";
        EXPECT_LE(label.Right, time.Left) << dpi << " dpi: チェックボックス -> シミュレーション時間";
        EXPECT_LE(time.Right, caption.Left) << dpi << " dpi: シミュレーション時間 -> 「スピード：」";
        EXPECT_LE(caption.Right, left.Left) << dpi << " dpi: 「スピード：」-> スライダー";
        EXPECT_LE(right.Right, ScaleToDpi(1140, dpi)) << dpi << " dpi: 右端が幅に収まらない";
        for (const DiagramRect& r : {lastButton, box, label, time, caption, left, right, SpeedThumbRect(0, dpi)})
        {
            EXPECT_GE(r.Top, 0) << dpi << " dpi: 帯の上にはみ出す";
            EXPECT_LE(r.Bottom, ScaleToDpi(64, dpi)) << dpi << " dpi: 帯の下にはみ出す";
        }
    }
}

// ---------------------------------------------------------------------------
// FR-004: 数字は 5 刻みと今の値。今の値から 2 目盛り以内の 5 刻みは表示しない。
// 根拠: spec.md FR-004、SimControls.h（IsSpeedLabelShown）。
// ---------------------------------------------------------------------------
TEST(F019Spec, F019_FR004_SpeedLabels_FiveStepsAndCurrent)
{
    RecordProperty("spec", "019:FR-004");
    EXPECT_TRUE(IsSpeedLabelShown(0, 0)) << "今の値";
    EXPECT_TRUE(IsSpeedLabelShown(-5, 0)) << "5 目盛り離れた 5 刻み";
    EXPECT_TRUE(IsSpeedLabelShown(10, 0));
    EXPECT_FALSE(IsSpeedLabelShown(2, 0)) << "5 刻みでも今の値でもない";
    EXPECT_TRUE(IsSpeedLabelShown(3, 3)) << "今の値";
    EXPECT_FALSE(IsSpeedLabelShown(5, 3)) << "今の値から 2 目盛り";
    EXPECT_TRUE(IsSpeedLabelShown(0, 3)) << "今の値から 3 目盛り";
    EXPECT_FALSE(IsSpeedLabelShown(-10, -9)) << "今の値から 1 目盛り";
    EXPECT_FALSE(IsSpeedLabelShown(-5, -7)) << "今の値から 2 目盛り";
}

// ---------------------------------------------------------------------------
// SC-002: 表示される数字どうしが重ならない（今の値が -10〜+10 のどれでも、DPI 96/120/144）。
// 根拠: spec.md SC-002。
// ---------------------------------------------------------------------------
TEST(F019Spec, F019_SC002_ShownLabels_NeverOverlap)
{
    RecordProperty("spec", "019:SC-002");
    for (int dpi : {96, 120, 144})
    {
        for (int current = -10; current <= 10; ++current)
        {
            std::vector<DiagramRect> shown;
            for (int s = -10; s <= 10; ++s)
                if (IsSpeedLabelShown(s, current)) shown.push_back(SpeedLabelRect(s, dpi));
            for (std::size_t i = 1; i < shown.size(); ++i)
                EXPECT_LE(shown[i - 1].Right, shown[i].Left)
                    << dpi << " dpi、今の値 " << current << ": " << i << " 番目の数字が左と重なる";
        }
    }
}
