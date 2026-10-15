// feature_014_spec_tests.cpp（tests/unit）
// 機能 014（specs/014-sim-pause-toolbar/spec.md）の受け入れシナリオと成功基準のうち、画面を使わずに確かめられるもの。
// 有効／無効は SimControls.h の GetSimCommandStates（メニューとボタンが共通に使う）、ボタンの範囲と当たり判定は
// SimButtonRect・FindSimButtonAt、再開後の時刻は ResumedSimTimeUs で確かめる。描画とボタンの押下の流れは画面の確認。

#include "support/spec_test_helpers.h"

#include "ReplayClock.h"
#include "SimControls.h"

using namespace CanSim;
using namespace spectest;

namespace
{
    std::string Describe(const SimCommandStates& s)
    {
        return std::string("開始=") + (s.Start ? "有効" : "無効") + " 中断=" + (s.Pause ? "有効" : "無効") +
               " 終了=" + (s.Stop ? "有効" : "無効");
    }

    bool Same(const SimCommandStates& s, bool start, bool pause, bool stop)
    {
        return s.Start == start && s.Pause == pause && s.Stop == stop;
    }
}

// 根拠: spec.md US1 受け入れシナリオ 1、FR-002・FR-004。待機では「開始」「中断」「終了」の 3 つのボタンが左から並び、「開始」だけが
// 有効。ボタンの位置は SimButtonRect（96 dpi）で、どのボタンの中を押してもそのボタンになる。
TEST(F014Spec, F014_US1_1_Idle_ThreeButtonsStartOnlyEnabled)
{
    RecordProperty("spec", "014:US1-1");
    const SimCommandStates idle = GetSimCommandStates(RunPhase::Idle, false);
    EXPECT_TRUE(Same(idle, true, false, false)) << "待機の有効／無効: " << Describe(idle);
    for (int i = 0; i < kSimButtonCount; ++i)
    {
        const DiagramRect r = SimButtonRect(i, 96);
        if (i > 0)
            EXPECT_LE(SimButtonRect(i - 1, 96).Right, r.Left) << i << " 番目のボタンが左のボタンと重なる";
        EXPECT_EQ(FindSimButtonAt((r.Left + r.Right) / 2, (r.Top + r.Bottom) / 2, 96), i) << i << " 番目のボタンの真ん中";
        EXPECT_LE(r.Bottom, kToolbarHeight) << i << " 番目のボタンがツールバーの帯の下にはみ出す";
    }
    EXPECT_EQ(FindSimButtonAt(0, 0, 96), -1) << "ボタンの外を押してボタンになった";
}

// 根拠: spec.md US1 受け入れシナリオ 2、FR-004。実行中は「開始」がグレー、「中断」「終了」が有効。
TEST(F014Spec, F014_US1_2_Running_PauseAndStopEnabled)
{
    RecordProperty("spec", "014:US1-2");
    const SimCommandStates running = GetSimCommandStates(RunPhase::Running, false);
    EXPECT_TRUE(Same(running, false, true, true)) << "実行中の有効／無効: " << Describe(running);
}

// 根拠: spec.md US2 受け入れシナリオ 1、FR-004。中断中は「開始」「終了」が有効、「中断」がグレー。
TEST(F014Spec, F014_US2_1_Paused_StartAndStopEnabled)
{
    RecordProperty("spec", "014:US2-1");
    const SimCommandStates paused = GetSimCommandStates(RunPhase::Paused, false);
    EXPECT_TRUE(Same(paused, true, false, true)) << "中断中の有効／無効: " << Describe(paused);
}

// 根拠: spec.md US2 受け入れシナリオ 2、FR-005。中断していた時間はシミュレーション時刻に数えない。再開後の時刻は
// 「中断した時刻 + 再開からの実経過時間 × 2^スピード」で、中断していた長さ（ここでは 10 秒）は入らない。
TEST(F014Spec, F014_US2_2_PausedDuration_NotCounted)
{
    RecordProperty("spec", "014:US2-2");
    const long long pausedAt = 3000000; // 3 秒で中断
    // 10 秒中断して再開した直後（再開からの経過 0）は、中断した時刻のまま
    EXPECT_EQ(ResumedSimTimeUs(pausedAt, 0, 0), pausedAt) << "再開直後の時刻が中断した時刻でない";
    // 再開から 1 秒（スピード 0）
    EXPECT_EQ(ResumedSimTimeUs(pausedAt, 1000000, 0), 4000000) << "再開から 1 秒後の時刻";
}

