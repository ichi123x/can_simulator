// feature_005_spec_tests.cpp（tests/unit）
// 機能 005（specs/005-simulation-speed/spec.md）の受け入れシナリオと成功基準のうち、画面と実時間の計測を使わずに確かめられるもの。
// 時計は ReplayClock.h の SimTimeAtUs、強調の対象は ActiveFrameCursor、保存は ProjectFile.h で確かめる。

#include "support/spec_test_helpers.h"

#include "FrameEvents.h"
#include "MetricsAggregator.h"
#include "ProjectFile.h"
#include "ReplayClock.h"

using namespace CanSim;
using namespace spectest;

namespace
{
    FrameEvent FrameEv(long long startUs, long long endUs, int sender, std::vector<int> receivers)
    {
        FrameEvent e;
        e.TxStartUs = startUs;
        e.TxEndUs = endUs;
        e.TxNodeId = sender;
        e.RxNodeIds = receivers;
        return e;
    }
}

// 根拠: spec.md US1 受け入れシナリオ 3・FR-002。スピードの初期値は 0。スピードを持たないプロジェクトも 0 として読み込む。
// （セッティングでの表示は機能 015 でツールバーのスライダーに移った。画面の確認）
TEST(F005Spec, F005_US1_3_DefaultSpeed_Zero)
{
    RecordProperty("spec", "005:US1-3");
    EXPECT_EQ(ProjectSettings().Speed, 0) << "スピードの初期値";
    const ConfigLoadResult r = LoadText(L"f005_us1_3.canprj", "[Project]\r\nEcuCount=2\r\n[Node1]\r\nNodeId=1\r\n");
    ASSERT_TRUE(r.Success) << r.ErrorMessage;
    EXPECT_EQ(r.Project.Speed, 0) << "スピードを持たないプロジェクトのスピード";
}

// 根拠: spec.md US2 受け入れシナリオ 2・FR-003・FR-005。スピード s のとき、シミュレーション時刻は実際の経過時間の 2^s 倍で進む。
// シミュレーション時間 T の完了までの実時間が T ÷ 2^s になる元の計算（例: +1 は 2 倍、−1 は 1/2、+3 は 8 倍、−10 は 1/1024）。
struct SpeedCase
{
    int Speed;
    long long RealUs;
    long long SimUs;
};

class F005SpeedScale : public ::testing::TestWithParam<SpeedCase>
{
};

TEST_P(F005SpeedScale, F005_US2_2_SimTime_IsRealTimesTwoToTheSpeed)
{
    RecordProperty("spec", "005:US2-2");
    const SpeedCase& c = GetParam();
    EXPECT_EQ(SimTimeAtUs(c.RealUs, c.Speed), c.SimUs) << "スピード " << c.Speed << " の シミュレーション時刻";
}

INSTANTIATE_TEST_SUITE_P(
    US2, F005SpeedScale,
    ::testing::Values(SpeedCase{ 0, 3000000, 3000000 }, SpeedCase{ 1, 5000000, 10000000 },
                      SpeedCase{ -1, 20000000, 10000000 }, SpeedCase{ 3, 1250000, 10000000 },
                      SpeedCase{ -10, 1024000, 1000 }, SpeedCase{ 9, 1000, 512000 }),
    [](const ::testing::TestParamInfo<SpeedCase>& info) {
        const int s = info.param.Speed;
        return (s < 0 ? "minus" + std::to_string(-s) : "plus" + std::to_string(s));
    });

// 根拠: spec.md US2 受け入れシナリオ 5・FR-002。スピードを変えて保存したプロジェクトを読み込むと、スピードも保存時の値に戻る。
TEST(F005Spec, F005_US2_5_SpeedSavedAndLoaded)
{
    RecordProperty("spec", "005:US2-5");
    for (int speed : { -10, -3, 0, 4, 10 })
    {
        ProjectSettings s;
        s.Speed = speed;
        Network defs;
        defs.Nodes.push_back(MakeNode(1));
        const std::wstring path = TempPath(L"f005_us2_5.canprj");
        ASSERT_TRUE(SaveProject(path, s, defs)) << "保存できない";
        const ConfigLoadResult r = LoadConfig(path);
        ASSERT_TRUE(r.Success) << r.ErrorMessage;
        EXPECT_EQ(r.Project.Speed, speed) << "保存したスピードに戻らない";
    }
}

