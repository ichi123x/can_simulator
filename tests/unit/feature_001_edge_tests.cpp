// feature_001_edge_tests.cpp（tests/unit）
// 機能 001 の補足のテスト（/coverage）。仕様の項目（受入シナリオ・成功基準）とは別に、spec.md の FR と
// contracts/（config-input-format.md・log-output-format.md）に根拠がある動きを確かめる。

// _wgetenv・_wfopen の非推奨の警告（C4996）を出さない
#define _CRT_SECURE_NO_WARNINGS

#include <gtest/gtest.h>

#include "ConfigLoader.h"
#include "Logger.h"
#include "MetricsAggregator.h"
#include "SimulationEngine.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>
#include <string>
#include <vector>

using namespace CanSim;

namespace
{
    std::wstring TempPath(const std::wstring& name)
    {
        const wchar_t* temp = _wgetenv(L"TEMP");
        std::wstring dir = temp ? temp : L".";
        return dir + L"\\cansim_f001e_" + name;
    }

    void WriteText(const std::wstring& path, const std::string& text)
    {
        FILE* f = _wfopen(path.c_str(), L"wb");
        ASSERT_NE(f, nullptr) << "一時ファイルを作れない";
        fwrite(text.data(), 1, text.size(), f);
        fclose(f);
    }

    std::string ReadText(const std::wstring& path)
    {
        std::string text;
        FILE* f = _wfopen(path.c_str(), L"rb");
        if (!f)
            return text;
        char buf[4096];
        for (std::size_t n; (n = fread(buf, 1, sizeof(buf), f)) > 0;)
            text.append(buf, n);
        fclose(f);
        return text;
    }

    ConfigLoadResult LoadText(const std::wstring& name, const std::string& text)
    {
        const std::wstring path = TempPath(name);
        WriteText(path, text);
        return LoadConfig(path);
    }

    std::vector<LogRow> RunSim(const Network& network, long long durationUs)
    {
        SimulationEngine engine(network, durationUs);
        Logger logger;
        engine.Run(logger);
        return logger.Rows();
    }

    std::vector<std::string> SplitLines(const std::string& text)
    {
        std::vector<std::string> lines;
        std::size_t start = 0;
        while (start < text.size())
        {
            std::size_t end = text.find("\r\n", start);
            if (end == std::string::npos)
                end = text.size();
            lines.push_back(text.substr(start, end - start));
            start = end + 2;
        }
        return lines;
    }

    // 正しい 1 ノード・1 フレームの構成の [Node1.Frame1] より前の部分と、フレームの部分
    const std::string kHead = "[Network]\r\nBitRateBps=500000\r\n[Node1]\r\nNodeId=1\r\n";
    const std::string kFrame = "[Node1.Frame1]\r\nName=F1\r\nCanId=0x100\r\nDlc=8\r\nPeriodUs=10000\r\n";
}

// 根拠: contracts/config-input-format.md「フィールド定義」の検証ルールと「エラー時の振る舞い」
// （該当箇所とエラー理由を示して拒否する）。FR-001（NodeId）・FR-002（通信速度）・FR-003（フレーム）・
// FR-016（受信処理遅延は 0 以上の整数）。各ケースは、正しい構成のキーを 1 つだけ不正にしたもの。
struct InvalidFieldCase
{
    const char* Name;
    const char* Spec;
    std::string Text;
};

class F001InvalidField : public ::testing::TestWithParam<InvalidFieldCase>
{
};

TEST_P(F001InvalidField, F001_CFG_InvalidField_RejectedWithReason)
{
    const InvalidFieldCase& c = GetParam();
    RecordProperty("spec", c.Spec);
    const std::wstring file(c.Name, c.Name + std::strlen(c.Name));
    const ConfigLoadResult r = LoadText(file + L".canprj", c.Text);
    EXPECT_FALSE(r.Success) << c.Name << ": 不正な値の構成が読み込めてしまった";
    EXPECT_FALSE(r.ErrorMessage.empty()) << c.Name << ": 拒否の理由が示されていない";
}

