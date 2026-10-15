// feature_028_spec_tests.cpp（tests/unit）
// 機能 028（specs/028-analysis-latency-busload/spec.md）の受け入れシナリオと成功基準のテスト。
// ウィンドウの表示は画面の確認なので、ここでは計算（CanSim::LogAnalysis）を確かめる。
// ログは実際の実行の結果をログの CSV に書き、ReadLogRecords で読み直したものを使う（FR-002）。

#include "support/spec_test_helpers.h"

#include "LogAnalysis.h"

#include <cmath>
#include <numeric>

using namespace CanSim;
using namespace spectest;

namespace
{
    // 実行 → ログの CSV → 読み直し
    std::vector<LogRecord> RunToRecords(const Network& net, long long durationUs, const std::wstring& name)
    {
        const std::vector<LogRow> rows = RunSim(net, durationUs);
        const std::wstring path = TempPath(name);
        EXPECT_TRUE(WriteLogCsv(rows, path)) << "ログの CSV を書けない";
        std::vector<LogRecord> records;
        std::string error;
        EXPECT_TRUE(ReadLogRecords(path, records, error)) << "ログを読めない: " << error;
        return records;
    }

    // 手で計算できる構成（500 kbps、100 ms）: ノード 1 が 0x080、ノード 2 が 0x100 を、どちらも初回オフセット 0・周期 10 ms で
    // 送る（毎回同時に要求）。ノード 3 は受信だけ。0x080 は 22 µs に送り始めて 268 µs に完了（246 µs、機能 031）。
    // 0x100 は調停に負け、休止期間の後の 274 µs に送り始めて 520 µs に完了。
    // 受信完了時間（受信完了 − 送信要求）: 0x080 は 246 µs、0x100 は 498 µs。
    Network PairNetwork()
    {
        Network net;
        net.BitRateBps = 500000;
        net.Nodes.push_back(MakeNode(1, { MakeFrame(0x080, 8, 10000) }));
        net.Nodes.push_back(MakeNode(2, { MakeFrame(0x100, 8, 10000) }));
        net.Nodes.push_back(MakeNode(3));
        return net;
    }
}

// 根拠: spec.md US1 受け入れシナリオ 2、FR-003。表は先頭が全体、続いて受信があった送信 ECU × 受信 ECU の組み合わせが
// 番号順に並び、各行に件数・最小・平均・最大・95 パーセンタイル（最近順位法、LogAnalysis.h）がある。
// 手計算: (1,2)・(1,3) は 246 µs × 10 件、(2,1)・(2,3) は 498 µs × 10 件。全体は 40 件、最小 246、最大 498、平均 372、
// 95 パーセンタイルは小さい順の ceil(0.95 × 40) = 38 番目 = 498。
TEST(F028Spec, F028_US1_2_LatencyTable_AllThenPairsWithStats)
{
    RecordProperty("spec", "028:US1-2");
    const LatencyAnalysis a = AnalyzeLatency(RunToRecords(PairNetwork(), 100000LL, L"f028_us1_2.log.csv"));
    ASSERT_EQ(a.Rows.size(), 5u) << "全体 + 4 つの組み合わせになっていない";
    const LatencyRow& all = a.Rows[0];
    EXPECT_EQ(all.TxNode, 0) << "先頭が全体の行でない";
    EXPECT_EQ(all.RxNode, 0) << "先頭が全体の行でない";
    EXPECT_EQ(all.Count, 40) << "全体の件数";
    EXPECT_EQ(all.MinUs, 246) << "全体の最小";
    EXPECT_EQ(all.MaxUs, 498) << "全体の最大";
    EXPECT_DOUBLE_EQ(all.MeanUs, 372.0) << "全体の平均";
    EXPECT_EQ(all.P95Us, 498) << "全体の 95 パーセンタイル";

    const int expectedPairs[4][2] = { { 1, 2 }, { 1, 3 }, { 2, 1 }, { 2, 3 } };
    const long long expectedUs[4] = { 246, 246, 498, 498 };
    for (int i = 0; i < 4; ++i)
    {
        const LatencyRow& r = a.Rows[i + 1];
        EXPECT_EQ(r.TxNode, expectedPairs[i][0]) << i + 1 << " 行目の送信 ECU";
        EXPECT_EQ(r.RxNode, expectedPairs[i][1]) << i + 1 << " 行目の受信 ECU";
        EXPECT_EQ(r.Count, 10) << i + 1 << " 行目の件数";
        EXPECT_EQ(r.MinUs, expectedUs[i]) << i + 1 << " 行目の最小";
        EXPECT_EQ(r.MaxUs, expectedUs[i]) << i + 1 << " 行目の最大";
        EXPECT_DOUBLE_EQ(r.MeanUs, static_cast<double>(expectedUs[i])) << i + 1 << " 行目の平均";
        EXPECT_EQ(r.P95Us, expectedUs[i]) << i + 1 << " 行目の 95 パーセンタイル";
    }
}

