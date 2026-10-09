// 機能 028（解析: 受信完了時間・CAN バス負荷）の仕様テスト
// 根拠: specs/028-analysis-latency-busload/spec.md と data-model.md、LogAnalysis.h のコメント。
// 仕様に ID が無いので、受け入れシナリオは US<ストーリー番号>-<シナリオ番号> と呼ぶ。
// ウィンドウ・メニュー・通知はテストせず、CanSim の計算を確かめる。

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "LogAnalysis.h"
#include "support/test_helpers.h"

using namespace CanSim;
using namespace TestHelpers;

namespace
{
    LogRecord Rec(unsigned id, int tx, int rx, long long req, long long start, long long end, long long rxEnd,
                  LogStatus st = LogStatus::Success)
    {
        LogRecord r;
        r.CanId = id;
        r.TxNode = tx;
        r.RxNode = rx;
        r.TxRequestUs = req;
        r.TxStartUs = start;
        r.TxEndUs = end;
        r.RxEndUs = rxEnd;
        r.Status = st;
        return r;
    }

    // 受信完了時間の手計算用のログ
    //   (1 -> 2): 100, 200, 300 us   (1 -> 3): 150 us   (2 -> 1): 400, 500 us
    //   ほかに、受信 ECU の無い行・送信側 OVERWRITE・受信側 OVERWRITE を 1 行ずつ
    std::vector<LogRecord> LatencyRecords()
    {
        return {
            Rec(0x100, 1, 2, 0, 0, 100, 100),
            Rec(0x100, 1, 3, 0, 0, 100, 150), // 受信完了時刻は受信ノードで同じでなくてよい（テスト用）
            Rec(0x101, 1, 2, 1000, 1000, 1200, 1200),
            Rec(0x102, 1, 2, 2000, 2100, 2300, 2300, LogStatus::RetrySuccess),
            Rec(0x200, 2, 1, 3000, 3200, 3400, 3400, LogStatus::RetrySuccess),
            Rec(0x201, 2, 1, 4000, 4300, 4500, 4500),
            Rec(0x300, 3, 0, 5000, 5000, 5200, -1),                          // 受信 ECU なし
            Rec(0x301, 3, 0, 6000, -1, -1, -1, LogStatus::Overwrite),        // 送信側の破棄
            Rec(0x200, 2, 3, 3000, 3200, 3400, 3400, LogStatus::Overwrite),  // 受信側の破棄
        };
    }
}

// ---------------------------------------------------------------------------
// US1-2 / US1-4 / FR-003: 表は先頭に全体、続けて（送信, 受信）の番号順。時間 = Rx_End − Tx_Request。
//   件数・最小・平均・最大・P95（最近順位法）。OVERWRITE と受信 ECU の無い行は数えず、破棄の件数は別に返る。
//   再送された（ARBITRATION_LOST_RETRY）フレームは受信完了した 1 回を 1 件とする。
// 根拠: spec.md US1 シナリオ 2・4、FR-003、Edge Cases（破棄・再送）、data-model.md「LatencyRow」「AnalyzeLatency」。
// ---------------------------------------------------------------------------
TEST(F028Spec, F028_US1_2_LatencyTable_PerPairStatsAndOrder)
{
    RecordProperty("spec", "028:US1-2");
    RecordProperty("spec", "028:US1-4");
    const LatencyAnalysis a = AnalyzeLatency(LatencyRecords());
    ASSERT_EQ(4u, a.Rows.size()) << "全体 + 3 つの組み合わせ";
    EXPECT_EQ(2, a.OverwriteCount) << "破棄の件数（表の外）";

    const LatencyRow& all = a.Rows[0];
    EXPECT_EQ(0, all.TxNode) << "先頭は全体の行";
    EXPECT_EQ(0, all.RxNode);
    EXPECT_EQ(6, all.Count) << "全体の件数";
    EXPECT_EQ(100, all.MinUs);
    EXPECT_EQ(500, all.MaxUs);
    EXPECT_DOUBLE_EQ(275.0, all.MeanUs) << "(100+150+200+300+400+500)/6";
    EXPECT_EQ(500, all.P95Us) << "ceil(0.95×6)=6 番目";
    EXPECT_EQ((std::vector<long long>{100, 150, 200, 300, 400, 500}), all.Values) << "昇順の一覧";

    EXPECT_EQ(1, a.Rows[1].TxNode);
    EXPECT_EQ(2, a.Rows[1].RxNode);
    EXPECT_EQ(3, a.Rows[1].Count);
    EXPECT_EQ(100, a.Rows[1].MinUs);
    EXPECT_EQ(300, a.Rows[1].MaxUs);
    EXPECT_DOUBLE_EQ(200.0, a.Rows[1].MeanUs);
    EXPECT_EQ(300, a.Rows[1].P95Us) << "ceil(0.95×3)=3 番目";

    EXPECT_EQ(1, a.Rows[2].TxNode);
    EXPECT_EQ(3, a.Rows[2].RxNode);
    EXPECT_EQ(1, a.Rows[2].Count);
    EXPECT_EQ(150, a.Rows[2].P95Us);

    EXPECT_EQ(2, a.Rows[3].TxNode);
    EXPECT_EQ(1, a.Rows[3].RxNode);
    EXPECT_EQ(2, a.Rows[3].Count);
    EXPECT_DOUBLE_EQ(450.0, a.Rows[3].MeanUs);
}

