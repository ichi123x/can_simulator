// feature_008_spec_tests.cpp（tests/unit）
// 機能 008（specs/008-frame-buffers/spec.md）の受け入れシナリオと成功基準のテスト。
// 期待値は spec.md（Doc 3.7a の FR-090〜FR-095）から書く。

#include "support/spec_test_helpers.h"

#include "MetricsAggregator.h"

#include <map>

using namespace CanSim;
using namespace spectest;

namespace
{
    // 送信側で捨てられた要求の行（受信ノードなし、送信開始が空欄）
    bool IsTxOverwrite(const LogRow& r)
    {
        return r.Status == kStatusOverwrite && r.RxNodeId < 0;
    }

    // 受信側で捨てられたフレームの行（受信ノードあり）
    bool IsRxOverwrite(const LogRow& r)
    {
        return r.Status == kStatusOverwrite && r.RxNodeId >= 0;
    }

    // ノード 1 に、初回オフセット 0・周期 100 ms のフレームを 20 個（ID 0x100〜0x113）置いた構成。
    // 要求は 100 ms ごとに 20 個同時にそろう（通常動作に入るのは 22 µs、500 kbps）。
    // 16 フレームの送信は 1 周期の中で十分に終わる（1 フレーム 246 µs + 休止 6 µs）。
    Network TwentyFramesNetwork()
    {
        Network net;
        net.BitRateBps = 500000;
        std::vector<FrameDefinition> frames;
        for (unsigned i = 0; i < 20; ++i)
            frames.push_back(MakeFrame(0x100 + i, 8, 100000));
        net.Nodes.push_back(MakeNode(1, frames));
        net.Nodes.push_back(MakeNode(2));
        return net;
    }

    // ノード 1 が初回オフセット 0・周期 10 ms のフレームを count 個（ID 0x100〜）続けて送り、
    // ノード 2 が受信処理遅延 delayUs で受ける構成
    Network BurstNetwork(unsigned count, long long delayUs)
    {
        Network net;
        net.BitRateBps = 500000;
        std::vector<FrameDefinition> frames;
        for (unsigned i = 0; i < count; ++i)
            frames.push_back(MakeFrame(0x100 + i, 8, 10000));
        net.Nodes.push_back(MakeNode(1, frames));
        Node rx = MakeNode(2);
        rx.ReceiveDelayUs = delayUs;
        net.Nodes.push_back(rx);
        return net;
    }

    const GroupMetrics* NodeGroup(const std::vector<GroupMetrics>& metrics, int nodeId)
    {
        for (const GroupMetrics& g : metrics)
            if (g.GroupType == "Node" && g.GroupKey == std::to_string(nodeId))
                return &g;
        return nullptr;
    }
}

// 根拠: spec.md US1 受け入れシナリオ 1、FR-001・FR-004（Doc FR-092b・FR-092c）。
// 1 つの ECU に同時にそろう 20 フレームを置くと、要求がそろう時刻ごとに、ID の小さい 16 フレームが送信待ちになり、
// ID の大きい 4 フレーム（0x110〜0x113）の要求は捨てられる。
TEST(F008Spec, F008_US1_1_TwentyFrames_LowestSixteenQueuedHighestFourDropped)
{
    RecordProperty("spec", "008:US1-1");
    const std::vector<LogRow> rows = RunSim(TwentyFramesNetwork(), 1000000LL);
    std::map<long long, std::set<unsigned>> droppedAt, sentAt; // 送信要求の時刻 -> CAN ID
    for (const LogRow& r : rows)
    {
        if (IsTxOverwrite(r))
            droppedAt[r.TxRequestUs].insert(r.CanId);
        else
            sentAt[r.TxRequestUs].insert(r.CanId);
    }
    ASSERT_EQ(droppedAt.size(), 10u) << "前提: 1 秒で 10 回、要求がそろっていない";
    std::set<unsigned> lowest16, highest4;
    for (unsigned i = 0; i < 16; ++i) lowest16.insert(0x100 + i);
    for (unsigned i = 16; i < 20; ++i) highest4.insert(0x100 + i);
    for (const auto& kv : droppedAt)
    {
        EXPECT_EQ(kv.second, highest4) << kv.first << " µs: 捨てられたのが ID の大きい 4 フレームでない";
        EXPECT_EQ(sentAt[kv.first], lowest16) << kv.first << " µs: 送信されたのが ID の小さい 16 フレームでない";
    }
}

