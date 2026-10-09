// 機能 008（フレームバッファ: 送信 16 本・受信 8 本）の仕様テスト
// 根拠: specs/008-frame-buffers/spec.md と contracts/log-and-summary.md。仕様に ID が無いので、
// 受け入れシナリオは US<ストーリー番号>-<シナリオ番号> と呼ぶ。
// 500 kbps で通常動作は 22 us から（機能 002）。送信要求は 22 us + 初回オフセット + k × 周期（機能 006）。

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdio>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "MetricsAggregator.h"
#include "support/test_helpers.h"

using namespace CanSim;
using namespace TestHelpers;

namespace
{
    bool IsOverwrite(const LogRow& r) { return r.Status == kStatusOverwrite; }
    bool IsTxSideOverwrite(const LogRow& r) { return IsOverwrite(r) && r.TxStartUs < 0; }
    bool IsRxSideOverwrite(const LogRow& r) { return IsOverwrite(r) && r.TxStartUs >= 0; }

    // ECU 1 が 0x100〜0x113 の 20 フレーム（DLC 8、初回オフセット 0、周期 10 ms）を送り、ECU 2 が受信する
    Network MakeTwentyFrameNetwork()
    {
        std::vector<FrameSpec> frames;
        for (unsigned i = 0; i < 20; ++i) frames.push_back({0x100 + i, 8, 10000, 0});
        Network net;
        net.BitRateBps = 500000;
        net.Nodes.push_back(MakeNode(1, frames));
        net.Nodes.push_back(MakeNode(2, {}));
        return net;
    }

    // ECU 1 が 0x100〜0x10B の 12 フレームを同時に要求し、続けて送る。
    // ECU 2 は受信処理遅延 rxDelayUs、ECU 3 は既定（0）。
    Network MakeRxBurstNetwork(long long rxDelayUs)
    {
        std::vector<FrameSpec> frames;
        for (unsigned i = 0; i < 12; ++i) frames.push_back({0x100 + i, 8, 100000, 0});
        Network net;
        net.BitRateBps = 500000;
        net.Nodes.push_back(MakeNode(1, frames));
        Node slow = MakeNode(2, {});
        slow.ReceiveDelayUs = rxDelayUs;
        net.Nodes.push_back(slow);
        net.Nodes.push_back(MakeNode(3, {}));
        return net;
    }

    const GroupMetrics* FindGroup(const std::vector<GroupMetrics>& metrics, const std::string& type, const std::string& key)
    {
        for (const GroupMetrics& g : metrics)
            if (g.GroupType == type && g.GroupKey == key) return &g;
        return nullptr;
    }
}

// ---------------------------------------------------------------------------
// US1-1 / SC-001 / US1-5: 20 フレームの要求がそろうたびに、ID の小さい 16 本が送信待ちになり、
//   ID の大きい 4 本（0x110〜0x113）の要求が捨てられる。送信が済んだバッファは次の周期で再び使える。
//   捨てられた要求は再送されない。
// 根拠: spec.md US1 シナリオ 1・5、SC-001、FR-003、FR-004、Edge Cases 2 つ目。
// ---------------------------------------------------------------------------
TEST(F008Spec, F008_US1_1_TwentyFramesTogether_FourHighestIdsDiscardedEachPeriod)
{
    RecordProperty("spec", "008:US1-1");
    RecordProperty("spec", "008:SC-001");
    RecordProperty("spec", "008:US1-5");
    const RunResult run = RunSim(MakeTwentyFrameNetwork(), 50000LL); // 要求は 22, 10022, ..., 40022 の 5 回

    std::map<long long, std::set<unsigned>> discardedByTime;
    std::map<unsigned, int> sentById;
    for (const LogRow& r : run.Rows)
        if (IsTxSideOverwrite(r)) discardedByTime[r.TxRequestUs].insert(r.CanId);
    for (const LogRow& r : UniqueTransmissions(run.Rows)) ++sentById[r.CanId];

    const std::set<unsigned> expected = {0x110, 0x111, 0x112, 0x113};
    ASSERT_EQ(5u, discardedByTime.size()) << "要求がそろう 5 回すべてで捨てられる";
    long long t = 22;
    for (const auto& kv : discardedByTime)
    {
        EXPECT_EQ(t, kv.first) << "捨てられた要求の時刻";
        EXPECT_EQ(expected, kv.second) << "捨てられるのは ID の大きい 4 本";
        t += 10000;
    }
    for (unsigned id = 0x100; id <= 0x10F; ++id)
        EXPECT_EQ(5, sentById[id]) << "ID 0x" << std::hex << id << " は毎周期送信される（バッファが空く）";
    for (unsigned id = 0x110; id <= 0x113; ++id)
        EXPECT_EQ(0, sentById[id]) << "捨てられた ID 0x" << std::hex << id << " は再送されない";
}