// 根拠: spec.md US1 受け入れシナリオ 4、Edge Cases（調停負けで再送されたフレームは、最終的に受信完了した 1 回を 1 件とし、
// 時間は再送による遅れを含む）。各受信について、送信要求の時刻から受信完了の時刻までを 1 件の時間とする。
// 0x100 は毎回調停に負けて再送されるが、1 回の送信につき受信 1 件で、時間は 498 µs（待ち 252 µs を含む）。
TEST(F028Spec, F028_US1_4_LatencyIsRequestToRxEnd_IncludingRetryDelay)
{
    RecordProperty("spec", "028:US1-4");
    const std::vector<LogRecord> records = RunToRecords(PairNetwork(), 100000LL, L"f028_us1_4.log.csv");
    long long retried = 0;
    for (const LogRecord& r : records)
        if (r.Status == LogStatus::RetrySuccess)
            ++retried;
    ASSERT_EQ(retried, 20) << "前提: 0x100 の 10 回の送信 × 受信 2 ECU が再送の行になっていない";
    const LatencyAnalysis a = AnalyzeLatency(records);
    for (const LatencyRow& r : a.Rows)
    {
        if (r.TxNode != 2)
            continue;
        EXPECT_EQ(r.Count, 10) << "再送の行が 1 送信 1 件になっていない（受信 ECU " << r.RxNode << "）";
        for (long long v : r.Values)
            EXPECT_EQ(v, 498) << "再送による遅れを含めた 送信要求 → 受信完了 の時間でない";
    }
}

// 根拠: spec.md US1 受け入れシナリオ 5、FR-004、LogAnalysis.h の MakeHistogram の約束（幅は 1・2・5 × 10^m、
// ビンの数はおよそ clamp(ceil(sqrt(n)), 10, 50)、度数の合計は n）。ビンの幅・範囲は分布に合わせて自動で決まる。
// 軸の目盛りと値の表示は画面の確認なので、ここでは幅と範囲（すべての値がどれかのビンに入る）を確かめる。
TEST(F028Spec, F028_US1_5_Histogram_RoundWidthCoveringAllValues)
{
    RecordProperty("spec", "028:US1-5");
    const LatencyAnalysis a = AnalyzeLatency(RunToRecords(PairNetwork(), 100000LL, L"f028_us1_5.log.csv"));
    const std::vector<long long>& values = a.Rows[0].Values;
    const Histogram h = MakeHistogram(values);
    ASSERT_FALSE(h.Counts.empty()) << "ヒストグラムが空";
    long long w = h.WidthUs;
    while (w % 10 == 0)
        w /= 10;
    EXPECT_TRUE(w == 1 || w == 2 || w == 5) << "ビンの幅が 1・2・5 × 10^m でない: " << h.WidthUs;
    EXPECT_LE(h.StartUs, values.front()) << "最小の値がビンの範囲の外";
    EXPECT_GT(h.StartUs + h.WidthUs * static_cast<long long>(h.Counts.size()), values.back()) << "最大の値がビンの範囲の外";
}

