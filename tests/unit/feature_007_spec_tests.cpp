// feature_007_spec_tests.cpp（tests/unit）
// 機能 007（specs/007-run-menu-lock/spec.md）の受け入れシナリオと成功基準のうち、画面を使わずに確かめられるもの。
// US2（インターミッション）はシミュレーションのログで確かめる。US1（実行中のメニューのロック）は画面の確認。

#include "support/spec_test_helpers.h"

#include "FrameTiming.h"

using namespace CanSim;
using namespace spectest;

namespace
{
    // 送信待ちが続く構成: ノード 1・2 が同時に送信要求する（2 番目は 1 番目の後に送信待ちから送る）
    Network BackToBack(long long bitRate)
    {
        Network net;
        net.BitRateBps = bitRate;
        net.Nodes.push_back(MakeNode(1, { MakeFrame(0x100, 8, 1000000) }));
        net.Nodes.push_back(MakeNode(2, { MakeFrame(0x080, 8, 1000000) }));
        return net;
    }
}

// 根拠: spec.md US2 受け入れシナリオ 1、FR（Doc FR-021 のインターミッション 3 ビット）。送信待ちのフレームがある状態で送信が終わると、
// 次のフレームの送信開始は前のフレームの EOF の終わりから、ちょうど 3 ビット時間後（125 kbps で 24 µs、250 kbps で 12 µs、
// 500 kbps で 6 µs、1 Mbps で 3 µs）。
struct RateCase
{
    long long BitRate;
    long long ThreeBitsUs;
};

class F007Intermission : public ::testing::TestWithParam<RateCase>
{
};

TEST_P(F007Intermission, F007_US2_1_Pending_NextStartsThreeBitsAfterEof)
{
    RecordProperty("spec", "007:US2-1");
    const RateCase& c = GetParam();
    const std::vector<LogRow> rows = RunSim(BackToBack(c.BitRate), 100000LL);
    const LogRow* first = FirstRowOf(rows, 0x080);
    const LogRow* second = FirstRowOf(rows, 0x100);
    ASSERT_NE(first, nullptr);
    ASSERT_NE(second, nullptr);
    EXPECT_EQ(second->TxStartUs - first->TxEndUs, c.ThreeBitsUs) << c.BitRate << " bps: 次の送信開始と前の EOF の差";
}

INSTANTIATE_TEST_SUITE_P(US2, F007Intermission,
                         ::testing::Values(RateCase{ 125000, 24 }, RateCase{ 250000, 12 }, RateCase{ 500000, 6 },
                                           RateCase{ 1000000, 3 }),
                         [](const ::testing::TestParamInfo<RateCase>& info) {
                             return std::to_string(info.param.BitRate) + "bps";
                         });

// 根拠: spec.md US2 受け入れシナリオ 2。送信待ちが無い状態で送信が終わり、3 ビット時間が経つ前に新しい送信要求が出たら、そのフレームは
// インターミッションの終わりを待ってから送信を始める。手計算（500 kbps）: ノード 1 の 0x100 は 22〜268 µs に送り、休止期間は 274 µs まで。
// ノード 2 の 0x080 は初回オフセット 248 µs なので 22 + 248 = 270 µs に要求し、休止期間の終わり 274 µs に送り始める。
TEST(F007Spec, F007_US2_2_RequestDuringIntermission_WaitsForItsEnd)
{
    RecordProperty("spec", "007:US2-2");
    Network net;
    net.BitRateBps = 500000;
    net.Nodes.push_back(MakeNode(1, { MakeFrame(0x100, 8, 1000000) }));
    net.Nodes.push_back(MakeNode(2, { MakeFrame(0x080, 8, 1000000, 248) }));
    const std::vector<LogRow> rows = RunSim(net, 10000LL);
    const LogRow* late = FirstRowOf(rows, 0x080);
    ASSERT_NE(late, nullptr);
    EXPECT_EQ(late->TxRequestUs, 270) << "前提: 要求の時刻";
    EXPECT_EQ(late->TxStartUs, 274) << "インターミッションの終わりを待たずに送信した";
}