// ---------------------------------------------------------------------------
// US1-2 / FR-004: 捨てられた送信要求は 1 件 1 行。Status = OVERWRITE、送信要求時刻はその要求の時刻、
//   受信ノードと送信・受信の各時刻は空欄。代表時刻は要求時刻。
// 根拠: spec.md US1 シナリオ 2、FR-004、contracts/log-and-summary.md の表（送信バッファ満杯の行）。
// ---------------------------------------------------------------------------
TEST(F008Spec, F008_US1_2_DiscardedRequest_OverwriteRowWithBlankColumns)
{
    RecordProperty("spec", "008:US1-2");
    const RunResult run = RunSim(MakeTwentyFrameNetwork(), 10000LL); // 要求は 22 us の 1 回
    std::vector<LogRow> discarded;
    for (const LogRow& r : run.Rows)
        if (IsOverwrite(r)) discarded.push_back(r);
    ASSERT_EQ(4u, discarded.size()) << "捨てられた要求 4 件につき 1 行ずつ";

    const std::wstring path = TempPath("f008_us1_2.csv");
    ASSERT_TRUE(WriteLogCsv(discarded, path));
    const std::vector<std::string> lines = SplitLines(ReadAll(path));
    DeleteFileW(path.c_str());
    ASSERT_EQ(5u, lines.size());
    for (std::size_t i = 1; i < lines.size(); ++i)
    {
        const std::vector<std::string> f = SplitFields(lines[i]);
        ASSERT_EQ(15u, f.size());
        EXPECT_EQ("22", f[0]) << "Timestamp_us = 要求時刻";
        EXPECT_EQ("1", f[3]) << "Tx_Node";
        EXPECT_EQ("", f[4]) << "Rx_Node は空欄";
        EXPECT_EQ("22", f[5]) << "Tx_Request = 捨てられた要求の時刻";
        for (int c = 6; c <= 10; ++c) EXPECT_EQ("", f[c]) << c << " 列目（送信・受信の時刻）は空欄";
        EXPECT_EQ("OVERWRITE", f[12]) << "Status";
    }
}