// P95 は最近順位法: 昇順の ceil(0.95 × n) 番目（1 始まり）。
// 根拠: data-model.md「LatencyRow」、LogAnalysis.h（sorted[ceil(0.95 n) - 1]）。
//   n = 20（1〜20）-> 19 番目 = 19、n = 21（1〜21）-> ceil(19.95)=20 番目 = 20
TEST(F028Spec, F028_US1_2_P95_NearestRank)
{
    RecordProperty("spec", "028:US1-2");
    for (int n : {20, 21})
    {
        std::vector<LogRecord> recs;
        for (int v = 1; v <= n; ++v) recs.push_back(Rec(0x100, 1, 2, 0, 0, v, v));
        const LatencyAnalysis a = AnalyzeLatency(recs);
        ASSERT_EQ(2u, a.Rows.size());
        EXPECT_EQ(n == 20 ? 19 : 20, a.Rows[0].P95Us) << "n = " << n << " の P95";
    }
}

// Edge Cases: 受信の行が 1 件も無いログでは、全体の行だけで件数 0、最小・平均・最大・P95 は 0。
// 根拠: spec.md Edge Cases 2 つ目、data-model.md（件数 0 の全体の行では 0 とする）。
TEST(F028Spec, F028_US1_2_NoReceives_OnlyAllRowWithZeros)
{
    RecordProperty("spec", "028:US1-2");
    const LatencyAnalysis a = AnalyzeLatency({Rec(0x100, 1, 0, 0, 0, 200, -1)});
    ASSERT_EQ(1u, a.Rows.size()) << "全体の行だけ";
    EXPECT_EQ(0, a.Rows[0].Count);
    EXPECT_EQ(0, a.Rows[0].MinUs);
    EXPECT_EQ(0, a.Rows[0].MaxUs);
    EXPECT_DOUBLE_EQ(0.0, a.Rows[0].MeanUs);
    EXPECT_EQ(0, a.Rows[0].P95Us);
}

