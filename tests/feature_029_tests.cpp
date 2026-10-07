// feature_029_tests.cpp（tests）
// tools/cansim_runner/checks.cpp の CheckFeature029() から移した（機能 030）。
// 本体は、パスの取り方以外は変えていない。Check() のラベルには SC/FR の ID が残っている。

#include "test_helpers.h"

using namespace cansimtest;

TEST(Feature029, SC001to003_FR003_FR005)
{
    std::cout << "\n== 機能 029: 送信待ち時間と送信ジッター ==\n";
    auto readLog = [](const std::string& name, const std::string& text, std::vector<LogRecord>& records) {
        const std::string path = Join(TempDir(), name);
        std::ofstream(path.c_str(), std::ios::binary) << text;
        std::string error;
        return ReadLogRecords(Widen(path), records, error);
    };
    const std::string header = "CAN_ID,Tx_Node,Rx_Node,Tx_Request,Tx_Start,Tx_End,Rx_End,Status\r\n";

    // 2 つの受信側に見える送信 1 回、別の送信 1 回、破棄の行
    std::vector<LogRecord> rec;
    bool ok = readLog("t029_collect.log.csv", header +
        "0x200,2,1,100,150,400,400,SUCCESS\r\n"
        "0x100,1,2,0,0,250,250,SUCCESS\r\n"
        "0x100,1,3,0,0,250,250,SUCCESS\r\n"
        "0x300,3,,500,,,,OVERWRITE\r\n", rec);
    long long overwrites = 0;
    const std::vector<Transmission> tx = CollectTransmissions(rec, overwrites);
    Check(ok && tx.size() == 2 && overwrites == 1 && tx[0].CanId == 0x100 && tx[1].CanId == 0x200 && tx[1].TxStartUs == 150,
        "029 R1: （Tx_Node, CAN_ID, Tx_Start）ごとに送信 1 回にまとめ、CAN ID 順に並べる。OVERWRITE は別に数える");

    // 待ち時間: 0x100 は 0 / 10 / 20、0x200 は 5
    std::vector<LogRecord> waitRec;
    ok = readLog("t029_wait.log.csv", header +
        "0x200,2,1,1000,1005,1250,1250,SUCCESS\r\n"
        "0x100,1,2,0,0,250,250,SUCCESS\r\n"
        "0x100,1,3,0,0,250,250,SUCCESS\r\n"
        "0x100,1,2,10000,10010,10260,10260,ARBITRATION_LOST_RETRY\r\n"
        "0x100,1,2,20000,20020,20270,20270,SUCCESS\r\n"
        "0x300,3,,500,,,,OVERWRITE\r\n", waitRec);
    const TxWaitAnalysis wait = AnalyzeTxWait(waitRec);
    const bool waitRows = ok && wait.Rows.size() == 3 && wait.Rows[0].All && wait.Rows[1].CanId == 0x100 &&
        wait.Rows[1].TxNode == 1 && wait.Rows[2].CanId == 0x200 && wait.OverwriteCount == 1;
    Check(waitRows && wait.Rows[1].Count == 3 && wait.Rows[1].MinUs == 0 && wait.Rows[1].MaxUs == 20 &&
        std::fabs(wait.Rows[1].MeanUs - 10.0) < 1e-9 && wait.Rows[1].P95Us == 20 &&
        wait.Rows[2].Count == 1 && wait.Rows[2].MinUs == 5 &&
        wait.Rows[0].Count == 4 && wait.Rows[0].Count == wait.Rows[1].Count + wait.Rows[2].Count &&
        std::fabs(wait.Rows[0].MeanUs - 35.0 / 4.0) < 1e-9,
        "029 FR-003/SC-002: 送信ごとの Tx_Start - Tx_Request。全体、続いて CAN ID ごとの行。件数の合計が合う");

    // ジッター、周期 10 ms: 開始 0 / 10050 / 20000 / 30120 -> +50, -50, +120
    std::vector<LogRecord> jitRec;
    ok = readLog("t029_jitter.log.csv", header +
        "0x100,1,2,0,0,250,250,SUCCESS\r\n"
        "0x100,1,2,10000,10050,10300,10300,SUCCESS\r\n"
        "0x100,1,3,10000,10050,10300,10300,SUCCESS\r\n"
        "0x100,1,2,20000,20000,20250,20250,SUCCESS\r\n"
        "0x100,1,2,30000,30120,30370,30370,ARBITRATION_LOST_RETRY\r\n"
        "0x200,2,1,0,30,280,280,SUCCESS\r\n"                         // 送信が 1 回抜けている:
        "0x200,2,1,20000,20010,20260,20260,SUCCESS\r\n"              // 要求は 20 ms 間隔 -> -20
        "0x300,3,1,500,500,750,750,SUCCESS\r\n", jitRec);           // 送信が 1 回だけ
    const JitterAnalysis jit = AnalyzeJitter(jitRec);
    const double mean = (50.0 - 50.0 + 120.0) / 3.0;
    const double sd = std::sqrt(((50 - mean) * (50 - mean) + (-50 - mean) * (-50 - mean) + (120 - mean) * (120 - mean)) / 3.0);
    const bool jitOk = ok && jit.Rows.size() == 3 &&
        jit.Rows[0].CanId == 0x100 && jit.Rows[0].PeriodUs == 10000 && jit.Rows[0].Count == 3 &&
        jit.Rows[0].Points.size() == 3 && jit.Rows[0].Points[0].DeviationUs == 50 && jit.Rows[0].Points[0].TimeUs == 10050 &&
        jit.Rows[0].Points[1].DeviationUs == -50 && jit.Rows[0].Points[2].DeviationUs == 120 &&
        jit.Rows[0].MinUs == -50 && jit.Rows[0].MaxUs == 120 && std::fabs(jit.Rows[0].StdDevUs - sd) < 1e-9;
    Check(jitOk, "029 FR-005: ジッター = 開始の間隔 - 要求の間隔。周期・最小・最大・母標準偏差が手計算と一致する");
    Check(ok && jit.Rows.size() == 3 && jit.Rows[1].Count == 1 && jit.Rows[1].Points[0].DeviationUs == -20 &&
        jit.Rows[1].PeriodUs == 20000 && jit.Rows[2].Count == 0 && jit.Rows[2].PeriodUs == 0 && jit.Rows[2].Points.empty(),
        "029 Edge Cases: バッファ満杯で抜けた送信は、ジッターの周期として数えない。1 回しか送られないフレームにジッターは無い");

    // 列ごとの最小・最大
    const std::vector<JitterPoint> pts = { { 0, 5 }, { 10, -3 }, { 20, 7 }, { 90, 1 } };
    const std::vector<ColumnRange> c = MinMaxPerColumn(pts, 0, 99, 4);
    Check(c.size() == 4 && c[0].Has && c[0].MinUs == -3 && c[0].MaxUs == 7 && !c[1].Has && !c[2].Has &&
        c[3].Has && c[3].MinUs == 1 && c[3].MaxUs == 1 && MinMaxPerColumn(pts, 0, 99, 0).empty(),
        "029 R5: MinMaxPerColumn は列ごとの谷と山の両方を残す");

    // 16 ECU の実車風サンプル、5 秒: 件数と、集計 CSV のジッター
    const std::string prefix = Join(TempDir(), "t029_vehicle16");
    bool sample = RunProjectToCsv(Join(SampleDir(), "sample_vehicle_16ecu.canprj"), prefix, 5000000LL);
    std::vector<LogRecord> vehicle;
    std::string error;
    sample = sample && ReadLogRecords(Widen(prefix) + L".log.csv", vehicle, error);
    const TxWaitAnalysis vw = AnalyzeTxWait(vehicle);
    const BusLoadAnalysis vb = AnalyzeBusLoad(vehicle);
    bool counts = sample && !vw.Rows.empty() && vw.Rows[0].Count == vb.TransmissionCount && vb.TransmissionCount > 0;
    long long idSum = 0;
    for (std::size_t i = 0; counts && i < vw.Rows.size(); ++i)
    {
        const Histogram hh = MakeHistogram(vw.Rows[i].Values);
        long long s = 0;
        for (long long n : hh.Counts)
            s += n;
        counts = counts && s == vw.Rows[i].Count;
        if (i > 0)
            idSum += vw.Rows[i].Count;
    }
    counts = counts && idSum == vw.Rows[0].Count;
    std::ostringstream what;
    what << "029 SC-001/SC-002: 16 ECU の実車風サンプル（" << vw.Rows[0].Count << " 回の送信）: 件数とヒストグラムの合計が一致する";
    Check(counts, what.str());

    // 集計 CSV: ID ごとの行、Jitter_Max_us / Jitter_Min_us / Jitter_StdDev_us（小数 2 けた）
    std::ifstream summary((prefix + ".summary.csv").c_str());
    std::string line;
    std::getline(summary, line);
    std::vector<std::string> names;
    {
        std::istringstream hs(line);
        std::string cell;
        while (std::getline(hs, cell, ','))
            names.push_back(cell);
    }
    auto column = [&names](const char* name) {
        for (std::size_t i = 0; i < names.size(); ++i)
            if (names[i] == name)
                return static_cast<int>(i);
        return -1;
    };
    const int cType = column("GroupType"), cKey = column("GroupKey"), cMax = column("Jitter_Max_us"),
        cMin = column("Jitter_Min_us"), cSd = column("Jitter_StdDev_us");
    const JitterAnalysis vj = AnalyzeJitter(vehicle);
    int compared = 0;
    bool same = sample && cType >= 0 && cKey >= 0 && cMax >= 0 && cMin >= 0 && cSd >= 0;
    while (same && std::getline(summary, line))
    {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        std::vector<std::string> cells;
        std::istringstream ls(line);
        std::string cell;
        while (std::getline(ls, cell, ','))
            cells.push_back(cell);
        if (static_cast<int>(cells.size()) <= cSd || cells[static_cast<std::size_t>(cType)] != "ID" ||
            cells[static_cast<std::size_t>(cMax)].empty())
            continue;
        const unsigned id = static_cast<unsigned>(std::stoul(cells[static_cast<std::size_t>(cKey)], nullptr, 16));
        const JitterRow* row = nullptr;
        for (const JitterRow& r : vj.Rows)
            if (r.CanId == id)
                row = &r;
        same = row != nullptr && row->Count > 0 &&
            std::fabs(std::stod(cells[static_cast<std::size_t>(cMax)]) - row->MaxUs) < 0.006 &&
            std::fabs(std::stod(cells[static_cast<std::size_t>(cMin)]) - row->MinUs) < 0.006 &&
            std::fabs(std::stod(cells[static_cast<std::size_t>(cSd)]) - row->StdDevUs) < 0.006;
        ++compared;
    }
    std::ostringstream what2;
    what2 << "029 SC-003: 全 " << compared << " 個の CAN ID のジッターが集計 CSV と一致する（最大・最小・標準偏差を小数 2 けたで）";
    Check(same && compared == static_cast<int>(vj.Rows.size()) && compared > 0, what2.str());

    // 改訂 1: 4 MB ずつ読み、進み具合の通知と中断がある（60 秒のログ、約 20 MB）
    const std::string bigPrefix = Join(TempDir(), "t029_vehicle16_60s");
    bool big = RunProjectToCsv(Join(SampleDir(), "sample_vehicle_16ecu.canprj"), bigPrefix, 60000000LL);
    const std::wstring bigLog = Widen(bigPrefix) + L".log.csv";
    long long lines = 0;
    std::string lastLine;
    {
        std::ifstream in((bigPrefix + ".log.csv").c_str(), std::ios::binary);
        std::string l;
        while (std::getline(in, l))
        {
            if (!l.empty() && l.back() == '\r')
                l.pop_back();
            if (!l.empty())
            {
                ++lines;
                lastLine = l;
            }
        }
    }
    int calls = 0;
    long long lastDone = -1, lastTotal = -2;
    std::vector<LogRecord> bigRecords;
    big = big && ReadLogRecords(bigLog, bigRecords, error, [&](long long done, long long total) {
        ++calls;
        lastDone = done;
        lastTotal = total;
        return true;
    });
    // 最後の行: "<Timestamp>,0x<id>,<dlc>,<tx>,<rx>,<request>,<start>,..."
    bool lastOk = big && !bigRecords.empty();
    if (lastOk)
    {
        std::vector<std::string> cells;
        std::istringstream ls(lastLine);
        std::string cell;
        while (std::getline(ls, cell, ','))
            cells.push_back(cell);
        const LogRecord& lr = bigRecords.back();
        lastOk = cells.size() > 6 && std::stoul(cells[1], nullptr, 16) == lr.CanId && std::stoi(cells[3]) == lr.TxNode &&
            std::stoll(cells[5]) == lr.TxRequestUs && std::stoll(cells[6]) == lr.TxStartUs;
    }
    std::ostringstream what3;
    what3 << "029 改訂 1: " << lastTotal / (1024 * 1024) << " MB のログを 4 MB ずつ読む（" << calls
          << " 回の進み具合の呼び出し）。1 行ずつ読んだときと行・最後の行が同じ";
    Check(big && lastTotal > 4 * 1024 * 1024 && calls >= 2 && lastDone == lastTotal &&
        static_cast<long long>(bigRecords.size()) == lines - 1 && lastOk, what3.str());

    std::vector<LogRecord> cancelled;
    std::string cancelError;
    const bool stopped = !ReadLogRecords(bigLog, cancelled, cancelError, [](long long, long long) { return false; });
    Check(stopped && cancelError == kLogReadCancelled && cancelled.empty(),
        "029 改訂 1: 進み具合の関数が false を返すと読み込みが止まる（kLogReadCancelled、行なし）");
    Check(AnalyzeTxWait(bigRecords).Rows[0].Count == AnalyzeBusLoad(bigRecords).TransmissionCount,
        "029 改訂 1: 速くした CollectTransmissions は、バス負荷と同じ送信の回数を保つ");
}
