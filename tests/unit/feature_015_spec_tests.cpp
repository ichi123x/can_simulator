// 機能 015（スピードスライダー）の仕様テスト
// 根拠: specs/015-speed-slider/spec.md、SimControls.h のコメント、CLAUDE.md。
// 範囲は機能 026 で -10〜+10（21 個）に変わり、位置は機能 019・023 で変わった（96 dpi で -10 が x 604、+10 が x 864）。
// ドラッグの捕捉・描画は画面側なのでテストせず、目盛り・丸め・当たり判定・スピード変更時の動作を確かめる。

#include <gtest/gtest.h>

#include <cstdlib>

#include "SimControls.h"

using namespace CanSim;

// ---------------------------------------------------------------------------
// FR-002: 目盛りは等間隔（96 dpi で 13 px）。左端 -10 は x 604、右端 +10 は x 864。
// 根拠: spec.md FR-002、SimControls.h（kSpeedSliderLeft = 604、kSpeedTickPitch = 13）、CLAUDE.md。
// ---------------------------------------------------------------------------
TEST(F015Spec, F015_FR002_Ticks_EvenlySpaced)
{
    RecordProperty("spec", "015:FR-002");
    EXPECT_EQ(604, SpeedTickX(-10, 96));
    EXPECT_EQ(734, SpeedTickX(0, 96)) << "604 + 10 × 13";
    EXPECT_EQ(864, SpeedTickX(10, 96));
    for (int s = -10; s < 10; ++s) EXPECT_EQ(13, SpeedTickX(s + 1, 96) - SpeedTickX(s, 96)) << "間隔 " << s;
    EXPECT_EQ(604, SpeedTickX(-13, 96)) << "範囲外は端に抑える";
    EXPECT_EQ(864, SpeedTickX(11, 96)) << "範囲外は端に抑える";
}

// ---------------------------------------------------------------------------
// FR-005 / SC-001: どこで押しても一番近い目盛りの値になる（目盛り以外では止まらない）。スライダーの外は端の値。
// 根拠: spec.md FR-005、SC-001、SimControls.h（SpeedAtX）。
// ---------------------------------------------------------------------------
TEST(F015Spec, F015_SC001_AnyX_NearestTick)
{
    RecordProperty("spec", "015:SC-001");
    RecordProperty("spec", "015:FR-005");
    EXPECT_EQ(-10, SpeedAtX(604 + 6, 96)) << "-10 から 6 px（-9 から 7 px）";
    EXPECT_EQ(-9, SpeedAtX(604 + 7, 96)) << "-9 から 6 px";
    EXPECT_EQ(-10, SpeedAtX(0, 96)) << "左の外";
    EXPECT_EQ(10, SpeedAtX(2000, 96)) << "右の外";
    for (int dpi : {96, 120, 144})
    {
        for (int x = SpeedTickX(-10, dpi) - 20; x <= SpeedTickX(10, dpi) + 20; ++x)
        {
            const int s = SpeedAtX(x, dpi);
            ASSERT_GE(s, -10);
            ASSERT_LE(s, 10);
            const int d = std::abs(x - SpeedTickX(s, dpi));
            for (int t = -10; t <= 10; ++t)
                EXPECT_LE(d, std::abs(x - SpeedTickX(t, dpi))) << dpi << " dpi, x " << x << ": " << t << " の方が近い";
        }
    }
}

// SimControls.h: 当たり判定は端の目盛りから半目盛り外まで、y は 4〜60。
TEST(F015Spec, F015_FR005_HitArea_HalfPitchBeyondEnds)
{
    RecordProperty("spec", "015:FR-005");
    EXPECT_TRUE(IsOnSpeedSlider(604, 20, 96));
    EXPECT_TRUE(IsOnSpeedSlider(864, 20, 96));
    EXPECT_TRUE(IsOnSpeedSlider(604 - 6, 20, 96)) << "半目盛り（6.5 px）の内側";
    EXPECT_FALSE(IsOnSpeedSlider(604 - 8, 20, 96)) << "半目盛りの外";
    EXPECT_FALSE(IsOnSpeedSlider(864 + 8, 20, 96)) << "半目盛りの外";
    EXPECT_TRUE(IsOnSpeedSlider(700, 4, 96)) << "上端";
    EXPECT_FALSE(IsOnSpeedSlider(700, 3, 96)) << "上の外";
    EXPECT_FALSE(IsOnSpeedSlider(700, 61, 96)) << "下の外";
}