// ---------------------------------------------------------------------------
// SC-001: 全体の行の件数 = フレームログの受信完了した行の数（実際の実行のログをファイルから読んで確かめる）。
// SC-002 / US1-5 / FR-004: どの行でもヒストグラムの度数の合計 = その行の件数。
// 根拠: spec.md SC-001、SC-002、data-model.md「Histogram」。
// ---------------------------------------------------------------------------
TEST(F028Spec, F028_SC001_RealLog_TotalCountAndHistogramSums)
{
    RecordProperty("spec", "028:SC-001");
    RecordProperty("spec", "028:SC-002");
    Network net;
    net.BitRateBps = 500000;
    net.Nodes.push_back(MakeNode(1, {{0x100, 8, 10000, 0}, {0x101, 4, 7000, 0}}));
    net.Nodes.push_back(MakeNode(2, {{0x080, 8, 5000, 0}}));
    net.Nodes.push_back(MakeNode(3, {{0x200, 2, 3000, 1000}}));
    const RunResult run = RunSim(net, 300000LL);
    int received = 0;
    for (const LogRow& r : run.Rows)
        if (r.RxNodeId > 0 && r.Status != kStatusOverwrite) ++received;

    const std::wstring path = TempPath("f028_sc001.log.csv");
    ASSERT_TRUE(WriteLogCsv(run.Rows, path));
    std::vector<LogRecord> recs;
    std::string error;
    const bool ok = ReadLogRecords(path, recs, error);
    DeleteFileW(path.c_str());
    ASSERT_TRUE(ok) << error;
    ASSERT_EQ(run.Rows.size(), recs.size()) << "ログの行数";

    const LatencyAnalysis a = AnalyzeLatency(recs);
    EXPECT_EQ(received, a.Rows[0].Count) << "全体の件数 = 受信完了した行の数";
    long long sumOfPairs = 0;
    for (std::size_t i = 0; i < a.Rows.size(); ++i)
    {
        if (i > 0) sumOfPairs += a.Rows[i].Count;
        const Histogram h = MakeHistogram(a.Rows[i].Values);
        long long total = 0;
        for (long long c : h.Counts) total += c;
        EXPECT_EQ(a.Rows[i].Count, total) << i << " 行目のヒストグラムの度数の合計";
    }
    EXPECT_EQ(a.Rows[0].Count, sumOfPairs) << "組み合わせの件数の合計 = 全体の件数";
}

// ---------------------------------------------------------------------------
// US1-5 / FR-004: ヒストグラムのビン。幅は 1・2・5 × 10^m、始まりは最小値を幅で切り捨てた値、各値は自分のビンに入る。
//   n = 0 は空、すべて同じ値は 1 本。
// 根拠: data-model.md「Histogram」「MakeHistogram」、spec.md Edge Cases（1 件・すべて同じ時間は 1 本の棒）。
//   0〜99 の 100 個: 目安 ceil(sqrt(100)) = 10 本、幅 99/10 = 9.9 -> 切り上げて 10、始まり 0 -> 10 本 × 10 個。
// ---------------------------------------------------------------------------
TEST(F028Spec, F028_US1_5_Histogram_RoundWidthAndBins)
{
    RecordProperty("spec", "028:US1-5");
    std::vector<long long> v;
    for (long long i = 0; i < 100; ++i) v.push_back(i);
    const Histogram h = MakeHistogram(v);
    EXPECT_EQ(0, h.StartUs) << "始まり";
    EXPECT_EQ(10, h.WidthUs) << "幅";
    EXPECT_EQ(std::vector<long long>(10, 10), h.Counts) << "10 本 × 10 個";

    EXPECT_TRUE(MakeHistogram({}).Counts.empty()) << "n = 0 は空";
    const Histogram one = MakeHistogram({250, 250, 250});
    ASSERT_EQ(1u, one.Counts.size()) << "すべて同じ値は 1 本";
    EXPECT_EQ(3, one.Counts[0]);
    const Histogram single = MakeHistogram({777});
    ASSERT_EQ(1u, single.Counts.size()) << "1 件は 1 本";

    // 幅がきりの良い値で、各値が [Start + iW, Start + (i+1)W) に数えられている
    const std::vector<long long> spread = {101, 133, 140, 187, 260, 333, 412, 418, 555, 901, 1203, 1250};
    const Histogram s = MakeHistogram(spread);
    long long w = s.WidthUs;
    while (w % 10 == 0 && w >= 10) w /= 10;
    EXPECT_TRUE(w == 1 || w == 2 || w == 5) << "幅 " << s.WidthUs << " は 1・2・5 × 10^m でない";
    EXPECT_EQ(0, s.StartUs % s.WidthUs) << "始まりは幅で切り捨てた値";
    EXPECT_LE(s.StartUs, 101);
    std::vector<long long> expected(s.Counts.size(), 0);
    for (long long x : spread)
    {
        const long long i = (x - s.StartUs) / s.WidthUs;
        ASSERT_LT(static_cast<std::size_t>(i), expected.size()) << x << " が入るビンが無い";
        ++expected[static_cast<std::size_t>(i)];
    }
    EXPECT_EQ(expected, s.Counts) << "各値が自分のビンに数えられている";
}

