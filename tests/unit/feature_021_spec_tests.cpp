// 機能 021（フレーム表示の既定を ON に）の仕様テスト
// 根拠: specs/021-frame-display-default-on/spec.md、CanTypes.h（ProjectSettings::FrameDisplay = true）、
//        CLAUDE.md（起動時は ON、FrameDisplay= の無いファイルも ON）。
// 起動直後のチェックボックスの見た目は画面側なのでテストしない。

#include <gtest/gtest.h>

#include "CanTypes.h"
#include "support/test_helpers.h"

using namespace CanSim;
using namespace TestHelpers;

TEST(F021Spec, F021_FR001_DefaultSettings_FrameDisplayOn)
{
    RecordProperty("spec", "021:FR-001");
    const ProjectSettings s;
    EXPECT_TRUE(s.FrameDisplay) << "起動時（既定）のフレーム表示は ON";
}

TEST(F021Spec, F021_FR002_FileWithoutFrameDisplay_LoadedAsOn)
{
    RecordProperty("spec", "021:FR-002");
    const ConfigLoadResult r = LoadFromText("f021_nokey.canprj", "[Project]\nEcuCount=2\nDurationSec=10\n");
    ASSERT_TRUE(r.Success) << r.ErrorMessage;
    EXPECT_TRUE(r.Project.FrameDisplay) << "FrameDisplay= の無いファイルは ON";
    const ConfigLoadResult off = LoadFromText("f021_off.canprj", "[Project]\nFrameDisplay=0\n");
    ASSERT_TRUE(off.Success) << off.ErrorMessage;
    EXPECT_FALSE(off.Project.FrameDisplay) << "0 と書いてあれば OFF";
}