namespace
{
    // スピード・フレーム表示を変えて、同じ定義の実行用ネットワークで最後まで実行する
    std::vector<LogRow> RunWith(int speed, bool frameDisplay, Network* after)
    {
        ProjectSettings s;
        s.EcuCount = 3;
        s.Speed = speed;
        s.FrameDisplay = frameDisplay;
        Network defs;
        defs.Nodes.push_back(MakeNode(1, { MakeFrame(0x100, 8, 2000) }));
        defs.Nodes.push_back(MakeNode(2, { MakeFrame(0x080, 4, 3000, 500) }));
        defs.Nodes.push_back(MakeNode(3, { MakeFrame(0x200, 0, 1000, 100) }));
        return RunSim(BuildRunNetwork(s, defs), 200000LL, after);
    }
}

// 根拠: spec.md US3 受け入れシナリオ 2・SC-005、FR-007・FR-013。スピード（0 / +3 / +10）とフレーム表示（ON / OFF）の組み合わせを
// 変えて最後まで実行しても、3 種の CSV は完全に一致する。所要時間の差は実時間の計測なので手動の確認。
TEST(F005Spec, F005_SC005_SpeedAndDisplay_IdenticalCsv)
{
    RecordProperty("spec", "005:SC-005");
    Network refAfter;
    const std::vector<LogRow> ref = RunWith(0, true, &refAfter);
    ASSERT_FALSE(ref.empty()) << "前提: ログが空";
    const std::wstring refBase = TempPath(L"f005_sc005_ref");
    WriteLogCsv(ref, refBase + L".log.csv");
    for (int speed : { 0, 3, 10 })
    {
        for (bool display : { true, false })
        {
            Network after;
            const std::vector<LogRow> rows = RunWith(speed, display, &after);
            const std::wstring base = TempPath(L"f005_sc005_" + std::to_wstring(speed) + (display ? L"_on" : L"_off"));
            WriteLogCsv(rows, base + L".log.csv");
            EXPECT_EQ(ReadText(base + L".log.csv"), ReadText(refBase + L".log.csv"))
                << "スピード " << speed << "・表示 " << display << " でログの CSV が違う";
        }
    }
}

// 根拠: spec.md US3 受け入れシナリオ 2、FR-013（スピードとフレーム表示で結果を変えない）。SC-005 と同じ確かめ方で、
// 集計・起動記録も含めて ON と OFF で一致することを確かめる（所要時間の差は手動の確認）。
TEST(F005Spec, F005_US3_2_DisplayOnOff_IdenticalAllCsv)
{
    RecordProperty("spec", "005:US3-2");
    Network a1, a2;
    const std::vector<LogRow> on = RunWith(0, true, &a1);
    const std::vector<LogRow> off = RunWith(0, false, &a2);
    ASSERT_EQ(on.size(), off.size()) << "行数が違う";
    for (std::size_t i = 0; i < on.size(); ++i)
        EXPECT_TRUE(SameRow(on[i], off[i])) << i << " 行目が違う";
    const std::wstring a = TempPath(L"f005_us3_2_on.summary.csv"), b = TempPath(L"f005_us3_2_off.summary.csv");
    WriteMetricsCsv(ComputeMetrics(on, a1), a);
    WriteMetricsCsv(ComputeMetrics(off, a2), b);
    EXPECT_EQ(ReadText(a), ReadText(b)) << "集計の CSV が違う";
}

// 根拠: spec.md US4 受け入れシナリオ 1・FR-009。画面の書き換え 1 回の間に 3 つのフレームが送信されたら、その 3 フレームの
// 送信元 ECU すべてが送信、受信した ECU すべてが受信の対象になり、バスは使用中。
TEST(F005Spec, F005_US4_1_ThreeFramesInOneRefresh_AllHighlighted)
{
    RecordProperty("spec", "005:US4-1");
    const std::vector<FrameEvent> events = { FrameEv(100, 200, 1, { 4 }), FrameEv(210, 300, 2, { 5 }),
                                             FrameEv(310, 400, 3, { 6 }) };
    ActiveFrameCursor cursor(events);
    const ActiveFrames a = cursor.Collect(50, 450);
    EXPECT_EQ(a.Senders, (std::vector<int>{ 1, 2, 3 })) << "送信の対象";
    EXPECT_EQ(a.Receivers, (std::vector<int>{ 4, 5, 6 })) << "受信の対象";
    EXPECT_TRUE(a.BusBusy) << "バスが使用中になっていない";
}

