// feature_031_tests.cpp（tests）
// 機能 031: ビットスタッフィングを CAN 2.0A の規格どおりに数える。
// 根拠: specs/031-can-bit-stuffing-fix/spec.md（SC-001〜SC-006）、contracts/frame-timing.md、
// ドメイン仕様書 Doc/CANバスシミュレータ仕様.md の FR-021 とログの例（0x080 = 123 ビット・スタッフ 15・246 µs）。
// 期待値はリテラルで書く。SC-001 の値は、製品のコードとは別に書いた Python のスクリプトで、
// 規格の定義（SOF〜CRC にスタッフ、差し込んだビットも次の連続に数える、末尾の 10 ビットはスタッフしない）
// から求めた（research R6、2026-10-13）。

#include "test_helpers.h"

using namespace cansimtest;

namespace
{
    // 規格どおりの参照の数え方（research R6）。製品の FrameTiming.cpp とは別の書き方にする:
    // 元のビット列を先頭から読み、「直前に送ったビットの値」と「その値が続いた数」を持つ。
    // 5 個続いたら反対の値を 1 つ送り（スタッフビット）、そのビットを新しい連続の 1 個目にする。
    struct RefResult
    {
        int StuffBits = 0;
        bool StuffAfterLastCrcBit = false; // CRC シーケンスの最後のビットの直後に差し込んだ
    };

    std::uint16_t RefCrc15(const std::vector<int>& bits)
    {
        std::uint16_t crc = 0;
        for (int b : bits)
        {
            const int next = ((crc >> 14) & 1) ^ b;
            crc = static_cast<std::uint16_t>((crc << 1) & 0x7FFF);
            if (next)
                crc ^= 0x4599;
        }
        return crc;
    }

    void PushBits(std::vector<int>& bits, unsigned value, int count)
    {
        for (int i = count - 1; i >= 0; --i)
            bits.push_back(static_cast<int>((value >> i) & 1u));
    }

    RefResult RefStuff(unsigned canId, unsigned dlc, const std::vector<std::uint8_t>& data)
    {
        std::vector<int> bits;
        bits.push_back(0);            // SOF
        PushBits(bits, canId, 11);    // ID
        bits.push_back(0);            // RTR
        bits.push_back(0);            // IDE
        bits.push_back(0);            // r0
        PushBits(bits, dlc, 4);       // DLC
        for (std::uint8_t d : data)
            PushBits(bits, d, 8);     // データ
        PushBits(bits, RefCrc15(bits), 15); // CRC シーケンス

        RefResult r;
        int lastSent = -1;
        int run = 0;
        for (std::size_t i = 0; i < bits.size(); ++i)
        {
            const int b = bits[i];
            run = (b == lastSent) ? run + 1 : 1;
            lastSent = b;
            if (run == 5)
            {
                ++r.StuffBits;
                lastSent = 1 - b; // 差し込んだスタッフビット
                run = 1;          // 次の連続の 1 個目
                if (i + 1 == bits.size())
                    r.StuffAfterLastCrcBit = true;
            }
        }
        return r;
    }

    struct Example
    {
        unsigned CanId;
        unsigned Dlc;
        std::vector<std::uint8_t> Data;
        int TotalBits;
        int StuffBits;
        long long TxDurationNs;
    };
}

// SC-001: 5 つのフレームで、総ビット数・スタッフビット数・伝送時間が規格どおりの手計算と完全に一致する（500 kbps）。
// 根拠: spec SC-001、contracts/frame-timing.md の値の例。0x080・DLC 8・FF はドメイン仕様書のログの例と同じ値。
TEST(Feature031, SC001_HandCountedFrames_MatchStandard)
{
    const std::vector<Example> examples = {
        {0x000, 0, {}, 50, 6, 100000},
        {0x7FF, 0, {}, 47, 3, 94000},
        {0x555, 1, {0xAA}, 54, 2, 108000},
        {0x123, 8, {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08}, 116, 8, 232000},
        {0x080, 8, std::vector<std::uint8_t>(8, 0xFF), 123, 15, 246000},
    };
    for (const Example& e : examples)
    {
        const FrameTimingResult t = ComputeFrameTiming(e.CanId, e.Dlc, e.Data, 500000);
        std::ostringstream name;
        name << "ID 0x" << std::hex << e.CanId << std::dec << "・DLC " << e.Dlc;
        EXPECT_EQ(e.TotalBits, t.TotalBits) << name.str() << " の総ビット数";
        EXPECT_EQ(e.StuffBits, t.StuffBitCount) << name.str() << " のスタッフビット数";
        EXPECT_EQ(e.TxDurationNs, t.TxDurationNs) << name.str() << " の伝送時間（ns）";
    }
}

