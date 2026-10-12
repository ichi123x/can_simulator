// 練習 001 の解答例（practice_001_tests.cpp の各レッスンを埋めたもの）
// 自分で書き終えてから見比べること。書き方は 1 つではないので、同じ性質を確かめていれば正解。
// 根拠: specs/001-can-simulation-core/spec.md（FR-002, FR-005, FR-010, FR-016, SC-003, SC-004）。
// 起動直後は 11 ビット時間（500 kbps で 22 us）のバス同期検出を経て通常動作に入るので、最初の送信要求は 22 us。

#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <vector>

#include "CanTypes.h"
#include "ConfigLoader.h"
#include "FrameTiming.h"
#include "support/test_helpers.h"

using namespace CanSim;
using namespace TestHelpers;

namespace
{
    const long long kNormalEntryUs = 22;
}

// ---------------------------------------------------------------------------
// レッスン 1: 最小のテスト（関数を 1 回呼んで、戻り値を EXPECT_EQ で比べる）
// 根拠: SC-003。0x7FF・DLC 0 は手計算で総ビット 47・スタッフ 3（feature_001_spec_tests.cpp と同じ値）。
//       500 kbps の 1 ビット = 2000 ns、インターミッションは 3 ビット（FR-010）。
// ---------------------------------------------------------------------------
TEST(P001Answer, P001_L1_Id7FFDlc0At500k_BitsAndDurationMatch)
{
    RecordProperty("spec", "001:SC-003");
    const FrameTimingResult r = ComputeFrameTiming(0x7FF, 0, {}, 500000);
    EXPECT_EQ(47, r.TotalBits) << "総ビット数";
    EXPECT_EQ(3, r.StuffBitCount) << "スタッフビット数";
    EXPECT_EQ(2000, r.BitTimeNs) << "1 ビット時間";
    EXPECT_EQ(47 * 2000, r.TxDurationNs) << "伝送時間 = 総ビット数 × 1 ビット時間";
    EXPECT_EQ(3 * 2000, r.IntermissionNs) << "インターミッションは 3 ビット時間";
}

// ---------------------------------------------------------------------------
// レッスン 2: ASSERT と EXPECT の使い分け（設定ファイルの読み込み）
// 根拠: FR-002（通信速度は 125k / 250k / 500k / 1M から選ぶ）、contracts/config-input-format.md。
// ---------------------------------------------------------------------------
TEST(P001Answer, P001_L2_BitRate250k_LoadedAsWritten)
{
    RecordProperty("spec", "001:FR-002");
    const std::string text =
        "[Project]\nEcuCount=2\n\n[Network]\nBitRateBps=250000\n\n"
        "[Node1]\nNodeId=1\n\n[Node1.Frame1]\nCanId=0x100\nDlc=8\nPeriodUs=10000\n\n"
        "[Node2]\nNodeId=2\n";
    const ConfigLoadResult result = LoadFromText("p001_l2_250k.canprj", text);
    // 読めなかったら以降の確認は意味が無い（Config が空）ので ASSERT で止める
    ASSERT_TRUE(result.Success) << "正しい構成が拒否された: " << result.ErrorMessage;
    EXPECT_EQ(250000, result.Config.BitRateBps) << "通信速度";
    ASSERT_EQ(2u, result.Config.Nodes.size()) << "ノード数";
    ASSERT_EQ(1u, result.Config.Nodes[0].Frames.size()) << "ノード 1 のフレーム数";
    EXPECT_EQ(0x100u, result.Config.Nodes[0].Frames[0].CanId) << "ノード 1 のフレームの CAN ID";
}

TEST(P001Answer, P001_L2_BitRate300k_Rejected)
{
    RecordProperty("spec", "001:FR-002");
    const std::string text =
        "[Project]\nEcuCount=1\n\n[Network]\nBitRateBps=300000\n\n"
        "[Node1]\nNodeId=1\n";
    const ConfigLoadResult result = LoadFromText("p001_l2_300k.canprj", text);
    EXPECT_FALSE(result.Success) << "選べない通信速度 300000 が受け入れられた";
    EXPECT_FALSE(result.ErrorMessage.empty()) << "拒否理由が示されていない";
}

