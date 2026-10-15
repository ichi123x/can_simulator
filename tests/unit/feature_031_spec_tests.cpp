// feature_031_spec_tests.cpp（tests/unit）
// 機能 031（specs/031-can-bit-stuffing-fix/spec.md）の受け入れシナリオと成功基準のテスト。
// 期待値は spec.md・contracts/frame-timing.md・data-model.md から書く。

#include "support/spec_test_helpers.h"

#include "FrameTiming.h"
#include "MetricsAggregator.h"
#include "StartupControl.h"

#include <cstdint>

using namespace CanSim;
using namespace spectest;

namespace
{
    // 仕様書のログの例と同じ構成: 0x080 と 0x100（どちらも DLC 8）が同時に送信要求する（500 kbps）
    Network LogExampleNetwork()
    {
        Network net;
        net.BitRateBps = 500000;
        net.Nodes.push_back(MakeNode(1, { MakeFrame(0x100, 8, 1000000) }));
        net.Nodes.push_back(MakeNode(2));
        net.Nodes.push_back(MakeNode(3, { MakeFrame(0x080, 8, 1000000) }));
        return net;
    }

    // ---- SC-003 用の、製品のコードとは別に書いた規格どおりの数え方 ----
    // SC-003 は「規格の定義どおりに独立に数えた値と一致する」ことを求めるため、ここだけはテストの中に
    // 規格（Bosch CAN 2.0A §5、data-model.md「差し込みの決まり」）の計算を持つ。正しさは SC-001 の手計算の表で確かめる。

    // CRC-15（生成多項式 x^15 + x^14 + x^10 + x^8 + x^7 + x^4 + x^3 + 1 = 0x4599）
    unsigned RefCrc15(const std::vector<int>& bits)
    {
        unsigned crc = 0;
        for (int b : bits)
        {
            const unsigned next = static_cast<unsigned>(b) ^ ((crc >> 14) & 1u);
            crc = (crc << 1) & 0x7FFFu;
            if (next)
                crc ^= 0x4599u;
        }
        return crc;
    }

    void PushBits(std::vector<int>& bits, unsigned value, int count)
    {
        for (int i = count - 1; i >= 0; --i)
            bits.push_back(static_cast<int>((value >> i) & 1u));
    }

    // SOF〜CRC シーケンスに差し込むスタッフビットの数
    int RefStuffBits(unsigned canId, unsigned dlc, const std::vector<std::uint8_t>& data)
    {
        std::vector<int> bits;
        bits.push_back(0);          // SOF（ドミナント）
        PushBits(bits, canId, 11);  // ID
        bits.push_back(0);          // RTR（データフレーム）
        bits.push_back(0);          // IDE（標準フォーマット）
        bits.push_back(0);          // r0
        PushBits(bits, dlc, 4);     // DLC
        for (std::uint8_t byte : data)
            PushBits(bits, byte, 8);
        PushBits(bits, RefCrc15(bits), 15);

        // 差し込んだビット列の上で、同じ値の連続を数える。差し込んだビットは次の連続の 1 個目
        int stuffed = 0;
        int last = -1;
        int run = 0;
        for (int b : bits)
        {
            if (b == last)
                ++run;
            else
            {
                last = b;
                run = 1;
            }
            if (run == 5)
            {
                ++stuffed;
                last = 1 - last;
                run = 1;
            }
        }
        return stuffed;
    }
}

// 根拠: spec.md US1 受け入れシナリオ 1、FR-002。ID 0x080・DLC 8（データはオール 1）を 500 kbps で送ると、
// フレームログの送信開始から送信完了までが 246 µs（123 ビット × 2 µs）。
TEST(F031Spec, F031_US1_1_Id080Dlc8_LogDurationIs246us)
{
    RecordProperty("spec", "031:US1-1");
    Network net;
    net.BitRateBps = 500000;
    net.Nodes.push_back(MakeNode(1, { MakeFrame(0x080, 8, 10000) }));
    net.Nodes.push_back(MakeNode(2));
    const std::vector<LogRow> rows = RunSim(net, 100000LL);
    ASSERT_FALSE(rows.empty()) << "前提: ログが空";
    for (const LogRow& r : rows)
        EXPECT_EQ(r.TxEndUs - r.TxStartUs, 246) << "0x080・DLC 8 の伝送時間が 246 µs でない";
}

