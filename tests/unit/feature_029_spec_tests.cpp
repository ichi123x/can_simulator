// feature_029_spec_tests.cpp（tests/unit）
// 機能 029（specs/029-tx-wait-jitter-analysis/spec.md）の受け入れシナリオと成功基準のテスト。
// ウィンドウ・メニューの表示は画面の確認なので、ここでは計算（CanSim::LogAnalysis）を確かめる。
// ログは実際の実行の結果をログの CSV に書き、ReadLogRecords で読み直したものを使う（FR-002）。

#include "support/spec_test_helpers.h"

#include "LogAnalysis.h"
#include "MetricsAggregator.h"

#include <cmath>
#include <numeric>

using namespace CanSim;
using namespace spectest;

namespace
{
    std::vector<LogRecord> ToRecords(const std::vector<LogRow>& rows, const std::wstring& name)
    {
        const std::wstring path = TempPath(name);
        EXPECT_TRUE(WriteLogCsv(rows, path)) << "ログの CSV を書けない";
        std::vector<LogRecord> records;
        std::string error;
        EXPECT_TRUE(ReadLogRecords(path, records, error)) << "ログを読めない: " << error;
        return records;
    }

    // 手で計算できる構成（500 kbps、100 ms）: ノード 1 が 0x080、ノード 2 が 0x100 を、どちらも初回オフセット 0・周期 10 ms で
    // 送る（毎回同時に要求）。ノード 3 は受信だけ。0x080 は待ち 0（22 µs に要求、22 µs に開始）、0x100 は調停に負けて
    // 274 µs に開始するので待ち 252 µs。受信 ECU が 2 つずつあるので、ログの行は送信 1 回につき 2 行。
    Network PairNetwork()
    {
        Network net;
        net.BitRateBps = 500000;
        net.Nodes.push_back(MakeNode(1, { MakeFrame(0x080, 8, 10000) }));
        net.Nodes.push_back(MakeNode(2, { MakeFrame(0x100, 8, 10000) }));
        net.Nodes.push_back(MakeNode(3));
        return net;
    }

    // ずれが手で計算できる構成（500 kbps、100 ms）: ノード 1 の 0x100 は周期 10 ms、ノード 2 の 0x080 は周期 20 ms（どちらも
    // 初回オフセット 0）。0x080 と重なる回（0・20・40… ms）だけ 0x100 は 252 µs 待つ。0x100 の送信開始は
    // 274、10,022、20,274、30,022 … µs で、ずれ（開始の間隔 − 要求の間隔 10,000）は −252、+252、−252 … と交互になる。
    Network AlternatingNetwork()
    {
        Network net;
        net.BitRateBps = 500000;
        net.Nodes.push_back(MakeNode(1, { MakeFrame(0x100, 8, 10000) }));
        net.Nodes.push_back(MakeNode(2, { MakeFrame(0x080, 8, 20000) }));
        net.Nodes.push_back(MakeNode(3));
        return net;
    }

    const TxWaitRow* WaitRowFor(const TxWaitAnalysis& a, unsigned canId)
    {
        for (const TxWaitRow& r : a.Rows)
            if (!r.All && r.CanId == canId)
                return &r;
        return nullptr;
    }

    const JitterRow* JitterRowFor(const JitterAnalysis& a, unsigned canId)
    {
        for (const JitterRow& r : a.Rows)
            if (r.CanId == canId)
                return &r;
        return nullptr;
    }
}

