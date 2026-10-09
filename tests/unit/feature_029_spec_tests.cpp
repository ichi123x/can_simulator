// 機能 029（解析: 送信待ち時間・送信ジッター。改訂 1 の読み込みの中断を含む）の仕様テスト
// 根拠: specs/029-tx-wait-jitter-analysis/spec.md と data-model.md、LogAnalysis.h のコメント。
// 仕様に ID が無いので、受け入れシナリオは US<ストーリー番号>-<シナリオ番号> と呼ぶ。
// ウィンドウ・メニュー・通知・解析中の表示はテストせず、CanSim の計算を確かめる。

#include <gtest/gtest.h>

#include <cstdio>
#include <map>
#include <string>
#include <vector>

#include "LogAnalysis.h"
#include "MetricsAggregator.h"
#include "support/test_helpers.h"

using namespace CanSim;
using namespace TestHelpers;

namespace
{
    LogRecord Rec(unsigned id, int tx, int rx, long long req, long long start, LogStatus st = LogStatus::Success)
    {
        LogRecord r;
        r.CanId = id;
        r.TxNode = tx;
        r.RxNode = rx;
        r.TxRequestUs = req;
        r.TxStartUs = start;
        r.TxEndUs = start >= 0 ? start + 200 : -1;
        r.RxEndUs = (start >= 0 && rx > 0) ? start + 200 : -1;
        r.Status = st;
        return r;
    }

    // 送信待ち時間の手計算用（受信 ECU 2 つの行が重複する）
    //   0x100（ECU 1）: (要求 0, 開始 0)、(要求 10000, 開始 10300・再送)  -> 待ち 0, 300
    //   0x080（ECU 2）: (要求 0, 開始 250)、(要求 10000, 開始 10000)      -> 待ち 250, 0
    //   0x300（ECU 3）: 送信側で破棄
    std::vector<LogRecord> WaitRecords()
    {
        return {
            Rec(0x100, 1, 2, 0, 0),
            Rec(0x100, 1, 3, 0, 0),
            Rec(0x080, 2, 1, 0, 250),
            Rec(0x080, 2, 3, 0, 250),
            Rec(0x100, 1, 2, 10000, 10300, LogStatus::RetrySuccess),
            Rec(0x100, 1, 3, 10000, 10300, LogStatus::RetrySuccess),
            Rec(0x080, 2, 1, 10000, 10000),
            Rec(0x080, 2, 3, 10000, 10000),
            Rec(0x300, 3, 0, 5000, -1, LogStatus::Overwrite),
        };
    }

    const GroupMetrics* FindGroup(const std::vector<GroupMetrics>& metrics, const std::string& type, const std::string& key)
    {
        for (const GroupMetrics& g : metrics)
            if (g.GroupType == type && g.GroupKey == key) return &g;
        return nullptr;
    }
}

// ---------------------------------------------------------------------------
// FR-003 / US1-4: 送信 1 回 = (送信 ECU, CAN ID, 送信開始) でまとめ、(CAN ID, 送信開始) の順。OVERWRITE は数だけ。
// 根拠: spec.md US1 シナリオ 4、Key Entities「送信の記録」、data-model.md「CollectTransmissions」。
// ---------------------------------------------------------------------------
TEST(F029Spec, F029_US1_4_CollectTransmissions_DedupSortAndCountOverwrite)
{
    RecordProperty("spec", "029:US1-4");
    long long overwrite = 0;
    const std::vector<Transmission> tx = CollectTransmissions(WaitRecords(), overwrite);
    EXPECT_EQ(1, overwrite) << "OVERWRITE の行の数";
    ASSERT_EQ(4u, tx.size()) << "受信 ECU の数だけの重複をまとめて 4 回";
    EXPECT_EQ(0x080u, tx[0].CanId);
    EXPECT_EQ(250, tx[0].TxStartUs);
    EXPECT_EQ(0x080u, tx[1].CanId);
    EXPECT_EQ(10000, tx[1].TxStartUs);
    EXPECT_EQ(0x100u, tx[2].CanId);
    EXPECT_EQ(0, tx[2].TxStartUs);
    EXPECT_EQ(0x100u, tx[3].CanId);
    EXPECT_EQ(10300, tx[3].TxStartUs);
    EXPECT_EQ(10000, tx[3].TxRequestUs) << "再送されたフレームも、送れた 1 回を 1 件";
}