// 根拠: spec.md US1 受け入れシナリオ 2、FR-002（インターミッション 3 ビットは伝送時間に含めない）。
// 0x080 と 0x100 が同時に送信要求すると、0x100 は 0x080 の送信完了の 6 µs 後に送信を始め、その伝送時間も 246 µs。
TEST(F031Spec, F031_US1_2_LogExample_SecondStartsAfter6usAndLasts246us)
{
    RecordProperty("spec", "031:US1-2");
    const std::vector<LogRow> rows = RunSim(LogExampleNetwork(), 100000LL);
    const LogRow* r080 = FirstRowOf(rows, 0x080);
    const LogRow* r100 = FirstRowOf(rows, 0x100);
    ASSERT_NE(r080, nullptr) << "0x080 の行が無い";
    ASSERT_NE(r100, nullptr) << "0x100 の行が無い";
    EXPECT_EQ(r100->TxStartUs, r080->TxEndUs + 6) << "0x100 の送信開始が 0x080 の送信完了の 6 µs 後でない";
    EXPECT_EQ(r100->TxEndUs - r100->TxStartUs, 246) << "0x100 の伝送時間が 246 µs でない";
}

namespace
{
    // 同じ構成を 2 回実行し、フレームログ・集計・起動記録が完全に一致するか確かめる
    void ExpectSameResultsTwice(const Network& net, long long durationUs)
    {
        Network after1, after2;
        const std::vector<LogRow> a = RunSim(net, durationUs, &after1);
        const std::vector<LogRow> b = RunSim(net, durationUs, &after2);
        ASSERT_FALSE(a.empty()) << "前提: ログが空";
        ASSERT_EQ(a.size(), b.size()) << "フレームログの行数が違う";
        for (std::size_t i = 0; i < a.size(); ++i)
            EXPECT_TRUE(SameRow(a[i], b[i])) << "フレームログの " << i << " 行目が違う";

        const std::vector<GroupMetrics> ma = ComputeMetrics(a, net);
        const std::vector<GroupMetrics> mb = ComputeMetrics(b, net);
        ASSERT_EQ(ma.size(), mb.size()) << "集計の数が違う";
        for (std::size_t i = 0; i < ma.size(); ++i)
        {
            const bool same = ma[i].GroupType == mb[i].GroupType && ma[i].GroupKey == mb[i].GroupKey &&
                              ma[i].TotalLatency.Count == mb[i].TotalLatency.Count &&
                              ma[i].TotalLatency.AvgUs == mb[i].TotalLatency.AvgUs &&
                              ma[i].QueueingDelay.AvgUs == mb[i].QueueingDelay.AvgUs &&
                              ma[i].TxDuration.AvgUs == mb[i].TxDuration.AvgUs &&
                              ma[i].AvgBusLoadPct == mb[i].AvgBusLoadPct &&
                              ma[i].JitterMaxUs == mb[i].JitterMaxUs && ma[i].JitterMinUs == mb[i].JitterMinUs &&
                              ma[i].JitterStdDevUs == mb[i].JitterStdDevUs;
            EXPECT_TRUE(same) << "集計の " << i << " 番目（" << ma[i].GroupType << " " << ma[i].GroupKey << "）が違う";
        }

        const std::vector<StartupRecord> sa = BuildStartupRecords(after1);
        const std::vector<StartupRecord> sb = BuildStartupRecords(after2);
        ASSERT_EQ(sa.size(), sb.size()) << "起動記録の数が違う";
        for (std::size_t i = 0; i < sa.size(); ++i)
        {
            const bool same = sa[i].NodeId == sb[i].NodeId && sa[i].PowerOnUs == sb[i].PowerOnUs &&
                              sa[i].InitDoneUs == sb[i].InitDoneUs && sa[i].NormalEntryUs == sb[i].NormalEntryUs &&
                              sa[i].ReachedNormal == sb[i].ReachedNormal;
            EXPECT_TRUE(same) << "起動記録の " << i << " 番目が違う";
        }
    }
}

// 根拠: spec.md US1 受け入れシナリオ 3、FR-005。仕様書のログの例と同じ構成を 2 回実行すると、
// フレームログ・集計・起動記録が完全に一致する。
TEST(F031Spec, F031_US1_3_LogExampleTwice_IdenticalResults)
{
    RecordProperty("spec", "031:US1-3");
    ExpectSameResultsTwice(LogExampleNetwork(), 1000000LL);
}