// ---------------------------------------------------------------------------
// US2-2 / US2-3 / FR-006 / SC-003: 1 フレームだけの周期送信（250 us を 10 ms ごとに 5 回、受信 2 ECU）。
//   範囲 = 0〜最後の送信終了を含む区間の終わり = 50 ms。平均 = 250 × 5 ÷ 50000 = 2.5%。各区間も 2.5%。
//   受信 ECU の数だけの重複は 1 回にまとめる。OVERWRITE 行は使用時間に入らない。
// 根拠: spec.md US2 シナリオ 2・3、FR-006、SC-003、data-model.md「BusLoadAnalysis」。
// ---------------------------------------------------------------------------
TEST(F028Spec, F028_SC003_PeriodicSingleFrame_AverageMatchesHandCalc)
{
    RecordProperty("spec", "028:US2-2");
    RecordProperty("spec", "028:SC-003");
    std::vector<LogRecord> recs;
    for (int k = 0; k < 5; ++k)
    {
        const long long s = 1000 + 10000LL * k;
        recs.push_back(Rec(0x100, 1, 2, s, s, s + 250, s + 250));
        recs.push_back(Rec(0x100, 1, 3, s, s, s + 250, s + 250));
    }
    recs.push_back(Rec(0x101, 1, 0, 2000, -1, -1, -1, LogStatus::Overwrite));
    const BusLoadAnalysis b = AnalyzeBusLoad(recs);
    EXPECT_EQ(10000, b.SectionUs);
    EXPECT_EQ(50000, b.RangeUs) << "範囲";
    EXPECT_EQ(5, b.TransmissionCount) << "送信は 5 回（受信 ECU の数だけの重複をまとめる）";
    EXPECT_DOUBLE_EQ(2.5, b.AveragePct) << "平均負荷";
    ASSERT_EQ(5u, b.SectionPct.size());
    for (double p : b.SectionPct) EXPECT_DOUBLE_EQ(2.5, p) << "各区間の負荷";
    EXPECT_DOUBLE_EQ(2.5, b.MaxPct);
    EXPECT_EQ(0, b.MaxSection) << "同じ値なら最初の区間";
    ASSERT_EQ(1u, b.Ecus.size()) << "送信した ECU だけ";
    EXPECT_EQ(1, b.Ecus[0].Node);
    EXPECT_EQ(5, b.Ecus[0].FrameCount);
    EXPECT_EQ(1250, b.Ecus[0].BusyUs);
    EXPECT_DOUBLE_EQ(2.5, b.Ecus[0].UsagePct);
}

// 区間をまたぐ送信は分けて足す。ECU ごとの使用率の合計と、区間の負荷の重み付き平均は、どちらも平均負荷と一致する。
// 根拠: data-model.md「AnalyzeBusLoad」（区間をまたぐ分は分けて足す、決まり）、SC-003。
//   ECU 1: [0, 500)、ECU 2: [9900, 10100) -> 区間 0 = 600/10000 = 6%、区間 1 = 100/10000 = 1%、
//   範囲 20000、平均 = 700/20000 = 3.5%、ECU 1 = 2.5%、ECU 2 = 1%
TEST(F028Spec, F028_SC003_FrameAcrossSections_SplitAndSumsMatch)
{
    RecordProperty("spec", "028:SC-003");
    const BusLoadAnalysis b = AnalyzeBusLoad({Rec(0x100, 1, 2, 0, 0, 500, 500), Rec(0x200, 2, 1, 9900, 9900, 10100, 10100)});
    EXPECT_EQ(20000, b.RangeUs);
    ASSERT_EQ(2u, b.SectionPct.size());
    EXPECT_DOUBLE_EQ(6.0, b.SectionPct[0]);
    EXPECT_DOUBLE_EQ(1.0, b.SectionPct[1]);
    EXPECT_DOUBLE_EQ(3.5, b.AveragePct);
    EXPECT_DOUBLE_EQ(6.0, b.MaxPct);
    EXPECT_EQ(0, b.MaxSection);
    ASSERT_EQ(2u, b.Ecus.size());
    EXPECT_DOUBLE_EQ(2.5, b.Ecus[0].UsagePct) << "ECU 1";
    EXPECT_DOUBLE_EQ(1.0, b.Ecus[1].UsagePct) << "ECU 2";
    EXPECT_NEAR(b.AveragePct, b.Ecus[0].UsagePct + b.Ecus[1].UsagePct, 1e-9) << "ECU ごとの使用率の合計 = 平均";
    EXPECT_NEAR(b.AveragePct, (b.SectionPct[0] + b.SectionPct[1]) * 10000 / 20000, 1e-9) << "区間の重み付き平均 = 平均";
}