// ---------------------------------------------------------------------------
// US1-2 / US1-4 / FR-003: 送信待ち時間 = 送信開始 − 送信要求。先頭に全体、続けて CAN ID の小さい順（送信 ECU 付き）。
// 根拠: spec.md US1 シナリオ 2・4、FR-003、data-model.md「TxWaitAnalysis」。
//   全体: 0, 0, 250, 300 -> 件数 4・最小 0・最大 300・平均 137.5・P95 = ceil(3.8)=4 番目 = 300
// ---------------------------------------------------------------------------
TEST(F029Spec, F029_US1_2_TxWaitTable_AllThenCanIdAscending)
{
    RecordProperty("spec", "029:US1-2");
    RecordProperty("spec", "029:US1-4");
    const TxWaitAnalysis a = AnalyzeTxWait(WaitRecords());
    EXPECT_EQ(1, a.OverwriteCount) << "破棄の件数（表の外）";
    ASSERT_EQ(3u, a.Rows.size()) << "全体 + 2 つの CAN ID";
    EXPECT_TRUE(a.Rows[0].All);
    EXPECT_EQ(4, a.Rows[0].Count);
    EXPECT_EQ(0, a.Rows[0].MinUs);
    EXPECT_EQ(300, a.Rows[0].MaxUs);
    EXPECT_DOUBLE_EQ(137.5, a.Rows[0].MeanUs);
    EXPECT_EQ(300, a.Rows[0].P95Us);
    EXPECT_EQ((std::vector<long long>{0, 0, 250, 300}), a.Rows[0].Values);

    EXPECT_FALSE(a.Rows[1].All);
    EXPECT_EQ(0x080u, a.Rows[1].CanId) << "CAN ID の小さい順";
    EXPECT_EQ(2, a.Rows[1].TxNode) << "送信 ECU";
    EXPECT_EQ(2, a.Rows[1].Count);
    EXPECT_DOUBLE_EQ(125.0, a.Rows[1].MeanUs);
    EXPECT_EQ(0x100u, a.Rows[2].CanId);
    EXPECT_EQ(1, a.Rows[2].TxNode);
    EXPECT_EQ(300, a.Rows[2].MaxUs);
}

// Edge Cases: 送信が 1 件も無いログでは、送信待ち時間は全体の行だけ（件数 0）、ジッターは 0 行。
// 根拠: spec.md Edge Cases（送信の行が 1 件も無いログ）、data-model.md（全体の行は送信が無くても必ずある）。
TEST(F029Spec, F029_US1_2_NoTransmissions_AllRowOnlyAndNoJitterRows)
{
    RecordProperty("spec", "029:US1-2");
    const std::vector<LogRecord> recs = {Rec(0x300, 3, 0, 5000, -1, LogStatus::Overwrite)};
    const TxWaitAnalysis w = AnalyzeTxWait(recs);
    ASSERT_EQ(1u, w.Rows.size());
    EXPECT_TRUE(w.Rows[0].All);
    EXPECT_EQ(0, w.Rows[0].Count);
    EXPECT_EQ(0, w.Rows[0].MaxUs);
    EXPECT_TRUE(AnalyzeJitter(recs).Rows.empty()) << "ジッターの表は 0 行";
}

