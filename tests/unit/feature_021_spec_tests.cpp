// feature_021_spec_tests.cpp（tests/unit）
// 機能 021（specs/021-frame-display-default-on/spec.md）の受け入れシナリオと成功基準のうち、画面を使わずに確かめられるもの。
// フレーム表示の既定値（CanTypes.h の ProjectSettings）と、プロジェクトファイルの読込み（ConfigLoader）で確かめる。

#include "support/spec_test_helpers.h"

using namespace CanSim;
using namespace spectest;

// 根拠: spec.md US1 受け入れシナリオ 1・SC-001、FR-001。起動直後はフレーム表示のチェックが入っている（既定値が ON）。
TEST(F021Spec, F021_US1_1_Startup_FrameDisplayOn)
{
    RecordProperty("spec", "021:US1-1");
    EXPECT_TRUE(ProjectSettings().FrameDisplay) << "起動直後のフレーム表示が ON でない";
}

// 根拠: spec.md US1 受け入れシナリオ 3、FR-002。FrameDisplay=0 と書かれたプロジェクトを読み込むと、チェックは外れる（ファイルの値が優先）。
TEST(F021Spec, F021_US1_3_FileWithZero_Off)
{
    RecordProperty("spec", "021:US1-3");
    const ConfigLoadResult r = LoadText(L"f021_us1_3.canprj", "[Project]\r\nFrameDisplay=0\r\n[Node1]\r\nNodeId=1\r\n");
    ASSERT_TRUE(r.Success) << r.ErrorMessage;
    EXPECT_FALSE(r.Project.FrameDisplay) << "FrameDisplay=0 なのに ON";
}

// 根拠: spec.md US1 受け入れシナリオ 4、FR-001。FrameDisplay= の行が無いプロジェクトファイルを読み込むと、チェックが入る（既定値）。
TEST(F021Spec, F021_US1_4_FileWithoutKey_On)
{
    RecordProperty("spec", "021:US1-4");
    const ConfigLoadResult r = LoadText(L"f021_us1_4.canprj", "[Project]\r\nEcuCount=3\r\n[Node1]\r\nNodeId=1\r\n");
    ASSERT_TRUE(r.Success) << r.ErrorMessage;
    EXPECT_TRUE(r.Project.FrameDisplay) << "FrameDisplay= の無いファイルで OFF";
    const ConfigLoadResult noProject = LoadText(L"f021_us1_4b.canprj", "[Node1]\r\nNodeId=1\r\n");
    ASSERT_TRUE(noProject.Success) << noProject.ErrorMessage;
    EXPECT_TRUE(noProject.Project.FrameDisplay) << "[Project] の無いファイルで OFF";
}

// 根拠: spec.md SC-001、FR-001。起動直後にチェックが入っている。FrameDisplay=1 のファイルも ON のまま。
TEST(F021Spec, F021_SC001_DefaultAndExplicitOne_On)
{
    RecordProperty("spec", "021:SC-001");
    EXPECT_TRUE(ProjectSettings().FrameDisplay) << "起動直後";
    const ConfigLoadResult r = LoadText(L"f021_sc001.canprj", "[Project]\r\nFrameDisplay=1\r\n[Node1]\r\nNodeId=1\r\n");
    ASSERT_TRUE(r.Success) << r.ErrorMessage;
    EXPECT_TRUE(r.Project.FrameDisplay) << "FrameDisplay=1";
}