// 根拠: spec.md US1 受け入れシナリオ 2、FR-004。捨てられた送信要求はそのフレームにつき 1 行で、Status は OVERWRITE、
// 送信要求時刻は捨てられた要求の時刻（22 µs + 100 ms × k）、受信ノードと送信・受信の各時刻は空欄。
// 空欄はログの CSV で確かめる（contracts/log-output-format.md、機能 008 の「負の時刻は空欄」）。
TEST(F008Spec, F008_US1_2_DroppedRequest_OneOverwriteRowWithBlanks)
{
    RecordProperty("spec", "008:US1-2");
    const std::vector<LogRow> rows = RunSim(TwentyFramesNetwork(), 1000000LL);
    std::vector<LogRow> dropped;
    for (const LogRow& r : rows)
        if (IsTxOverwrite(r))
            dropped.push_back(r);
    ASSERT_EQ(dropped.size(), 40u) << "捨てられた要求の行が 4 × 10 回 = 40 行でない";
    for (const LogRow& r : dropped)
        EXPECT_EQ((r.TxRequestUs - 22) % 100000, 0) << "送信要求時刻が、要求がそろう時刻でない: " << r.TxRequestUs;

    const std::wstring path = TempPath(L"f008_us1_2.log.csv");
    ASSERT_TRUE(WriteLogCsv({ dropped.front() }, path)) << "ログの CSV を書けない";
    const std::vector<std::string> lines = SplitLines(ReadText(path));
    ASSERT_EQ(lines.size(), 2u) << "ヘッダー + 1 行でない";
    // Timestamp_us,CAN_ID,DLC,Tx_Node,Rx_Node,Tx_Request,Tx_Start,Tx_End,Rx_Start,Rx_End,Rx_Process,Bus_Load_Pct,Status,...
    std::vector<std::string> cols;
    std::size_t start = 0;
    for (std::size_t comma; (comma = lines[1].find(',', start)) != std::string::npos; start = comma + 1)
        cols.push_back(lines[1].substr(start, comma - start));
    cols.push_back(lines[1].substr(start));
    ASSERT_EQ(cols.size(), 15u) << "列の数が 15 でない: " << lines[1];
    EXPECT_EQ(cols[4], "") << "受信ノードが空欄でない";
    EXPECT_EQ(cols[5], "22") << "送信要求時刻が捨てられた要求の時刻でない";
    for (int i = 6; i <= 10; ++i)
        EXPECT_EQ(cols[i], "") << i << " 列目（送信・受信の時刻）が空欄でない";
    EXPECT_EQ(cols[12], "OVERWRITE") << "Status が OVERWRITE でない";
}

// 根拠: spec.md US1 受け入れシナリオ 3、FR-009。その ECU の送信バッファ満杯による破棄回数（集計）が、
// ログの送信側 OVERWRITE 行の数と一致する。
TEST(F008Spec, F008_US1_3_TxDiscardCount_EqualsTxOverwriteRows)
{
    RecordProperty("spec", "008:US1-3");
    const Network net = TwentyFramesNetwork();
    const std::vector<LogRow> rows = RunSim(net, 1000000LL);
    int overwriteRows = 0;
    for (const LogRow& r : rows)
        if (IsTxOverwrite(r) && r.TxNodeId == 1)
            ++overwriteRows;
    const std::vector<GroupMetrics> metrics = ComputeMetrics(rows, net);
    const GroupMetrics* node1 = NodeGroup(metrics, 1);
    ASSERT_NE(node1, nullptr) << "ノード 1 の集計が無い";
    EXPECT_TRUE(node1->HasDiscards) << "ノード 1 の集計に破棄回数が無い";
    EXPECT_EQ(node1->TxDiscardCount, overwriteRows) << "送信側の破棄回数がログの OVERWRITE 行の数と違う";
    EXPECT_EQ(overwriteRows, 40) << "前提: 捨てられた要求は 4 × 10 回";
}