// ---------------------------------------------------------------------------
// SC-001 / SC-002: 実際の実行のログで、全体の件数 = 送信の回数（重複と破棄を除く）、CAN ID ごとの件数の合計 = 全体、
//   どの行もヒストグラムの度数の合計 = 件数。
// 根拠: spec.md SC-001、SC-002、FR-004（ビンの規則は 028 と同じ）。
// ---------------------------------------------------------------------------
TEST(F029Spec, F029_SC001_RealLog_CountsMatchTransmissions)
{
    RecordProperty("spec", "029:SC-001");
    RecordProperty("spec", "029:SC-002");
    std::vector<FrameSpec> frames;
    for (unsigned i = 0; i < 18; ++i) frames.push_back({0x100 + i, 8, 20000, 0}); // 16 本を超えて破棄も起こす
    Network net;
    net.BitRateBps = 500000;
    net.Nodes.push_back(MakeNode(1, frames));
    net.Nodes.push_back(MakeNode(2, {{0x080, 8, 5000, 0}}));
    net.Nodes.push_back(MakeNode(3, {}));
    const RunResult run = RunSim(net, 200000LL);
    const std::size_t expected = UniqueTransmissions(run.Rows).size();

    const std::wstring path = TempPath("f029_sc001.log.csv");
    ASSERT_TRUE(WriteLogCsv(run.Rows, path));
    std::vector<LogRecord> recs;
    std::string error;
    const bool ok = ReadLogRecords(path, recs, error);
    DeleteFileW(path.c_str());
    ASSERT_TRUE(ok) << error;

    const TxWaitAnalysis a = AnalyzeTxWait(recs);
    EXPECT_GT(a.OverwriteCount, 0) << "前提: 破棄がある";
    EXPECT_EQ(static_cast<long long>(expected), a.Rows[0].Count) << "全体の件数 = 送信の回数";
    long long sum = 0;
    for (std::size_t i = 0; i < a.Rows.size(); ++i)
    {
        if (i > 0) sum += a.Rows[i].Count;
        long long hist = 0;
        for (long long c : MakeHistogram(a.Rows[i].Values).Counts) hist += c;
        EXPECT_EQ(a.Rows[i].Count, hist) << i << " 行目のヒストグラムの度数の合計";
    }
    EXPECT_EQ(a.Rows[0].Count, sum) << "CAN ID ごとの件数の合計 = 全体";
}

// ---------------------------------------------------------------------------
// US2-3 / FR-005 / SC-003: ずれ = (Start[k] − Start[k−1]) − (Request[k] − Request[k−1])。正は遅れ、負は早まり。
//   周期 = 送信要求の間隔の最頻値。最小・最大・母標準偏差。点は後の送信の開始時刻。
// 根拠: spec.md US2 シナリオ 3、FR-005、SC-003（手で計算できる小さな構成）、data-model.md「JitterRow」。
//   要求 0, 10000, 20000, 30000 / 開始 0, 10300, 20000, 30100 -> ずれ 300, −300, 100
//   平均 100/3、母分散 = ((800/3)² + (1000/3)² + (200/3)²) / 3 = 1680000/27 ≒ 62222.2、標準偏差 ≒ 249.444
// ---------------------------------------------------------------------------
TEST(F029Spec, F029_SC003_HandCalcJitter_DeviationsAndPopulationStdDev)
{
    RecordProperty("spec", "029:US2-3");
    RecordProperty("spec", "029:SC-003");
    const std::vector<LogRecord> recs = {
        Rec(0x100, 1, 2, 0, 0), Rec(0x100, 1, 2, 10000, 10300), Rec(0x100, 1, 2, 20000, 20000),
        Rec(0x100, 1, 2, 30000, 30100)};
    const JitterAnalysis j = AnalyzeJitter(recs);
    ASSERT_EQ(1u, j.Rows.size());
    const JitterRow& r = j.Rows[0];
    EXPECT_EQ(0x100u, r.CanId);
    EXPECT_EQ(1, r.TxNode);
    EXPECT_EQ(10000, r.PeriodUs) << "周期 = 送信要求の間隔の最頻値";
    EXPECT_EQ(3, r.Count) << "件数 = 送信の回数 − 1";
    EXPECT_EQ(-300, r.MinUs);
    EXPECT_EQ(300, r.MaxUs);
    EXPECT_NEAR(249.444, r.StdDevUs, 0.001) << "母標準偏差";
    ASSERT_EQ(3u, r.Points.size());
    EXPECT_EQ(10300, r.Points[0].TimeUs);
    EXPECT_EQ(300, r.Points[0].DeviationUs);
    EXPECT_EQ(20000, r.Points[1].TimeUs);
    EXPECT_EQ(-300, r.Points[1].DeviationUs);
    EXPECT_EQ(30100, r.Points[2].TimeUs);
    EXPECT_EQ(100, r.Points[2].DeviationUs);
}

