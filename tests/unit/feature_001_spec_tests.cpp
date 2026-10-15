// feature_001_spec_tests.cpp（tests/unit）
// 機能 001（specs/001-can-simulation-core/spec.md）の受け入れシナリオと成功基準のテスト。
// 期待値は spec.md・contracts/・関連する仕様（機能 031 の手計算の値など）から書く。

// _wgetenv・_wfopen の非推奨の警告（C4996）を出さない
#define _CRT_SECURE_NO_WARNINGS

#include <gtest/gtest.h>

#include "ConfigLoader.h"
#include "MetricsAggregator.h"
#include "SimulationEngine.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <vector>

using namespace CanSim;

namespace
{
    // 一時ファイルのパス（%TEMP%\cansim_f001_<name>）
    std::wstring TempPath(const std::wstring& name)
    {
        const wchar_t* temp = _wgetenv(L"TEMP");
        std::wstring dir = temp ? temp : L".";
        return dir + L"\\cansim_f001_" + name;
    }

    // テキストをそのまま（バイト列として）書く
    void WriteText(const std::wstring& path, const std::string& text)
    {
        FILE* f = _wfopen(path.c_str(), L"wb");
        ASSERT_NE(f, nullptr) << "一時ファイルを作れない";
        fwrite(text.data(), 1, text.size(), f);
        fclose(f);
    }

    ConfigLoadResult LoadText(const std::wstring& name, const std::string& text)
    {
        const std::wstring path = TempPath(name);
        WriteText(path, text);
        return LoadConfig(path);
    }

    // 設定ファイルの [NodeN] と、フレームを 1 つ持つ [NodeN.Frame1] を作る（CanId は 16 進）
    std::string NodeWithFrame(int nodeId, unsigned canId)
    {
        char id[16];
        std::snprintf(id, sizeof(id), "0x%03X", canId);
        const std::string n = std::to_string(nodeId);
        return "[Node" + n + "]\r\nNodeId=" + n + "\r\n" +
               "[Node" + n + ".Frame1]\r\nName=F" + n + "\r\nCanId=" + id +
               "\r\nDlc=8\r\nPeriodUs=10000\r\n";
    }

    FrameDefinition MakeFrame(unsigned canId, unsigned dlc, long long periodUs, long long offsetUs = 0)
    {
        FrameDefinition f;
        f.Name = "F";
        f.CanId = canId;
        f.Dlc = dlc;
        f.Data.assign(dlc, 0xFF); // データは DLC 分の 0xFF（機能 006 FR-014）
        f.PeriodUs = periodUs;
        f.InitialOffsetUs = offsetUs;
        return f;
    }

    Node MakeNode(int nodeId, const std::vector<FrameDefinition>& frames)
    {
        Node n;
        n.NodeId = nodeId;
        n.Name = "ECU" + std::to_string(nodeId);
        n.Frames = frames;
        return n;
    }

    std::vector<LogRow> RunSim(const Network& network, long long durationUs)
    {
        SimulationEngine engine(network, durationUs);
        Logger logger;
        engine.Run(logger);
        return logger.Rows();
    }

    // 送信 1 回ごとの代表の行（受信ノードの数だけ重複する行を、送信ノード・CAN ID・送信開始でまとめる）
    std::vector<LogRow> Transmissions(const std::vector<LogRow>& rows, unsigned canId)
    {
        std::vector<LogRow> out;
        std::set<long long> seenStarts;
        for (const LogRow& r : rows)
        {
            if (r.CanId != canId || r.Status == kStatusOverwrite)
                continue;
            if (seenStarts.insert(r.TxStartUs).second)
                out.push_back(r);
        }
        return out;
    }

    const LogRow* FirstRowOf(const std::vector<LogRow>& rows, unsigned canId)
    {
        for (const LogRow& r : rows)
            if (r.CanId == canId)
                return &r;
        return nullptr;
    }
}

