// feature_016_spec_tests.cpp（tests/unit）
// 機能 016（specs/016-receiver-blue-highlight/spec.md）の受け入れシナリオと成功基準のうち、画面を使わずに確かめられるもの。
// 青く強調する受信 ECU の集まりは ReplayClock.h の ActiveFrameCursor で確かめる。色・線の太さは画面の確認。

#include "support/spec_test_helpers.h"

#include "FrameEvents.h"
#include "ReplayClock.h"

using namespace CanSim;
using namespace spectest;

namespace
{
    // ノード 1 が 0x100 を周期 2 ms で送る。ノード 2 はすべて受信、ノード 3 は 0x200 だけを受信（受信フィルタ）、
    // ノード 4 は 50 ms に電源投入（それまでは受信しない）
    Network ReceiverNetwork()
    {
        Network net;
        net.BitRateBps = 500000;
        net.Nodes.push_back(MakeNode(1, { MakeFrame(0x100, 8, 2000) }));
        net.Nodes.push_back(MakeNode(2));
        Node filtered = MakeNode(3);
        filtered.RxFilterAll = false;
        filtered.RxFilter = { 0x200 };
        net.Nodes.push_back(filtered);
        Node late = MakeNode(4);
        late.PowerOnTimeUs = 50000;
        net.Nodes.push_back(late);
        return net;
    }
}

// 根拠: spec.md US1 受け入れシナリオ 1、FR（受信 ECU を青で強調）。フレームの送信中、そのフレームを受信している ECU が受信の強調の対象になる。
TEST(F016Spec, F016_US1_1_FrameInFlight_ReceiversHighlighted)
{
    RecordProperty("spec", "016:US1-1");
    const std::vector<FrameEvent> events = BuildFrameEvents(RunSim(ReceiverNetwork(), 100000LL));
    ASSERT_FALSE(events.empty());
    const FrameEvent& late = events.back(); // 50 ms より後のフレーム（ノード 4 も受信する）
    ActiveFrameCursor cursor(events);
    const ActiveFrames a = cursor.Collect(late.TxStartUs - 1, late.TxEndUs);
    EXPECT_EQ(a.Senders, (std::vector<int>{ 1 })) << "送信 ECU";
    EXPECT_EQ(a.Receivers, (std::vector<int>{ 2, 4 })) << "受信 ECU";
}

// 根拠: spec.md US1 受け入れシナリオ 3。そのフレームを受信しない ECU（受信フィルタで受けない ECU、起動前の ECU）は強調されない。
TEST(F016Spec, F016_US1_3_NonReceivers_NotHighlighted)
{
    RecordProperty("spec", "016:US1-3");
    const std::vector<FrameEvent> events = BuildFrameEvents(RunSim(ReceiverNetwork(), 100000LL));
    ActiveFrameCursor cursor(events);
    const ActiveFrames early = cursor.Collect(0, 10000); // ノード 4 の電源投入より前
    EXPECT_EQ(early.Receivers, (std::vector<int>{ 2 })) << "受信フィルタのノード 3・起動前のノード 4 が受信の対象に入っている";
}

// 根拠: spec.md US1 受け入れシナリオ 4・SC-002。送信が終わり、次のフレームまでバスが空いている間は、受信 ECU の強調は消える。
TEST(F016Spec, F016_US1_4_IdleBus_NoReceiverHighlight)
{
    RecordProperty("spec", "016:US1-4");
    const std::vector<FrameEvent> events = BuildFrameEvents(RunSim(ReceiverNetwork(), 100000LL));
    ASSERT_GE(events.size(), 2u);
    ActiveFrameCursor cursor(events);
    const ActiveFrames idle = cursor.Collect(events[0].TxEndUs + 10, events[1].TxStartUs - 10);
    EXPECT_TRUE(idle.Receivers.empty()) << "バスが空いているのに受信の強調がある";
    EXPECT_TRUE(idle.Senders.empty()) << "バスが空いているのに送信の強調がある";
    EXPECT_FALSE(idle.BusBusy) << "バスが空いているのに使用中";
}

// 根拠: spec.md SC-002。バスが空いている間（強調の対象のフレームが無い書き換え）には、青い ECU が 1 つも無い。
// 実行全体を 16 ms ごとの書き換えに分け、フレームが無い書き換えでは受信の対象が空であることを確かめる。
TEST(F016Spec, F016_SC002_EveryIdleRefresh_NoReceiver)
{
    RecordProperty("spec", "016:SC-002");
    Network net = ReceiverNetwork();
    net.Nodes[0].Frames[0].PeriodUs = 40000; // 書き換えの間にフレームの無い区間を作る
    const std::vector<FrameEvent> events = BuildFrameEvents(RunSim(net, 200000LL));
    ActiveFrameCursor cursor(events);
    int idleRefreshes = 0;
    for (long long t = 0; t < 200000; t += 16000)
    {
        const ActiveFrames a = cursor.Collect(t, t + 16000);
        if (!a.BusBusy)
        {
            ++idleRefreshes;
            EXPECT_TRUE(a.Receivers.empty()) << t << " µs: フレームが無いのに受信の対象がある";
        }
    }
    EXPECT_GT(idleRefreshes, 0) << "前提: フレームの無い書き換えが無い";
}