// 根拠: spec.md US1 受け入れシナリオ 4、FR-002（Doc FR-092a・FR-024）。送信待ちが 16 本未満で、すでに送信待ちの同じ
// フレームに次の周期の要求が来たら、新しいバッファは使わずデータを更新して Update_Count を 1 増やす。送信要求時刻は最初の要求のまま。
// 手計算（500 kbps）: ID 0x7FF・DLC 0 は 94 µs（機能 031）。周期 40 µs の要求は 22・62・102 µs。1 回目は 22〜116 µs に送り、
// 休止期間は 122 µs まで。62 µs の要求が送信待ちになり、102 µs の要求で更新される。2 回目は 122 µs に送信開始、要求時刻 62 µs、更新 1 回。
TEST(F008Spec, F008_US1_4_RequestForWaitingFrame_UpdatesInsteadOfNewBuffer)
{
    RecordProperty("spec", "008:US1-4");
    Network net;
    net.BitRateBps = 500000;
    net.Nodes.push_back(MakeNode(1, { MakeFrame(0x7FF, 0, 40) }));
    const std::vector<LogRow> rows = RunSim(net, 300LL);
    ASSERT_GE(rows.size(), 2u) << "前提: 送信が 2 回未満";
    EXPECT_EQ(rows[1].TxRequestUs, 62) << "2 回目の送信要求時刻が最初の要求のままでない";
    EXPECT_EQ(rows[1].TxStartUs, 122) << "2 回目の送信開始";
    EXPECT_EQ(rows[1].UpdateCount, 1) << "Update_Count が 1 でない";
    for (const LogRow& r : rows)
        EXPECT_NE(r.Status, kStatusOverwrite) << "送信待ちが 16 本未満なのに要求が捨てられた";
}

// 根拠: spec.md US1 受け入れシナリオ 5、FR-003（送信が成功したらバッファを空ける）。20 フレームの構成で、
// 2 回目以降の要求がそろう時刻でも、ID の小さい 16 フレームは毎回受け付けられて送信される。
TEST(F008Spec, F008_US1_5_AfterSuccessfulSend_BufferAcceptsNextRequest)
{
    RecordProperty("spec", "008:US1-5");
    const std::vector<LogRow> rows = RunSim(TwentyFramesNetwork(), 1000000LL);
    for (unsigned i = 0; i < 16; ++i)
        EXPECT_EQ(Transmissions(rows, 0x100 + i).size(), 10u)
            << "ID 0x" << std::hex << (0x100 + i) << std::dec << " が 10 周期すべてで送信されていない";
}

// 根拠: spec.md US2 受け入れシナリオ 1、FR-005（Doc FR-093）。受信完了時刻にバッファに入り、受信処理時刻（受信完了 + 受信処理遅延）に
// 取り出されて空く。受信処理遅延 2 ms で 8 フレームずつ続けて届く構成では、1 回の 8 フレームは 8 本に収まり、
// 次の 10 ms 後の 8 フレームも（前の 8 フレームが取り出された後なので）すべて受け付けられる。
TEST(F008Spec, F008_US2_1_RxBuffer_FreedAtProcessTime)
{
    RecordProperty("spec", "008:US2-1");
    const std::vector<LogRow> rows = RunSim(BurstNetwork(8, 2000), 100000LL);
    int received = 0;
    for (const LogRow& r : rows)
    {
        if (r.RxNodeId != 2)
            continue;
        ++received;
        EXPECT_NE(r.Status, kStatusOverwrite) << "8 本に収まるのに受信が捨てられた";
        EXPECT_EQ(r.RxProcessUs, r.RxEndUs + 2000) << "受信処理時刻が 受信完了 + 2000 µs でない";
    }
    EXPECT_EQ(received, 80) << "8 フレーム × 10 回の受信がそろっていない";
}

