// feature_026_spec_tests.cpp（tests/unit）
// 機能 026（specs/026-window-buttons-speed-range/spec.md）の受け入れシナリオと成功基準のうち、画面を使わずに確かめられるもの。
// ウィンドウのボタン・最小化・最大化は画面の確認。スピードの範囲は SimControls.h（スライダーの座標）・ReplayClock.h・
// ConfigLoader（プロジェクトファイル）で確かめる。

#include "support/spec_test_helpers.h"

#include "BusDiagramLayout.h"
#include "ProjectFile.h"
#include "ReplayClock.h"
#include "SimControls.h"

using namespace CanSim;
using namespace spectest;

namespace
{
    std::string SpeedProject(const std::string& speed)
    {
        return "[Project]\r\nSpeed=" + speed + "\r\n[Node1]\r\nNodeId=1\r\n";
    }
}

// 根拠: spec.md US3 受け入れシナリオ 1、FR-004。目盛りは -10〜+10 の 21 個で、左端は -10、右端は +10（目盛りは左から順に並び、
// どの目盛りの位置を押してもその値になる）。数字は 5 刻みと今の値を表示し、今の値から 2 目盛り以内の 5 刻みは隠す（機能 019 と同じ）。
TEST(F026Spec, F026_US3_1_TwentyOneTicks_LabelsEveryFiveAndCurrent)
{
    RecordProperty("spec", "026:US3-1");
    std::set<int> xs;
    for (int s = -10; s <= 10; ++s)
    {
        const int x = SpeedTickX(s, 96);
        if (s > -10)
            EXPECT_GT(x, SpeedTickX(s - 1, 96)) << s << " の目盛りが左から順に並んでいない";
        EXPECT_EQ(SpeedAtX(x, 96), s) << s << " の目盛りの位置を押した値";
        xs.insert(x);
    }
    EXPECT_EQ(xs.size(), 21u) << "目盛りが 21 個でない";

    // 今の値 0: 5 刻み（-10・-5・0・+5・+10）を表示し、ほかは隠す
    for (int s = -10; s <= 10; ++s)
        EXPECT_EQ(IsSpeedLabelShown(s, 0), s % 5 == 0) << "今の値 0 のとき " << s << " の数字";
    // 今の値 +3: +3 を表示し、2 目盛り以内の +5 は隠す。0・-5・-10・+10 は表示
    EXPECT_TRUE(IsSpeedLabelShown(3, 3)) << "今の値 +3 の数字が表示されない";
    EXPECT_FALSE(IsSpeedLabelShown(5, 3)) << "今の値から 2 目盛り以内の +5 が隠れない";
    EXPECT_TRUE(IsSpeedLabelShown(0, 3)) << "+3 から 3 目盛り離れた 0 が表示されない";
    EXPECT_TRUE(IsSpeedLabelShown(10, 3)) << "右端の +10 が表示されない";
    // 今の値 -9: 2 目盛り以内の -10 は隠す
    EXPECT_FALSE(IsSpeedLabelShown(-10, -9)) << "今の値 -9 のとき -10 が隠れない";
}

// 根拠: spec.md US3 受け入れシナリオ 2、FR-004。スライダーの左端より左を押す・左へドラッグすると -10 で止まり、-11 以下にならない。
// 右端より右は +10 で止まる。
TEST(F026Spec, F026_US3_2_BeyondEnds_ClampedToMinus10AndPlus10)
{
    RecordProperty("spec", "026:US3-2");
    for (int dpi : { 96, 120, 144 })
    {
        EXPECT_EQ(SpeedAtX(SpeedTickX(-10, dpi) - 100, dpi), -10) << dpi << " DPI: 左端より左";
        EXPECT_EQ(SpeedAtX(0, dpi), -10) << dpi << " DPI: x 0";
        EXPECT_EQ(SpeedAtX(SpeedTickX(10, dpi) + 100, dpi), 10) << dpi << " DPI: 右端より右";
    }
}

// 根拠: spec.md US3 受け入れシナリオ 3、FR-006。スピード -10 は実時間の 1/1024 で進む。
TEST(F026Spec, F026_US3_3_MinusTen_OneOver1024)
{
    RecordProperty("spec", "026:US3-3");
    EXPECT_EQ(SimTimeAtUs(1024000, -10), 1000) << "実時間 1.024 秒で、シミュレーション時刻が 1 ms 進まない";
    EXPECT_EQ(SimTimeAtUs(10240000, -10), 10000) << "実時間 10.24 秒で、シミュレーション時刻が 10 ms 進まない";
}

