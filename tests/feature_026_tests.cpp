// feature_026_tests.cpp（tests）
// tools/cansim_runner/checks.cpp の CheckFeature026() から移した（機能 030）。
// 本体は、パスの取り方以外は変えていない。Check() のラベルには SC/FR の ID が残っている。

#include "test_helpers.h"

using namespace cansimtest;

TEST(Feature026, SC003to004_FR004to005)
{
    std::cout << "\n== 機能 026: スピードの範囲 -10..+10 ==\n";
    const int dpis[] = { 96, 120, 144 };
    bool ticks = kMinSpeed == -10 && kMaxSpeed == 10 && SpeedTickX(-10, 96) == 604 && SpeedTickX(10, 96) == 864 &&
        kMaxSpeed - kMinSpeed + 1 == 21;
    bool fits = true;
    for (int dpi : dpis)
    {
        ticks = ticks && SpeedAtX(0, dpi) == -10 && SpeedAtX(ScaleToDpi(2000, dpi), dpi) == 10 &&
            SpeedTickX(-13, dpi) == SpeedTickX(-10, dpi); // 範囲より小さい値は -10 の位置に描く
        const int band = ScaleToDpi(kToolbarHeight, dpi);
        fits = fits && SpeedLabelRect(10, dpi).Right <= ScaleToDpi(kDiagramWidth, dpi) &&
            SpeedLabelRect(10, dpi).Bottom <= band &&
            SpeedCaptionRect(dpi).Right <= SpeedTickX(-10, dpi) - ScaleToDpi(kSpeedTickPitch / 2, dpi);
    }
    Check(ticks, "026 FR-004: 目盛りは 21 個で、-10 は x 604、+10 は x 864（96 DPI）。SpeedAtX は -10 / +10 で止まる");
    Check(fits, "026 SC-003: +10 の数字と「スピード：」の文字は帯の中に収まる（DPI 96/120/144）");

    struct Case { const char* Text; int Expected; };
    const Case legacy[] = { { "-13", -10 }, { "-12", -10 }, { "-11", -10 }, { "-10", -10 }, { "10", 10 } };
    bool read = true;
    for (const Case& c : legacy)
    {
        ConfigLoadResult r = LoadText("s026_ok.canprj", std::string("[Project]\r\nSpeed=") + c.Text + "\r\n");
        read = read && r.Success && r.Project.Speed == c.Expected;
    }
    Check(read, "026 FR-005/SC-004: 古いファイルの Speed -13 / -12 / -11 は -10 として読み込まれ、-10 と 10 はそのまま読まれる");

    bool rejected = true;
    const char* bad[] = { "-14", "11" };
    for (const char* text : bad)
    {
        ConfigLoadResult r = LoadText("s026_bad.canprj", std::string("[Project]\r\nSpeed=") + text + "\r\n");
        rejected = rejected && !r.Success && r.ErrorMessage.find("-10 and 10") != std::string::npos;
    }
    Check(rejected, "026 FR-005: Speed の -14 と 11 は拒否され、メッセージに -10 と 10 が示される");
}