// 根拠: spec.md US1 受け入れシナリオ 1、FR-017（1 フレーム × 受信ノードごとに 1 行）。
// 6 ノード・500 kbps、各ノードに送信フレームを 1 つ以上設定して 10 秒実行すると、各フレームは設定した周期どおりに
// 送信要求され、送信・受信の記録がログに残る。受信フィルタは未指定なので、送信元以外の 5 ノードが受信する（FR-005・FR-015）。
TEST(F001Spec, F001_US1_1_SixNodes10Sec_EachFrameAtPeriodWithRxRows)
{
    RecordProperty("spec", "001:US1-1");
    Network net;
    net.BitRateBps = 500000;
    const unsigned ids[] = { 0x080, 0x100, 0x180, 0x200, 0x280, 0x300 };
    const long long periods[] = { 10000, 20000, 50000, 100000, 200000, 500000 };
    for (int i = 0; i < 6; ++i)
        net.Nodes.push_back(MakeNode(i + 1, { MakeFrame(ids[i], 8, periods[i]) }));

    const std::vector<LogRow> rows = RunSim(net, 10000000LL);

    for (int i = 0; i < 6; ++i)
    {
        const std::vector<LogRow> tx = Transmissions(rows, ids[i]);
        ASSERT_GE(tx.size(), 2u) << "CAN ID " << ids[i] << " の送信が 2 回未満";
        for (std::size_t k = 1; k < tx.size(); ++k)
        {
            EXPECT_EQ(tx[k].TxRequestUs - tx[k - 1].TxRequestUs, periods[i])
                << "CAN ID " << ids[i] << " の " << k << " 回目の送信要求の間隔が周期と違う";
        }
        // 送信 1 回ごとに、送信元以外の 5 ノード分の行がある
        for (const LogRow& t : tx)
        {
            std::set<int> receivers;
            for (const LogRow& r : rows)
                if (r.CanId == ids[i] && r.TxStartUs == t.TxStartUs && r.RxNodeId > 0)
                    receivers.insert(r.RxNodeId);
            EXPECT_EQ(receivers.size(), 5u)
                << "CAN ID " << ids[i] << " の送信（開始 " << t.TxStartUs << " us）の受信ノードの行が 5 つでない";
            EXPECT_EQ(receivers.count(i + 1), 0u) << "送信元が自分の送信を受信している";
        }
    }
}

// 根拠: spec.md US1 受け入れシナリオ 2、FR-001・FR-004、contracts/config-input-format.md「エラー時の振る舞い」。
// 17 ノードの構成、ノードが 0 個の構成、同じ ID を 2 つのノードが送信する構成は、読み込みで拒否され、理由が示される。
struct RejectCase
{
    const char* Name;
    std::string Text;
};

class F001RejectTable : public ::testing::TestWithParam<RejectCase>
{
};

TEST_P(F001RejectTable, F001_US1_2_InvalidConfig_RejectedWithReason)
{
    RecordProperty("spec", "001:US1-2");
    const RejectCase& c = GetParam();
    const std::wstring file(c.Name, c.Name + std::strlen(c.Name));
    const ConfigLoadResult r = LoadText(file + L".canprj", c.Text);
    EXPECT_FALSE(r.Success) << c.Name << ": 拒否されるべき構成が読み込めてしまった";
    EXPECT_FALSE(r.ErrorMessage.empty()) << c.Name << ": 拒否の理由が示されていない";
}

namespace
{
    std::string SeventeenNodes()
    {
        std::string text = "[Network]\r\nBitRateBps=500000\r\n";
        for (int i = 1; i <= 17; ++i)
            text += NodeWithFrame(i, 0x100 + i);
        return text;
    }
}

INSTANTIATE_TEST_SUITE_P(
    US1Reject, F001RejectTable,
    ::testing::Values(
        RejectCase{ "seventeen_nodes", SeventeenNodes() },
        RejectCase{ "zero_nodes", "[Network]\r\nBitRateBps=500000\r\n" },
        RejectCase{ "duplicate_id",
                    "[Network]\r\nBitRateBps=500000\r\n" + NodeWithFrame(1, 0x123) + NodeWithFrame(2, 0x123) }),
    [](const ::testing::TestParamInfo<RejectCase>& info) { return std::string(info.param.Name); });