// 根拠: spec.md US1 受け入れシナリオ 2、FR-003。表は先頭が全体、続いて CAN ID の小さい順の行で、各行に CAN ID・送信 ECU・
// 件数・最小・平均・最大・95 パーセンタイル（最近順位法、LogAnalysis.h）。手計算: 0x080（ECU 1）は 0 µs × 10 件、
// 0x100（ECU 2）は 252 µs × 10 件。全体は 20 件、最小 0、最大 252、平均 126、95 パーセンタイルは ceil(0.95 × 20) = 19 番目 = 252。
TEST(F029Spec, F029_US1_2_TxWaitTable_AllThenByCanId)
{
    RecordProperty("spec", "029:US1-2");
    const TxWaitAnalysis a = AnalyzeTxWait(ToRecords(RunSim(PairNetwork(), 100000LL), L"f029_us1_2.log.csv"));
    ASSERT_EQ(a.Rows.size(), 3u) << "全体 + CAN ID 2 つになっていない";
    const TxWaitRow& all = a.Rows[0];
    EXPECT_TRUE(all.All) << "先頭が全体の行でない";
    EXPECT_EQ(all.Count, 20) << "全体の件数";
    EXPECT_EQ(all.MinUs, 0) << "全体の最小";
    EXPECT_EQ(all.MaxUs, 252) << "全体の最大";
    EXPECT_DOUBLE_EQ(all.MeanUs, 126.0) << "全体の平均";
    EXPECT_EQ(all.P95Us, 252) << "全体の 95 パーセンタイル";

    EXPECT_EQ(a.Rows[1].CanId, 0x080u) << "2 行目が CAN ID の小さい 0x080 でない";
    EXPECT_EQ(a.Rows[1].TxNode, 1) << "0x080 の送信 ECU";
    EXPECT_EQ(a.Rows[1].Count, 10) << "0x080 の件数";
    EXPECT_EQ(a.Rows[1].MaxUs, 0) << "0x080 の最大";
    EXPECT_EQ(a.Rows[2].CanId, 0x100u) << "3 行目が 0x100 でない";
    EXPECT_EQ(a.Rows[2].TxNode, 2) << "0x100 の送信 ECU";
    EXPECT_EQ(a.Rows[2].Count, 10) << "0x100 の件数";
    EXPECT_EQ(a.Rows[2].MinUs, 252) << "0x100 の最小";
    EXPECT_EQ(a.Rows[2].MaxUs, 252) << "0x100 の最大";
    EXPECT_DOUBLE_EQ(a.Rows[2].MeanUs, 252.0) << "0x100 の平均";
    EXPECT_EQ(a.Rows[2].P95Us, 252) << "0x100 の 95 パーセンタイル";
}

// 根拠: spec.md US1 受け入れシナリオ 4。送信 1 回ごとに、送信要求から送信開始までを 1 件とする。受信 ECU の数だけログの行が
// あっても同じ送信は 1 件。調停負けで再送されたフレームは、最終的に送れた 1 回を 1 件とし、時間は再送までの待ちを含む。
TEST(F029Spec, F029_US1_4_OneValuePerTransmission_IncludingRetryWait)
{
    RecordProperty("spec", "029:US1-4");
    const std::vector<LogRow> rows = RunSim(PairNetwork(), 100000LL);
    ASSERT_EQ(rows.size(), 40u) << "前提: 20 回の送信 × 受信 2 ECU = 40 行でない";
    const TxWaitAnalysis a = AnalyzeTxWait(ToRecords(rows, L"f029_us1_4.log.csv"));
    const TxWaitRow* r100 = WaitRowFor(a, 0x100);
    ASSERT_NE(r100, nullptr) << "0x100 の行が無い";
    EXPECT_EQ(r100->Count, 10) << "受信 ECU の数だけ重複して数えている";
    for (long long v : r100->Values)
        EXPECT_EQ(v, 252) << "再送までの待ちを含む 送信要求 → 送信開始 の時間でない";
}