// 送信が 1 つも無いときの範囲は 10,000 us。
// 根拠: data-model.md「BusLoadAnalysis」（送信が無ければ 10,000）。
TEST(F028Spec, F028_US2_2_NoTransmissions_RangeIsOneSection)
{
    RecordProperty("spec", "028:US2-2");
    const BusLoadAnalysis b = AnalyzeBusLoad({});
    EXPECT_EQ(10000, b.RangeUs);
    EXPECT_DOUBLE_EQ(0.0, b.AveragePct);
    EXPECT_TRUE(b.Ecus.empty());
    EXPECT_EQ(0, b.TransmissionCount);
}

// ---------------------------------------------------------------------------
// FR-002 / US3-3: フレームログを列名で読む。空欄は RxNode 0・時刻 -1。列が足りない・数値にならない・
//   ファイルが無いときは失敗し、数値の誤りは行番号付き。
// 根拠: spec.md FR-002、US3 シナリオ 3、data-model.md「LogRecord」「ReadLogRecords」。
// ---------------------------------------------------------------------------
TEST(F028Spec, F028_FR002_ReadLogRecords_ColumnsByNameAndBlanks)
{
    RecordProperty("spec", "028:FR-002");
    const std::string header =
        "Timestamp_us,CAN_ID,DLC,Tx_Node,Rx_Node,Tx_Request,Tx_Start,Tx_End,Rx_Start,Rx_End,Rx_Process,Bus_Load_Pct,"
        "Status,Retry_Count,Update_Count\r\n";
    const std::wstring path = TempPath("f028_read.log.csv");
    WriteAll(path, header +
                       "300,0x080,8,3,1,10,20,270,20,270,300,1.0,SUCCESS,0,0\r\n"
                       "\r\n"
                       "600,0x100,8,1,,10,280,530,,,,1.0,ARBITRATION_LOST_RETRY,1,0\r\n"
                       "10,0x7FF,0,2,,10,,,,,,0.0,OVERWRITE,0,0\r\n");
    std::vector<LogRecord> recs;
    std::string error;
    const bool ok = ReadLogRecords(path, recs, error);
    DeleteFileW(path.c_str());
    ASSERT_TRUE(ok) << error;
    ASSERT_EQ(3u, recs.size()) << "空行は飛ばす";
    EXPECT_EQ(0x080u, recs[0].CanId);
    EXPECT_EQ(3, recs[0].TxNode);
    EXPECT_EQ(1, recs[0].RxNode);
    EXPECT_EQ(10, recs[0].TxRequestUs);
    EXPECT_EQ(20, recs[0].TxStartUs);
    EXPECT_EQ(270, recs[0].TxEndUs);
    EXPECT_EQ(270, recs[0].RxEndUs);
    EXPECT_EQ(LogStatus::Success, recs[0].Status);
    EXPECT_EQ(0, recs[1].RxNode) << "空欄の Rx_Node は 0";
    EXPECT_EQ(-1, recs[1].RxEndUs) << "空欄の時刻は -1";
    EXPECT_EQ(LogStatus::RetrySuccess, recs[1].Status);
    EXPECT_EQ(0x7FFu, recs[2].CanId);
    EXPECT_EQ(-1, recs[2].TxStartUs);
    EXPECT_EQ(LogStatus::Overwrite, recs[2].Status);
}