// 根拠: spec.md US2 受け入れシナリオ 3、FR-006。中断中の「開始」は再開で、中断した時刻から中断前と同じスピードで続く。
// スピード +2 で 0.5 秒進めると 2 秒、スピード -1 で 2 秒進めると 1 秒、中断した時刻に足される。
TEST(F014Spec, F014_US2_3_Resume_ContinuesFromPausedTimeAtSameSpeed)
{
    RecordProperty("spec", "014:US2-3");
    EXPECT_EQ(ResumedSimTimeUs(1234567, 500000, 2), 1234567 + 2000000) << "スピード +2";
    EXPECT_EQ(ResumedSimTimeUs(1234567, 2000000, -1), 1234567 + 1000000) << "スピード -1";
    EXPECT_EQ(ResumedSimTimeUs(1234567, 777, 0), 1234567 + 777) << "スピード 0";
}

// 根拠: spec.md SC-001・FR-004。3 つの状態（待機・実行中・中断中）と編集中のそれぞれで、有効／無効が FR-004 の表と一致する。
// 編集中は待機の「開始」だけが無効になる（機能 010）。
struct StateCase
{
    const char* Name;
    RunPhase Phase;
    bool Editing;
    bool Start, Pause, Stop;
};

class F014StateTable : public ::testing::TestWithParam<StateCase>
{
};

TEST_P(F014StateTable, F014_SC001_StateTable_MatchesFr004)
{
    RecordProperty("spec", "014:SC-001");
    const StateCase& c = GetParam();
    const SimCommandStates s = GetSimCommandStates(c.Phase, c.Editing);
    EXPECT_TRUE(Same(s, c.Start, c.Pause, c.Stop)) << c.Name << ": " << Describe(s);
}

INSTANTIATE_TEST_SUITE_P(
    SC001, F014StateTable,
    ::testing::Values(StateCase{ "Idle", RunPhase::Idle, false, true, false, false },
                      StateCase{ "IdleEditing", RunPhase::Idle, true, false, false, false },
                      StateCase{ "Running", RunPhase::Running, false, false, true, true },
                      StateCase{ "RunningEditing", RunPhase::Running, true, false, true, true },
                      StateCase{ "Paused", RunPhase::Paused, false, true, false, true },
                      StateCase{ "PausedEditing", RunPhase::Paused, true, true, false, true }),
    [](const ::testing::TestParamInfo<StateCase>& info) { return std::string(info.param.Name); });

// 根拠: spec.md SC-003、FR-006・FR-010。中断・再開を 5 回くり返しても、再生の時刻は中断しなかった場合と同じ進み方をする
// （欠け・重複が無い）。実行中の合計の実時間が同じなら、最後の時刻は中断しなかった場合の SimTimeAtUs と一致する。
// CSV は実行の開始時に最後まで計算した結果なので、中断の有無で変わらない（シミュレーションの計算は中断に関わらない）。
TEST(F014Spec, F014_SC003_FivePauseResume_SameClockAsUninterrupted)
{
    RecordProperty("spec", "014:SC-003");
    const int speed = 1;
    const long long segments[6] = { 300000, 120000, 450000, 80000, 600000, 250000 }; // 実行中の実時間（中断の間を除く）
    long long simUs = 0;
    long long totalReal = 0;
    for (long long seg : segments)
    {
        simUs = ResumedSimTimeUs(simUs, seg, speed); // 中断した時刻から、次の実行区間の分だけ進める
        totalReal += seg;
    }
    EXPECT_EQ(simUs, SimTimeAtUs(totalReal, speed)) << "中断・再開をくり返した時刻が、中断しなかった場合と違う";
}

// 根拠: spec.md SC-004、FR-007。中断中に「終了」した場合、出力される CSV の最後の時刻が中断した時刻を超えない
// （機能 005 の「終了」と同じ: その時刻までエンジンを実行した結果）。
TEST(F014Spec, F014_SC004_StopWhilePaused_NoRowAfterPausedTime)
{
    RecordProperty("spec", "014:SC-004");
    Network net;
    net.BitRateBps = 500000;
    net.Nodes.push_back(MakeNode(1, { MakeFrame(0x100, 8, 1300) }));
    net.Nodes.push_back(MakeNode(2, { MakeFrame(0x080, 4, 900, 200) }));
    net.Nodes.push_back(MakeNode(3));
    const long long pausedAt = 45678;
    const std::vector<LogRow> rows = RunSim(net, pausedAt);
    ASSERT_FALSE(rows.empty()) << "前提: ログが空";
    for (const LogRow& r : rows)
    {
        EXPECT_LE(r.TimestampUs, pausedAt) << "中断した時刻より後の行がある";
        EXPECT_LE(r.TxEndUs, pausedAt) << "中断した時刻より後に送信完了した行がある";
    }
}