// 根拠: spec.md US3 受け入れシナリオ 4・SC-004、FR-005。以前の版のスピード -11〜-13 のプロジェクトファイルは、エラーにせず
// -10 として読み込む。保存すると -10 で書かれる。
TEST(F026Spec, F026_US3_4_LegacyMinus11To13_ReadAsMinus10)
{
    RecordProperty("spec", "026:US3-4");
    for (const char* v : { "-11", "-12", "-13" })
    {
        const ConfigLoadResult r = LoadText(Widen(std::string("f026_us3_4_") + v + ".canprj"), SpeedProject(v));
        ASSERT_TRUE(r.Success) << "スピード " << v << " がエラーになった: " << r.ErrorMessage;
        EXPECT_EQ(r.Project.Speed, -10) << "スピード " << v << " が -10 として読み込まれない";

        const std::wstring saved = TempPath(L"f026_us3_4_saved.canprj");
        ASSERT_TRUE(SaveProject(saved, r.Project, r.Config)) << "保存できない";
        EXPECT_NE(ReadText(saved).find("Speed=-10\r\n"), std::string::npos) << "保存すると -10 で書かれない";
    }
}

// 根拠: spec.md US3 受け入れシナリオ 5、FR-005。スピード -14 以下・+11 以上は範囲外のエラーで、エラーの文言の範囲は -10〜+10。
TEST(F026Spec, F026_US3_5_OutOfRange_ErrorWithMinus10To10)
{
    RecordProperty("spec", "026:US3-5");
    for (const char* v : { "-14", "-100", "11", "50" })
    {
        const ConfigLoadResult r = LoadText(Widen(std::string("f026_us3_5_") + v + ".canprj"), SpeedProject(v));
        EXPECT_FALSE(r.Success) << "スピード " << v << " が読み込めてしまった";
        EXPECT_NE(r.ErrorMessage.find("-10"), std::string::npos) << "エラーの文言に -10 が無い: " << r.ErrorMessage;
        EXPECT_NE(r.ErrorMessage.find("10"), std::string::npos) << "エラーの文言に 10 が無い: " << r.ErrorMessage;
        EXPECT_EQ(r.ErrorMessage.find("-13"), std::string::npos) << "エラーの文言に古い範囲 -13 がある: " << r.ErrorMessage;
    }
    // 範囲の端は読み込める
    EXPECT_EQ(LoadText(L"f026_us3_5_m10.canprj", SpeedProject("-10")).Project.Speed, -10);
    EXPECT_EQ(LoadText(L"f026_us3_5_p10.canprj", SpeedProject("10")).Project.Speed, 10);
}

// 根拠: spec.md SC-003。スライダーで選べる値が -10〜+10 の 21 個ちょうどで、目盛りと数字・つまみが帯の中に重ならずに収まる
// （DPI 96/120/144）。帯の高さは kToolbarHeight（96 dpi で 64、SimControls.h）を DPI で換算したもの。
TEST(F026Spec, F026_SC003_SliderFitsInBand_AtEachDpi)
{
    RecordProperty("spec", "026:SC-003");
    for (int dpi : { 96, 120, 144 })
    {
        const int band = ScaleToDpi(kToolbarHeight, dpi);
        std::set<int> values;
        for (int x = SpeedTickX(-10, dpi) - 50; x <= SpeedTickX(10, dpi) + 50; ++x)
            values.insert(SpeedAtX(x, dpi));
        EXPECT_EQ(values.size(), 21u) << dpi << " DPI: 選べる値が 21 個でない";
        EXPECT_EQ(*values.begin(), -10) << dpi << " DPI: 最小";
        EXPECT_EQ(*values.rbegin(), 10) << dpi << " DPI: 最大";

        for (int s = -10; s <= 10; ++s)
        {
            const DiagramRect thumb = SpeedThumbRect(s, dpi);
            EXPECT_GE(thumb.Top, 0) << dpi << " DPI: " << s << " のつまみが帯の上にはみ出す";
            EXPECT_LE(thumb.Bottom, band) << dpi << " DPI: " << s << " のつまみが帯の下にはみ出す";
            const DiagramRect label = SpeedLabelRect(s, dpi);
            EXPECT_GE(label.Top, 0) << dpi << " DPI: " << s << " の数字が帯の上にはみ出す";
            EXPECT_LE(label.Bottom, band) << dpi << " DPI: " << s << " の数字が帯の下にはみ出す";
        }
        // 表示される数字（今の値 0 のときの 5 刻み）が左右で重ならない
        const int shown[] = { -10, -5, 0, 5, 10 };
        for (int i = 1; i < 5; ++i)
            EXPECT_LE(SpeedLabelRect(shown[i - 1], dpi).Right, SpeedLabelRect(shown[i], dpi).Left)
                << dpi << " DPI: " << shown[i - 1] << " と " << shown[i] << " の数字が重なる";
        // 「スピード：」の文字が -10 の数字と重ならない
        EXPECT_LE(SpeedCaptionRect(dpi).Right, SpeedLabelRect(-10, dpi).Left)
            << dpi << " DPI: 「スピード：」と -10 の数字が重なる";
    }
}