// Edge Cases: 破棄で 1 回分の送信が抜けても、抜けた周期をずれとして数えない（要求の間隔を引く）。
//   周期の最頻値が同数なら小さい方。
// 根拠: spec.md Edge Cases（破棄で 1 回分の送信が抜けた場合）、FR-005、data-model.md（同数なら小さい方）。
//   要求 0, 10000, (20000 は破棄), 30000 / 開始 0, 10000, 30050 -> ずれ 0, 50。要求の間隔 10000・20000 が 1 回ずつ -> 周期 10000
TEST(F029Spec, F029_US2_3_DiscardedTransmission_GapNotCountedAsJitter)
{
    RecordProperty("spec", "029:US2-3");
    const std::vector<LogRecord> recs = {
        Rec(0x100, 1, 2, 0, 0), Rec(0x100, 1, 2, 10000, 10000), Rec(0x100, 1, 0, 20000, -1, LogStatus::Overwrite),
        Rec(0x100, 1, 2, 30000, 30050)};
    const JitterAnalysis j = AnalyzeJitter(recs);
    ASSERT_EQ(1u, j.Rows.size());
    EXPECT_EQ(10000, j.Rows[0].PeriodUs) << "最頻値が同数なら小さい方";
    EXPECT_EQ(2, j.Rows[0].Count);
    EXPECT_EQ(0, j.Rows[0].MinUs);
    EXPECT_EQ(50, j.Rows[0].MaxUs) << "抜けた周期（10000 us）はずれに入らない";
}

// US2-5: 送信が 1 回だけの CAN ID は、件数 0・周期 0・最小・最大・標準偏差 0（表では空欄）。
// 根拠: spec.md US2 シナリオ 5、data-model.md「JitterRow」。
TEST(F029Spec, F029_US2_5_SingleTransmission_CountZero)
{
    RecordProperty("spec", "029:US2-5");
    const JitterAnalysis j = AnalyzeJitter({Rec(0x200, 2, 1, 500, 520), Rec(0x100, 1, 2, 0, 0), Rec(0x100, 1, 2, 1000, 1000)});
    ASSERT_EQ(2u, j.Rows.size());
    EXPECT_EQ(0x100u, j.Rows[0].CanId) << "CAN ID の小さい順";
    const JitterRow& once = j.Rows[1];
    EXPECT_EQ(0x200u, once.CanId);
    EXPECT_EQ(0, once.Count);
    EXPECT_EQ(0, once.PeriodUs);
    EXPECT_EQ(0, once.MinUs);
    EXPECT_EQ(0, once.MaxUs);
    EXPECT_DOUBLE_EQ(0.0, once.StdDevUs);
    EXPECT_TRUE(once.Points.empty());
}

// ---------------------------------------------------------------------------
// SC-003: 破棄の無い構成では、ジッターの最小・最大・標準偏差が集計 CSV の CAN ID ごとの値と一致する。
// 根拠: spec.md SC-003、LogAnalysis.h（破棄が無ければ機能 001 FR-019 と同じ）。
// ---------------------------------------------------------------------------
TEST(F029Spec, F029_SC003_NoDiscards_MatchesSummaryJitter)
{
    RecordProperty("spec", "029:SC-003");
    Network net;
    net.BitRateBps = 500000;
    net.Nodes.push_back(MakeNode(1, {{0x100, 8, 10000, 0}, {0x101, 4, 7000, 300}}));
    net.Nodes.push_back(MakeNode(2, {{0x080, 8, 5000, 0}, {0x081, 8, 3000, 0}}));
    net.Nodes.push_back(MakeNode(3, {{0x200, 2, 2000, 100}}));
    const RunResult run = RunSim(net, 500000LL);
    const std::vector<GroupMetrics> metrics = ComputeMetrics(run.Rows, net);

    const std::wstring path = TempPath("f029_sc003.log.csv");
    ASSERT_TRUE(WriteLogCsv(run.Rows, path));
    std::vector<LogRecord> recs;
    std::string error;
    const bool ok = ReadLogRecords(path, recs, error);
    DeleteFileW(path.c_str());
    ASSERT_TRUE(ok) << error;
    const JitterAnalysis j = AnalyzeJitter(recs);
    ASSERT_EQ(5u, j.Rows.size());
    long long nonZero = 0;
    for (const JitterRow& r : j.Rows)
    {
        char key[8];
        snprintf(key, sizeof(key), "0x%03X", r.CanId);
        const GroupMetrics* g = FindGroup(metrics, "ID", key);
        ASSERT_NE(nullptr, g) << key << " の集計が無い";
        ASSERT_TRUE(g->HasJitter);
        EXPECT_NEAR(g->JitterMaxUs, static_cast<double>(r.MaxUs), 1e-6) << key << " のジッター最大";
        EXPECT_NEAR(g->JitterMinUs, static_cast<double>(r.MinUs), 1e-6) << key << " のジッター最小";
        EXPECT_NEAR(g->JitterStdDevUs, r.StdDevUs, 1e-6) << key << " のジッター標準偏差";
        if (r.MaxUs != 0 || r.MinUs != 0) ++nonZero;
    }
    EXPECT_GT(nonZero, 0) << "前提: ずれのある CAN ID がある";
}