// 根拠: spec.md US2 受け入れシナリオ 2、FR-006（Doc FR-094）。受信バッファが 8 本とも埋まっているときに受信が完了した
// 新しいフレームは捨てられ、その受信ノードの行の Status は OVERWRITE、受信処理時刻は空欄。
// 受信処理遅延 5 ms で 9 フレームが続けて届くと、9 本目（0x108）の受信完了は 1 本目の受信処理時刻より前なので捨てられる。
TEST(F008Spec, F008_US2_2_RxBufferFull_NewFrameDroppedWithBlankProcessTime)
{
    RecordProperty("spec", "008:US2-2");
    const std::vector<LogRow> rows = RunSim(BurstNetwork(9, 5000), 10000LL);
    const LogRow* ninth = nullptr;
    for (const LogRow& r : rows)
        if (r.CanId == 0x108 && r.RxNodeId == 2)
            ninth = &r;
    ASSERT_NE(ninth, nullptr) << "9 本目（0x108）の受信ノード 2 の行が無い";
    EXPECT_EQ(ninth->Status, kStatusOverwrite) << "9 本目が捨てられていない";
    EXPECT_LT(ninth->RxProcessUs, 0) << "捨てられたフレームの受信処理時刻が空欄（負）でない";

    const std::wstring path = TempPath(L"f008_us2_2.log.csv");
    ASSERT_TRUE(WriteLogCsv({ *ninth }, path)) << "ログの CSV を書けない";
    const std::vector<std::string> lines = SplitLines(ReadText(path));
    ASSERT_EQ(lines.size(), 2u) << "ヘッダー + 1 行でない";
    // Rx_Process（11 列目）が空欄: "...,<Rx_End>,,<Bus_Load_Pct>,OVERWRITE,..."
    EXPECT_NE(lines[1].find(",,"), std::string::npos) << "CSV で受信処理時刻が空欄になっていない: " << lines[1];
    for (const LogRow& r : rows)
        if (r.RxNodeId == 2 && r.CanId < 0x108)
            EXPECT_NE(r.Status, kStatusOverwrite) << "8 本目までが捨てられた";
}

// 根拠: spec.md US2 受け入れシナリオ 3、FR-005（Doc FR-095）。受信処理遅延 0 µs（既定）では受信した瞬間に空くので、
// 20 フレームが続けて届いても受信バッファの満杯は起きない。
TEST(F008Spec, F008_US2_3_ZeroReceiveDelay_NoRxOverwrite)
{
    RecordProperty("spec", "008:US2-3");
    const std::vector<LogRow> rows = RunSim(BurstNetwork(20, 0), 100000LL);
    int received = 0;
    for (const LogRow& r : rows)
    {
        if (r.RxNodeId == 2)
            ++received;
        EXPECT_FALSE(IsRxOverwrite(r)) << "受信処理遅延 0 なのに受信側で捨てられた";
    }
    EXPECT_GT(received, 0) << "前提: 受信が無い";
}

// 根拠: spec.md US2 受け入れシナリオ 4、FR-009。その ECU の受信バッファ満杯による破棄回数（集計）が、
// ログの受信側 OVERWRITE 行の数と一致する。
TEST(F008Spec, F008_US2_4_RxDiscardCount_EqualsRxOverwriteRows)
{
    RecordProperty("spec", "008:US2-4");
    const Network net = BurstNetwork(12, 5000);
    const std::vector<LogRow> rows = RunSim(net, 100000LL);
    int overwriteRows = 0;
    for (const LogRow& r : rows)
        if (IsRxOverwrite(r) && r.RxNodeId == 2)
            ++overwriteRows;
    ASSERT_GT(overwriteRows, 0) << "前提: 受信側で捨てられたフレームが無い";
    const std::vector<GroupMetrics> metrics = ComputeMetrics(rows, net);
    const GroupMetrics* node2 = NodeGroup(metrics, 2);
    ASSERT_NE(node2, nullptr) << "ノード 2 の集計が無い";
    EXPECT_EQ(node2->RxDiscardCount, overwriteRows) << "受信側の破棄回数がログの OVERWRITE 行の数と違う";
}