// ---------------------------------------------------------------------------
// US1-3 / SC-003 / FR-009: 集計の ECU ごとの送信側破棄数 = ログの送信側 OVERWRITE 行の数。
// FR-010: OVERWRITE 行は性能指標に入らない（捨てられた ID の ID 行は無く、全体の送信待ち件数は実際の送信数）。
// 根拠: spec.md US1 シナリオ 3、SC-003、FR-009、FR-010、contracts/log-and-summary.md「集計」。
// ---------------------------------------------------------------------------
TEST(F008Spec, F008_US1_3_TxDiscardCount_EqualsOverwriteRowsAndExcludedFromStats)
{
    RecordProperty("spec", "008:US1-3");
    RecordProperty("spec", "008:SC-003");
    RecordProperty("spec", "008:FR-010");
    const Network net = MakeTwentyFrameNetwork();
    const RunResult run = RunSim(net, 50000LL);
    int txOverwrite = 0;
    for (const LogRow& r : run.Rows)
        if (IsTxSideOverwrite(r)) ++txOverwrite;
    ASSERT_EQ(20, txOverwrite) << "前提: 4 件 × 5 回";

    const std::vector<GroupMetrics> metrics = ComputeMetrics(run.Rows, net);
    const GroupMetrics* node1 = FindGroup(metrics, "Node", "1");
    ASSERT_NE(nullptr, node1);
    ASSERT_TRUE(node1->HasDiscards);
    EXPECT_EQ(20, node1->TxDiscardCount) << "送信側破棄数 = 送信側 OVERWRITE 行の数";
    EXPECT_EQ(0, node1->RxDiscardCount) << "受信側破棄数";
    for (unsigned id = 0x110; id <= 0x113; ++id)
    {
        char key[8];
        snprintf(key, sizeof(key), "0x%03X", id);
        EXPECT_EQ(nullptr, FindGroup(metrics, "ID", key)) << key << " は送信されていないので ID 行が無い";
    }
    const GroupMetrics* all = FindGroup(metrics, "ALL", "");
    ASSERT_NE(nullptr, all);
    EXPECT_EQ(80, all->QueueingDelay.Count) << "全体の送信待ち件数 = 実際の送信 16 本 × 5 回（OVERWRITE を除く）";
}

// ---------------------------------------------------------------------------
// US1-4 / FR-002: 送信待ちの同じフレームに次の周期の要求が来たら、新しいバッファを使わず
//   Update_Count を 1 増やす。送信要求時刻は最初のまま。
// 根拠: spec.md US1 シナリオ 4、FR-002。
// 構成（125 kbps、通常動作は 88 us から）: 0x300（DLC 0、周期 1500 us）が DLC 8 の 2 本の後まで待つ間に
//   1588 us の要求が来る。
// ---------------------------------------------------------------------------
TEST(F008Spec, F008_US1_4_RequestWhileQueued_UpdateCountedNoNewBuffer)
{
    RecordProperty("spec", "008:US1-4");
    Network net;
    net.BitRateBps = 125000;
    net.Nodes.push_back(MakeNode(1, {{0x100, 8, 10000, 0}}));
    net.Nodes.push_back(MakeNode(2, {{0x200, 8, 10000, 0}}));
    net.Nodes.push_back(MakeNode(3, {{0x300, 0, 1500, 0}}));
    const RunResult run = RunSim(net, 2900LL);
    std::vector<LogRow> tx300;
    for (const LogRow& r : UniqueTransmissions(run.Rows))
        if (r.CanId == 0x300) tx300.push_back(r);
    for (const LogRow& r : run.Rows) EXPECT_FALSE(IsOverwrite(r)) << "バッファ満杯ではないので OVERWRITE は無い";
    ASSERT_EQ(1u, tx300.size()) << "2 回の要求が 1 本のバッファにまとまり、1 回だけ送信される";
    EXPECT_EQ(88, tx300[0].TxRequestUs) << "送信要求時刻は最初の要求のまま";
    EXPECT_EQ(1, tx300[0].UpdateCount) << "Update_Count は 1";
}