INSTANTIATE_TEST_SUITE_P(
    Contract, F001InvalidField,
    ::testing::Values(
        // [Network] BitRateBps: 125000 / 250000 / 500000 / 1000000 のいずれか（FR-002）
        InvalidFieldCase{ "bitrate_not_number", "001:FR-002",
                          "[Network]\r\nBitRateBps=abc\r\n[Node1]\r\nNodeId=1\r\n" + kFrame },
        InvalidFieldCase{ "bitrate_300k", "001:FR-002",
                          "[Network]\r\nBitRateBps=300000\r\n[Node1]\r\nNodeId=1\r\n" + kFrame },
        // [NodeN] NodeId: 必須、1〜16、ファイル内で一意（FR-001）
        InvalidFieldCase{ "nodeid_missing", "001:FR-001",
                          "[Network]\r\nBitRateBps=500000\r\n[Node1]\r\nName=X\r\n" + kFrame },
        InvalidFieldCase{ "nodeid_0", "001:FR-001",
                          "[Network]\r\nBitRateBps=500000\r\n[Node1]\r\nNodeId=0\r\n" + kFrame },
        InvalidFieldCase{ "nodeid_17", "001:FR-001",
                          "[Network]\r\nBitRateBps=500000\r\n[Node1]\r\nNodeId=17\r\n" + kFrame },
        InvalidFieldCase{ "nodeid_duplicate", "001:FR-001",
                          kHead + kFrame + "[Node2]\r\nNodeId=1\r\n" },
        // [NodeN] ReceiveDelayUs: 0 以上の整数（FR-016）
        InvalidFieldCase{ "receive_delay_negative", "001:FR-016",
                          kHead + "ReceiveDelayUs=-5\r\n" + kFrame },
        // [NodeN] RxFilter: 16 進 ID（0x000〜0x7FF）のカンマ区切り。範囲外の ID があれば読み込みエラー（FR-005）
        InvalidFieldCase{ "rxfilter_0x800", "001:FR-005",
                          kHead + "RxFilter=0x080,0x800\r\n" + kFrame },
        // [NodeN.FrameM] CanId: 必須、0x000〜0x7FF（FR-003）
        InvalidFieldCase{ "canid_missing", "001:FR-003",
                          kHead + "[Node1.Frame1]\r\nName=F1\r\nDlc=8\r\nPeriodUs=10000\r\n" },
        InvalidFieldCase{ "canid_0x800", "001:FR-003",
                          kHead + "[Node1.Frame1]\r\nName=F1\r\nCanId=0x800\r\nDlc=8\r\nPeriodUs=10000\r\n" },
        // [NodeN.FrameM] Dlc: 必須、0〜8（FR-003）
        InvalidFieldCase{ "dlc_9", "001:FR-003",
                          kHead + "[Node1.Frame1]\r\nName=F1\r\nCanId=0x100\r\nDlc=9\r\nPeriodUs=10000\r\n" },
        // [NodeN.FrameM] PeriodUs: 必須、正の整数（FR-003）
        InvalidFieldCase{ "period_0", "001:FR-003",
                          kHead + "[Node1.Frame1]\r\nName=F1\r\nCanId=0x100\r\nDlc=8\r\nPeriodUs=0\r\n" },
        // [NodeN.FrameM] InitialOffsetUs: 0 以上の整数（FR-003）
        InvalidFieldCase{ "offset_negative", "001:FR-003",
                          kHead + "[Node1.Frame1]\r\nName=F1\r\nCanId=0x100\r\nDlc=8\r\nPeriodUs=10000\r\n"
                                  "InitialOffsetUs=-1\r\n" }),
    [](const ::testing::TestParamInfo<InvalidFieldCase>& info) { return std::string(info.param.Name); });

