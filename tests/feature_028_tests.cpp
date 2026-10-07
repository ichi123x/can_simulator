// feature_028_tests.cpp（tests）
// tools/cansim_runner/checks.cpp の CheckFeature028() から移した（機能 030）。
// 本体は、パスの取り方以外は変えていない。Check() のラベルには SC/FR の ID が残っている。

#include "test_helpers.h"

using namespace cansimtest;

TEST(Feature028, SC001to003_FR002to004_FR006)
{
    std::cout << "\n== 機能 028: 受信完了時間とバス負荷の解析 ==\n";
    auto writeLog = [](const std::string& name, const std::string& text) {
        const std::string path = Join(TempDir(), name);
        std::ofstream(path.c_str(), std::ios::binary) << text;
        return Widen(path);
    };
    std::vector<LogRecord> records;
    std::string error;

    // 読込み: 列は名前で探す（順序の入れ替え・余分な列）、空の欄
    std::string latencyLog = "Status,Extra,CAN_ID,Tx_Node,Rx_Node,Tx_Request,Tx_Start,Tx_End,Rx_End\r\n"
        "SUCCESS,x,0x080,1,2,0,0,250,100\r\n"
        "SUCCESS,x,0x080,1,3,0,0,250,200\r\n"
        "OVERWRITE,x,0x100,1,,500,,,\r\n";
    for (int k = 1; k <= 20; ++k)
    {
        std::ostringstream row;
        row << (k % 2 ? "SUCCESS" : "ARBITRATION_LOST_RETRY") << ",x,0x200,2,1," << 1000 * k << ',' << 1000 * k << ','
            << 1000 * k + 250 << ',' << 1000 * k + k << "\r\n";
        latencyLog += row.str();
    }
    latencyLog += "\r\n"; // 空行は読み飛ばす
    const bool read = ReadLogRecords(writeLog("t028_latency.log.csv", latencyLog), records, error);
    Check(read && records.size() == 23 && records[2].Status == LogStatus::Overwrite && records[2].RxNode == 0 &&
        records[2].TxStartUs == -1 && records[0].CanId == 0x080 && records[3].Status == LogStatus::Success &&
        records[4].Status == LogStatus::RetrySuccess,
        "028 FR-002: ReadLogRecords は列を名前で探し、空の欄は RxNode 0・時刻 -1 になる");

    bool rejected = true;
    const char* bad[] = {
        "CAN_ID,Tx_Node,Rx_Node,Tx_Request,Tx_Start,Tx_End,Status\r\n",                                // Rx_End が無い
        "CAN_ID,Tx_Node,Rx_Node,Tx_Request,Tx_Start,Tx_End,Rx_End,Status\r\n0x080,1,2,0,0,250,100,FOO\r\n", // Status が不正
        "CAN_ID,Tx_Node,Rx_Node,Tx_Request,Tx_Start,Tx_End,Rx_End,Status\r\n0x080,1,2,abc,0,250,100,SUCCESS\r\n",
    };
    const char* lines[] = { "line 1:", "line 2:", "line 2:" };
    for (int i = 0; i < 3; ++i)
    {
        std::vector<LogRecord> r;
        std::string e;
        rejected = rejected && !ReadLogRecords(writeLog("t028_bad.log.csv", bad[i]), r, e) && e.find(lines[i]) == 0;
    }
    std::vector<LogRecord> none;
    rejected = rejected && !ReadLogRecords(Widen(Join(TempDir(), "t028_missing.log.csv")), none, error);
    Check(rejected, "028 US3: 列の不足・不正な Status や数値・ファイルが無いときは、行番号つきのエラーになる");

    // 受信完了時間の表
    const LatencyAnalysis lat = AnalyzeLatency(records);
    const bool rowsOk = lat.Rows.size() == 4 &&
        lat.Rows[0].TxNode == 0 && lat.Rows[1].TxNode == 1 && lat.Rows[1].RxNode == 2 &&
        lat.Rows[2].TxNode == 1 && lat.Rows[2].RxNode == 3 && lat.Rows[3].TxNode == 2 && lat.Rows[3].RxNode == 1;
    Check(rowsOk && lat.OverwriteCount == 1,
        "028 FR-003 (Q1): 行は全体、続いて（送信, 受信）の組の順。OVERWRITE は別に数える");
    Check(rowsOk && lat.Rows[3].Count == 20 && lat.Rows[3].MinUs == 1 && lat.Rows[3].MaxUs == 20 &&
        std::fabs(lat.Rows[3].MeanUs - 10.5) < 1e-9 && lat.Rows[3].P95Us == 19 &&
        lat.Rows[1].Count == 1 && lat.Rows[1].MinUs == 100 && lat.Rows[1].P95Us == 100 &&
        lat.Rows[0].Count == 22 && lat.Rows[0].MinUs == 1 && lat.Rows[0].MaxUs == 200 &&
        std::fabs(lat.Rows[0].MeanUs - 510.0 / 22.0) < 1e-9 && lat.Rows[0].P95Us == 100,
        "028 FR-003 (Q2): Rx_End - Tx_Request。件数・最小・平均・最大・最近順位法の P95 が手計算と一致する");

    // ヒストグラム
    bool hist = MakeHistogram({}).Counts.empty();
    const Histogram same = MakeHistogram({ 7, 7, 7, 7, 7 });
    hist = hist && same.Counts.size() == 1 && same.Counts[0] == 5 && same.StartUs == 7;
    std::vector<long long> thousand;
    for (int v = 1; v <= 1000; ++v)
        thousand.push_back(v);
    const Histogram h = MakeHistogram(thousand);
    long long w = h.WidthUs;
    while (w % 10 == 0)
        w /= 10;
    long long sum = 0;
    for (long long c : h.Counts)
        sum += c;
    hist = hist && (w == 1 || w == 2 || w == 5) && h.Counts.size() >= 10 && h.Counts.size() <= 50 && sum == 1000 &&
        h.StartUs <= 1 && h.StartUs + h.WidthUs * static_cast<long long>(h.Counts.size()) > 1000;
    Check(hist, "028 FR-004: ヒストグラムのビン幅はきりのよい値で 10..50 本、件数の合計が合う（空・値が 1 つの場合も）");

    // バス負荷: 送信 1 回 [0, 250) と、2 つの受信側に見える 1 回 [9900, 10150)
    std::vector<LogRecord> busRecords;
    const std::string busLog = "CAN_ID,Tx_Node,Rx_Node,Tx_Request,Tx_Start,Tx_End,Rx_End,Status\r\n"
        "0x080,1,2,0,0,250,250,SUCCESS\r\n"
        "0x100,2,1,9800,9900,10150,10150,ARBITRATION_LOST_RETRY\r\n"
        "0x100,2,3,9800,9900,10150,10150,ARBITRATION_LOST_RETRY\r\n"
        "0x300,3,,9000,,,,OVERWRITE\r\n";
    const bool busRead = ReadLogRecords(writeLog("t028_bus.log.csv", busLog), busRecords, error);
    const BusLoadAnalysis bus = AnalyzeBusLoad(busRecords);
    Check(busRead && bus.TransmissionCount == 2 && bus.RangeUs == 20000 && bus.SectionPct.size() == 2 &&
        std::fabs(bus.SectionPct[0] - 3.5) < 1e-9 && std::fabs(bus.SectionPct[1] - 1.5) < 1e-9 &&
        std::fabs(bus.AveragePct - 2.5) < 1e-9 && std::fabs(bus.MaxPct - 3.5) < 1e-9 && bus.MaxSection == 0,
        "028 FR-006: 2 つの受信側に見える送信は 1 回と数え、10 ms の境で分ける。平均 = 使用時間 ÷ 範囲");
    Check(bus.Ecus.size() == 2 && bus.Ecus[0].Node == 1 && bus.Ecus[1].Node == 2 && bus.Ecus[1].FrameCount == 1 &&
        std::fabs(bus.Ecus[0].UsagePct + bus.Ecus[1].UsagePct - bus.AveragePct) < 1e-9,
        "028 SC-003: ECU ごとの使用率の合計は平均負荷になる");

    // グラフの補助
    const std::vector<double> ticks = NiceTicks(0, 100, 5);
    const std::vector<double> cols = MaxPerColumn({ 1, 5, 2, 8, 3, 1 }, 3);
    Check(ticks == std::vector<double>({ 0, 20, 40, 60, 80, 100 }) && cols == std::vector<double>({ 5, 8, 3 }) &&
        MaxPerColumn({ 1, 2 }, 3) == std::vector<double>({ 1, 2 }),
        "028: NiceTicks はきりのよい目盛りを返し、MaxPerColumn は列ごとの最大を残す");

    // 16 ECU の実車風サンプル、5 秒
    const std::string prefix = Join(TempDir(), "t028_vehicle16");
    std::vector<LogRow> rows;
    bool sample = RunProjectToCsv(Join(SampleDir(), "sample_vehicle_16ecu.canprj"), prefix, 5000000LL, &rows);
    std::vector<LogRecord> vehicle;
    sample = sample && ReadLogRecords(Widen(prefix) + L".log.csv", vehicle, error);
    const LatencyAnalysis vl = AnalyzeLatency(vehicle);
    long long receives = 0;
    for (const LogRecord& r : vehicle)
        if (r.RxNode > 0 && r.Status != LogStatus::Overwrite)
            ++receives;
    bool counts = sample && !vl.Rows.empty() && vl.Rows[0].Count == receives && receives > 0;
    long long pairSum = 0;
    for (std::size_t i = 0; counts && i < vl.Rows.size(); ++i)
    {
        const Histogram hh = MakeHistogram(vl.Rows[i].Values);
        long long s = 0;
        for (long long c : hh.Counts)
            s += c;
        counts = counts && s == vl.Rows[i].Count;
        if (i > 0)
            pairSum += vl.Rows[i].Count;
    }
    counts = counts && pairSum == receives;
    const BusLoadAnalysis vb = AnalyzeBusLoad(vehicle);
    double ecuSum = 0.0, sectionMean = 0.0;
    for (const EcuBusUsage& e : vb.Ecus)
        ecuSum += e.UsagePct;
    for (double p : vb.SectionPct)
        sectionMean += p;
    sectionMean /= static_cast<double>(vb.SectionPct.size());
    std::ostringstream what;
    what << "028 SC-001/SC-002/SC-003: 16 ECU の実車風サンプル（" << receives << " 回の受信、平均負荷 "
         << vb.AveragePct << " %）: 件数・ヒストグラムの合計・負荷の合計が一致する";
    Check(counts && vb.AveragePct > 0.0 && std::fabs(ecuSum - vb.AveragePct) < 1e-6 &&
        std::fabs(sectionMean - vb.AveragePct) < 1e-6, what.str());
}
