// feature_017_tests.cpp（tests）
// tools/cansim_runner/checks.cpp の CheckFeature017() から移した（機能 030）。
// 本体は、パスの取り方以外は変えていない。Check() のラベルには SC/FR の ID が残っている。

#include "test_helpers.h"

using namespace cansimtest;

TEST(Feature017, FR003_FR007)
{
    std::cout << "\n== 機能 017: フレーム表示チェックボックスとシミュレーション時間 ==\n";
    const int dpis[] = { 96, 120, 144 };
    bool order = true, inside = true, toggle = true;
    for (int dpi : dpis)
    {
        const int band = ScaleToDpi(kToolbarHeight, dpi);
        const int hitLeft = ScaleToDpi(kFrameDisplayHitLeft, dpi);
        const int hitRight = ScaleToDpi(kFrameDisplayHitRight, dpi);
        const int sliderLeft = SpeedTickX(kMinSpeed, dpi) - ScaleToDpi(kSpeedTickPitch / 2, dpi);
        const int sliderRight = SpeedTickX(kMaxSpeed, dpi) + ScaleToDpi(kSpeedTickPitch / 2, dpi);
        const DiagramRect area = SimTimeAreaRect(dpi);
        // 019: シミュレーション時間は「スピード：」の文字とスライダーより前に来る
        order = order && SimButtonRect(2, dpi).Right < hitLeft && hitRight <= area.Left &&
            area.Right <= SpeedCaptionRect(dpi).Left && SpeedCaptionRect(dpi).Right <= sliderLeft &&
            sliderRight <= ScaleToDpi(kDiagramWidth, dpi);

        const DiagramRect box = FrameDisplayBoxRect(dpi);
        const DiagramRect label = FrameDisplayLabelRect(dpi);
        const DiagramRect value = SimTimeValueRect(dpi);
        const DiagramRect unit = SimTimeUnitRect(dpi);
        const DiagramRect caption = SimTimeCaptionRect(dpi);
        inside = inside && area.Right <= ScaleToDpi(kDiagramWidth, dpi) && area.Bottom <= band &&
            box.Left >= hitLeft && box.Right <= label.Left && label.Right <= hitRight &&
            box.Top >= ScaleToDpi(kFrameDisplayHitTop, dpi) && label.Bottom <= ScaleToDpi(kFrameDisplayHitBottom, dpi) &&
            ScaleToDpi(kFrameDisplayHitBottom, dpi) <= band &&
            value.Right <= unit.Left && caption.Bottom <= value.Top && unit.Right <= area.Right;

        const DiagramRect stop = SimButtonRect(2, dpi);
        toggle = toggle && IsOnFrameDisplayToggle((box.Left + box.Right) / 2, (box.Top + box.Bottom) / 2, dpi) &&
            IsOnFrameDisplayToggle((label.Left + label.Right) / 2, (label.Top + label.Bottom) / 2, dpi) &&
            !IsOnFrameDisplayToggle((stop.Left + stop.Right) / 2, (stop.Top + stop.Bottom) / 2, dpi) &&
            !IsOnFrameDisplayToggle(SpeedTickX(kMinSpeed, dpi), ScaleToDpi(20, dpi), dpi) &&
            !IsOnFrameDisplayToggle((box.Left + box.Right) / 2, band + 1, dpi);
    }
    Check(order, "017 / 019 FR-001: ボタン・チェックボックス・シミュレーション時間・スピード：・スライダーは重ならずに左から右に並ぶ（DPI 96/120/144）");
    Check(inside, "017: すべてがツールバーの帯とウィンドウ幅の中に収まり、箱と文字は当たり判定の範囲の中にある");
    Check(toggle, "017 FR-003: IsOnFrameDisplayToggle は箱と文字だけを含む");
    Check(FormatSimTimeUs(0) == "0" && FormatSimTimeUs(999) == "999" && FormatSimTimeUs(1000) == "1,000" &&
        FormatSimTimeUs(1234567) == "1,234,567" && FormatSimTimeUs(10000000) == "10,000,000" &&
        FormatSimTimeUs(600000000) == "600,000,000" && FormatSimTimeUs(-5) == "0",
        "017 FR-007: FormatSimTimeUs は 3 けたごとに区切る");
}
