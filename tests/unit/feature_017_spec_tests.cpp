// feature_017_spec_tests.cpp（tests/unit）
// 機能 017（specs/017-toolbar-display-time/spec.md）の受け入れシナリオと成功基準のうち、画面を使わずに確かめられるもの。
// チェックボックス・シミュレーション時間の欄の位置と当たり判定、数値の書式は SimControls.h、保存は ProjectFile.h で確かめる。
// 強調の描画と、表示する時刻の更新は画面の確認。

#include "support/spec_test_helpers.h"

#include "ProjectFile.h"
#include "SimControls.h"

using namespace CanSim;
using namespace spectest;

// 根拠: spec.md US1 受け入れシナリオ 1、FR-002・FR-005。起動直後、終了ボタンとスピードスライダーの間に「フレーム表示」の
// チェックボックスがあり、チェックは入っていない（起動時のフレーム表示 OFF）。
TEST(F017Spec, F017_US1_1_CheckBoxBetweenStopAndSlider_UncheckedAtStartup)
{
    RecordProperty("spec", "017:US1-1");
    const DiagramRect box = FrameDisplayBoxRect(96);
    const DiagramRect label = FrameDisplayLabelRect(96);
    EXPECT_GE(box.Left, SimButtonRect(2, 96).Right) << "チェック欄が終了ボタンより左";
    EXPECT_LE(box.Right, label.Left) << "チェック欄が文字の右にある";
    EXPECT_LT(label.Right, SpeedTickX(-10, 96)) << "「フレーム表示」の文字がスピードスライダーより右";
    if (ProjectSettings().FrameDisplay)
        GTEST_SKIP() << "仕様と実装の食い違い（人の判断待ち）: 017 は起動直後のチェックを外れている（フレーム表示 OFF）とするが、"
                        "実装は機能 021 のとおり ON。017 の記述が 021 で置き換わったかの判断が必要";
    EXPECT_FALSE(ProjectSettings().FrameDisplay) << "起動直後のフレーム表示";
}

// 根拠: spec.md US1 受け入れシナリオ 2、FR-003。チェック欄または文字をクリックすると切り替わる（どちらも当たり判定に入る）。
// 反転そのものはダイアログの処理（画面の確認）。
TEST(F017Spec, F017_US1_2_ClickOnBoxOrLabel_HitsToggle)
{
    RecordProperty("spec", "017:US1-2");
    for (int dpi : { 96, 120, 144 })
    {
        const DiagramRect box = FrameDisplayBoxRect(dpi);
        const DiagramRect label = FrameDisplayLabelRect(dpi);
        EXPECT_TRUE(IsOnFrameDisplayToggle((box.Left + box.Right) / 2, (box.Top + box.Bottom) / 2, dpi))
            << dpi << " DPI: チェック欄の真ん中";
        EXPECT_TRUE(IsOnFrameDisplayToggle((label.Left + label.Right) / 2, (label.Top + label.Bottom) / 2, dpi))
            << dpi << " DPI: 文字の真ん中";
        const DiagramRect stop = SimButtonRect(2, dpi);
        EXPECT_FALSE(IsOnFrameDisplayToggle((stop.Left + stop.Right) / 2, (stop.Top + stop.Bottom) / 2, dpi))
            << dpi << " DPI: 終了ボタンの上で切り替わる";
    }
}

// 根拠: spec.md US1 受け入れシナリオ 6・SC-003、FR-005。チェックを入れて保存し、外してから読み込み直すとチェックが入った状態に戻る。
// ON・OFF の両方で保存・読込みで保たれる。
TEST(F017Spec, F017_US1_6_FrameDisplaySavedAndLoaded)
{
    RecordProperty("spec", "017:US1-6");
    Network defs;
    defs.Nodes.push_back(MakeNode(1));
    for (bool on : { true, false })
    {
        ProjectSettings s;
        s.FrameDisplay = on;
        const std::wstring path = TempPath(L"f017_us1_6.canprj");
        ASSERT_TRUE(SaveProject(path, s, defs));
        const ConfigLoadResult r = LoadConfig(path);
        ASSERT_TRUE(r.Success) << r.ErrorMessage;
        EXPECT_EQ(r.Project.FrameDisplay, on) << "保存・読込みでフレーム表示が変わった";
    }
}

// 根拠: spec.md US2 受け入れシナリオ 1、FR-006。ツールバーの右端（スピードスライダーの右）に、上の段に「シミュレーション時間」の
// ラベル、下の段に数値と単位「μs」が並ぶ。
TEST(F017Spec, F017_US2_1_SimTimeArea_CaptionAboveValueAndUnit)
{
    RecordProperty("spec", "017:US2-1");
    const DiagramRect caption = SimTimeCaptionRect(96);
    const DiagramRect value = SimTimeValueRect(96);
    const DiagramRect unit = SimTimeUnitRect(96);
    EXPECT_LE(caption.Bottom, value.Top) << "ラベルが数値の上の段にない";
    EXPECT_LE(value.Right, unit.Left) << "単位が数値の右にない";
    EXPECT_LE(SimTimeAreaRect(96).Bottom, kToolbarHeight) << "シミュレーション時間の欄がツールバーの帯の下にはみ出す";
    if (SimTimeAreaRect(96).Left < SpeedTickX(10, 96))
        GTEST_SKIP() << "仕様と実装の食い違い（人の判断待ち）: 017 はシミュレーション時間をツールバーの右端（スピードスライダーの右）"
                        "とするが、実装は機能 019 のとおりチェックボックスとスライダーの間。017 の記述が 019 で置き換わったかの判断が必要";
    EXPECT_GT(SimTimeAreaRect(96).Left, SpeedTickX(10, 96)) << "スピードスライダーの右にない";
}

// 根拠: spec.md US2 受け入れシナリオ 3・SC-004、FR-007。数値はけたを省略せず、3 けたごとに区切る（最大 600 秒 = 600,000,000 μs）。
// 欄からはみ出さないことは、文字の幅に依るので画面の確認。
TEST(F017Spec, F017_US2_3_SimTime_GroupedByThreeDigits)
{
    RecordProperty("spec", "017:US2-3");
    EXPECT_EQ(FormatSimTimeUs(0), "0") << "起動直後の 0";
    EXPECT_EQ(FormatSimTimeUs(999), "999");
    EXPECT_EQ(FormatSimTimeUs(1000), "1,000");
    EXPECT_EQ(FormatSimTimeUs(10000000), "10,000,000") << "10 秒の終端";
    EXPECT_EQ(FormatSimTimeUs(600000000), "600,000,000") << "最大 600 秒";
    EXPECT_EQ(FormatSimTimeUs(1234567), "1,234,567");
}

// 根拠: spec.md SC-004、FR-007（「終了」後の表示は停止時刻、自然終了後は終端の時刻。表示は 3 けた区切り）。表示する時刻の
// 取り方は画面の処理なので、ここでは停止時刻・終端の時刻の書式が終了メッセージと同じ 3 けた区切りになることを確かめる。
TEST(F017Spec, F017_SC004_StopAndEndTimes_FormattedForDisplay)
{
    RecordProperty("spec", "017:SC-004");
    EXPECT_EQ(FormatSimTimeUs(3456789), "3,456,789") << "「終了」した時刻";
    EXPECT_EQ(FormatSimTimeUs(600LL * 1000000), "600,000,000") << "600 秒の終端";
    EXPECT_EQ(FormatSimTimeUs(-5), "0") << "負の値は 0";
}