// 根拠: spec.md US1 受け入れシナリオ 5、FR-004（ビンの規則は 028 と同じ: 幅は 1・2・5 × 10^m、LogAnalysis.h の MakeHistogram）。
// 軸の目盛りと値の表示は画面の確認なので、ここでは幅と範囲（すべての値がどれかのビンに入る）を確かめる。
TEST(F029Spec, F029_US1_5_Histogram_SameRuleAs028)
{
    RecordProperty("spec", "029:US1-5");
    const TxWaitAnalysis a = AnalyzeTxWait(ToRecords(RunSim(PairNetwork(), 100000LL), L"f029_us1_5.log.csv"));
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

// 根拠: spec.md US2 受け入れシナリオ 2、FR-005。表は CAN ID の小さい順で、各行に CAN ID・送信 ECU・周期・件数・最小・最大・
// 標準偏差。周期どおりで待ちの変わらない構成（どちらも毎回同じ待ち）では、ずれはすべて 0。件数は送信回数 − 1 = 9。
TEST(F029Spec, F029_US2_2_JitterTable_ByCanIdWithPeriod)
{
    RecordProperty("spec", "029:US2-2");
    const JitterAnalysis a = AnalyzeJitter(ToRecords(RunSim(PairNetwork(), 100000LL), L"f029_us2_2.log.csv"));
    ASSERT_EQ(a.Rows.size(), 2u) << "CAN ID 2 つの行になっていない";
    EXPECT_EQ(a.Rows[0].CanId, 0x080u) << "1 行目が CAN ID の小さい 0x080 でない";
    EXPECT_EQ(a.Rows[1].CanId, 0x100u) << "2 行目が 0x100 でない";
    for (int i = 0; i < 2; ++i)
    {
        const JitterRow& r = a.Rows[i];
        EXPECT_EQ(r.TxNode, i + 1) << "送信 ECU";
        EXPECT_EQ(r.PeriodUs, 10000) << "周期";
        EXPECT_EQ(r.Count, 9) << "件数（送信回数 − 1）";
        EXPECT_EQ(r.MinUs, 0) << "最小";
        EXPECT_EQ(r.MaxUs, 0) << "最大";
        EXPECT_DOUBLE_EQ(r.StdDevUs, 0.0) << "標準偏差";
    }
}

// 根拠: spec.md US2 受け入れシナリオ 3、FR-005（機能 001 FR-019 と同じ定義。正は遅れ、負は早まり）。手計算: 0x100 のずれは
// −252、+252 が交互で 9 件（−252 が 5 件、+252 が 4 件）。最小 −252、最大 +252。標準偏差（母標準偏差、LogAnalysis.h）は、
// 平均 −28、2 乗の平均 63,504 なので √(63,504 − 784) = √62,720。最初のずれの時刻は 2 回目の送信開始 10,022 µs。
TEST(F029Spec, F029_US2_3_Deviation_StartIntervalMinusPeriod)
{
    RecordProperty("spec", "029:US2-3");
    const JitterAnalysis a = AnalyzeJitter(ToRecords(RunSim(AlternatingNetwork(), 100000LL), L"f029_us2_3.log.csv"));
    const JitterRow* r = JitterRowFor(a, 0x100);
    ASSERT_NE(r, nullptr) << "0x100 の行が無い";
    EXPECT_EQ(r->Count, 9) << "件数";
    EXPECT_EQ(r->MinUs, -252) << "最小（早まり）";
    EXPECT_EQ(r->MaxUs, 252) << "最大（遅れ）";
    EXPECT_NEAR(r->StdDevUs, std::sqrt(62720.0), 1e-6) << "標準偏差";
    ASSERT_EQ(r->Points.size(), 9u) << "ずれの点の数";
    EXPECT_EQ(r->Points[0].TimeUs, 10022) << "最初のずれの時刻";
    EXPECT_EQ(r->Points[0].DeviationUs, -252) << "最初のずれ";
    EXPECT_EQ(r->Points[1].DeviationUs, 252) << "2 番目のずれ";
}

// 根拠: spec.md US2 受け入れシナリオ 5。送信が 1 回しか無い CAN ID は、件数 0 として行を表示する（最小・最大・標準偏差は空欄。
// 空欄の表示は画面の確認。周期は 0、LogAnalysis.h）。周期 1 s のフレームを 0.5 s 実行すると送信は 1 回。
TEST(F029Spec, F029_US2_5_SentOnce_RowWithZeroCount)
{
    RecordProperty("spec", "029:US2-5");
    Network net;
    net.BitRateBps = 500000;
    net.Nodes.push_back(MakeNode(1, { MakeFrame(0x123, 8, 1000000) }));
    net.Nodes.push_back(MakeNode(2));
    const JitterAnalysis a = AnalyzeJitter(ToRecords(RunSim(net, 500000LL), L"f029_us2_5.log.csv"));
    const JitterRow* r = JitterRowFor(a, 0x123);
    ASSERT_NE(r, nullptr) << "送信が 1 回の CAN ID の行が無い";
    EXPECT_EQ(r->Count, 0) << "件数が 0 でない";
    EXPECT_EQ(r->PeriodUs, 0) << "周期が 0 でない";
    EXPECT_TRUE(r->Points.empty()) << "ずれの点がある";
}

namespace
{
    // 送信バッファの満杯（破棄）を含む構成: ノード 1 が同時にそろう 18 フレームを周期 20 ms で送り、ノード 2・3 が受信する
    Network DiscardNetwork()
    {
        Network net;
        net.BitRateBps = 500000;
        std::vector<FrameDefinition> frames;
        for (unsigned i = 0; i < 18; ++i)
            frames.push_back(MakeFrame(0x100 + i, 8, 20000));
        net.Nodes.push_back(MakeNode(1, frames));
        net.Nodes.push_back(MakeNode(2, { MakeFrame(0x080, 4, 5000) }));
        net.Nodes.push_back(MakeNode(3));
        return net;
    }
}

// 根拠: spec.md SC-001、Edge Cases（破棄された送信は含めない）。送信待ち時間の全体の件数が、フレームログの送信の回数
// （受信 ECU の数だけの重複を除き、破棄を除いた数）と一致する。
TEST(F029Spec, F029_SC001_AllCount_EqualsDistinctTransmissions)
{
    RecordProperty("spec", "029:SC-001");
    const std::vector<LogRow> rows = RunSim(DiscardNetwork(), 100000LL);
    std::set<std::pair<int, std::pair<unsigned, long long>>> sent; // (送信 ECU, (CAN ID, 送信開始))
    long long overwrite = 0;
    for (const LogRow& r : rows)
    {
        if (r.Status == kStatusOverwrite)
            ++overwrite;
        else
            sent.insert({ r.TxNodeId, { r.CanId, r.TxStartUs } });
    }
    ASSERT_GT(overwrite, 0) << "前提: 破棄が無い";
    const TxWaitAnalysis a = AnalyzeTxWait(ToRecords(rows, L"f029_sc001.log.csv"));
    EXPECT_EQ(a.Rows[0].Count, static_cast<long long>(sent.size())) << "全体の件数が送信の回数と違う";
    EXPECT_EQ(a.OverwriteCount, overwrite) << "表の外の破棄の件数";
}

// 根拠: spec.md SC-002。送信待ち時間のどの行でも、ヒストグラムの度数の合計がその行の件数と一致し、CAN ID ごとの件数の合計が
// 全体の件数と一致する。
TEST(F029Spec, F029_SC002_HistogramSums_AndIdCountsAddUp)
{
    RecordProperty("spec", "029:SC-002");
    const TxWaitAnalysis a = AnalyzeTxWait(ToRecords(RunSim(DiscardNetwork(), 100000LL), L"f029_sc002.log.csv"));
    long long idSum = 0;
    for (const TxWaitRow& r : a.Rows)
    {
        const Histogram h = MakeHistogram(r.Values);
        const long long sum = std::accumulate(h.Counts.begin(), h.Counts.end(), 0LL);
        EXPECT_EQ(sum, r.Count) << (r.All ? std::string("全体") : "CAN ID " + std::to_string(r.CanId))
                                << " の度数の合計が件数と違う";
        if (!r.All)
            idSum += r.Count;
    }
    EXPECT_EQ(idSum, a.Rows[0].Count) << "CAN ID ごとの件数の合計が全体の件数と違う";
}

// 根拠: spec.md SC-003。破棄の無い構成で、ジッターの最小・最大・標準偏差が、集計 CSV（MetricsAggregator）の CAN ID ごとの
// 値と一致する。手計算との一致は US2-3 で確かめている。
TEST(F029Spec, F029_SC003_NoDiscard_JitterEqualsSummary)
{
    RecordProperty("spec", "029:SC-003");
    const Network net = AlternatingNetwork();
    const std::vector<LogRow> rows = RunSim(net, 100000LL);
    const JitterAnalysis a = AnalyzeJitter(ToRecords(rows, L"f029_sc003.log.csv"));
    const std::vector<GroupMetrics> metrics = ComputeMetrics(rows, net);
    int compared = 0;
    for (const GroupMetrics& g : metrics)
    {
        if (g.GroupType != "ID" || !g.HasJitter)
            continue;
        const unsigned canId = static_cast<unsigned>(std::stoul(g.GroupKey, nullptr, 16));
        const JitterRow* r = JitterRowFor(a, canId);
        ASSERT_NE(r, nullptr) << g.GroupKey << " のジッターの行が無い";
        EXPECT_DOUBLE_EQ(static_cast<double>(r->MinUs), g.JitterMinUs) << g.GroupKey << " の最小";
        EXPECT_DOUBLE_EQ(static_cast<double>(r->MaxUs), g.JitterMaxUs) << g.GroupKey << " の最大";
        EXPECT_NEAR(r->StdDevUs, g.JitterStdDevUs, 1e-6) << g.GroupKey << " の標準偏差";
        ++compared;
    }
    EXPECT_EQ(compared, 2) << "前提: 比べた CAN ID が 2 つでない";
}