// 根拠: spec.md US4 受け入れシナリオ 2。画面の書き換え 1 回の間にフレームが 1 つもない（バスが空いている）ときは、どの ECU も強調されない。
TEST(F005Spec, F005_US4_2_NoFrameInRefresh_NothingHighlighted)
{
    RecordProperty("spec", "005:US4-2");
    const std::vector<FrameEvent> events = { FrameEv(100, 200, 1, { 2 }), FrameEv(1000, 1100, 2, { 1 }) };
    ActiveFrameCursor cursor(events);
    const ActiveFrames a = cursor.Collect(300, 900);
    EXPECT_TRUE(a.Senders.empty()) << "送信の対象がある";
    EXPECT_TRUE(a.Receivers.empty()) << "受信の対象がある";
    EXPECT_FALSE(a.BusBusy) << "バスが使用中になっている";
}

// 根拠: spec.md US4 受け入れシナリオ 3。同じ ECU が同じ書き換えの間に送信と受信の両方をしたら、送信の強調が優先される。
TEST(F005Spec, F005_US4_3_SendAndReceiveInSameRefresh_SendWins)
{
    RecordProperty("spec", "005:US4-3");
    const std::vector<FrameEvent> events = { FrameEv(100, 200, 1, { 2, 3 }), FrameEv(210, 300, 2, { 1, 3 }) };
    ActiveFrameCursor cursor(events);
    const ActiveFrames a = cursor.Collect(0, 400);
    EXPECT_EQ(a.Senders, (std::vector<int>{ 1, 2 })) << "送信の対象";
    EXPECT_EQ(a.Receivers, (std::vector<int>{ 3 })) << "送信もした ECU 1・2 が受信の対象に入っている";
}

// 根拠: spec.md US5 受け入れシナリオ 2・SC-008、FR-012。実行中に「終了」を選ぶと、その時点のシミュレーション時刻（例: 約 3 秒）までに
// 送信を完了したフレームだけを含む結果が出る。3 秒で終了した行が、最後まで実行したログのうち送信完了が 3 秒以下の行と一致する。
TEST(F005Spec, F005_US5_2_StopAt3s_OnlyFramesCompletedBy3s)
{
    RecordProperty("spec", "005:US5-2");
    Network net;
    net.BitRateBps = 500000;
    net.Nodes.push_back(MakeNode(1, { MakeFrame(0x100, 8, 7000) }));
    net.Nodes.push_back(MakeNode(2, { MakeFrame(0x080, 8, 11000, 300) }));
    const long long t = 3000000;
    const std::vector<LogRow> full = RunSim(net, 10000000LL);
    std::vector<LogRow> expected;
    for (const LogRow& r : full)
        if (r.TxEndUs <= t)
            expected.push_back(r);
    const std::vector<LogRow> stopped = RunSim(net, t);
    ASSERT_EQ(stopped.size(), expected.size()) << "3 秒までに送信を完了したフレームの行数と違う";
    for (std::size_t i = 0; i < stopped.size(); ++i)
        EXPECT_TRUE(SameRow(stopped[i], expected[i])) << i << " 行目が違う";
}

// 根拠: spec.md SC-008、FR-012。スピード 0 で実行中に「終了」を選ぶと、終了時点までに送信を完了したフレームの数と、
// 出力されたログのフレーム数が一致する。いくつかの終了時刻で確かめる。
TEST(F005Spec, F005_SC008_StopTimes_FrameCountMatches)
{
    RecordProperty("spec", "005:SC-008");
    Network net;
    net.BitRateBps = 125000;
    net.Nodes.push_back(MakeNode(1, { MakeFrame(0x100, 8, 4000) }));
    net.Nodes.push_back(MakeNode(2, { MakeFrame(0x080, 1, 2500, 100) }));
    net.Nodes.push_back(MakeNode(3));
    const std::vector<LogRow> full = RunSim(net, 1000000LL);
    for (long long t : { 999LL, 12345LL, 250000LL, 777777LL })
    {
        std::size_t completed = 0;
        for (const LogRow& r : full)
            if (r.TxEndUs <= t)
                ++completed;
        EXPECT_EQ(RunSim(net, t).size(), completed) << t << " µs で終了したときの行数";
    }
}
