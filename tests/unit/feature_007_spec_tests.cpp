// 機能 007（実行中のメニューの制限とインターミッション）の仕様テスト
// 根拠: specs/007-run-menu-lock/spec.md。メニューの制限（FR-001〜FR-003、SC-001・SC-002）は画面側なのでテストせず、
// バスのインターミッション（FR-005〜FR-007、SC-003・SC-004）を確かめる。
// FR-004（実行中はフレーム表示を切り替えられない）は機能 017 で「いつでも切り替えられる」に変わった。

#include <gtest/gtest.h>

#include <algorithm>
#include <vector>

#include "FrameEvents.h"
#include "FrameTiming.h"
#include "ReplayClock.h"
#include "support/test_helpers.h"

using namespace CanSim;
using namespace TestHelpers;

namespace
{
    // 4 ECU が同時に要求して送信待ちが続く構成（500 kbps、1 ビット時間 2 us）
    Network MakeBusyNetwork()
    {
        Network net;
        net.BitRateBps = 500000;
        net.Nodes.push_back(MakeNode(1, {{0x100, 8, 2000, 0}}));
        net.Nodes.push_back(MakeNode(2, {{0x101, 4, 2000, 0}}));
        net.Nodes.push_back(MakeNode(3, {{0x102, 0, 2000, 0}}));
        net.Nodes.push_back(MakeNode(4, {{0x103, 8, 2000, 0}}));
        return net;
    }
}

// ---------------------------------------------------------------------------
// FR-005 / SC-003: 送信待ちが続いている区間では、連続する 2 フレームの「後の送信開始 − 前の送信終了」が
//   3 ビット時間（500 kbps で 6 us）。
// 根拠: spec.md FR-005、SC-003。
// ---------------------------------------------------------------------------
TEST(F007Spec, F007_SC003_BackToBackFrames_IntermissionThreeBits)
{
    RecordProperty("spec", "007:SC-003");
    RecordProperty("spec", "007:FR-005");
    std::vector<LogRow> tx = UniqueTransmissions(RunSim(MakeBusyNetwork(), 20000LL).Rows);
    std::sort(tx.begin(), tx.end(), [](const LogRow& a, const LogRow& b) { return a.TxStartUs < b.TxStartUs; });
    int checked = 0;
    for (std::size_t i = 1; i < tx.size(); ++i)
    {
        // 後のフレームが前のフレームの送信中に要求されていた（＝送信待ちが続いている）組だけを見る
        if (tx[i].TxRequestUs < tx[i - 1].TxEndUs)
        {
            ++checked;
            EXPECT_EQ(6, tx[i].TxStartUs - tx[i - 1].TxEndUs) << i << " 番目: 送信待ちの間の空きが 3 ビット時間でない";
        }
    }
    EXPECT_GE(checked, 20) << "前提: 送信待ちが続く組が十分にある";
}

// ---------------------------------------------------------------------------
// FR-006 / SC-004: ログの伝送時間（Tx_Start〜Tx_End）は SOF〜EOF だけで、インターミッションを含まない
//   （= そのフレームのビット数 × ビット時間）。
// 根拠: spec.md FR-006、SC-004。ビット数そのものの手計算との一致は機能 001 SC-003 で確かめる。
// ---------------------------------------------------------------------------
TEST(F007Spec, F007_FR006_LoggedDuration_ExcludesIntermission)
{
    RecordProperty("spec", "007:FR-006");
    RecordProperty("spec", "007:SC-004");
    const Network net = MakeBusyNetwork();
    for (const LogRow& r : UniqueTransmissions(RunSim(net, 10000LL).Rows))
    {
        const FrameTimingResult t = ComputeFrameTiming(r.CanId, r.Dlc, std::vector<std::uint8_t>(r.Dlc, 0xFF), 500000);
        EXPECT_EQ(t.TotalBits * 2, r.TxEndUs - r.TxStartUs)
            << "0x" << std::hex << r.CanId << ": 伝送時間 = ビット数 × 2 us（インターミッション 6 us を含まない）";
    }
}

// ---------------------------------------------------------------------------
// FR-007: フレーム表示でバスを「使用中」とするのは SOF〜EOF の間だけ。インターミッションの間はバスは空き。
// 根拠: spec.md FR-007、ReplayClock.h（送信 [TxStartUs, TxEndUs] と重なるフレームを集める）。
// ---------------------------------------------------------------------------
TEST(F007Spec, F007_FR007_IntermissionOnly_BusNotBusy)
{
    RecordProperty("spec", "007:FR-007");
    std::vector<FrameEvent> ev(2);
    ev[0].TxStartUs = 100; ev[0].TxEndUs = 350; ev[0].TxNodeId = 1; ev[0].RxNodeIds = {2};
    ev[1].TxStartUs = 356; ev[1].TxEndUs = 600; ev[1].TxNodeId = 2; ev[1].RxNodeIds = {1}; // 350〜356 がインターミッション
    ActiveFrameCursor cursor(ev);
    EXPECT_TRUE(cursor.Collect(200, 300).BusBusy) << "送信中";
    const ActiveFrames gap = cursor.Collect(350, 355);
    EXPECT_FALSE(gap.BusBusy) << "インターミッションだけの間はバスは使用中ではない";
    EXPECT_TRUE(gap.Senders.empty());
}