// 根拠: spec.md US2 受け入れシナリオ 1、FR-010・FR-012・FR-013、Edge Cases（バスがアイドルになった時点で調停に参加する）。
// ID 0x080 と 0x100 が同一時刻に送信要求すると、0x080 が先に送信され、0x100 は 0x080 の送信完了と 3 ビットの
// フレーム間休止期間のあとに送信される（500 kbps の 3 ビットは 6 µs）。0x100 のログには調停負けと再送信回数 1 が残る
// （Status は ARBITRATION_LOST_RETRY、contracts/log-output-format.md）。
TEST(F001Spec, F001_US2_1_SimultaneousRequests_LowerIdFirstAndRetryLogged)
{
    RecordProperty("spec", "001:US2-1");
    Network net;
    net.BitRateBps = 500000;
    net.Nodes.push_back(MakeNode(1, { MakeFrame(0x100, 8, 1000000) }));
    net.Nodes.push_back(MakeNode(2, { MakeFrame(0x080, 8, 1000000) }));

    const std::vector<LogRow> rows = RunSim(net, 100000LL);
    const LogRow* r080 = FirstRowOf(rows, 0x080);
    const LogRow* r100 = FirstRowOf(rows, 0x100);
    ASSERT_NE(r080, nullptr) << "0x080 の行が無い";
    ASSERT_NE(r100, nullptr) << "0x100 の行が無い";

    EXPECT_EQ(r080->TxRequestUs, r100->TxRequestUs) << "前提: 2 つの送信要求が同じ時刻でない";
    EXPECT_LT(r080->TxStartUs, r100->TxStartUs) << "ID の小さい 0x080 が先に送信されていない";
    EXPECT_EQ(r100->TxStartUs, r080->TxEndUs + 6) << "0x100 が 0x080 の送信完了 + 休止期間（6 µs）の直後に送信されていない";
    EXPECT_EQ(r100->Status, "ARBITRATION_LOST_RETRY") << "0x100 のログに調停負けが記録されていない";
    EXPECT_EQ(r100->RetryCount, 1) << "0x100 の再送信回数が 1 でない";
    EXPECT_EQ(r080->Status, "SUCCESS") << "0x080 のログが成功になっていない";
    EXPECT_EQ(r080->RetryCount, 0) << "0x080 の再送信回数が 0 でない";
}

// 根拠: spec.md US3 受け入れシナリオ 1、FR-018〜FR-020。
// 集計で、ID ごと・ノードごと・全体の 総送信レイテンシ／送信待ち遅延／フレーム伝送時間／バス負荷率／ジッター
// （最大・最小・標準偏差）が算出される。期待値: 送信 1 ノード・受信 1 ノードで、送信がほかと重ならない構成では
// 送信待ち遅延 0、伝送時間は 0x080・DLC 8 の 246 µs（機能 031 spec.md US1・SC-001 の手計算の値、500 kbps）、
// 受信処理遅延 0 なので総送信レイテンシも 246 µs、送信間隔 = 周期なのでジッターは 0（FR-019）。
TEST(F001Spec, F001_US3_1_Metrics_ById_ByNode_All)
{
    RecordProperty("spec", "001:US3-1");
    Network net;
    net.BitRateBps = 500000;
    net.Nodes.push_back(MakeNode(1, { MakeFrame(0x080, 8, 10000) }));
    net.Nodes.push_back(MakeNode(2, {}));

    const std::vector<LogRow> rows = RunSim(net, 1000000LL);
    const std::vector<GroupMetrics> metrics = ComputeMetrics(rows, net);

    const GroupMetrics* byId = nullptr;
    const GroupMetrics* byNode = nullptr;
    const GroupMetrics* all = nullptr;
    for (const GroupMetrics& g : metrics)
    {
        if (g.GroupType == "ID" && g.GroupKey == "0x080") byId = &g;
        if (g.GroupType == "Node" && g.GroupKey == "1") byNode = &g;
        if (g.GroupType == "ALL") all = &g;
    }
    ASSERT_NE(byId, nullptr) << "ID ごとの集計（0x080）が無い";
    ASSERT_NE(byNode, nullptr) << "ノードごとの集計（ノード 1）が無い";
    ASSERT_NE(all, nullptr) << "全体の集計が無い";

    for (const GroupMetrics* g : { byId, byNode, all })
    {
        EXPECT_GT(g->TotalLatency.Count, 0) << g->GroupType << ": 総送信レイテンシが算出されていない";
        EXPECT_DOUBLE_EQ(g->TotalLatency.MaxUs, 246.0) << g->GroupType << ": 総送信レイテンシの最大";
        EXPECT_DOUBLE_EQ(g->QueueingDelay.MaxUs, 0.0) << g->GroupType << ": 送信待ち遅延の最大";
        EXPECT_DOUBLE_EQ(g->TxDuration.MinUs, 246.0) << g->GroupType << ": フレーム伝送時間の最小";
        EXPECT_DOUBLE_EQ(g->TxDuration.MaxUs, 246.0) << g->GroupType << ": フレーム伝送時間の最大";
        EXPECT_GT(g->AvgBusLoadPct, 0.0) << g->GroupType << ": バス負荷率が算出されていない";
    }
    EXPECT_TRUE(byId->HasJitter) << "ID ごとの集計にジッターが無い";
    EXPECT_DOUBLE_EQ(byId->JitterMaxUs, 0.0) << "ジッターの最大";
    EXPECT_DOUBLE_EQ(byId->JitterMinUs, 0.0) << "ジッターの最小";
    EXPECT_DOUBLE_EQ(byId->JitterStdDevUs, 0.0) << "ジッターの標準偏差";
}

