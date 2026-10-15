// feature_002_edge_tests.cpp（tests/unit）
// 機能 002 の補足のテスト（/coverage）。受け入れシナリオ・成功基準とは別に、spec.md の FR と contracts/ に根拠がある動きを確かめる。

#include "support/spec_test_helpers.h"

#include "StartupControl.h"

using namespace CanSim;
using namespace spectest;

namespace
{
    Node StartupNode(int nodeId, long long powerOnUs, long long initUs, const std::vector<FrameDefinition>& frames = {})
    {
        Node n = MakeNode(nodeId, frames);
        n.PowerOnTimeUs = powerOnUs;
        n.InitDurationUs = initUs;
        return n;
    }
}

// 根拠: spec.md FR-007。同期の成立に必要な連続レセシブが途中で途切れた（他のノードの送信が始まった）ら、計数をやり直し、
// 次に条件を満たす機会まで同期しない。手計算（500 kbps）: A（0x100）は 22〜268 µs に送り、休止期間は 274 µs まで。
// B は 260 µs に検出を始め、282 µs に同期するはずだったが、C（0x080、初回オフセット 254 µs）が 276 µs に送り始めて途切れる。
// C の送信は 522 µs に終わり、レセシブの始まりは 522 − 16 = 506 µs。B の同期は max(260, 506) + 22 = 528 µs。
TEST(F002Edge, F002_FR007_RecessiveRunInterrupted_CountRestarts)
{
    RecordProperty("spec", "002:FR-007");
    Network net;
    net.BitRateBps = 500000;
    net.Nodes.push_back(MakeNode(1, { MakeFrame(0x100, 8, 1000) }));
    net.Nodes.push_back(StartupNode(2, 260, 0));
    net.Nodes.push_back(MakeNode(3, { MakeFrame(0x080, 8, 1000, 254) }));
    Network after;
    const std::vector<LogRow> rows = RunSim(net, 3000LL, &after);
    const LogRow* c = FirstRowOf(rows, 0x080);
    ASSERT_NE(c, nullptr);
    ASSERT_EQ(c->TxStartUs, 276) << "前提: C の送信開始";
    EXPECT_EQ(after.Nodes[1].NormalEntryUs, 528) << "途切れた後、C のフレームの休止期間の終わりで同期していない";
}

// 根拠: contracts/config-input-format-delta.md（PowerOnTimeUs・InitDurationUs は 0 以上の整数）。範囲外・数でない値は読み込まない。
TEST(F002Edge, F002_FR001_InvalidStartupKeys_Rejected)
{
    RecordProperty("spec", "002:FR-001");
    for (const char* kv : { "PowerOnTimeUs=-1", "PowerOnTimeUs=abc", "InitDurationUs=-5", "InitDurationUs=1.5" })
    {
        const ConfigLoadResult r = LoadText(L"f002e_fr001.canprj", std::string("[Node1]\r\nNodeId=1\r\n") + kv + "\r\n");
        EXPECT_FALSE(r.Success) << kv << " が読み込めてしまった";
        EXPECT_FALSE(r.ErrorMessage.empty()) << kv << ": 理由が無い";
    }
}

// 根拠: contracts/startup-log-format.md。Node_Name はカンマを含む場合は二重引用符で囲む。未到達の Init_Done_us・Normal_Entry_us は空欄で、
// Reached_Normal は TRUE / FALSE。
TEST(F002Edge, F002_FR012_StartupCsv_QuotedNameAndBlankWhenNotReached)
{
    RecordProperty("spec", "002:FR-012");
    StartupRecord reached;
    reached.NodeId = 1;
    reached.NodeName = "ECU1, Engine";
    reached.PowerOnUs = 0;
    reached.InitDoneUs = 0;
    reached.NormalEntryUs = 22;
    reached.ReachedNormal = true;
    StartupRecord notReached;
    notReached.NodeId = 3;
    notReached.NodeName = "ECU3";
    notReached.PowerOnUs = 2000000;
    const std::wstring path = TempPath(L"f002e_fr012.startup.csv");
    ASSERT_TRUE(WriteStartupCsv({ reached, notReached }, path));
    const std::vector<std::string> lines = SplitLines(ReadText(path));
    ASSERT_EQ(lines.size(), 3u);
    EXPECT_EQ(lines[0], "Node_Id,Node_Name,Power_On_us,Init_Done_us,Normal_Entry_us,Reached_Normal") << "ヘッダー";
    EXPECT_EQ(lines[1], "1,\"ECU1, Engine\",0,0,22,TRUE") << "カンマを含む名前が二重引用符で囲まれていない";
    EXPECT_EQ(lines[2], "3,ECU3,2000000,,,FALSE") << "未到達の時刻が空欄になっていない";
}