// 根拠: spec.md US2 受け入れシナリオ 3・SC-004。各フレームの伝送時間（Tx_Start〜Tx_End）にインターミッションは含まれず、
// SOF〜EOF のビット数 × ビット時間に一致する。
TEST(F007Spec, F007_US2_3_TxDuration_ExcludesIntermission)
{
    RecordProperty("spec", "007:US2-3");
    const std::vector<LogRow> rows = RunSim(BackToBack(500000), 100000LL);
    ASSERT_FALSE(rows.empty());
    for (const LogRow& r : rows)
    {
        const FrameTimingResult t = ComputeFrameTiming(r.CanId, r.Dlc, std::vector<std::uint8_t>(r.Dlc, 0xFF), 500000);
        EXPECT_EQ((r.TxEndUs - r.TxStartUs) * 1000, t.TxDurationNs) << "CAN ID " << r.CanId << " の伝送時間";
    }
}

// 根拠: spec.md SC-003。送信待ちが続いている区間のすべての連続する 2 フレームで、後の送信開始 − 前の送信終了 = 3 ビット時間（全通信速度で一致）。
// 5 つのノードが同時に要求する構成で、5 フレームが続けて送られる区間を確かめる。
TEST(F007Spec, F007_SC003_BackToBackFrames_GapIsThreeBitsAtEveryRate)
{
    RecordProperty("spec", "007:SC-003");
    for (long long rate : { 125000LL, 250000LL, 500000LL, 1000000LL })
    {
        Network net;
        net.BitRateBps = rate;
        for (int i = 0; i < 5; ++i)
            net.Nodes.push_back(MakeNode(i + 1, { MakeFrame(0x100 + i, static_cast<unsigned>(i), 1000000) }));
        const std::vector<LogRow> rows = RunSim(net, 100000LL);
        std::vector<LogRow> tx;
        for (unsigned id = 0x100; id < 0x105; ++id)
            tx.push_back(Transmissions(rows, id).front());
        const long long threeBitsUs = 3 * 1000000 / rate;
        for (std::size_t i = 1; i < tx.size(); ++i)
            EXPECT_EQ(tx[i].TxStartUs - tx[i - 1].TxEndUs, threeBitsUs) << rate << " bps: " << i << " 番目のフレームの前の間";
    }
}

// 根拠: spec.md SC-004。すべてのフレームの伝送時間が、そのフレームのビット数（スタッフビット込み）× ビット時間と一致し、
// インターミッションを含まない。いろいろな ID・DLC・通信速度で確かめる。
TEST(F007Spec, F007_SC004_EveryFrame_DurationIsBitsTimesBitTime)
{
    RecordProperty("spec", "007:SC-004");
    for (long long rate : { 125000LL, 1000000LL })
    {
        Network net;
        net.BitRateBps = rate;
        net.Nodes.push_back(MakeNode(1, { MakeFrame(0x000, 0, 3000), MakeFrame(0x7FF, 8, 5000, 100) }));
        net.Nodes.push_back(MakeNode(2, { MakeFrame(0x555, 1, 4000, 50), MakeFrame(0x123, 5, 7000, 300) }));
        const std::vector<LogRow> rows = RunSim(net, 100000LL);
        ASSERT_FALSE(rows.empty());
        for (const LogRow& r : rows)
        {
            const FrameTimingResult t = ComputeFrameTiming(r.CanId, r.Dlc, std::vector<std::uint8_t>(r.Dlc, 0xFF), rate);
            EXPECT_EQ((r.TxEndUs - r.TxStartUs) * 1000, static_cast<long long>(t.TotalBits) * t.BitTimeNs)
                << rate << " bps・CAN ID " << r.CanId << " の伝送時間";
        }
    }
}
