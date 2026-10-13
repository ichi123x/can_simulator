// feature_001_tests.cpp（tests）
// tools/cansim_runner/checks.cpp の CheckFeature001() から移した（機能 030）。
// 本体は、パスの取り方以外は変えていない。Check() のラベルには SC/FR の ID が残っている。

#include "test_helpers.h"

using namespace cansimtest;

TEST(Feature001, SC001to005_FR004)
{
    std::cout << "\n== 機能 001: CAN シミュレーションコア ==\n";
    const std::string cfgPath = Join(SampleDir(), "sample_appendix_a.canprj");
    const long long tenSeconds = 10 * 1000000LL;

    RunResult run1 = RunConfigFile(cfgPath, tenSeconds);
    Check(run1.Loaded, "sample_appendix_a.canprj を読み込める");
    if (!run1.Loaded)
    {
        std::cout << "  エラー: " << run1.Error << "\n";
        return;
    }
    Check(run1.Config.Nodes.size() == 6, "sample_appendix_a.canprj のノードは 6 個");
    Check(!run1.Rows.empty(), "US1 AS#1: 実行でログ行が出る");

    // SC-001 再現性
    RunResult run2 = RunConfigFile(cfgPath, tenSeconds);
    Check(RowsEqual(run1.Rows, run2.Rows), "SC-001: 同じ条件の 2 回の実行は同じログになる");

    // SC-002 / US2 AS#1: 0x080 が 0x100 との調停に勝つ
    long long first080 = -1, first100 = -1;
    int retry100 = -1;
    bool sawLostStatus = false;
    for (const LogRow& row : run1.Rows)
    {
        if (row.CanId == 0x080 && first080 < 0)
            first080 = row.TxStartUs;
        if (row.CanId == 0x100 && first100 < 0)
        {
            first100 = row.TxStartUs;
            retry100 = row.RetryCount;
        }
        if (row.CanId == 0x100 && row.Status == "ARBITRATION_LOST_RETRY")
            sawLostStatus = true;
    }
    std::cout << "  0x080 の最初の Tx_Start=" << first080
              << " us, 0x100 の最初の Tx_Start=" << first100
              << " us, 0x100 の Retry_Count=" << retry100 << "\n";
    Check(first080 >= 0 && first100 >= 0, "0x080 と 0x100 の両方が送信される");
    Check(first080 < first100, "SC-002: 小さい CAN ID (0x080) が先に送信される");
    Check(retry100 >= 1, "US2 AS#1: 0x100 の Retry_Count が 1 以上になる");
    Check(sawLostStatus, "US2 AS#1: 0x100 の Status が ARBITRATION_LOST_RETRY になる");

    // 機能 002 の導入後は、どのノードも送信前にレセシブ 11 ビット時間
    // （500 kbps で 22 us）を待つので、最初の一斉送信の先頭フレームは
    // 0 ではなく 22 us から始まる。先に送る 0x050（DLC 2・データ 0xFF）は規格どおりのスタッフで
    // 65 ビット = 130 us（機能 031。以前の数え方では 67 ビット・134 us で 162 us だった）。
    Check(first080 == 158, "002 SC-006: 0x080 の最初の Tx_Start は 22 us + 0x050 の 130 us + インターミッション 6 us = 158 us");

    // SC-004: 送信回数は シミュレーション時間 ÷ 周期 に従う
    std::map<unsigned, long long> periods;
    for (const Node& n : run1.Config.Nodes)
        for (const FrameDefinition& fd : n.Frames)
            periods[fd.CanId] = fd.PeriodUs;
    std::map<unsigned, std::set<long long> > starts;
    for (const LogRow& row : run1.Rows)
        starts[row.CanId].insert(row.TxStartUs);
    for (std::map<unsigned, std::set<long long> >::const_iterator it = starts.begin();
         it != starts.end(); ++it)
    {
        const long long expected = tenSeconds / periods[it->first];
        const long long actual = static_cast<long long>(it->second.size());
        std::ostringstream os;
        os << "SC-004: CAN ID 0x" << std::hex << it->first << std::dec
           << " の送信回数は " << actual << " 回（期待値は約 " << expected << " 回）";
        Check(actual >= expected - 1 && actual <= expected + 1, os.str());
    }

    // SC-003: フレーム長は スタッフ前 34 + 8*DLC、+ スタッフビット + 固定 10 ビット
    {
        std::vector<std::uint8_t> none;
        FrameTimingResult t0 = ComputeFrameTiming(0x000, 0, none, 500000);
        Check(t0.TotalBits - t0.StuffBitCount - 10 == 34,
            "SC-003: DLC=0 のスタッフ前 SOF〜CRC の長さは 34 ビット");

        std::vector<std::uint8_t> data8(8, 0x00);
        FrameTimingResult t8 = ComputeFrameTiming(0x080, 8, data8, 500000);
        Check(t8.TotalBits == 34 + 64 + t8.StuffBitCount + 10,
            "SC-003: DLC=8 の総ビット数 = スタッフ前 + スタッフビット + 固定 10 ビット");
        Check(t8.StuffBitCount >= 0 && t8.StuffBitCount <= 24,
            "SC-003: DLC=8 のスタッフビット数は最悪値の上限 24 以内");
    }

    // US1 AS#2 / SC-005: 構成の拒否
    {
        std::ostringstream cfg;
        cfg << "[Network]\r\nBitRateBps=500000\r\n";
        for (int i = 1; i <= 17; ++i)
        {
            cfg << "[Node" << i << "]\r\nNodeId=" << i << "\r\n";
            cfg << "[Node" << i << ".Frame1]\r\nCanId=" << std::hex << (0x200 + i) << std::dec
                << "\r\nDlc=0\r\nData=\r\nPeriodUs=1000\r\n";
        }
        const std::string path = Join(TempDir(), "too_many_nodes.cfg");
        WriteTextFile(path, cfg.str());
        ConfigLoadResult r = LoadConfig(Widen(path));
        Check(!r.Success, "US1 AS#2 / SC-005: 17 ノードの構成は拒否される");
        if (!r.Success)
            std::cout << "  理由: " << r.ErrorMessage << "\n";
    }
    {
        std::ostringstream cfg;
        cfg << "[Network]\r\nBitRateBps=500000\r\n"
            << "[Node1]\r\nNodeId=1\r\n[Node1.Frame1]\r\nCanId=0x080\r\nDlc=0\r\nData=\r\nPeriodUs=1000\r\n"
            << "[Node2]\r\nNodeId=2\r\n[Node2.Frame1]\r\nCanId=0x080\r\nDlc=0\r\nData=\r\nPeriodUs=1000\r\n";
        const std::string path = Join(TempDir(), "duplicate_id.cfg");
        WriteTextFile(path, cfg.str());
        ConfigLoadResult r = LoadConfig(Widen(path));
        Check(!r.Success, "FR-004: 2 つのノードに同じ CAN ID があると拒否される");
        if (!r.Success)
            std::cout << "  理由: " << r.ErrorMessage << "\n";
    }
}