// SC-003・SC-004: 全 ID（0x000〜0x7FF）× DLC 0〜8（データは DLC 分の 0xFF）で、スタッフビット数が規格どおりの
// 参照の数え方と一致し（過少も過大も 0 件）、ドメイン仕様書の最悪値 ⌊(34 + 8 × DLC − 1) / 4⌋ を超えない。
// 総ビット数は 34 + 8 × DLC + スタッフビット数 + 10（data-model.md）。
// spec の Edge Cases「CRC シーケンスの最後のビットで 5 連続になったときも差し込む」場合もこの照合に含まれる。
// その場合が 1 件も無いと確かめたことにならないので、件数が 0 なら失敗にする。
TEST(Feature031, SC003_SC004_AllIdsAndDlcs_MatchReferenceAndWithinWorstCase)
{
    int mismatches = 0;
    int overWorst = 0;
    int badTotal = 0;
    int stuffAfterCrcEnd = 0;
    std::ostringstream firstErrors;
    for (unsigned dlc = 0; dlc <= 8; ++dlc)
    {
        const std::vector<std::uint8_t> data = MakeFrameData(dlc);
        const int worst = static_cast<int>((34 + 8 * dlc - 1) / 4);
        for (unsigned id = 0; id <= 0x7FF; ++id)
        {
            const FrameTimingResult t = ComputeFrameTiming(id, dlc, data, 500000);
            const RefResult ref = RefStuff(id, dlc, data);
            if (ref.StuffAfterLastCrcBit)
                ++stuffAfterCrcEnd;
            if (t.StuffBitCount != ref.StuffBits)
            {
                if (mismatches < 5)
                    firstErrors << " [ID 0x" << std::hex << id << std::dec << " DLC " << dlc << ": 実装 "
                                << t.StuffBitCount << " / 規格 " << ref.StuffBits << "]";
                ++mismatches;
            }
            if (t.StuffBitCount > worst)
                ++overWorst;
            if (t.TotalBits != static_cast<int>(34 + 8 * dlc) + t.StuffBitCount + 10)
                ++badTotal;
        }
    }
    EXPECT_EQ(0, mismatches) << "規格どおりの参照と合わない (ID, DLC) の数。最初の数件:" << firstErrors.str();
    EXPECT_EQ(0, overWorst) << "スタッフビット数が最悪値を超えた (ID, DLC) の数";
    EXPECT_EQ(0, badTotal) << "総ビット数 = 34 + 8 × DLC + スタッフ + 10 にならない (ID, DLC) の数";
    EXPECT_GT(stuffAfterCrcEnd, 0) << "CRC の最後のビットの直後に差し込む場合が照合に 1 件も無い";
}

// SC-005: 通信速度によらずビット数は同じで、伝送時間 = 総ビット数 × ビット時間、インターミッション = 3 ビット時間。
// 根拠: spec SC-005、contracts/frame-timing.md「出力の約束」。0x080・DLC 8・FF は 123 ビット。
TEST(Feature031, SC005_EachBitRate_DurationIsBitsTimesBitTime)
{
    const std::vector<std::uint8_t> data(8, 0xFF);
    const struct
    {
        long long Bps;
        long long BitTimeNs;
    } rates[] = {{125000, 8000}, {250000, 4000}, {500000, 2000}, {1000000, 1000}};
    for (const auto& r : rates)
    {
        const FrameTimingResult t = ComputeFrameTiming(0x080, 8, data, r.Bps);
        EXPECT_EQ(r.BitTimeNs, t.BitTimeNs) << r.Bps << " bps の 1 ビット時間";
        EXPECT_EQ(123, t.TotalBits) << r.Bps << " bps の総ビット数";
        EXPECT_EQ(123 * r.BitTimeNs, t.TxDurationNs) << r.Bps << " bps の伝送時間";
        EXPECT_EQ(3 * r.BitTimeNs, t.IntermissionNs) << r.Bps << " bps のインターミッション";
    }
}

// SC-002: ドメイン仕様書のログの例と同じ構成（sample_collision: ECU 3 が 0x080、ECU 1 が 0x100、DLC 8、500 kbps、
// 同時に送信要求）で、0x080・0x100 の伝送時間がどちらも 246 µs、0x100 の送信開始が 0x080 の送信完了の 6 µs 後。
// SC-006: 同じ構成を 2 回実行すると、フレームログの全行が一致する。
TEST(Feature031, SC002_SC006_CollisionSample_MatchesDomainLogExampleAndRepeats)
{
    const std::string path = Join(SampleDir(), "sample_collision.canprj");
    const RunResult a = RunConfigFile(path, 100000);
    ASSERT_TRUE(a.Loaded) << a.Error;

    std::map<long long, const LogRow*> first080; // 送信要求の時刻 -> 0x080 の行
    std::map<long long, const LogRow*> first100;
    for (const LogRow& r : a.Rows)
    {
        if (r.CanId == 0x080 && first080.count(r.TxRequestUs) == 0)
            first080[r.TxRequestUs] = &r;
        if (r.CanId == 0x100 && first100.count(r.TxRequestUs) == 0)
            first100[r.TxRequestUs] = &r;
    }
    ASSERT_EQ(10u, first080.size()) << "100 ms の 0x080 の送信回数";
    ASSERT_EQ(10u, first100.size()) << "100 ms の 0x100 の送信回数";
    for (const auto& kv : first080)
    {
        const LogRow& hi = *kv.second;
        ASSERT_EQ(1u, first100.count(kv.first)) << "同じ時刻に要求した 0x100 が無い";
        const LogRow& lo = *first100[kv.first];
        EXPECT_EQ(246, hi.TxEndUs - hi.TxStartUs) << "0x080 の伝送時間（要求 " << kv.first << " us）";
        EXPECT_EQ(246, lo.TxEndUs - lo.TxStartUs) << "0x100 の伝送時間（要求 " << kv.first << " us）";
        EXPECT_EQ(hi.TxEndUs + 6, lo.TxStartUs) << "0x100 の送信開始 = 0x080 の送信完了 + 6 us";
    }

    const RunResult b = RunConfigFile(path, 100000);
    EXPECT_TRUE(RowsEqual(a.Rows, b.Rows)) << "同じ構成の 2 回の実行でフレームログが一致しない";
}