// ---------------------------------------------------------------------------
// US2-4 / FR-006: ずれのグラフの間引き。時刻 [t0, t1] を列に分け、列ごとに最小と最大の両方を残す（山と谷を見落とさない）。
// 根拠: spec.md US2 シナリオ 4、data-model.md「MinMaxPerColumn」（列 = (TimeUs − t0) × columns / (t1 − t0 + 1)、columns <= 0 は空）。
//   時刻 0〜9 のずれ 0, 1, −2, 3, −4, 5, −6, 7, −8, 9 を 5 列に -> 列 c は時刻 2c・2c+1
// ---------------------------------------------------------------------------
TEST(F029Spec, F029_US2_4_MinMaxPerColumn_KeepsPeaksAndDips)
{
    RecordProperty("spec", "029:US2-4");
    std::vector<JitterPoint> pts;
    const long long dev[10] = {0, 1, -2, 3, -4, 5, -6, 7, -8, 9};
    for (int t = 0; t < 10; ++t) pts.push_back({t, dev[t]});
    const std::vector<ColumnRange> cols = MinMaxPerColumn(pts, 0, 9, 5);
    ASSERT_EQ(5u, cols.size());
    const long long mins[5] = {0, -2, -4, -6, -8};
    const long long maxs[5] = {1, 3, 5, 7, 9};
    for (int c = 0; c < 5; ++c)
    {
        EXPECT_TRUE(cols[c].Has) << c << " 列目";
        EXPECT_EQ(mins[c], cols[c].MinUs) << c << " 列目の最小";
        EXPECT_EQ(maxs[c], cols[c].MaxUs) << c << " 列目の最大";
    }

    const std::vector<ColumnRange> sparse = MinMaxPerColumn({{5, 10}, {95, -10}}, 0, 99, 10);
    ASSERT_EQ(10u, sparse.size());
    EXPECT_TRUE(sparse[0].Has);
    EXPECT_TRUE(sparse[9].Has);
    for (int c = 1; c <= 8; ++c) EXPECT_FALSE(sparse[c].Has) << c << " 列目には点が無い";
    EXPECT_TRUE(MinMaxPerColumn(pts, 0, 9, 0).empty()) << "columns <= 0 は空";
}

// ---------------------------------------------------------------------------
// 改訂 1: ReadLogRecords の progress は最後に（読んだバイト数 = 全体）で呼ばれる。false を返すと読むのをやめ、
//   records は空、error は kLogReadCancelled。
// 根拠: LogAnalysis.h の ReadLogRecords のコメント（029 revision 1）。
// ---------------------------------------------------------------------------
TEST(F029Spec, F029_REV1_ReadProgress_CalledAtEndAndCancellable)
{
    RecordProperty("spec", "029:REV1");
    Network net;
    net.BitRateBps = 500000;
    net.Nodes.push_back(MakeNode(1, {{0x100, 8, 10000, 0}}));
    net.Nodes.push_back(MakeNode(2, {}));
    const std::wstring path = TempPath("f029_rev1.log.csv");
    ASSERT_TRUE(WriteLogCsv(RunSim(net, 100000LL).Rows, path));

    std::vector<std::pair<long long, long long>> calls;
    std::vector<LogRecord> recs;
    std::string error;
    EXPECT_TRUE(ReadLogRecords(path, recs, error, [&](long long read, long long total) {
        calls.push_back({read, total});
        return true;
    })) << error;
    EXPECT_EQ(10u, recs.size()) << "10 回送信 × 受信 1 ECU";
    ASSERT_FALSE(calls.empty()) << "progress が呼ばれない";
    EXPECT_EQ(calls.back().second, calls.back().first) << "最後の呼び出しは 読んだバイト数 = 全体";
    EXPECT_GT(calls.back().second, 0);

    recs.clear();
    error.clear();
    EXPECT_FALSE(ReadLogRecords(path, recs, error, [](long long, long long) { return false; }))
        << "中断したのに成功が返った";
    DeleteFileW(path.c_str());
    EXPECT_TRUE(recs.empty()) << "中断したら records は空";
    EXPECT_EQ(std::string(kLogReadCancelled), error) << "中断の error";
}

