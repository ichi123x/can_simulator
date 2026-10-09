// feature_019_tests.cpp（tests）
// tools/cansim_runner/checks.cpp の CheckFeature019() から移した（機能 030）。
// 本体は、パスの取り方以外は変えていない。Check() のラベルには SC/FR の ID が残っている。

#include "test_helpers.h"

using namespace cansimtest;

TEST(Feature019, FR002to005)
{
    std::cout << "\n== 機能 019: ツールバーの小型化 ==\n";
    bool half = true;
    for (int i = 0; i < kSimButtonCount; ++i)
    {
        const DiagramRect box = SimButtonBoxRect(i, 96);
        half = half && box.Right - box.Left == 24 && box.Bottom - box.Top == 18;
    }
    half = half && SpeedTickX(kMinSpeed + 1, 96) - SpeedTickX(kMinSpeed, 96) == 13 &&
        SpeedThumbRect(0, 96).Right - SpeedThumbRect(0, 96).Left <= 5;
    Check(half, "019 FR-002/FR-003: ボタンは 24 x 18、スライダーの目盛りは 13 間隔（半分の大きさ、96 DPI）");

    auto shown = [](int current) {
        std::vector<int> v;
        for (int s = kMinSpeed; s <= kMaxSpeed; ++s)
            if (IsSpeedLabelShown(s, current))
                v.push_back(s);
        return v;
    };
    Check(shown(0) == std::vector<int>{ -10, -5, 0, 5, 10 } &&
        shown(-9) == std::vector<int>{ -9, -5, 0, 5, 10 } &&
        shown(9) == std::vector<int>{ -10, -5, 0, 5, 9 } &&
        shown(3) == std::vector<int>{ -10, -5, 0, 3, 10 },
        "019 FR-004: 5 刻みの値と今の値を表示し、今の値から 2 目盛り以内の 5 刻みの値は隠す");

    bool caption = true;
    const int dpis[] = { 96, 120, 144 };
    for (int dpi : dpis)
    {
        const DiagramRect c = SpeedCaptionRect(dpi);
        caption = caption && c.Right <= SpeedTickX(kMinSpeed, dpi) - ScaleToDpi(kSpeedTickPitch / 2, dpi) &&
            c.Left >= SimTimeAreaRect(dpi).Right && c.Bottom <= ScaleToDpi(kToolbarHeight, dpi) &&
            !IsOnSpeedSlider((c.Left + c.Right) / 2, (c.Top + c.Bottom) / 2, dpi);
    }
    Check(caption, "019 FR-005: 「スピード：」の文字はシミュレーション時間とスライダーの間にあり、スライダーの当たり判定の外にある");
}