// 根拠: contracts/config-input-format.md「構文」「フィールド定義」、FR-002・FR-005・FR-016。
// 省略できるキーの既定値（BitRateBps 500000、RxFilter 省略時は全 ID 受信、ReceiveDelayUs 0、InitialOffsetUs 0）と、
// 構文の例のとおりの空行・「;」から行末のコメントを受け付け、RxFilter は 16 進 ID のカンマ区切りで読む。
TEST(F001Edge, F001_FR005_ContractExample_DefaultsCommentsAndRxFilter)
{
    RecordProperty("spec", "001:FR-005");
    const std::string text =
        "[Network]\r\n"
        "\r\n"
        "[Node1]\r\n"
        "NodeId=1\r\n"
        "Name=ECU1\r\n"
        "RxFilter=0x080, 0x081        ; 省略時は全ID受信\r\n"
        "ReceiveDelayUs=40             ; 省略時は既定値0（FR-016）\r\n"
        "\r\n"
        "[Node1.Frame1]\r\n"
        "CanId=0x100\r\n"
        "Dlc=8\r\n"
        "PeriodUs=10000\r\n"
        "   \r\n"
        "[Node2]\r\n"
        "NodeId=2\r\n";
    const ConfigLoadResult r = LoadText(L"contract_example.canprj", text);
    ASSERT_TRUE(r.Success) << "契約の例の形の構成が読み込めない: " << r.ErrorMessage;
    EXPECT_EQ(r.Config.BitRateBps, 500000) << "BitRateBps を省略したときの既定値が 500000 でない";
    ASSERT_EQ(r.Config.Nodes.size(), 2u) << "ノード数";

    const Node& n1 = r.Config.Nodes[0];
    EXPECT_FALSE(n1.RxFilterAll) << "RxFilter を指定したのに全 ID 受信になっている";
    EXPECT_EQ(n1.RxFilter, (std::set<unsigned>{ 0x080, 0x081 })) << "RxFilter の ID が違う";
    EXPECT_EQ(n1.ReceiveDelayUs, 40) << "ReceiveDelayUs が読めていない";
    ASSERT_EQ(n1.Frames.size(), 1u) << "フレーム数";
    EXPECT_EQ(n1.Frames[0].InitialOffsetUs, 0) << "InitialOffsetUs を省略したときの既定値が 0 でない";

    const Node& n2 = r.Config.Nodes[1];
    EXPECT_TRUE(n2.RxFilterAll) << "RxFilter を省略したのに全 ID 受信になっていない";
    EXPECT_EQ(n2.ReceiveDelayUs, 0) << "ReceiveDelayUs を省略したときの既定値が 0 でない";
}

// 根拠: contracts/config-input-format.md の RxFilter（16 進 ID 0x000〜0x7FF のカンマ区切り）、FR-005。
// 範囲の両端の 0x000 と 0x7FF は受け付ける。
TEST(F001Edge, F001_FR005_RxFilterBounds_0x000And0x7FFAccepted)
{
    RecordProperty("spec", "001:FR-005");
    const ConfigLoadResult r = LoadText(L"rxfilter_bounds.canprj", kHead + "RxFilter=0x000,0x7FF\r\n" + kFrame);
    ASSERT_TRUE(r.Success) << "RxFilter の範囲の両端が読み込めない: " << r.ErrorMessage;
    ASSERT_EQ(r.Config.Nodes.size(), 1u) << "ノード数";
    EXPECT_EQ(r.Config.Nodes[0].RxFilter, (std::set<unsigned>{ 0x000, 0x7FF })) << "RxFilter の ID が違う";
}

