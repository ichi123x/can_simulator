// feature_005_edge_tests.cpp（tests/unit）
// 機能 005・014 の補足のテスト（/coverage）。ReplayClock.h・SimControls.h の約束（桁あふれせずに飽和する）を確かめる。

#include "support/spec_test_helpers.h"

#include "ReplayClock.h"
#include "SimControls.h"

#include <climits>

using namespace CanSim;
using namespace spectest;

// 根拠: ReplayClock.h（SimTimeAtUs は桁あふれせずに飽和する）、spec.md FR-003。非常に長い実時間・速いスピードでも、時刻は
// 最大値で止まり、負や小さな値に回り込まない。
TEST(F005Edge, F005_FR003_HugeElapsed_SaturatesInsteadOfOverflow)
{
    RecordProperty("spec", "005:FR-003");
    EXPECT_EQ(SimTimeAtUs(LLONG_MAX / 2, 9), LLONG_MAX) << "2^9 倍で桁あふれした";
    EXPECT_EQ(SimTimeAtUs(LLONG_MAX, 1), LLONG_MAX) << "2 倍で桁あふれした";
}

// 根拠: SimControls.h（ResumedSimTimeUs は中断の時刻 + SimTimeAtUs で、桁あふれせずに飽和する）、014 spec.md FR-006。
TEST(F005Edge, F014_FR006_ResumedTime_SaturatesInsteadOfOverflow)
{
    RecordProperty("spec", "014:FR-006");
    EXPECT_EQ(ResumedSimTimeUs(LLONG_MAX - 5, 100, 0), LLONG_MAX) << "中断の時刻に足して桁あふれした";
    EXPECT_EQ(ResumedSimTimeUs(LLONG_MAX - 100, 100, 0), LLONG_MAX) << "ちょうど最大値";
}