// ---------------------------------------------------------------------------
// US2-1 / US2-2 / SC-002 / FR-005・FR-006: 受信処理遅延 5 ms の ECU 2 に 12 本が続けて届く（間隔は 0.3 ms 未満）。
//   最初の 8 本は受信完了でバッファに入り、受信処理時刻 = 受信完了 + 5000 us。9 本目以降の 4 本は捨てられ、
//   その行は OVERWRITE で受信処理時刻が空欄。バッファの中身は常に 8 本以下。
// US2-3 / SC-004: 受信処理遅延 0 の ECU 3 では満杯は起きない。
// US2-4 / SC-003: 集計の受信側破棄数 = 受信側 OVERWRITE 行の数。受信しかしていない ECU にも Node 行が出る。
// 根拠: spec.md US2 シナリオ 1〜4、SC-002〜SC-004、FR-005、FR-006、FR-009、contracts/log-and-summary.md。
// ---------------------------------------------------------------------------
TEST(F008Spec, F008_US2_1_SlowReceiver_NinthAndLaterDiscarded)
{
    RecordProperty("spec", "008:US2-1");
    RecordProperty("spec", "008:US2-2");
    RecordProperty("spec", "008:US2-3");
    RecordProperty("spec", "008:US2-4");
    RecordProperty("spec", "008:SC-002");
    RecordProperty("spec", "008:SC-004");
    const Network net = MakeRxBurstNetwork(5000);
    const RunResult run = RunSim(net, 50000LL);

    std::vector<LogRow> node2;
    int node3 = 0;
    for (const LogRow& r : run.Rows)
    {
        if (r.RxNodeId == 2) node2.push_back(r);
        if (r.RxNodeId == 3)
        {
            ++node3;
            EXPECT_FALSE(IsOverwrite(r)) << "受信処理遅延 0 の ECU 3 で受信側の OVERWRITE が出た";
        }
    }
    ASSERT_EQ(12u, node2.size()) << "ECU 2 の行は 12 本分";
    EXPECT_EQ(12, node3) << "ECU 3 は 12 本すべて受信";
    std::sort(node2.begin(), node2.end(), [](const LogRow& a, const LogRow& b) { return a.RxEndUs < b.RxEndUs; });
    for (std::size_t i = 0; i < node2.size(); ++i)
    {
        const LogRow& r = node2[i];
        if (i < 8)
        {
            EXPECT_EQ("SUCCESS", r.Status) << i + 1 << " 本目はバッファに入る";
            EXPECT_EQ(r.RxEndUs + 5000, r.RxProcessUs) << i + 1 << " 本目の受信処理時刻 = 受信完了 + 5000 us";
        }
        else
        {
            EXPECT_EQ("OVERWRITE", r.Status) << i + 1 << " 本目は捨てられる";
            EXPECT_EQ(-1, r.RxProcessUs) << i + 1 << " 本目の受信処理時刻は空欄";
            EXPECT_GE(r.RxEndUs, 0) << "受信完了時刻は値あり";
        }
    }
    // SC-002: どの到着時点でも、受け入れ済みで未処理のフレームは 8 本以下
    for (const LogRow& at : node2)
    {
        int held = 0;
        for (const LogRow& r : node2)
            if (!IsOverwrite(r) && r.RxEndUs <= at.RxEndUs && at.RxEndUs < r.RxProcessUs) ++held;
        EXPECT_LE(held, 8) << "受信バッファが 8 本を超えた";
    }

    const std::vector<GroupMetrics> metrics = ComputeMetrics(run.Rows, net);
    const GroupMetrics* g2 = FindGroup(metrics, "Node", "2");
    ASSERT_NE(nullptr, g2) << "受信しかしていない ECU 2 にも Node 行が出る（破棄があるため）";
    EXPECT_EQ(4, g2->RxDiscardCount) << "受信側破棄数 = 受信側 OVERWRITE 行の数";
    EXPECT_EQ(0, g2->TxDiscardCount) << "送信側破棄数";
    const GroupMetrics* g3 = FindGroup(metrics, "Node", "3");
    if (g3 != nullptr) EXPECT_EQ(0, g3->RxDiscardCount) << "ECU 3 の受信側破棄数";
}