// 根拠: contracts/log-output-format.md「集計サマリ出力」（受信ノードのないフレームだけのグループでは、
// 総送信レイテンシの件数を 0 とし、最小・最大・平均は 0 を出す）、FR-017（受信ノードがないフレームは送信側の情報のみ）。
// 送信ノード 1 つだけの構成では、0x080 のフレームに受信ノードがない。
TEST(F001Edge, F001_FR018_NoReceiver_TotalLatencyZeroCount)
{
    RecordProperty("spec", "001:FR-018");
    Network net;
    net.BitRateBps = 500000;
    Node n;
    n.NodeId = 1;
    FrameDefinition f;
    f.CanId = 0x080;
    f.Dlc = 8;
    f.Data.assign(8, 0xFF);
    f.PeriodUs = 10000;
    n.Frames.push_back(f);
    net.Nodes.push_back(n);

    const std::vector<LogRow> rows = RunSim(net, 100000LL);
    ASSERT_FALSE(rows.empty()) << "前提: 送信が無い";
    const std::vector<GroupMetrics> metrics = ComputeMetrics(rows, net);
    const GroupMetrics* byId = nullptr;
    for (const GroupMetrics& g : metrics)
        if (g.GroupType == "ID" && g.GroupKey == "0x080")
            byId = &g;
    ASSERT_NE(byId, nullptr) << "ID ごとの集計（0x080）が無い";
    EXPECT_EQ(byId->TotalLatency.Count, 0) << "受信ノードが無いのに総送信レイテンシの件数が 0 でない";
    EXPECT_DOUBLE_EQ(byId->TotalLatency.MinUs, 0.0) << "総送信レイテンシの最小が 0 でない";
    EXPECT_DOUBLE_EQ(byId->TotalLatency.MaxUs, 0.0) << "総送信レイテンシの最大が 0 でない";
    EXPECT_DOUBLE_EQ(byId->TotalLatency.AvgUs, 0.0) << "総送信レイテンシの平均が 0 でない";
}

// 根拠: FR-005（受信フィルタに合致する ID だけを受信する）・FR-015・FR-016（受信処理時刻 = 受信完了時刻 +
// 受信処理遅延）。ノード 2 は 0x080 だけを受信し、受信処理遅延は 40 µs。ノード 3 はフィルタ・遅延なし。
TEST(F001Edge, F001_FR016_RxFilterAndReceiveDelay_AppliedInRun)
{
    RecordProperty("spec", "001:FR-016");
    const std::string text =
        "[Network]\r\nBitRateBps=500000\r\n"
        "[Node1]\r\nNodeId=1\r\n"
        "[Node1.Frame1]\r\nCanId=0x080\r\nDlc=8\r\nPeriodUs=10000\r\n"
        "[Node1.Frame2]\r\nCanId=0x100\r\nDlc=8\r\nPeriodUs=10000\r\nInitialOffsetUs=5000\r\n"
        "[Node2]\r\nNodeId=2\r\nRxFilter=0x080\r\nReceiveDelayUs=40\r\n"
        "[Node3]\r\nNodeId=3\r\n";
    const ConfigLoadResult r = LoadText(L"rxfilter_delay.canprj", text);
    ASSERT_TRUE(r.Success) << r.ErrorMessage;

    const std::vector<LogRow> rows = RunSim(r.Config, 100000LL);
    int rx080By2 = 0, rx100By3 = 0;
    for (const LogRow& row : rows)
    {
        if (row.CanId == 0x100)
            EXPECT_NE(row.RxNodeId, 2) << "受信フィルタに無い 0x100 をノード 2 が受信している";
        if (row.CanId == 0x100 && row.RxNodeId == 3)
            ++rx100By3;
        if (row.CanId == 0x080 && row.RxNodeId == 2)
        {
            ++rx080By2;
            EXPECT_EQ(row.RxProcessUs, row.RxEndUs + 40) << "受信処理時刻が 受信完了 + 40 µs でない";
        }
        if (row.RxNodeId == 3)
            EXPECT_EQ(row.RxProcessUs, row.RxEndUs) << "遅延 0 のノード 3 の受信処理時刻が受信完了と違う";
    }
    EXPECT_GT(rx080By2, 0) << "ノード 2 が 0x080 を受信していない";
    EXPECT_GT(rx100By3, 0) << "フィルタの無いノード 3 が 0x100 を受信していない";
}