// 根拠: spec.md SC-001、FR-007（同一構成からは常に同一の実行結果）。
// 同時の送信要求（調停・再送）を含む構成を 2 回実行し、ログの全行・全列が一致する。
TEST(F001Spec, F001_SC001_SameConfigTwice_IdenticalLog)
{
    RecordProperty("spec", "001:SC-001");
    Network net;
    net.BitRateBps = 500000;
    net.Nodes.push_back(MakeNode(1, { MakeFrame(0x100, 8, 10000), MakeFrame(0x101, 4, 20000, 5000) }));
    net.Nodes.push_back(MakeNode(2, { MakeFrame(0x080, 8, 10000) }));
    net.Nodes.push_back(MakeNode(3, { MakeFrame(0x200, 2, 5000) }));

    const std::vector<LogRow> a = RunSim(net, 1000000LL);
    const std::vector<LogRow> b = RunSim(net, 1000000LL);
    ASSERT_FALSE(a.empty()) << "前提: ログが空";
    ASSERT_EQ(a.size(), b.size()) << "2 回の実行で行数が違う";
    for (std::size_t i = 0; i < a.size(); ++i)
    {
        const bool same =
            a[i].TimestampUs == b[i].TimestampUs && a[i].CanId == b[i].CanId && a[i].Dlc == b[i].Dlc &&
            a[i].TxNodeId == b[i].TxNodeId && a[i].RxNodeId == b[i].RxNodeId &&
            a[i].TxRequestUs == b[i].TxRequestUs && a[i].TxStartUs == b[i].TxStartUs &&
            a[i].TxEndUs == b[i].TxEndUs && a[i].RxStartUs == b[i].RxStartUs && a[i].RxEndUs == b[i].RxEndUs &&
            a[i].RxProcessUs == b[i].RxProcessUs && a[i].BusLoadPct == b[i].BusLoadPct &&
            a[i].Status == b[i].Status && a[i].RetryCount == b[i].RetryCount &&
            a[i].UpdateCount == b[i].UpdateCount;
        EXPECT_TRUE(same) << i << " 行目が 2 回の実行で一致しない";
    }
}

// 根拠: spec.md SC-002、FR-012（ID が最も小さいノードが調停の勝者）・FR-013（負けたノードは再送信）。
// 複数ノードが同時に送信要求を出すケースで、ID の小さい順に送信される。ID の並び（ノード番号との対応）を変えた
// いくつかのケースで確かめる。
class F001ArbitrationOrder : public ::testing::TestWithParam<std::vector<unsigned>>
{
};

