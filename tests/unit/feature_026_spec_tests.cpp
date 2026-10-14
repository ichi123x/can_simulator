// 機能 026（最小化・最大化のボタンと、スピードの範囲 -10〜+10）の仕様テスト
// 根拠: specs/026-window-buttons-speed-range/spec.md、CanTypes.h（kMinSpeed・kLegacyMinSpeed）、SimControls.h。
// ウィンドウのボタン・最大化の配置は画面側なのでテストせず、スピードの範囲を確かめる。

#include <gtest/gtest.h>

#include <set>

#include "SimControls.h"
#include "FrameEditor.h" 
#include "support/test_helpers.h"

using namespace CanSim;
using namespace TestHelpers;

// ---------------------------------------------------------------------------
// FR-004 / SC-003: スライダーで選べる値は -10〜+10 の 21 個ちょうどで、-11 以下は作らない。
// 根拠: spec.md FR-004、SC-003、SimControls.h（SpeedAtX）。
// ---------------------------------------------------------------------------
TEST(F026Spec, F026_SC003_SliderValues_Exactly21)
{
    RecordProperty("spec", "026:FR-004");
    RecordProperty("spec", "026:SC-003");
    for (int dpi : {96, 120, 144})
    {
        std::set<int> values;
        for (int x = -100; x < ScaleToDpi(1400, dpi); ++x) values.insert(SpeedAtX(x, dpi));
        EXPECT_EQ(21u, values.size()) << dpi << " dpi: 選べる値の数";
        EXPECT_EQ(-10, *values.begin()) << dpi << " dpi: 最小";
        EXPECT_EQ(10, *values.rbegin()) << dpi << " dpi: 最大";
    }
}

// ---------------------------------------------------------------------------
// FR-005 / SC-004: 読込みでは -13〜-11 を -10 として受け入れ、-14 以下・+11 以上・整数でない値はエラー。
// 根拠: spec.md FR-005、SC-004、005 spec.md Edge Cases（整数でない値は読み込まない）。
// ---------------------------------------------------------------------------
TEST(F026Spec, F026_SC004_LegacySpeeds_ReadAsMinus10)
{
    RecordProperty("spec", "026:SC-004");
    RecordProperty("spec", "026:FR-005");
    for (int legacy : {-13, -12, -11})
    {
    //    Network definitions;
    //   EnsureNode(definitions, 1); // ノード 0 個の構成は読込みで拒否されるので、ノードを 1 つ入れておく

        //const ConfigLoadResult r =
        //    LoadFromText("f026_legacy.canprj", "[Project]\nSpeed=" + std::to_string(legacy) + "\n");
        const ConfigLoadResult r =
            LoadFromText("f026_legacy.canprj", "[Project]\nSpeed=" + std::to_string(legacy) + "\n[Node1]\nNodeId=1\n");
        ASSERT_TRUE(r.Success) << legacy << ": " << r.ErrorMessage;
        EXPECT_EQ(-10, r.Project.Speed) << legacy << " は -10 として読む";
    }
    for (int ok : {-10, 0, 10})
    {
    //    Network definitions;
    //    EnsureNode(definitions, 1); // ノード 0 個の構成は読込みで拒否されるので、ノードを 1 つ入れておく
    
        //const ConfigLoadResult r = LoadFromText("f026_ok.canprj", "[Project]\nSpeed=" + std::to_string(ok) + "\n");
        const ConfigLoadResult r = LoadFromText("f026_ok.canprj", "[Project]\nSpeed=" + std::to_string(ok) + "\n[Node1]\nNodeId=1\n");

        ASSERT_TRUE(r.Success) << ok << ": " << r.ErrorMessage;
        EXPECT_EQ(ok, r.Project.Speed);
    }
    for (const char* bad : {"-14", "11", "1.5", "fast", ""})
    {
        const ConfigLoadResult r = LoadFromText("f026_bad.canprj", std::string("[Project]\nSpeed=") + bad + "\n[Node1]\nNodeId=1\n");
        EXPECT_FALSE(r.Success) << "Speed=" << bad << " が受け入れられた";
        if (!r.Success)
            EXPECT_NE(std::string::npos, r.ErrorMessage.find("-10")) << "エラーの文言の範囲は -10〜+10: " << r.ErrorMessage;
    }
}