TEST(F028Spec, F028_US3_3_UnreadableLog_FailsWithReason)
{
    RecordProperty("spec", "028:US3-3");
    std::vector<LogRecord> recs;
    std::string error;

    const std::wstring missingCol = TempPath("f028_missing_col.log.csv");
    WriteAll(missingCol, "Timestamp_us,CAN_ID,Tx_Node\r\n1,0x100,1\r\n");
    EXPECT_FALSE(ReadLogRecords(missingCol, recs, error)) << "列が足りないログを読めた";
    EXPECT_FALSE(error.empty()) << "理由が無い";
    DeleteFileW(missingCol.c_str());

    const std::wstring badNumber = TempPath("f028_bad_number.log.csv");
    WriteAll(badNumber,
             "Timestamp_us,CAN_ID,DLC,Tx_Node,Rx_Node,Tx_Request,Tx_Start,Tx_End,Rx_Start,Rx_End,Rx_Process,"
             "Bus_Load_Pct,Status,Retry_Count,Update_Count\r\n"
             "300,0x080,8,3,1,10,20,270,20,270,300,1.0,SUCCESS,0,0\r\n"
             "600,0x100,8,1,2,abc,280,530,280,530,600,1.0,SUCCESS,0,0\r\n");
    error.clear();
    EXPECT_FALSE(ReadLogRecords(badNumber, recs, error)) << "数値にならない欄があるログを読めた";
    EXPECT_NE(std::string::npos, error.find("3")) << "行番号（3 行目）が無い: " << error;
    DeleteFileW(badNumber.c_str());

    // 列が足りない行・CAN_ID が 0x の 16 進でない行・知らない Status の行も、行番号付きで失敗
    // （data-model.md「LogRecord」: CAN_ID は 16 進（0x080 など）、Status は 3 種類以外は読めない）
    const std::string header =
        "Timestamp_us,CAN_ID,DLC,Tx_Node,Rx_Node,Tx_Request,Tx_Start,Tx_End,Rx_Start,Rx_End,Rx_Process,"
        "Bus_Load_Pct,Status,Retry_Count,Update_Count\r\n";
    const char* const badRows[] = {"300,0x080,8,3\r\n", "300,128,8,3,1,10,20,270,20,270,300,1.0,SUCCESS,0,0\r\n",
                                   "300,0xG0,8,3,1,10,20,270,20,270,300,1.0,SUCCESS,0,0\r\n",
                                   "300,0x080,8,3,1,10,20,270,20,270,300,1.0,ERROR,0,0\r\n"};
    for (const char* row : badRows)
    {
        const std::wstring p = TempPath("f028_bad_row.log.csv");
        WriteAll(p, header + row);
        error.clear();
        EXPECT_FALSE(ReadLogRecords(p, recs, error)) << "読めない行を読めた: " << row;
        EXPECT_NE(std::string::npos, error.find("2")) << "行番号（2 行目）が無い: " << error;
        DeleteFileW(p.c_str());
    }

    // 1 行目がヘッダーなので、空のファイルは読めない（data-model.md「ReadLogRecords」）
    const std::wstring emptyFile = TempPath("f028_empty.log.csv");
    WriteAll(emptyFile, "");
    error.clear();
    EXPECT_FALSE(ReadLogRecords(emptyFile, recs, error)) << "空のファイル（ヘッダーなし）を読めた";
    EXPECT_FALSE(error.empty());
    DeleteFileW(emptyFile.c_str());

    error.clear();
    EXPECT_FALSE(ReadLogRecords(TempPath("f028_no_such_file.log.csv"), recs, error)) << "無いファイルを読めた";
    EXPECT_FALSE(error.empty());
}

// ---------------------------------------------------------------------------
// US1-5 / US2-2: グラフの軸の目盛りは 1・2・5 × 10^m 刻み。点が描画域の列より多いときは列ごとの最大で間引く。
// 根拠: data-model.md「グラフの補助」。
//   NiceTicks(0, 100, 5): 100/5 = 20 -> 刻み 20 -> 0, 20, ..., 100。hi <= lo なら {lo}。
//   MaxPerColumn(1..10, 5): 列 c は [2c, 2c+2) -> 2, 4, 6, 8, 10。
// ---------------------------------------------------------------------------
TEST(F028Spec, F028_US2_2_GraphHelpers_TicksAndColumnMax)
{
    RecordProperty("spec", "028:US2-2");
    EXPECT_EQ((std::vector<double>{0, 20, 40, 60, 80, 100}), NiceTicks(0, 100, 5)) << "目盛り";
    EXPECT_EQ((std::vector<double>{7}), NiceTicks(7, 7, 5)) << "hi <= lo";
    EXPECT_EQ((std::vector<double>{2, 4, 6, 8, 10}), MaxPerColumn({1, 2, 3, 4, 5, 6, 7, 8, 9, 10}, 5)) << "列ごとの最大";
    EXPECT_EQ((std::vector<double>{3, 1, 2}), MaxPerColumn({3, 1, 2}, 5)) << "列より少なければそのまま";
}
