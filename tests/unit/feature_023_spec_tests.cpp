// 機能 023（ツールバーの保存・読込みボタン）の仕様テスト
// 根拠: specs/023-toolbar-file-buttons/spec.md、SimControls.h のコメント、CLAUDE.md（96 dpi で箱の左端 8・36・92・120・148）。
// ファイル選択・上書き保存・メッセージは画面側なのでテストせず、ボタンの位置・当たり判定・有効／無効を確かめる。

#include <gtest/gtest.h>

#include "SimControls.h"

using namespace CanSim;

// ---------------------------------------------------------------------------
// FR-001 / FR-002: 左から 保存・読込み・（空き 1 つ分）・開始・中断・終了。すべて同じ大きさ。
// 根拠: spec.md FR-001、FR-002、SimControls.h（ToolbarButton、slot）、CLAUDE.md。
// ---------------------------------------------------------------------------
TEST(F023Spec, F023_FR001_ButtonOrderAndSameSize)
{
    RecordProperty("spec", "023:FR-001");
    RecordProperty("spec", "023:FR-002");
    const int lefts[5] = {8, 36, 92, 120, 148};
    for (int i = 0; i < kToolbarButtonCount; ++i)
    {
        const DiagramRect b = ToolbarButtonBoxRect(i, 96);
        EXPECT_EQ(lefts[i], b.Left) << i << " 番目の箱の左端";
        EXPECT_EQ(24, b.Right - b.Left) << i << " 番目の箱の幅";
        EXPECT_EQ(18, b.Bottom - b.Top) << i << " 番目の箱の高さ";
    }
    EXPECT_EQ(0, static_cast<int>(ToolbarButton::Save));
    EXPECT_EQ(1, static_cast<int>(ToolbarButton::Load));
    EXPECT_EQ(2, static_cast<int>(ToolbarButton::Start));
}

// FR-007: 当たり判定は箱と名前。読込みと開始の間の空きでは -1。
// 根拠: spec.md FR-007、SimControls.h（FindToolbarButtonAt: -1 also on the free slot）。
TEST(F023Spec, F023_FR007_HitTest_FreeSlotIsNothing)
{
    RecordProperty("spec", "023:FR-007");
    for (int dpi : {96, 120, 144})
    {
        for (int i = 0; i < kToolbarButtonCount; ++i)
        {
            const DiagramRect r = ToolbarButtonRect(i, dpi);
            EXPECT_EQ(i, FindToolbarButtonAt((r.Left + r.Right) / 2, (r.Top + r.Bottom) / 2, dpi)) << dpi << " dpi, " << i;
            const DiagramRect label = ToolbarButtonLabelRect(i, dpi);
            EXPECT_EQ(i, FindToolbarButtonAt((label.Left + label.Right) / 2, label.Top, dpi)) << dpi << " dpi: 名前の上";
        }
        const int freeX = (ToolbarButtonRect(1, dpi).Right + ToolbarButtonRect(2, dpi).Left) / 2;
        EXPECT_EQ(-1, FindToolbarButtonAt(freeX, ScaleToDpi(23, dpi), dpi)) << dpi << " dpi: 空き";
    }
}

// ---------------------------------------------------------------------------
// FR-006 / SC-003: 保存・読込みは待機中だけ有効（編集ウィンドウが開いていても有効）。実行中・中断中は無効。
//   開始・中断・終了は機能 014 の表のまま。
// 根拠: spec.md FR-006、SC-003、SimControls.h（IsToolbarButtonEnabled）。
// ---------------------------------------------------------------------------
TEST(F023Spec, F023_FR006_FileButtons_EnabledOnlyWhileIdle)
{
    RecordProperty("spec", "023:FR-006");
    RecordProperty("spec", "023:SC-003");
    for (int i : {0, 1})
    {
        EXPECT_TRUE(IsToolbarButtonEnabled(i, RunPhase::Idle, false)) << i << ": 待機";
        EXPECT_TRUE(IsToolbarButtonEnabled(i, RunPhase::Idle, true)) << i << ": 待機・編集中";
        EXPECT_FALSE(IsToolbarButtonEnabled(i, RunPhase::Running, false)) << i << ": 実行中";
        EXPECT_FALSE(IsToolbarButtonEnabled(i, RunPhase::Paused, false)) << i << ": 中断中";
    }
    EXPECT_FALSE(IsToolbarButtonEnabled(2, RunPhase::Idle, true)) << "開始は編集中なら無効（014）";
    EXPECT_TRUE(IsToolbarButtonEnabled(3, RunPhase::Running, false)) << "中断は実行中に有効（014）";
    EXPECT_TRUE(IsToolbarButtonEnabled(4, RunPhase::Paused, false)) << "終了は中断中に有効（014）";
}
