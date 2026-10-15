// feature_002_spec_tests.cpp（tests/unit）
// 機能 002（specs/002-node-startup-control/spec.md）の受け入れシナリオと成功基準のテスト。
// 期待値は spec.md と StartupControl.h の約束（同期成立 = max(検出開始, レセシブ期間の始まり) + 11 ビット時間、
// 送信のレセシブ期間の始まり = 送信完了 − 8 ビット時間）から手計算で書く。

#include "support/spec_test_helpers.h"

#include "StartupControl.h"

#include <map>

using namespace CanSim;
using namespace spectest;

namespace
{
    std::map<int, StartupRecord> RecordsById(const Network& after)
    {
        std::map<int, StartupRecord> out;
        for (const StartupRecord& r : BuildStartupRecords(after))
            out[r.NodeId] = r;
        return out;
    }

    Node StartupNode(int nodeId, long long powerOnUs, long long initUs,
        const std::vector<FrameDefinition>& frames = {})
    {
        Node n = MakeNode(nodeId, frames);
        n.PowerOnTimeUs = powerOnUs;
        n.InitDurationUs = initUs;
        return n;
    }
}

// 根拠: spec.md US1 受け入れシナリオ 1、FR-011・FR-012。6 ノードの電源投入時刻を 0〜50 ms にずらすと、各ノードが
// 電源投入・初期化完了・通常動作開始の 3 つの時刻とともに記録され、通常動作開始時刻は電源投入時刻の順に並ぶ。
TEST(F002Spec, F002_US1_1_StaggeredPowerOn_RecordedInPowerOnOrder)
{
    RecordProperty("spec", "002:US1-1");
    Network net;
    net.BitRateBps = 500000;
    for (int i = 0; i < 6; ++i)
        net.Nodes.push_back(StartupNode(i + 1, i * 10000LL, 0, { MakeFrame(0x100 + i, 8, 100000) }));
    Network after;
    RunSim(net, 200000LL, &after);
    const std::vector<StartupRecord> records = BuildStartupRecords(after);
    ASSERT_EQ(records.size(), 6u) << "起動記録が 6 件でない";
    for (int i = 0; i < 6; ++i)
    {
        EXPECT_EQ(records[i].NodeId, i + 1) << "起動記録の並び";
        EXPECT_EQ(records[i].PowerOnUs, i * 10000LL) << "ノード " << i + 1 << " の電源投入時刻";
        EXPECT_GE(records[i].InitDoneUs, records[i].PowerOnUs) << "ノード " << i + 1 << " の初期化完了時刻";
        EXPECT_TRUE(records[i].ReachedNormal) << "ノード " << i + 1 << " が通常動作に入っていない";
        if (i > 0)
            EXPECT_LT(records[i - 1].NormalEntryUs, records[i].NormalEntryUs)
                << "通常動作開始時刻が電源投入時刻の順に並んでいない";
    }
}

// 根拠: spec.md US1 受け入れシナリオ 2、FR-001・FR-005。電源投入時刻・初期化時間が既定値（0）の構成では、全ノードが
// 同じ時刻に通常動作へ入り、その時刻はバスが空で同期が成立するのに要する 11 ビット時間（500 kbps で 22 µs）。
TEST(F002Spec, F002_US1_2_Defaults_AllNodesNormalAt11BitTimes)
{
    RecordProperty("spec", "002:US1-2");
    Network net;
    net.BitRateBps = 500000;
    for (int i = 0; i < 4; ++i)
        net.Nodes.push_back(MakeNode(i + 1, { MakeFrame(0x100 + i, 8, 10000) }));
    Network after;
    RunSim(net, 100000LL, &after);
    for (const Node& n : after.Nodes)
        EXPECT_EQ(n.NormalEntryUs, 22) << "ノード " << n.NodeId << " の通常動作開始時刻が 22 µs でない";
}

// 根拠: spec.md US1 受け入れシナリオ 3、FR-008・FR-013。シミュレーション時間より後に電源投入されるノードは、通常動作に
// 到達しなかったことがわかる形で記録され、そのノードのフレームは 1 件も送信されない。
TEST(F002Spec, F002_US1_3_PowerOnAfterEnd_NotReachedAndNoTransmission)
{
    RecordProperty("spec", "002:US1-3");
    Network net;
    net.BitRateBps = 500000;
    net.Nodes.push_back(MakeNode(1, { MakeFrame(0x100, 8, 10000) }));
    net.Nodes.push_back(StartupNode(2, 2000000, 0, { MakeFrame(0x080, 8, 10000) }));
    Network after;
    const std::vector<LogRow> rows = RunSim(net, 1000000LL, &after);
    const std::map<int, StartupRecord> rec = RecordsById(after);
    ASSERT_EQ(rec.count(2), 1u) << "ノード 2 の起動記録が無い";
    EXPECT_FALSE(rec.at(2).ReachedNormal) << "通常動作に到達しなかったことが記録されていない";
    EXPECT_LT(rec.at(2).NormalEntryUs, 0) << "到達していないのに通常動作開始時刻がある";
    for (const LogRow& r : rows)
        EXPECT_NE(r.TxNodeId, 2) << "電源投入前のノード 2 のフレームが送信された";
}