// ---------------------------------------------------------------------------
// data-model.md「CollectTransmissions」: 対象は Overwrite 以外で送信要求・送信開始の時刻がある行。
//   並びは (CAN ID, 送信開始) の昇順（ログの行の順によらない）。
// ---------------------------------------------------------------------------
TEST(F029Spec, F029_US1_4_CollectTransmissions_SkipsMissingTimesAndSorts)
{
    RecordProperty("spec", "029:US1-4");
    long long overwrite = 0;
    const std::vector<Transmission> tx = CollectTransmissions(
        {Rec(0x100, 1, 2, 20000, 20000), Rec(0x100, 1, 2, 0, 0), Rec(0x100, 1, 2, 10000, 10050),
         Rec(0x080, 2, 1, -1, 500), Rec(0x080, 2, 1, 600, -1), Rec(0x080, 2, 1, 700, 750)},
        overwrite);
    EXPECT_EQ(0, overwrite);
    ASSERT_EQ(4u, tx.size()) << "時刻の無い 2 行は対象外";
    EXPECT_EQ(0x080u, tx[0].CanId);
    EXPECT_EQ(750, tx[0].TxStartUs);
    EXPECT_EQ(0, tx[1].TxStartUs) << "0x100 は送信開始の昇順に並べ直す";
    EXPECT_EQ(10050, tx[2].TxStartUs);
    EXPECT_EQ(20000, tx[3].TxStartUs);
}

// ---------------------------------------------------------------------------
// 改訂 1: progress は 4 MB ごとと最後に呼ばれ、途中で false を返すと読むのをやめる（records は空、kLogReadCancelled）。
//   4 MB の切れ目をまたぐ行も正しく読む。
// 根拠: LogAnalysis.h の ReadLogRecords のコメント（called after every 4 MB and at the end）。
// ---------------------------------------------------------------------------
TEST(F029Spec, F029_REV1_LargeLog_ProgressEvery4MBAndCancelMidway)
{
    RecordProperty("spec", "029:REV1");
    std::string text =
        "Timestamp_us,CAN_ID,DLC,Tx_Node,Rx_Node,Tx_Request,Tx_Start,Tx_End,Rx_Start,Rx_End,Rx_Process,"
        "Bus_Load_Pct,Status,Retry_Count,Update_Count\r\n";
    const std::string row = "1000300,0x080,8,3,1,1000000,1000010,1000270,1000010,1000270,1000300,12.5,SUCCESS,0,0\r\n";
    long long rows = 0;
    while (text.size() < 5u * 1024 * 1024 + 100)
    {
        text += row;
        ++rows;
    }
    const std::wstring path = TempPath("f029_rev1_large.log.csv");
    WriteAll(path, text);

    std::vector<std::pair<long long, long long>> calls;
    std::vector<LogRecord> recs;
    std::string error;
    EXPECT_TRUE(ReadLogRecords(path, recs, error, [&](long long read, long long total) {
        calls.push_back({read, total});
        return true;
    })) << error;
    EXPECT_GE(calls.size(), 2u) << "4 MB ごとと最後に呼ばれる";
    EXPECT_EQ(static_cast<long long>(text.size()), calls.back().first) << "最後は全体";
    EXPECT_EQ(rows, static_cast<long long>(recs.size())) << "すべての行を読む（4 MB の切れ目をまたぐ行も）";
    for (const LogRecord& r : recs) ASSERT_EQ(0x080u, r.CanId) << "切れ目で行が壊れた";

    int n = 0;
    recs.clear();
    error.clear();
    EXPECT_FALSE(ReadLogRecords(path, recs, error, [&](long long, long long) { return ++n > 1; }))
        << "途中で中断したのに成功が返った";
    EXPECT_TRUE(recs.empty());
    EXPECT_EQ(std::string(kLogReadCancelled), error);
    DeleteFileW(path.c_str());
}