// 根拠: FR-011・Edge Cases（送信待ちの間に同じフレームの次の周期の送信要求が来たら、データを最新値で上書きし、
// 上書き回数を記録する。最初の送信要求時刻は変えない）、FR-010（3 ビットの休止期間）。
// 手計算（500 kbps、ビット時間 2 µs）: ID 0x7FF・DLC 0 は 47 ビット = 94 µs（機能 031 spec.md US2 シナリオ 2）。
// 通常動作に入るのは 22 µs（11 ビット時間、機能 002）なので、周期 40 µs の送信要求は 22・62・102 µs。
// 1 回目は 22 µs に送信を始めて 116 µs に終わり、休止期間の終わりは 122 µs。その間の 62 µs の要求が送信待ちになり、
// 102 µs の要求でその送信待ちが上書きされる。2 回目の送信は 122 µs に始まり、送信要求時刻は最初の 62 µs、上書き回数は 1。
TEST(F001Edge, F001_FR011_RequestWhileWaiting_DataUpdatedFirstRequestKept)
{
    RecordProperty("spec", "001:FR-011");
    Network net;
    net.BitRateBps = 500000;
    Node n;
    n.NodeId = 1;
    FrameDefinition f;
    f.CanId = 0x7FF;
    f.Dlc = 0;
    f.PeriodUs = 40;
    n.Frames.push_back(f);
    net.Nodes.push_back(n);

    const std::vector<LogRow> rows = RunSim(net, 300LL);
    ASSERT_GE(rows.size(), 2u) << "前提: 送信が 2 回未満";
    EXPECT_EQ(rows[0].TxRequestUs, 22) << "1 回目の送信要求時刻";
    EXPECT_EQ(rows[0].TxStartUs, 22) << "1 回目の送信開始時刻";
    EXPECT_EQ(rows[0].UpdateCount, 0) << "1 回目の上書き回数";
    EXPECT_EQ(rows[1].TxRequestUs, 62) << "2 回目の送信要求時刻が、最初の要求（62 µs）のままでない";
    EXPECT_EQ(rows[1].TxStartUs, 122) << "2 回目の送信開始が、休止期間の終わり（122 µs）でない";
    EXPECT_EQ(rows[1].UpdateCount, 1) << "送信待ちの間の上書き回数が 1 でない";
}

// 根拠: SC-004 の式「(実行時間 − 送信開始可能になった時刻 − 初回オフセット) ÷ 送信周期」の切り上げ、FR-021。
// 実行時間 1 s、周期 2 s・初回オフセット 1.5 s のフレームは ceil((1000000 − 22 − 1500000) / 2000000) = 0 回。
TEST(F001Edge, F001_SC004_FirstRequestAfterEnd_NoTransmission)
{
    RecordProperty("spec", "001:SC-004");
    Network net;
    net.BitRateBps = 500000;
    Node n;
    n.NodeId = 1;
    FrameDefinition late;
    late.CanId = 0x100;
    late.Dlc = 8;
    late.Data.assign(8, 0xFF);
    late.PeriodUs = 2000000;
    late.InitialOffsetUs = 1500000;
    n.Frames.push_back(late);
    net.Nodes.push_back(n);

    const std::vector<LogRow> rows = RunSim(net, 1000000LL);
    EXPECT_TRUE(rows.empty()) << "初回の送信要求が実行時間より後のフレームが送信されている";
}