// 根拠: spec.md US2 受け入れシナリオ 2、FR-005・FR-006。10 ms ごとの区間の負荷は「区間の中で使われていた時間 ÷ 区間の長さ」、
// 範囲は 0 からログの最後のフレームを含む区間の終わりまで。手計算: 各 10 ms の区間に 0x080 と 0x100 の 246 µs × 2 = 492 µs、
// 負荷 4.92%。最後の送信完了は 90,520 µs なので範囲は 100,000 µs（10 区間）。
TEST(F028Spec, F028_US2_2_SectionLoad_Every10ms)
{
    RecordProperty("spec", "028:US2-2");
    const BusLoadAnalysis b = AnalyzeBusLoad(RunToRecords(PairNetwork(), 100000LL, L"f028_us2_2.log.csv"));
    EXPECT_EQ(b.SectionUs, 10000) << "区間の長さ";
    EXPECT_EQ(b.RangeUs, 100000) << "範囲（最後のフレームを含む区間の終わり）";
    ASSERT_EQ(b.SectionPct.size(), 10u) << "区間の数";
    for (std::size_t i = 0; i < b.SectionPct.size(); ++i)
        EXPECT_NEAR(b.SectionPct[i], 4.92, 1e-9) << i << " 番目の区間の負荷";
    EXPECT_NEAR(b.MaxPct, 4.92, 1e-9) << "最大負荷";
}

// 根拠: spec.md US2 受け入れシナリオ 3、FR-005・FR-006。送信 ECU ごとに送信したフレーム数と使用率
// （その ECU の送信が使った時間 ÷ 範囲の長さ）。手計算: どちらも 10 フレーム × 246 µs = 2,460 µs、100,000 µs で 2.46%。
TEST(F028Spec, F028_US2_3_PerEcuFramesAndUsage)
{
    RecordProperty("spec", "028:US2-3");
    const BusLoadAnalysis b = AnalyzeBusLoad(RunToRecords(PairNetwork(), 100000LL, L"f028_us2_3.log.csv"));
    ASSERT_EQ(b.Ecus.size(), 2u) << "送信した ECU の数";
    for (int i = 0; i < 2; ++i)
    {
        EXPECT_EQ(b.Ecus[i].Node, i + 1) << "ECU の並び";
        EXPECT_EQ(b.Ecus[i].FrameCount, 10) << "ECU " << i + 1 << " の送信フレーム数";
        EXPECT_EQ(b.Ecus[i].BusyUs, 2460) << "ECU " << i + 1 << " の使用時間";
        EXPECT_NEAR(b.Ecus[i].UsagePct, 2.46, 1e-9) << "ECU " << i + 1 << " の使用率";
    }
}

// 根拠: spec.md US3 受け入れシナリオ 3、FR-007。フレームログを読めない・形式が違うときは、読めなかったことがわかる
// （ReadLogRecords が失敗を返し、理由を示す。LogAnalysis.h）。通知の表示は画面の確認。
TEST(F028Spec, F028_US3_3_UnreadableLog_ReadFailsWithReason)
{
    RecordProperty("spec", "028:US3-3");
    std::vector<LogRecord> records;
    std::string error;

    const std::wstring noColumn = TempPath(L"f028_us3_3_nocolumn.log.csv");
    WriteText(noColumn, "Timestamp_us,CAN_ID\r\n1,0x080\r\n");
    EXPECT_FALSE(ReadLogRecords(noColumn, records, error)) << "列の足りないログが読めてしまった";
    EXPECT_FALSE(error.empty()) << "列が足りないときの理由が無い";

    const std::wstring badValue = TempPath(L"f028_us3_3_badvalue.log.csv");
    const std::string header = "Timestamp_us,CAN_ID,DLC,Tx_Node,Rx_Node,Tx_Request,Tx_Start,Tx_End,Rx_Start,Rx_End,"
                               "Rx_Process,Bus_Load_Pct,Status,Retry_Count,Update_Count\r\n";
    WriteText(badValue, header + "268,0x080,8,1,2,22,abc,268,22,268,268,4.9,SUCCESS,0,0\r\n");
    error.clear();
    EXPECT_FALSE(ReadLogRecords(badValue, records, error)) << "読めない値のあるログが読めてしまった";
    EXPECT_FALSE(error.empty()) << "値が読めないときの理由が無い";

    error.clear();
    EXPECT_FALSE(ReadLogRecords(TempPath(L"f028_no_such_file.log.csv"), records, error)) << "無いファイルが読めてしまった";
}