// 根拠: spec.md US2 受け入れシナリオ 1、FR-008。電源投入時刻 50 ms のノードの、初回オフセット 0・周期 10 ms のフレームは、
// 通常動作開始（バスが空なので 50 ms + 22 µs = 50,022 µs）より前には送信されず、最初の送信は通常動作開始の後。
TEST(F002Spec, F002_US2_1_LatePowerOn_FirstSendAfterNormalEntry)
{
    RecordProperty("spec", "002:US2-1");
    Network net;
    net.BitRateBps = 500000;
    net.Nodes.push_back(StartupNode(1, 50000, 0, { MakeFrame(0x100, 8, 10000) }));
    net.Nodes.push_back(MakeNode(2));
    Network after;
    const std::vector<LogRow> rows = RunSim(net, 200000LL, &after);
    EXPECT_EQ(after.Nodes[0].NormalEntryUs, 50022) << "通常動作開始時刻";
    const std::vector<LogRow> tx = Transmissions(rows, 0x100);
    ASSERT_FALSE(tx.empty()) << "送信が無い";
    for (const LogRow& r : tx)
        EXPECT_GE(r.TxStartUs, 50022) << "通常動作開始より前に送信された";
}

// 根拠: spec.md US2 受け入れシナリオ 2、FR-010。起動前に発生した送信要求は保持され、最初の送信のログ行の送信要求時刻は、
// 最初に要求が発生した時刻（初回オフセット 0 の時点 = 0 µs）のまま記録され、送信待ち遅延に起動待ち時間が含まれる。
TEST(F002Spec, F002_US2_2_HeldRequest_KeepsFirstRequestTime)
{
    RecordProperty("spec", "002:US2-2");
    GTEST_SKIP() << "仕様と実装の食い違い（人の判断待ち）: 002 は起動前の要求を最初の要求時刻（0 µs）のまま保持するとするが、"
                    "実装は機能 006 FR-015 のとおり通常動作開始（50,022 µs）を送信要求の起点にしている"
                    "（送信要求時刻 50,022 µs、送信待ち遅延 0）。002 の US2・FR-010 が 006 で置き換わったかの判断が必要";
    Network net;
    net.BitRateBps = 500000;
    net.Nodes.push_back(StartupNode(1, 50000, 0, { MakeFrame(0x100, 8, 10000) }));
    net.Nodes.push_back(MakeNode(2));
    const std::vector<LogRow> rows = RunSim(net, 200000LL);
    const std::vector<LogRow> tx = Transmissions(rows, 0x100);
    ASSERT_FALSE(tx.empty()) << "送信が無い";
    EXPECT_EQ(tx.front().TxRequestUs, 0) << "最初の送信の送信要求時刻が、最初の要求（0 µs）のままでない";
    EXPECT_GE(tx.front().TxStartUs - tx.front().TxRequestUs, 50022) << "送信待ち遅延に起動待ち時間が含まれていない";
}

// 根拠: spec.md US3 受け入れシナリオ 1、FR-006。ノード A の送信中にノード B が同期検出を始めると、B はそのフレームの
// フレーム間休止期間の終わりに通常動作に入る。手計算（500 kbps、ビット 2 µs）: A は 22 µs に通常動作に入り、0x100・DLC 8
// （246 µs）を 22〜268 µs に送る。休止期間の終わりは 274 µs。B は 100 µs に電源投入（初期化 0）して検出を始める。
// レセシブ期間の始まりは 268 − 16 = 252 µs で、max(100, 252) + 22 = 274 µs。
TEST(F002Spec, F002_US3_1_LateJoinDuringFrame_NormalAtIntermissionEnd)
{
    RecordProperty("spec", "002:US3-1");
    Network net;
    net.BitRateBps = 500000;
    net.Nodes.push_back(MakeNode(1, { MakeFrame(0x100, 8, 1000) }));
    net.Nodes.push_back(StartupNode(2, 100, 0));
    Network after;
    RunSim(net, 5000LL, &after);
    EXPECT_EQ(after.Nodes[1].NormalEntryUs, 274) << "B の通常動作開始が A のフレームの休止期間の終わり（274 µs）でない";
}