// 根拠: contracts/log-output-format.md「列定義（ヘッダ順）」と出力例、機能 008 による列名の変更（Update_Count）。
// 受信ノードのある行は契約の出力例の 1 行目と同じ値、受信ノードの無い行は Rx_Node と受信の時刻が空欄（FR-017）。
// CAN_ID は 16 進文字列（例: 0x080）、Bus_Load_Pct は小数第 1 位まで。Logger::WriteCsv と WriteLogCsv の両方で確かめる。
TEST(F001Edge, F001_FR017_LogCsv_HeaderAndRowsPerContract)
{
    RecordProperty("spec", "001:FR-017");
    LogRow withRx;
    withRx.TimestampUs = 10000290;
    withRx.CanId = 0x080;
    withRx.Dlc = 8;
    withRx.TxNodeId = 3;
    withRx.RxNodeId = 1;
    withRx.TxRequestUs = 10000000;
    withRx.TxStartUs = 10000000;
    withRx.TxEndUs = 10000250;
    withRx.RxStartUs = 10000000;
    withRx.RxEndUs = 10000250;
    withRx.RxProcessUs = 10000290;
    withRx.BusLoadPct = 8.1;
    withRx.Status = "SUCCESS";

    LogRow noRx;
    noRx.TimestampUs = 20000246;
    noRx.CanId = 0x100;
    noRx.Dlc = 8;
    noRx.TxNodeId = 1;
    noRx.RxNodeId = -1;
    noRx.TxRequestUs = 20000000;
    noRx.TxStartUs = 20000000;
    noRx.TxEndUs = 20000246;
    noRx.BusLoadPct = 12.34;
    noRx.Status = "ARBITRATION_LOST_RETRY";
    noRx.RetryCount = 2;
    noRx.UpdateCount = 1;

    Logger logger;
    logger.AddRow(withRx);
    logger.AddRow(noRx);
    const std::wstring path = TempPath(L"format.log.csv");
    ASSERT_TRUE(logger.WriteCsv(path)) << "ログの CSV を書けない";

    const std::vector<std::string> lines = SplitLines(ReadText(path));
    ASSERT_EQ(lines.size(), 3u) << "ヘッダー + 2 行になっていない";
    EXPECT_EQ(lines[0], "Timestamp_us,CAN_ID,DLC,Tx_Node,Rx_Node,Tx_Request,Tx_Start,Tx_End,Rx_Start,Rx_End,"
                        "Rx_Process,Bus_Load_Pct,Status,Retry_Count,Update_Count")
        << "ヘッダーが契約の列順と違う";
    EXPECT_EQ(lines[1], "10000290,0x080,8,3,1,10000000,10000000,10000250,10000000,10000250,10000290,8.1,SUCCESS,0,0")
        << "受信ノードのある行が契約の出力例と違う";
    EXPECT_EQ(lines[2], "20000246,0x100,8,1,,20000000,20000000,20000246,,,,12.3,ARBITRATION_LOST_RETRY,2,1")
        << "受信ノードの無い行で、Rx_Node と受信の時刻が空欄になっていない";
}

// 根拠: FR-017（1 フレーム × 受信ノードごとに 1 行）、contracts/log-output-format.md（ヘッダ行あり）。
// 行が多くても（書き出しのまとまり 4 MB を超える 60,000 行）、ヘッダー + 行の数だけの行になり、最後の行も欠けない。
TEST(F001Edge, F001_FR017_LogCsv_ManyRows_EveryRowWritten)
{
    RecordProperty("spec", "001:FR-017");
    std::vector<LogRow> rows;
    for (int i = 0; i < 60000; ++i)
    {
        LogRow r;
        r.TimestampUs = 1000000LL + i;
        r.CanId = 0x080;
        r.Dlc = 8;
        r.TxNodeId = 1;
        r.RxNodeId = 2;
        r.TxRequestUs = r.TxStartUs = r.RxStartUs = 1000000LL + i;
        r.TxEndUs = r.RxEndUs = r.RxProcessUs = 1000000LL + i;
        r.BusLoadPct = 50.0;
        r.Status = "SUCCESS";
        rows.push_back(r);
    }
    const std::wstring path = TempPath(L"many.log.csv");
    ASSERT_TRUE(WriteLogCsv(rows, path)) << "ログの CSV を書けない";
    const std::string text = ReadText(path);
    ASSERT_GT(text.size(), 4u * 1024 * 1024) << "前提: 4 MB を超える大きさになっていない";
    const std::vector<std::string> lines = SplitLines(text);
    ASSERT_EQ(lines.size(), 60001u) << "ヘッダー + 60,000 行になっていない";
    EXPECT_EQ(lines.back(), "1059999,0x080,8,1,2,1059999,1059999,1059999,1059999,1059999,1059999,50.0,SUCCESS,0,0")
        << "最後の行が欠けている";
}