// 根拠: spec.md US3 受け入れシナリオ 1、FR-008（Doc FR-051）。ログの最後の列の見出しは Update_Count。
TEST(F008Spec, F008_US3_1_LogHeader_LastColumnIsUpdateCount)
{
    RecordProperty("spec", "008:US3-1");
    const std::wstring path = TempPath(L"f008_us3_1.log.csv");
    ASSERT_TRUE(WriteLogCsv({}, path)) << "ログの CSV を書けない";
    const std::vector<std::string> lines = SplitLines(ReadText(path));
    ASSERT_FALSE(lines.empty()) << "ヘッダーが無い";
    const std::string& header = lines[0];
    const std::string last = header.substr(header.rfind(',') + 1);
    EXPECT_EQ(last, "Update_Count") << "最後の列の見出しが Update_Count でない";
}

// 根拠: spec.md SC-001。同時にそろう 20 フレームの構成で、要求がそろうたびに捨てられる送信要求はちょうど 4 件。
TEST(F008Spec, F008_SC001_EachAlignedRequest_ExactlyFourDropped)
{
    RecordProperty("spec", "008:SC-001");
    const std::vector<LogRow> rows = RunSim(TwentyFramesNetwork(), 1000000LL);
    std::map<long long, int> droppedAt;
    for (const LogRow& r : rows)
        if (IsTxOverwrite(r))
            ++droppedAt[r.TxRequestUs];
    ASSERT_EQ(droppedAt.size(), 10u) << "前提: 要求がそろう 10 回すべてで捨てられていない";
    for (const auto& kv : droppedAt)
        EXPECT_EQ(kv.second, 4) << kv.first << " µs: 捨てられた送信要求が 4 件でない";
}

namespace
{
    // 送信バッファと受信バッファの両方が満杯になる構成（16 × 2 ノードの送信、受信処理遅延 5 ms の受信）
    Network HeavyNetwork()
    {
        Network net;
        net.BitRateBps = 500000;
        std::vector<FrameDefinition> a, b;
        for (unsigned i = 0; i < 18; ++i)
            a.push_back(MakeFrame(0x100 + i, 8, 50000));
        for (unsigned i = 0; i < 18; ++i)
            b.push_back(MakeFrame(0x200 + i, 8, 50000, 1000));
        net.Nodes.push_back(MakeNode(1, a));
        net.Nodes.push_back(MakeNode(2, b));
        Node slow = MakeNode(3);
        slow.ReceiveDelayUs = 5000;
        net.Nodes.push_back(slow);
        net.Nodes.push_back(MakeNode(4));
        return net;
    }
}

// 根拠: spec.md SC-002、FR-001。どの時点でも、各 ECU の送信待ち（送信中のものを含む）は 16 本以下、受信バッファは 8 本以下。
// 送信待ちは「送信要求 ≦ t < 送信完了」、受信バッファは「受信完了 ≦ t < 受信処理」（FR-007: 同じ時刻なら取り出しが先）として、
// ログの各時刻で数える。
TEST(F008Spec, F008_SC002_AnyTime_TxAtMost16RxAtMost8)
{
    RecordProperty("spec", "008:SC-002");
    const std::vector<LogRow> rows = RunSim(HeavyNetwork(), 300000LL);
    std::set<long long> times;
    for (const LogRow& r : rows)
    {
        if (r.Status == kStatusOverwrite)
            continue;
        times.insert(r.TxRequestUs);
        times.insert(r.RxEndUs);
    }
    for (long long t : times)
    {
        std::map<int, std::set<std::pair<unsigned, long long>>> txWaiting; // ノード -> (ID, 要求時刻)
        std::map<int, int> rxHeld;
        for (const LogRow& r : rows)
        {
            if (r.Status == kStatusOverwrite)
                continue;
            if (r.TxRequestUs <= t && t < r.TxEndUs)
                txWaiting[r.TxNodeId].insert({ r.CanId, r.TxRequestUs });
            if (r.RxNodeId > 0 && r.RxEndUs <= t && t < r.RxProcessUs)
                ++rxHeld[r.RxNodeId];
        }
        for (const auto& kv : txWaiting)
            EXPECT_LE(kv.second.size(), 16u) << t << " µs: ノード " << kv.first << " の送信待ちが 16 本を超えた";
        for (const auto& kv : rxHeld)
            EXPECT_LE(kv.second, 8) << t << " µs: ノード " << kv.first << " の受信バッファが 8 本を超えた";
    }
}