TEST_P(F001ArbitrationOrder, F001_SC002_SimultaneousRequests_SentInAscendingIdOrder)
{
    RecordProperty("spec", "001:SC-002");
    const std::vector<unsigned>& ids = GetParam();
    Network net;
    net.BitRateBps = 500000;
    for (std::size_t i = 0; i < ids.size(); ++i)
        net.Nodes.push_back(MakeNode(static_cast<int>(i) + 1, { MakeFrame(ids[i], 8, 1000000) }));

    const std::vector<LogRow> rows = RunSim(net, 100000LL);
    std::map<long long, unsigned> byStart; // 送信開始 -> CAN ID
    for (const LogRow& r : rows)
        byStart[r.TxStartUs] = r.CanId;

    std::vector<unsigned> expected = ids;
    std::sort(expected.begin(), expected.end());
    std::vector<unsigned> actual;
    for (const auto& kv : byStart)
        actual.push_back(kv.second);
    EXPECT_EQ(actual, expected) << "同時の送信要求が ID の小さい順に送信されていない";
}

INSTANTIATE_TEST_SUITE_P(
    SC002, F001ArbitrationOrder,
    ::testing::Values(std::vector<unsigned>{ 0x300, 0x100, 0x200, 0x050 },
                      std::vector<unsigned>{ 0x050, 0x100, 0x200, 0x300 },
                      std::vector<unsigned>{ 0x7FF, 0x000 },
                      std::vector<unsigned>{ 0x123, 0x122, 0x124, 0x121, 0x125 }));

// 根拠: spec.md SC-003、FR-008・FR-009。フレーム伝送時間が、ビット列（スタッフビット込み）から手計算した値と
// 完全に一致する。手計算の値は機能 031 の spec.md（US1 シナリオ 1・US2 シナリオ 2）: 500 kbps で
// ID 0x080・DLC 8（データ 0xFF）は 123 ビット・246 µs、ID 0x7FF・DLC 0 は 47 ビット・94 µs。
// 計算結果（ComputeFrameTiming）と、シミュレーションのログの 送信完了 − 送信開始 の両方で確かめる。
struct HandCalcCase
{
    unsigned CanId;
    unsigned Dlc;
    int Bits;
    long long DurationUs;
};

class F001HandCalcTiming : public ::testing::TestWithParam<HandCalcCase>
{
};

TEST_P(F001HandCalcTiming, F001_SC003_FrameTiming_MatchesHandCalc)
{
    RecordProperty("spec", "001:SC-003");
    const HandCalcCase& c = GetParam();
    const FrameTimingResult t = ComputeFrameTiming(c.CanId, c.Dlc, std::vector<std::uint8_t>(c.Dlc, 0xFF), 500000);
    EXPECT_EQ(t.TotalBits, c.Bits) << "総ビット数が手計算と違う";
    EXPECT_EQ(t.TxDurationNs, c.DurationUs * 1000) << "伝送時間が手計算と違う";

    Network net;
    net.BitRateBps = 500000;
    net.Nodes.push_back(MakeNode(1, { MakeFrame(c.CanId, c.Dlc, 10000) }));
    const std::vector<LogRow> rows = RunSim(net, 100000LL);
    ASSERT_FALSE(rows.empty()) << "前提: ログが空";
    for (const LogRow& r : rows)
        EXPECT_EQ(r.TxEndUs - r.TxStartUs, c.DurationUs) << "ログの伝送時間が手計算と違う";
}

INSTANTIATE_TEST_SUITE_P(
    SC003, F001HandCalcTiming,
    ::testing::Values(HandCalcCase{ 0x080, 8, 123, 246 }, HandCalcCase{ 0x7FF, 0, 47, 94 }));

// 根拠: spec.md SC-004。バス負荷率が 100% 未満の構成で一定時間実行したとき、各フレームの送信回数は
// 「(実行時間 − 送信開始可能になった時刻 − 初回オフセット) ÷ 送信周期」の切り上げ値と一致する。
// 送信開始可能になった時刻: 電源投入・初期化 0 の既定でも、バスが空であることを 11 ビット時間観測してから通常動作に入る
// （機能 002、500 kbps で 22 µs、CLAUDE.md「起動制御による時刻への影響」）。
// 実行時間 1 s: 周期 30 ms・オフセット 0 → ceil((1000000 − 22 − 0) / 30000) = ceil(33.33) = 34 回、
//              周期 30 ms・オフセット 25 ms → ceil((1000000 − 22 − 25000) / 30000) = ceil(32.50) = 33 回。
TEST(F001Spec, F001_SC004_TransmissionCount_MatchesFormula)
{
    RecordProperty("spec", "001:SC-004");
    Network net;
    net.BitRateBps = 500000;
    net.Nodes.push_back(MakeNode(1, { MakeFrame(0x100, 8, 30000, 0), MakeFrame(0x101, 8, 30000, 25000) }));

    const std::vector<LogRow> rows = RunSim(net, 1000000LL);
    EXPECT_EQ(Transmissions(rows, 0x100).size(), 34u) << "オフセット 0 のフレームの送信回数が式と違う";
    EXPECT_EQ(Transmissions(rows, 0x101).size(), 33u) << "オフセット 25 ms のフレームの送信回数が式と違う";
}