// ---------------------------------------------------------------------------
// レッスン 3: シミュレーションを動かす（準備 → 実行 → 確認）
// 根拠: FR-003（送信周期・初回オフセット）、機能 006（最初の要求 = 通常動作に入った時刻 + 初回オフセット）。
// 期待値: 周期 10 ms で 100 ms 実行 → 要求は 22, 10022, ..., 90022 us の 10 回。
// ---------------------------------------------------------------------------
TEST(P001Answer, P001_L3_OneSenderPeriod10ms_TenTransmissionsAtPeriod)
{
    RecordProperty("spec", "001:FR-003");
    // 準備
    CanSim::Network net;
    net.BitRateBps = 500000;
    net.Nodes.push_back(MakeNode(1, {{0x100, 8, 10000, 0}}));
    net.Nodes.push_back(MakeNode(2, {}));
    // 実行
    const RunResult run = RunSim(net, 100000LL);
    const std::vector<CanSim::LogRow> tx = UniqueTransmissions(run.Rows);
    // 確認
    ASSERT_EQ(10u, tx.size()) << "100 ms 中の送信回数";
    for (size_t k = 0; k < tx.size(); ++k)
    {
        EXPECT_EQ(kNormalEntryUs + 10000LL * static_cast<long long>(k), tx[k].TxRequestUs)
            << k << " 回目の送信要求時刻";
    }
}

// ---------------------------------------------------------------------------
// レッスン 4: ログの全行に同じ性質が成り立つことを確かめる（ループ + 条件）
// 根拠: FR-005（受信フィルタ。未指定は全 ID 受信）、FR-016（受信処理時刻 = 受信完了 + 受信処理遅延）。
// 構成: ノード 1 が 0x100 と 0x200 を送信。ノード 2 は全受信・受信処理遅延 500 us。
//       ノード 3 は 0x200 だけ受信する。
// ---------------------------------------------------------------------------
TEST(P001Answer, P001_L4_RxFilterAndReceiveDelay_AppliedToEveryRow)
{
    RecordProperty("spec", "001:FR-005");
    RecordProperty("spec", "001:FR-016");
    CanSim::Network net;
    net.BitRateBps = 500000;
    net.Nodes.push_back(MakeNode(1, {{0x100, 8, 10000, 0}, {0x200, 8, 10000, 0}}));
    CanSim::Node n2 = MakeNode(2, {});
    n2.ReceiveDelayUs = 500;
    net.Nodes.push_back(n2);
    CanSim::Node n3 = MakeNode(3, {});
    n3.RxFilterAll = false;
    n3.RxFilter = {0x200};
    net.Nodes.push_back(n3);

    const RunResult run = RunSim(net, 100000LL);
    ASSERT_FALSE(run.Rows.empty()) << "ログが 1 行も無い";

    int node2Rows = 0;
    int node3Rows = 0;
    for (const CanSim::LogRow& r : run.Rows)
    {
        if (r.RxNodeId == 2)
        {
            ++node2Rows;
            EXPECT_EQ(r.RxEndUs + 500, r.RxProcessUs) << "ノード 2 の受信処理時刻は受信完了 + 500 us";
        }
        if (r.RxNodeId == 3)
        {
            ++node3Rows;
            EXPECT_EQ(0x200u, r.CanId) << "ノード 3 はフィルタに無い ID を受信してはいけない";
            EXPECT_EQ(r.RxEndUs, r.RxProcessUs) << "ノード 3 の受信処理遅延は 0";
        }
    }
    // 0x100 と 0x200 を 10 回ずつ。ノード 2 は 20 行、ノード 3 は 0x200 の 10 行だけ
    EXPECT_EQ(20, node2Rows) << "ノード 2 の受信行数";
    EXPECT_EQ(10, node3Rows) << "ノード 3 の受信行数";
}

// ---------------------------------------------------------------------------
// レッスン 5: パラメータ化テスト（同じ確認を表の行ごとにくり返す）
// 根拠: SC-003、FR-002。総ビット数は通信速度に関係なく 47、1 ビット時間 = 10^9 / 通信速度 ns。
// ---------------------------------------------------------------------------
struct BitRateCase
{
    long long BitRate;
    long long ExpectedBitTimeNs;
};

class P001AnswerBitRate : public ::testing::TestWithParam<BitRateCase>
{
};

TEST_P(P001AnswerBitRate, P001_L5_Id7FFDlc0_DurationScalesWithBitRate)
{
    RecordProperty("spec", "001:SC-003");
    const BitRateCase& c = GetParam();
    const FrameTimingResult r = ComputeFrameTiming(0x7FF, 0, {}, c.BitRate);
    EXPECT_EQ(47, r.TotalBits) << "総ビット数は通信速度に関係しない";
    EXPECT_EQ(c.ExpectedBitTimeNs, r.BitTimeNs) << "1 ビット時間";
    EXPECT_EQ(47 * c.ExpectedBitTimeNs, r.TxDurationNs) << "伝送時間";
}

INSTANTIATE_TEST_SUITE_P(
    AllBitRates, P001AnswerBitRate,
    ::testing::Values(
        BitRateCase{125000, 8000},
        BitRateCase{250000, 4000},
        BitRateCase{500000, 2000},
        BitRateCase{1000000, 1000}),
    [](const ::testing::TestParamInfo<BitRateCase>& info) { return std::to_string(info.param.BitRate) + "bps"; });
