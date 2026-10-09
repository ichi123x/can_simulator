// feature_015_edge_tests.cpp（tests）
// スピードのスライダーの溝（SpeedTrackRect）の単体テスト。
// カバレッジで実行されていなかった SpeedTrackRect を通す。
// 期待値は specs/015 の research.md・tasks.md（溝は端の目盛りを結ぶ横線、y の中央は kSpeedTrackY）に従う。
// SimControls.cpp の 88 行目（IsToolbarButtonEnabled で、ボタンの番号が範囲外のとき false）は、
// 画面からは範囲外の番号が来ない守りなのでテストしない（2026-10-09 判断）。

#include "test_helpers.h"

using namespace cansimtest;

// 溝は最小と最大のスピードの目盛りを左右の端とし、kSpeedTrackY を中央に kSpeedTrackThickness の太さで描く
TEST(Feature015Edge, SpeedTrackRect_SpansTicksAtTrackY)
{
    for (int dpi : { 96, 144 })
    {
        const DiagramRect r = SpeedTrackRect(dpi);

        EXPECT_EQ(r.Left, SpeedTickX(kMinSpeed, dpi)) << "dpi " << dpi;
        EXPECT_EQ(r.Right, SpeedTickX(kMaxSpeed, dpi)) << "dpi " << dpi;
        EXPECT_EQ(r.Top, ScaleToDpi(kSpeedTrackY - kSpeedTrackThickness / 2, dpi)) << "dpi " << dpi;
        EXPECT_EQ(r.Bottom, ScaleToDpi(kSpeedTrackY + kSpeedTrackThickness / 2, dpi)) << "dpi " << dpi;
        EXPECT_LT(r.Top, r.Bottom) << "dpi " << dpi << ": 溝に太さがある";
    }
}