// 根拠: spec.md SC-001。表の件数の合計（全体の行の件数）が、フレームログの受信完了した行の数と一致する。
// 受信のある行・送信だけの行・受信側の破棄（OVERWRITE）を含む構成で確かめる。
TEST(F028Spec, F028_SC001_AllRowCount_EqualsReceivedRows)
{
    RecordProperty("spec", "028:SC-001");
    Network net;
    net.BitRateBps = 500000;
    std::vector<FrameDefinition> burst;
    for (unsigned i = 0; i < 10; ++i)
        burst.push_back(MakeFrame(0x100 + i, 8, 10000));
    net.Nodes.push_back(MakeNode(1, burst));
    Node slow = MakeNode(2);
    slow.ReceiveDelayUs = 5000; // 受信バッファが満杯になり、受信側の破棄が出る
    net.Nodes.push_back(slow);
    net.Nodes.push_back(MakeNode(3, { MakeFrame(0x050, 2, 3000) }));
    const std::vector<LogRecord> records = RunToRecords(net, 50000LL, L"f028_sc001.log.csv");
    long long received = 0, overwrite = 0;
    for (const LogRecord& r : records)
    {
        if (r.Status == LogStatus::Overwrite)
            ++overwrite;
        else if (r.RxNode > 0)
            ++received;
    }
    ASSERT_GT(overwrite, 0) << "前提: 破棄が無い";
    const LatencyAnalysis a = AnalyzeLatency(records);
    EXPECT_EQ(a.Rows[0].Count, received) << "全体の件数が、受信完了した行の数と違う";
    long long sum = 0;
    for (std::size_t i = 1; i < a.Rows.size(); ++i)
        sum += a.Rows[i].Count;
    EXPECT_EQ(sum, received) << "組み合わせの行の件数の合計が、受信完了した行の数と違う";
    EXPECT_EQ(a.OverwriteCount, overwrite) << "表の外の破棄の件数";
}

// 根拠: spec.md SC-002。どの行でも、ヒストグラムの度数の合計がその行の件数と一致する。
TEST(F028Spec, F028_SC002_EveryRow_HistogramSumEqualsCount)
{
    RecordProperty("spec", "028:SC-002");
    Network net;
    net.BitRateBps = 250000;
    net.Nodes.push_back(MakeNode(1, { MakeFrame(0x080, 8, 5000), MakeFrame(0x081, 1, 7000, 1000) }));
    net.Nodes.push_back(MakeNode(2, { MakeFrame(0x100, 8, 5000) }));
    net.Nodes.push_back(MakeNode(3, { MakeFrame(0x0F0, 4, 3000, 200) }));
    net.Nodes.push_back(MakeNode(4));
    const LatencyAnalysis a = AnalyzeLatency(RunToRecords(net, 200000LL, L"f028_sc002.log.csv"));
    ASSERT_GT(a.Rows.size(), 1u) << "前提: 組み合わせの行が無い";
    for (const LatencyRow& r : a.Rows)
    {
        const Histogram h = MakeHistogram(r.Values);
        const long long sum = std::accumulate(h.Counts.begin(), h.Counts.end(), 0LL);
        EXPECT_EQ(sum, r.Count) << "送信 " << r.TxNode << "・受信 " << r.RxNode << " の度数の合計が件数と違う";
    }
}

// 根拠: spec.md SC-003、FR-006。ECU ごとの使用率の合計と、10 ms の区間の負荷の平均（区間の長さで重み付け）が、
// どちらも平均負荷と一致する。手計算: 平均負荷 = 伝送時間 × 送信回数 ÷ 範囲 = 246 × 20 ÷ 100,000 = 4.92%。
TEST(F028Spec, F028_SC003_AverageLoad_EqualsEcuSumAndSectionMean)
{
    RecordProperty("spec", "028:SC-003");
    const BusLoadAnalysis b = AnalyzeBusLoad(RunToRecords(PairNetwork(), 100000LL, L"f028_sc003.log.csv"));
    EXPECT_NEAR(b.AveragePct, 4.92, 1e-9) << "平均負荷が手計算と違う";
    double ecuSum = 0.0;
    for (const EcuBusUsage& e : b.Ecus)
        ecuSum += e.UsagePct;
    EXPECT_NEAR(ecuSum, b.AveragePct, 1e-9) << "ECU ごとの使用率の合計が平均負荷と違う";
    double weighted = 0.0;
    for (double pct : b.SectionPct)
        weighted += pct * static_cast<double>(b.SectionUs);
    EXPECT_NEAR(weighted / static_cast<double>(b.RangeUs), b.AveragePct, 1e-9) << "区間の負荷の平均が平均負荷と違う";
}