// ---------------------------------------------------------------------------
// FR-007: 同じ時刻に取り出しと新しい受信完了が起きたら、取り出しを先に行う（新しいフレームは入る）。
// 根拠: spec.md FR-007、Edge Cases（取り出しと受信完了が同時刻）。
// 1 回目の実行で 1 本目・9 本目の受信完了時刻を調べ、受信処理遅延 = その差にすると、
//   1 本目の取り出しと 9 本目の受信完了が同時刻になる。遅延を 1 us 増やすと 9 本目は捨てられる。
// ---------------------------------------------------------------------------
TEST(F008Spec, F008_FR007_RemovalAndArrivalSameTime_RemovalFirst)
{
    RecordProperty("spec", "008:FR-007");
    const RunResult probe = RunSim(MakeRxBurstNetwork(0), 50000LL);
    std::vector<long long> ends;
    for (const LogRow& r : probe.Rows)
        if (r.RxNodeId == 2) ends.push_back(r.RxEndUs);
    std::sort(ends.begin(), ends.end());
    ASSERT_GE(ends.size(), 9u);
    const long long delay = ends[8] - ends[0];

    auto ninthStatus = [](const RunResult& run) {
        std::vector<LogRow> rows;
        for (const LogRow& r : run.Rows)
            if (r.RxNodeId == 2) rows.push_back(r);
        std::sort(rows.begin(), rows.end(), [](const LogRow& a, const LogRow& b) { return a.RxEndUs < b.RxEndUs; });
        return rows.size() >= 9 ? rows[8].Status : std::string("(none)");
    };
    EXPECT_EQ("SUCCESS", ninthStatus(RunSim(MakeRxBurstNetwork(delay), 50000LL)))
        << "取り出しが先なので 9 本目はバッファに入る";
    EXPECT_EQ("OVERWRITE", ninthStatus(RunSim(MakeRxBurstNetwork(delay + 1), 50000LL)))
        << "取り出しより 1 us 前に届く 9 本目は捨てられる";
}

// ---------------------------------------------------------------------------
// US3-1 / FR-008: ログの最後の列の見出しは Update_Count。
// 根拠: spec.md US3 シナリオ 1、FR-008、contracts/log-and-summary.md の見出し。
// ---------------------------------------------------------------------------
TEST(F008Spec, F008_US3_1_LogHeader_LastColumnIsUpdateCount)
{
    RecordProperty("spec", "008:US3-1");
    const std::wstring path = TempPath("f008_us3_1.csv");
    ASSERT_TRUE(WriteLogCsv(std::vector<LogRow>(), path));
    const std::vector<std::string> lines = SplitLines(ReadAll(path));
    DeleteFileW(path.c_str());
    ASSERT_FALSE(lines.empty());
    EXPECT_EQ("Timestamp_us,CAN_ID,DLC,Tx_Node,Rx_Node,Tx_Request,Tx_Start,Tx_End,Rx_Start,Rx_End,Rx_Process,"
              "Bus_Load_Pct,Status,Retry_Count,Update_Count",
              lines[0])
        << "ログの見出し（契約どおり）";
}

// ---------------------------------------------------------------------------
// SC-006: OVERWRITE 行を含む構成を 2 回実行すると、ログが完全に一致する。
// 根拠: spec.md SC-006。
// ---------------------------------------------------------------------------
TEST(F008Spec, F008_SC006_OverwriteConfigRunTwice_LogsIdentical)
{
    RecordProperty("spec", "008:SC-006");
    std::string out[2];
    for (int k = 0; k < 2; ++k)
    {
        Network net = MakeTwentyFrameNetwork();
        net.Nodes[1].ReceiveDelayUs = 5000; // 受信側の OVERWRITE も起こす
        const std::wstring path = TempPath("f008_sc006.csv");
        ASSERT_TRUE(WriteLogCsv(RunSim(net, 50000LL).Rows, path));
        out[k] = ReadAll(path);
        DeleteFileW(path.c_str());
    }
    EXPECT_NE(std::string::npos, out[0].find("OVERWRITE")) << "前提: OVERWRITE 行がある";
    EXPECT_TRUE(out[0] == out[1]) << "2 回の実行のログが一致しない";
}