// 根拠: spec.md US3 受け入れシナリオ 2、FR-007。B の検出開始が、進行中のフレームの残りのレセシブ期間が 11 ビットに
// 満たない時点なら、その機会では同期せず、次に条件を満たす機会（ここではバスが十分な時間空いた時点）で通常動作に入る。
// 手計算: A のフレームは 22〜268 µs、レセシブ期間の始まりは 252 µs。B は 260 µs に検出を始める（252 µs より後）。
// 休止期間の終わり 274 µs では足りず、次の A の送信（1,022 µs）まではバスが空いているので、260 + 22 = 282 µs。
TEST(F002Spec, F002_US3_2_TooLateInFrame_SyncAtNextOpportunity)
{
    RecordProperty("spec", "002:US3-2");
    Network net;
    net.BitRateBps = 500000;
    net.Nodes.push_back(MakeNode(1, { MakeFrame(0x100, 8, 1000) }));
    net.Nodes.push_back(StartupNode(2, 260, 0));
    Network after;
    RunSim(net, 5000LL, &after);
    EXPECT_EQ(after.Nodes[1].NormalEntryUs, 282) << "B の通常動作開始が次の機会（282 µs）でない";
}

// 根拠: spec.md SC-001。電源投入時刻をずらした構成で、各ノードの通常動作開始時刻が
// 「電源投入時刻 + 初期化時間 + 同期成立までの時間」の手計算値と一致する（500 kbps、ノード A は周期 1 ms で送信し続ける）。
//   A: 電源 0、初期化 0                          → 22 µs（バスが空）
//   B: 電源 100、初期化 0 → 検出 100（A の 22〜268 µs のフレーム中）→ 268 + 6 = 274 µs
//   C: 電源 2,000、初期化 100 → 検出 2,100（A の 2,022〜2,268 µs のフレーム中）→ 2,268 + 6 = 2,274 µs
//   D: 電源 5,500、初期化 0 → 検出 5,500（バスは 5,268 µs の送信完了から空き、レセシブの始まりは 5,252）→ 5,500 + 22 = 5,522 µs
TEST(F002Spec, F002_SC001_StaggeredStartup_MatchesHandCalc)
{
    RecordProperty("spec", "002:SC-001");
    Network net;
    net.BitRateBps = 500000;
    net.Nodes.push_back(MakeNode(1, { MakeFrame(0x100, 8, 1000) }));
    net.Nodes.push_back(StartupNode(2, 100, 0));
    net.Nodes.push_back(StartupNode(3, 2000, 100));
    net.Nodes.push_back(StartupNode(4, 5500, 0));
    Network after;
    RunSim(net, 10000LL, &after);
    EXPECT_EQ(after.Nodes[0].NormalEntryUs, 22) << "A";
    EXPECT_EQ(after.Nodes[1].NormalEntryUs, 274) << "B";
    EXPECT_EQ(after.Nodes[2].NormalEntryUs, 2274) << "C";
    EXPECT_EQ(after.Nodes[3].NormalEntryUs, 5522) << "D";
}

// 根拠: spec.md SC-002、FR-008。フレームログのすべての行で、送信開始時刻がその送信ノードの通常動作開始時刻以降。
TEST(F002Spec, F002_SC002_AllRows_TxStartNotBeforeNormalEntry)
{
    RecordProperty("spec", "002:SC-002");
    Network net;
    net.BitRateBps = 500000;
    net.Nodes.push_back(MakeNode(1, { MakeFrame(0x100, 8, 2000) }));
    net.Nodes.push_back(StartupNode(2, 3000, 500, { MakeFrame(0x080, 8, 1000) }));
    net.Nodes.push_back(StartupNode(3, 7100, 0, { MakeFrame(0x200, 4, 1500, 300) }));
    net.Nodes.push_back(StartupNode(4, 40000, 2000, { MakeFrame(0x050, 0, 3000) }));
    Network after;
    const std::vector<LogRow> rows = RunSim(net, 100000LL, &after);
    std::map<int, long long> entry;
    for (const Node& n : after.Nodes)
        entry[n.NodeId] = n.NormalEntryUs;
    ASSERT_FALSE(rows.empty()) << "前提: ログが空";
    for (const LogRow& r : rows)
    {
        if (r.Status == kStatusOverwrite)
            continue;
        EXPECT_GE(r.TxStartUs, entry[r.TxNodeId])
            << "ノード " << r.TxNodeId << " が通常動作開始（" << entry[r.TxNodeId] << " µs）より前に送信した";
    }
}