// 根拠: spec.md SC-001、contracts/frame-timing.md「規格どおりの値の例」（2026-10-13 に製品とは別の計算で求めた手計算の値）。
// 500 kbps で、総ビット数・スタッフビット数・伝送時間が表のとおり。
struct HandCountCase
{
    const char* Name;
    unsigned CanId;
    std::vector<std::uint8_t> Data;
    int TotalBits;
    int StuffBits;
    long long DurationNs;
};

class F031HandCount : public ::testing::TestWithParam<HandCountCase>
{
};

TEST_P(F031HandCount, F031_SC001_Frame_MatchesHandCount)
{
    RecordProperty("spec", "031:SC-001");
    const HandCountCase& c = GetParam();
    const FrameTimingResult t =
        ComputeFrameTiming(c.CanId, static_cast<unsigned>(c.Data.size()), c.Data, 500000);
    EXPECT_EQ(t.TotalBits, c.TotalBits) << c.Name << ": 総ビット数";
    EXPECT_EQ(t.StuffBitCount, c.StuffBits) << c.Name << ": スタッフビット数";
    EXPECT_EQ(t.TxDurationNs, c.DurationNs) << c.Name << ": 伝送時間";
    // SC-003 のための独立した数え方が、手計算の表と合っていること（数え方の正しさの確認）
    EXPECT_EQ(RefStuffBits(c.CanId, static_cast<unsigned>(c.Data.size()), c.Data), c.StuffBits)
        << c.Name << ": テストの中の独立した数え方が手計算と合わない";
}

INSTANTIATE_TEST_SUITE_P(
    SC001, F031HandCount,
    ::testing::Values(
        HandCountCase{ "Id000Dlc0", 0x000, {}, 50, 6, 100000 },
        HandCountCase{ "Id7FFDlc0", 0x7FF, {}, 47, 3, 94000 },
        HandCountCase{ "Id555Dlc1DataAA", 0x555, { 0xAA }, 54, 2, 108000 },
        HandCountCase{ "Id123Dlc8Data01to08", 0x123, { 1, 2, 3, 4, 5, 6, 7, 8 }, 116, 8, 232000 },
        HandCountCase{ "Id080Dlc8DataFF", 0x080, { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF }, 123, 15, 246000 }),
    [](const ::testing::TestParamInfo<HandCountCase>& info) { return std::string(info.param.Name); });

// 根拠: spec.md SC-002。仕様書のログの例と同じ構成で、0x080 と 0x100 の伝送時間がどちらも 246 µs、
// 0x100 の送信開始が 0x080 の送信完了の 6 µs 後。
TEST(F031Spec, F031_SC002_LogExample_BothLast246usSecondAfter6us)
{
    RecordProperty("spec", "031:SC-002");
    const std::vector<LogRow> rows = RunSim(LogExampleNetwork(), 100000LL);
    const LogRow* r080 = FirstRowOf(rows, 0x080);
    const LogRow* r100 = FirstRowOf(rows, 0x100);
    ASSERT_NE(r080, nullptr) << "0x080 の行が無い";
    ASSERT_NE(r100, nullptr) << "0x100 の行が無い";
    EXPECT_EQ(r080->TxEndUs - r080->TxStartUs, 246) << "0x080 の伝送時間";
    EXPECT_EQ(r100->TxEndUs - r100->TxStartUs, 246) << "0x100 の伝送時間";
    EXPECT_EQ(r100->TxStartUs - r080->TxEndUs, 6) << "0x100 の送信開始と 0x080 の送信完了の差";
}

// 根拠: spec.md SC-003、FR-001・FR-003。ID 0x000〜0x7FF の全 ID と DLC 0〜8 のすべての組み合わせ（データは 0xFF）で、
// スタッフビット数が規格の定義どおりに独立に数えた値と一致する（過少も過大も 0 件）。
TEST(F031Spec, F031_SC003_AllIdsAndDlcs_StuffBitsMatchIndependentCount)
{
    RecordProperty("spec", "031:SC-003");
    int mismatches = 0;
    for (unsigned dlc = 0; dlc <= 8; ++dlc)
    {
        const std::vector<std::uint8_t> data(dlc, 0xFF);
        for (unsigned id = 0; id <= 0x7FF; ++id)
        {
            const int expected = RefStuffBits(id, dlc, data);
            const FrameTimingResult t = ComputeFrameTiming(id, dlc, data, 500000);
            if (t.StuffBitCount != expected)
            {
                if (++mismatches <= 5)
                    ADD_FAILURE() << "ID 0x" << std::hex << id << std::dec << "・DLC " << dlc << ": スタッフビット "
                                  << t.StuffBitCount << "（独立に数えた値は " << expected << "）";
            }
        }
    }
    EXPECT_EQ(mismatches, 0) << "独立に数えた値と合わない組み合わせの数";
}

