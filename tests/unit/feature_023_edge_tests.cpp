// feature_023_edge_tests.cpp（tests/unit）
// 機能 023 の補足のテスト（/coverage）。SimControls.h の約束（開始・中断・終了のボタンの有効／無効は GetSimCommandStates と同じ）を確かめる。

#include "support/spec_test_helpers.h"

#include "SimControls.h"

using namespace CanSim;
using namespace spectest;

// 根拠: spec.md FR-008（シミュレーションのボタンの動作は変えない）、SimControls.h（IsToolbarButtonEnabled の開始・中断・終了は
// GetSimCommandStates のとおり）。どの状態・編集中でも、5 つのボタンの並びの開始・中断・終了は、機能 014 の有効／無効と一致する。
TEST(F023Edge, F023_FR008_SimButtons_SameAsCommandStates)
{
    RecordProperty("spec", "023:FR-008");
    for (RunPhase phase : { RunPhase::Idle, RunPhase::Running, RunPhase::Paused })
    {
        for (bool editing : { false, true })
        {
            const SimCommandStates s = GetSimCommandStates(phase, editing);
            EXPECT_EQ(IsToolbarButtonEnabled(static_cast<int>(ToolbarButton::Start), phase, editing), s.Start) << "開始";
            EXPECT_EQ(IsToolbarButtonEnabled(static_cast<int>(ToolbarButton::Pause), phase, editing), s.Pause) << "中断";
            EXPECT_EQ(IsToolbarButtonEnabled(static_cast<int>(ToolbarButton::Stop), phase, editing), s.Stop) << "終了";
        }
    }
}