// 根拠: spec.md SC-005、FR-001。16 ノードの構成は正常に読み込まれて実行でき、17 ノード以上の構成と
// ノードが 0 個の構成は必ず拒否される。
TEST(F001Spec, F001_SC005_SixteenNodesRun_SeventeenAndZeroRejected)
{
    RecordProperty("spec", "001:SC-005");
    std::string sixteen = "[Network]\r\nBitRateBps=500000\r\n";
    for (int i = 1; i <= 16; ++i)
        sixteen += NodeWithFrame(i, 0x100 + i);
    const ConfigLoadResult ok = LoadText(L"sc005_16.canprj", sixteen);
    ASSERT_TRUE(ok.Success) << "16 ノードの構成が読み込めない: " << ok.ErrorMessage;
    EXPECT_EQ(ok.Config.Nodes.size(), 16u) << "読み込んだノード数が 16 でない";

    const std::vector<LogRow> rows = RunSim(ok.Config, 100000LL);
    std::set<int> senders;
    for (const LogRow& r : rows)
        senders.insert(r.TxNodeId);
    EXPECT_EQ(senders.size(), 16u) << "16 ノードすべてが送信していない";

    EXPECT_FALSE(LoadText(L"sc005_17.canprj", SeventeenNodes()).Success) << "17 ノードの構成が拒否されない";
    EXPECT_FALSE(LoadText(L"sc005_0.canprj", "[Network]\r\nBitRateBps=500000\r\n").Success)
        << "ノード 0 個の構成が拒否されない";
}

// 根拠: spec.md SC-006、FR-020、contracts/log-output-format.md「集計サマリ出力」。性能指標（総送信レイテンシ・
// 送信待ち遅延・フレーム伝送時間・バス負荷率・ジッター）を、ログから手作業で追加計算せずに確認できる
// （ID ごと・ノードごと・全体の集計を CSV に出力する）。列名は契約で決まっていないため、CSV が書かれ、
// 集計の数だけ行があることを確かめる。
TEST(F001Spec, F001_SC006_MetricsCsv_WrittenForEveryGroup)
{
    RecordProperty("spec", "001:SC-006");
    Network net;
    net.BitRateBps = 500000;
    net.Nodes.push_back(MakeNode(1, { MakeFrame(0x100, 8, 10000) }));
    net.Nodes.push_back(MakeNode(2, { MakeFrame(0x080, 8, 10000) }));

    const std::vector<LogRow> rows = RunSim(net, 1000000LL);
    const std::vector<GroupMetrics> metrics = ComputeMetrics(rows, net);
    std::set<std::string> types;
    for (const GroupMetrics& g : metrics)
        types.insert(g.GroupType);
    EXPECT_EQ(types, (std::set<std::string>{ "ALL", "ID", "Node" })) << "ID ごと・ノードごと・全体の集計がそろっていない";

    const std::wstring path = TempPath(L"sc006.summary.csv");
    ASSERT_TRUE(WriteMetricsCsv(metrics, path)) << "集計の CSV を書けない";
    FILE* f = _wfopen(path.c_str(), L"rb");
    ASSERT_NE(f, nullptr) << "集計の CSV が無い";
    int lines = 0;
    for (int ch; (ch = fgetc(f)) != EOF;)
        if (ch == '\n')
            ++lines;
    fclose(f);
    EXPECT_GE(lines, static_cast<int>(metrics.size()) + 1) << "集計の CSV の行数が、ヘッダー + 集計の数より少ない";
}