// 根拠: spec.md SC-004、contracts/frame-timing.md「出力の約束」。SC-003 のすべての組み合わせで、スタッフビット数が
// ドメイン仕様書の最悪値 ⌊(34 + 8 × DLC − 1) / 4⌋ を超えない。
TEST(F031Spec, F031_SC004_AllIdsAndDlcs_StuffBitsWithinWorstCase)
{
    RecordProperty("spec", "031:SC-004");
    int over = 0;
    for (unsigned dlc = 0; dlc <= 8; ++dlc)
    {
        const std::vector<std::uint8_t> data(dlc, 0xFF);
        const int worst = static_cast<int>((34 + 8 * dlc - 1) / 4);
        for (unsigned id = 0; id <= 0x7FF; ++id)
        {
            const FrameTimingResult t = ComputeFrameTiming(id, dlc, data, 500000);
            if (t.StuffBitCount < 0 || t.StuffBitCount > worst)
                ++over;
        }
    }
    EXPECT_EQ(over, 0) << "スタッフビット数が最悪値を超えた（または負の）組み合わせの数";
}

// 根拠: spec.md SC-005、Edge Cases（通信速度によらずビット数は同じ）。125k / 250k / 500k / 1M bps のそれぞれで、
// 伝送時間が総ビット数 × ビット時間（8 / 4 / 2 / 1 µs）と一致する。ID 0x080・DLC 8・0xFF は 123 ビット（SC-001）。
struct BitRateCase
{
    long long BitRateBps;
    long long BitTimeNs;
};

class F031BitRate : public ::testing::TestWithParam<BitRateCase>
{
};

TEST_P(F031BitRate, F031_SC005_EachBitRate_DurationIsBitsTimesBitTime)
{
    RecordProperty("spec", "031:SC-005");
    const BitRateCase& c = GetParam();
    const FrameTimingResult t = ComputeFrameTiming(0x080, 8, std::vector<std::uint8_t>(8, 0xFF), c.BitRateBps);
    EXPECT_EQ(t.TotalBits, 123) << c.BitRateBps << " bps: 総ビット数が通信速度で変わった";
    EXPECT_EQ(t.BitTimeNs, c.BitTimeNs) << c.BitRateBps << " bps: ビット時間";
    EXPECT_EQ(t.TxDurationNs, 123 * c.BitTimeNs) << c.BitRateBps << " bps: 伝送時間";
}

INSTANTIATE_TEST_SUITE_P(SC005, F031BitRate,
                         ::testing::Values(BitRateCase{ 125000, 8000 }, BitRateCase{ 250000, 4000 },
                                           BitRateCase{ 500000, 2000 }, BitRateCase{ 1000000, 1000 }),
                         [](const ::testing::TestParamInfo<BitRateCase>& info) {
                             return std::to_string(info.param.BitRateBps) + "bps";
                         });

// 根拠: spec.md SC-006、FR-005。同じ構成で 2 回実行したとき、フレームログ・集計・起動記録が完全に一致する。
// 調停・再送と、電源投入時刻・初期化時間をずらした起動を含む構成で確かめる。
TEST(F031Spec, F031_SC006_MixedConfigTwice_IdenticalResults)
{
    RecordProperty("spec", "031:SC-006");
    Network net;
    net.BitRateBps = 250000;
    Node n1 = MakeNode(1, { MakeFrame(0x100, 8, 10000), MakeFrame(0x101, 3, 20000, 7000) });
    Node n2 = MakeNode(2, { MakeFrame(0x080, 8, 10000) });
    n2.PowerOnTimeUs = 1500;
    n2.InitDurationUs = 300;
    Node n3 = MakeNode(3, { MakeFrame(0x200, 0, 5000) });
    n3.ReceiveDelayUs = 25;
    net.Nodes = { n1, n2, n3 };
    ExpectSameResultsTwice(net, 1000000LL);
}
