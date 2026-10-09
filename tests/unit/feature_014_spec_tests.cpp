// 機能 014（中断とツールバー）の仕様テスト
// 根拠: specs/014-sim-pause-toolbar/spec.md、SimControls.h のコメント。
// ボタンの描画・押し方・メニューとの連動は画面側なのでテストせず、状態ごとの有効／無効・再開の時刻・
// ボタンの範囲を確かめる。ボタンの位置は機能 023 で右へずれた（96 dpi で開始・中断・終了の箱の左端 92・120・148）。

#include <gtest/gtest.h>

#include <climits>

#include "SimControls.h"
#include "support/test_helpers.h"

using namespace CanSim;
using namespace TestHelpers;

// ---------------------------------------------------------------------------
// FR-004 / SC-001: 状態ごとの「開始・中断・終了」の有効／無効（表のとおり。待機の開始は編集中なら無効）。
// 根拠: spec.md FR-004 の表、SC-001、SimControls.h（GetSimCommandStates）。
// ---------------------------------------------------------------------------
struct StateCase
{
    RunPhase Phase;
    bool Editing;
    bool Start;
    bool Pause;
    bool Stop;
    const char* Name;
};

class CommandStateTable : public ::testing::TestWithParam<StateCase>
{
};

TEST_P(CommandStateTable, F014_SC001_PhaseTable_EnabledStatesMatch)
{
    RecordProperty("spec", "014:SC-001");
    RecordProperty("spec", "014:FR-004");
    const StateCase& c = GetParam();
    const SimCommandStates s = GetSimCommandStates(c.Phase, c.Editing);
    EXPECT_EQ(c.Start, s.Start) << c.Name << ": 開始";
    EXPECT_EQ(c.Pause, s.Pause) << c.Name << ": 中断";
    EXPECT_EQ(c.Stop, s.Stop) << c.Name << ": 終了";
}

INSTANTIATE_TEST_SUITE_P(
    Phases, CommandStateTable,
    ::testing::Values(StateCase{RunPhase::Idle, false, true, false, false, "Idle"},
                      StateCase{RunPhase::Idle, true, false, false, false, "IdleEditing"},
                      StateCase{RunPhase::Running, false, false, true, true, "Running"},
                      StateCase{RunPhase::Paused, false, true, false, true, "Paused"}),
    [](const ::testing::TestParamInfo<StateCase>& info) { return std::string(info.param.Name); });

// ---------------------------------------------------------------------------
// FR-005 / FR-006: 再開は中断した時刻から同じスピードで続く。中断の間の実時間は数えない
//   （再開後の時刻 = 中断の時刻 + 再開からの実経過 × 2^スピード）。桁あふれは飽和。
// 根拠: spec.md FR-005、FR-006、SimControls.h（ResumedSimTimeUs）。
// ---------------------------------------------------------------------------
TEST(F014Spec, F014_FR006_Resume_ContinuesFromPausedTime)
{
    RecordProperty("spec", "014:FR-006");
    EXPECT_EQ(3000000, ResumedSimTimeUs(3000000, 0, 0)) << "再開の瞬間は中断の時刻（戻らない・飛ばない）";
    EXPECT_EQ(3500000, ResumedSimTimeUs(3000000, 500000, 0)) << "スピード 0 で 0.5 秒";
    EXPECT_EQ(5000000, ResumedSimTimeUs(3000000, 500000, 2)) << "スピード +2 で 0.5 秒 × 4";
    EXPECT_EQ(250000, ResumedSimTimeUs(0, 1000000, -2)) << "新しい実行は 0 から";
    EXPECT_EQ(LLONG_MAX, ResumedSimTimeUs(LLONG_MAX - 10, 1000000, 0)) << "飽和";
}

// ---------------------------------------------------------------------------
// FR-007 / SC-004: 中断した時刻で「終了」すると、その時刻までに送信を完了したフレームだけが出る
//   （CSV の時刻が中断の時刻を超えない）。終了はその時刻までエンジンを実行し直す。
// 根拠: spec.md FR-007、SC-004、CLAUDE.md（「終了」はその時点のシミュレーション時刻までエンジンを再実行）。
// ---------------------------------------------------------------------------
TEST(F014Spec, F014_SC004_StopAtPausedTime_NoTimeBeyond)
{
    RecordProperty("spec", "014:SC-004");
    Network net;
    net.BitRateBps = 500000;
    net.Nodes.push_back(MakeNode(1, {{0x100, 8, 1000, 0}, {0x101, 8, 1700, 300}}));
    net.Nodes.push_back(MakeNode(2, {{0x080, 8, 2300, 0}}));
    const long long pausedAt = 12345;
    const RunResult run = RunSim(net, pausedAt);
    ASSERT_FALSE(run.Rows.empty());
    for (const LogRow& r : run.Rows)
    {
        EXPECT_LE(r.TxEndUs, pausedAt) << "送信完了が中断の時刻を超えた";
        EXPECT_LE(r.TimestampUs, pausedAt) << "行の時刻が中断の時刻を超えた";
    }
}

// ---------------------------------------------------------------------------
// FR-002 / FR-003: 3 つのボタンの範囲（箱と名前）と当たり判定。SimButtonRect(i) は ToolbarButtonRect(i + 2) と同じ。
// 根拠: spec.md FR-002、FR-003、SimControls.h（kSimButton*、SimButtonRect のコメント）、CLAUDE.md（箱の左端 92・120・148）。
// ---------------------------------------------------------------------------
TEST(F014Spec, F014_FR002_SimButtons_RectsAndHitTest)
{
    RecordProperty("spec", "014:FR-002");
    const int lefts[3] = {92, 120, 148};
    for (int i = 0; i < 3; ++i)
    {
        const DiagramRect box = SimButtonBoxRect(i, 96);
        EXPECT_EQ(lefts[i], box.Left) << i << " 番目の箱の左端";
        EXPECT_EQ(lefts[i] + 24, box.Right) << "箱の幅 24";
        EXPECT_EQ(14, box.Top);
        EXPECT_EQ(32, box.Bottom) << "箱の高さ 18";
        const DiagramRect label = SimButtonLabelRect(i, 96);
        EXPECT_EQ(33, label.Top) << "名前は箱の下";
        EXPECT_EQ(49, label.Bottom);
        for (int dpi : {96, 120, 144})
        {
            const DiagramRect a = SimButtonRect(i, dpi);
            const DiagramRect b = ToolbarButtonRect(i + 2, dpi);
            EXPECT_TRUE(a.Left == b.Left && a.Top == b.Top && a.Right == b.Right && a.Bottom == b.Bottom)
                << dpi << " dpi: SimButtonRect と ToolbarButtonRect が違う";
            EXPECT_EQ(i, FindSimButtonAt(a.Left, a.Top, dpi)) << dpi << " dpi: 左上の角";
            EXPECT_EQ(i, FindSimButtonAt(a.Right - 1, a.Bottom - 1, dpi)) << dpi << " dpi: 右下の角";
            EXPECT_EQ(-1, FindSimButtonAt(a.Left, a.Bottom, dpi)) << dpi << " dpi: 下の外";
        }
    }
    EXPECT_EQ(-1, FindSimButtonAt(20, 20, 96)) << "保存ボタン（シミュレーションのボタンではない）";
}