// FR-003: つまみは縦長の棒で、今の値の目盛りの位置にある。
TEST(F015Spec, F015_FR003_Thumb_VerticalBarOnTick)
{
    RecordProperty("spec", "015:FR-003");
    for (int s = -10; s <= 10; ++s)
    {
        const DiagramRect t = SpeedThumbRect(s, 96);
        EXPECT_GT(t.Bottom - t.Top, t.Right - t.Left) << "縦長";
        EXPECT_EQ(SpeedTickX(s, 96), (t.Left + t.Right) / 2) << "目盛りの位置 " << s;
    }
}

// ---------------------------------------------------------------------------
// FR-006〜FR-009: スピードを変えたときの動作。待機中は値だけ、実行中は今の時刻から新しいスピード、
//   中断中は再開から、実行中・中断中の +10 はその場で完了。
// 根拠: spec.md FR-006〜FR-009、SimControls.h（GetSpeedChangeAction）。
// ---------------------------------------------------------------------------
TEST(F015Spec, F015_FR007_SpeedChange_ActionByPhase)
{
    RecordProperty("spec", "015:FR-007");
    RecordProperty("spec", "015:FR-009");
    EXPECT_EQ(SpeedChangeAction::None, GetSpeedChangeAction(RunPhase::Idle, 3)) << "待機";
    EXPECT_EQ(SpeedChangeAction::None, GetSpeedChangeAction(RunPhase::Idle, 10)) << "待機の +10";
    EXPECT_EQ(SpeedChangeAction::Rebase, GetSpeedChangeAction(RunPhase::Running, -4)) << "実行中";
    EXPECT_EQ(SpeedChangeAction::FinishNow, GetSpeedChangeAction(RunPhase::Running, 10)) << "実行中の +10";
    EXPECT_EQ(SpeedChangeAction::None, GetSpeedChangeAction(RunPhase::Paused, 5)) << "中断中は再開から";
    EXPECT_EQ(SpeedChangeAction::FinishNow, GetSpeedChangeAction(RunPhase::Paused, 10)) << "中断中の +10";
}

// FR-002 / FR-003: 溝は左端の目盛り（-10）から右端の目盛り（+10）まで。つまみは溝を覆う。
// 根拠: spec.md FR-002・FR-003、SimControls.h（SpeedTrackRect、kSpeedTrackY = 20、kSpeedTrackThickness = 2）。
TEST(F015Spec, F015_FR002_Track_FromFirstToLastTick)
{
    RecordProperty("spec", "015:FR-002");
    const DiagramRect t = SpeedTrackRect(96);
    EXPECT_EQ(604, t.Left);
    EXPECT_EQ(864, t.Right);
    EXPECT_EQ(19, t.Top) << "y 20 を中心に太さ 2";
    EXPECT_EQ(21, t.Bottom);
    for (int dpi : {120, 144})
    {
        const DiagramRect r = SpeedTrackRect(dpi);
        EXPECT_EQ(SpeedTickX(-10, dpi), r.Left) << dpi << " dpi";
        EXPECT_EQ(SpeedTickX(10, dpi), r.Right) << dpi << " dpi";
        const DiagramRect thumb = SpeedThumbRect(0, dpi);
        EXPECT_LE(thumb.Top, r.Top) << dpi << " dpi: つまみが溝を覆う";
        EXPECT_GE(thumb.Bottom, r.Bottom) << dpi << " dpi: つまみが溝を覆う";
    }
}