// 根拠: spec.md SC-003、FR-003〜FR-005。バスが常に空の構成（送信フレームなし）では、通常動作開始時刻が
// 「電源投入時刻 + 初期化時間 + 11 ビット時間（500 kbps で 22 µs）」と一致する。
TEST(F002Spec, F002_SC003_IdleBus_NormalAtPowerOnPlusInitPlus11Bits)
{
    RecordProperty("spec", "002:SC-003");
    Network net;
    net.BitRateBps = 500000;
    net.Nodes.push_back(StartupNode(1, 0, 0));
    net.Nodes.push_back(StartupNode(2, 1000, 250));
    net.Nodes.push_back(StartupNode(3, 40000, 5000));
    Network after;
    RunSim(net, 100000LL, &after);
    EXPECT_EQ(after.Nodes[0].NormalEntryUs, 22) << "電源 0・初期化 0";
    EXPECT_EQ(after.Nodes[1].NormalEntryUs, 1272) << "電源 1,000・初期化 250";
    EXPECT_EQ(after.Nodes[2].NormalEntryUs, 45022) << "電源 40,000・初期化 5,000";
    EXPECT_EQ(after.Nodes[1].InitDoneUs, 1250) << "初期化完了時刻 = 電源投入 + 初期化時間";
}

// 根拠: spec.md SC-004、FR-006。他ノードの送信中に同期検出を始めたノードの通常動作開始時刻が、進行中のフレームの
// フレーム間休止期間の終わりと一致する。250 kbps（ビット 4 µs）: A は 44 µs に通常動作に入り、0x100・DLC 8（123 ビット = 492 µs）を
// 44〜536 µs に送る。休止期間（12 µs）の終わりは 548 µs。B は 200 µs に検出を始める（レセシブの始まり 536 − 32 = 504 µs より前）。
TEST(F002Spec, F002_SC004_LateJoin250k_NormalAtIntermissionEnd)
{
    RecordProperty("spec", "002:SC-004");
    Network net;
    net.BitRateBps = 250000;
    net.Nodes.push_back(MakeNode(1, { MakeFrame(0x100, 8, 2000) }));
    net.Nodes.push_back(StartupNode(2, 200, 0));
    Network after;
    RunSim(net, 5000LL, &after);
    EXPECT_EQ(after.Nodes[0].NormalEntryUs, 44) << "前提: A の通常動作開始";
    EXPECT_EQ(after.Nodes[1].NormalEntryUs, 548) << "B の通常動作開始が A のフレームの休止期間の終わりでない";
}

// 根拠: spec.md SC-005、FR-014。同一構成で 2 回実行したとき、起動記録とフレームログの両方が完全に一致する。
TEST(F002Spec, F002_SC005_StartupConfigTwice_IdenticalRecordsAndLog)
{
    RecordProperty("spec", "002:SC-005");
    Network net;
    net.BitRateBps = 500000;
    net.Nodes.push_back(MakeNode(1, { MakeFrame(0x100, 8, 1000) }));
    net.Nodes.push_back(StartupNode(2, 150, 300, { MakeFrame(0x080, 8, 2000) }));
    net.Nodes.push_back(StartupNode(3, 2600, 0, { MakeFrame(0x200, 2, 700) }));
    Network a1, a2;
    const std::vector<LogRow> a = RunSim(net, 50000LL, &a1);
    const std::vector<LogRow> b = RunSim(net, 50000LL, &a2);
    ASSERT_EQ(a.size(), b.size()) << "フレームログの行数が違う";
    for (std::size_t i = 0; i < a.size(); ++i)
        EXPECT_TRUE(SameRow(a[i], b[i])) << "フレームログの " << i << " 行目が違う";
    const std::vector<StartupRecord> ra = BuildStartupRecords(a1);
    const std::vector<StartupRecord> rb = BuildStartupRecords(a2);
    ASSERT_EQ(ra.size(), rb.size()) << "起動記録の数が違う";
    for (std::size_t i = 0; i < ra.size(); ++i)
        EXPECT_TRUE(ra[i].NodeId == rb[i].NodeId && ra[i].PowerOnUs == rb[i].PowerOnUs &&
                    ra[i].InitDoneUs == rb[i].InitDoneUs && ra[i].NormalEntryUs == rb[i].NormalEntryUs &&
                    ra[i].ReachedNormal == rb[i].ReachedNormal)
            << "起動記録の " << i << " 番目が違う";
}