// 根拠: spec.md SC-003、FR-009。集計の ECU ごとの送信側・受信側の破棄回数が、ログの OVERWRITE 行の数と全 ECU で一致する。
TEST(F008Spec, F008_SC003_AllEcus_DiscardCountsMatchOverwriteRows)
{
    RecordProperty("spec", "008:SC-003");
    const Network net = HeavyNetwork();
    const std::vector<LogRow> rows = RunSim(net, 300000LL);
    std::map<int, int> txDrops, rxDrops;
    for (const LogRow& r : rows)
    {
        if (IsTxOverwrite(r)) ++txDrops[r.TxNodeId];
        if (IsRxOverwrite(r)) ++rxDrops[r.RxNodeId];
    }
    ASSERT_FALSE(txDrops.empty()) << "前提: 送信側の破棄が無い";
    ASSERT_FALSE(rxDrops.empty()) << "前提: 受信側の破棄が無い";
    const std::vector<GroupMetrics> metrics = ComputeMetrics(rows, net);
    for (int nodeId = 1; nodeId <= 4; ++nodeId)
    {
        // 受信だけで破棄も無い ECU（ノード 4）には、集計のノードの行が出ない。行が無い ECU は破棄 0 として比べる
        const GroupMetrics* g = NodeGroup(metrics, nodeId);
        const int txCount = g ? g->TxDiscardCount : 0;
        const int rxCount = g ? g->RxDiscardCount : 0;
        EXPECT_EQ(txCount, txDrops[nodeId]) << "ノード " << nodeId << " の送信側の破棄回数";
        EXPECT_EQ(rxCount, rxDrops[nodeId]) << "ノード " << nodeId << " の受信側の破棄回数";
    }
}

// 根拠: spec.md SC-004。受信処理遅延 0 の構成（送信バッファは満杯になる）では、受信側の OVERWRITE が 0 件。
TEST(F008Spec, F008_SC004_ZeroDelayConfig_NoRxOverwrite)
{
    RecordProperty("spec", "008:SC-004");
    Network net = HeavyNetwork();
    net.Nodes[2].ReceiveDelayUs = 0;
    const std::vector<LogRow> rows = RunSim(net, 300000LL);
    int rxOverwrite = 0, txOverwrite = 0;
    for (const LogRow& r : rows)
    {
        if (IsRxOverwrite(r)) ++rxOverwrite;
        if (IsTxOverwrite(r)) ++txOverwrite;
    }
    EXPECT_GT(txOverwrite, 0) << "前提: 送信側の破棄が起きていない";
    EXPECT_EQ(rxOverwrite, 0) << "受信処理遅延 0 なのに受信側の OVERWRITE がある";
}

// 根拠: spec.md SC-006。同じ構成を 2 回実行したとき、OVERWRITE の行を含めてログが完全に一致する。
TEST(F008Spec, F008_SC006_OverwriteConfigTwice_IdenticalLog)
{
    RecordProperty("spec", "008:SC-006");
    const std::vector<LogRow> a = RunSim(HeavyNetwork(), 300000LL);
    const std::vector<LogRow> b = RunSim(HeavyNetwork(), 300000LL);
    int overwrites = 0;
    for (const LogRow& r : a)
        if (r.Status == kStatusOverwrite)
            ++overwrites;
    ASSERT_GT(overwrites, 0) << "前提: OVERWRITE の行が無い";
    ASSERT_EQ(a.size(), b.size()) << "2 回の実行で行数が違う";
    for (std::size_t i = 0; i < a.size(); ++i)
        EXPECT_TRUE(SameRow(a[i], b[i])) << i << " 行目が違う";
}
