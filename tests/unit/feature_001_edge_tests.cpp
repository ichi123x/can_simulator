// 機能 001 の境界・補足テスト（/coverage で、仕様に根拠のある未カバー行に対して足したもの）
// 根拠は各テストの直前のコメントに書く。
// 起動直後は各ノードが 11 ビット時間のバス同期検出を経て通常動作に入る（specs/002-node-startup-control）。
// 500 kbps で 22 us、125 kbps で 88 us。送信要求の起点はその時刻（機能 006）。

#include <gtest/gtest.h>

#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

#include "CanTypes.h"
#include "ConfigLoader.h"
#include "Logger.h"
#include "MetricsAggregator.h"
#include "ProjectFile.h"
#include "SimulationEngine.h"

// 一時ファイルの作成・削除に使う（ConfigLoader が std::wstring のパスを取るため Windows 専用）
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

using namespace CanSim;

namespace
{
    std::wstring TempPath(const std::string& name)
    {
        wchar_t dir[MAX_PATH];
        GetTempPathW(MAX_PATH, dir);
        std::wstring path = dir;
        path += std::wstring(name.begin(), name.end());
        return path;
    }

    std::string ReadAll(const std::wstring& path)
    {
        std::ifstream in(path.c_str(), std::ios::binary);
        return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    }

    ConfigLoadResult LoadFromText(const std::string& name, const std::string& text)
    {
        const std::wstring path = TempPath(name);
        {
            std::ofstream out(path.c_str(), std::ios::binary);
            out << text;
        }
        ConfigLoadResult result = LoadConfig(path);
        DeleteFileW(path.c_str());
        return result;
    }

    std::vector<LogRow> RunNetwork(const Network& net, long long durationUs)
    {
        SimulationEngine engine(net, durationUs);
        Logger logger;
        engine.Run(logger);
        return logger.Rows();
    }

    std::vector<std::string> SplitLines(const std::string& text)
    {
        std::vector<std::string> lines;
        std::string line;
        std::istringstream in(text);
        while (std::getline(in, line))
        {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            lines.push_back(line);
        }
        return lines;
    }

    std::vector<std::string> SplitFields(const std::string& line)
    {
        std::vector<std::string> fields;
        std::string field;
        std::istringstream in(line);
        while (std::getline(in, field, ',')) fields.push_back(field);
        if (!line.empty() && line.back() == ',') fields.push_back("");
        return fields;
    }

    Node MakeNode(int nodeId, unsigned canId, unsigned dlc, long long periodUs)
    {
        Node node;
        node.NodeId = nodeId;
        FrameDefinition fd;
        fd.CanId = canId;
        fd.Dlc = dlc;
        fd.Data.assign(dlc, 0xFF);
        fd.PeriodUs = periodUs;
        node.Frames.push_back(fd);
        return node;
    }

    // 正しい 2 ノード構成の一部を差し替えて使う
    const char* const kNode1 = "[Node1]\nNodeId=1\n\n[Node1.Frame1]\nCanId=0x100\nDlc=8\nPeriodUs=10000\n\n";
}

// ---------------------------------------------------------------------------
// 設定ファイルの検証: 契約に反する設定は拒否される。
// 根拠: specs/001-can-simulation-core/contracts/config-input-format.md「フィールド定義」の必須・検証ルールと
//        「エラー時の振る舞い」（NodeId の重複・範囲外）。FR-002、FR-003。
// ---------------------------------------------------------------------------
struct RejectCase
{
    const char* Name;
    std::string Text;
};

class ConfigRejectTable : public ::testing::TestWithParam<RejectCase>
{
};

TEST_P(ConfigRejectTable, F001_CFG_InvalidField_Rejected)
{
    RecordProperty("spec", "001:FR-002");
    const RejectCase& c = GetParam();
    const ConfigLoadResult result = LoadFromText("f001_cfg_reject.canprj", c.Text);
    EXPECT_FALSE(result.Success) << c.Name << ": 契約に反する設定が受け入れられた";
    EXPECT_FALSE(result.ErrorMessage.empty()) << c.Name << ": 拒否理由が示されていない";
}

INSTANTIATE_TEST_SUITE_P(
    Contract, ConfigRejectTable,
    ::testing::Values(
        // BitRateBps は 125000 / 250000 / 500000 / 1000000 のいずれか（整数）
        RejectCase{"BitRate300000", std::string("[Network]\nBitRateBps=300000\n\n") + kNode1},
        RejectCase{"BitRateNotNumber", std::string("[Network]\nBitRateBps=fast\n\n") + kNode1},
        // NodeId は必須、1〜16、ファイル内で一意
        RejectCase{"NodeIdMissing", "[Node1]\nName=A\n"},
        RejectCase{"NodeId0", "[Node1]\nNodeId=0\n"},
        RejectCase{"NodeId17", "[Node1]\nNodeId=17\n"},
        RejectCase{"NodeIdDuplicate", "[Node1]\nNodeId=1\n\n[Node2]\nNodeId=1\n"},
        // CanId・Dlc・PeriodUs は必須
        RejectCase{"CanIdMissing", "[Node1]\nNodeId=1\n\n[Node1.Frame1]\nDlc=8\nPeriodUs=10000\n"},
        RejectCase{"DlcMissing", "[Node1]\nNodeId=1\n\n[Node1.Frame1]\nCanId=0x100\nPeriodUs=10000\n"},
        RejectCase{"PeriodMissing", "[Node1]\nNodeId=1\n\n[Node1.Frame1]\nCanId=0x100\nDlc=8\n"},
        // CanId は 0x000〜0x7FF
        RejectCase{"CanId0x800", "[Node1]\nNodeId=1\n\n[Node1.Frame1]\nCanId=0x800\nDlc=8\nPeriodUs=10000\n"},
        // Dlc は 0〜8
        RejectCase{"Dlc9", "[Node1]\nNodeId=1\n\n[Node1.Frame1]\nCanId=0x100\nDlc=9\nPeriodUs=10000\n"},
        // PeriodUs は正の整数
        RejectCase{"Period0", "[Node1]\nNodeId=1\n\n[Node1.Frame1]\nCanId=0x100\nDlc=8\nPeriodUs=0\n"},
        // InitialOffsetUs は 0 以上の整数
        RejectCase{"OffsetNegative",
                   "[Node1]\nNodeId=1\n\n[Node1.Frame1]\nCanId=0x100\nDlc=8\nPeriodUs=10000\nInitialOffsetUs=-5\n"},
        // ReceiveDelayUs・InitialOffsetUs は整数
        RejectCase{"ReceiveDelayNotNumber", "[Node1]\nNodeId=1\nReceiveDelayUs=slow\n"},
        RejectCase{"OffsetNotNumber",
                   "[Node1]\nNodeId=1\n\n[Node1.Frame1]\nCanId=0x100\nDlc=8\nPeriodUs=10000\nInitialOffsetUs=1ms\n"},
        // RxFilter は 16 進 ID（0x000〜0x7FF）のカンマ区切り
        RejectCase{"RxFilterOutOfRange", "[Node1]\nNodeId=1\nRxFilter=0x080,0x800\n"}),
    [](const ::testing::TestParamInfo<RejectCase>& info) { return std::string(info.param.Name); });

// ReceiveDelayUs は 0 以上の整数。負の値は拒否される。
// 根拠: contracts/config-input-format.md「フィールド定義」（ReceiveDelayUs: 0以上の整数）。
TEST(F001Edge, F001_CFG_NegativeReceiveDelay_Rejected)
{
    RecordProperty("spec", "001:FR-016");
    GTEST_SKIP() << "仕様と実装の食い違い（人の判断待ち）: ReceiveDelayUs=-1 が受け入れられる"
                    "（std::stoul が負の数を巻き戻して 4294967295 にする）";
    const ConfigLoadResult result =
        LoadFromText("f001_cfg_delay_negative.canprj", "[Node1]\nNodeId=1\nReceiveDelayUs=-1\n");
    EXPECT_FALSE(result.Success) << "負の受信処理遅延が受け入れられた";
    EXPECT_FALSE(result.ErrorMessage.empty()) << "拒否理由が示されていない";
}

// PeriodUs は正の整数。負の値は拒否される。
// 根拠: contracts/config-input-format.md「フィールド定義」（PeriodUs: 正の整数）。
TEST(F001Edge, F001_CFG_NegativePeriod_Rejected)
{
    RecordProperty("spec", "001:FR-003");
    GTEST_SKIP() << "仕様と実装の食い違い（人の判断待ち）: PeriodUs=-10000 が受け入れられる"
                    "（std::stoul が負の数を巻き戻して非常に大きな周期にする）";
    const ConfigLoadResult result = LoadFromText(
        "f001_cfg_period_negative.canprj",
        "[Node1]\nNodeId=1\n\n[Node1.Frame1]\nCanId=0x100\nDlc=8\nPeriodUs=-10000\n");
    EXPECT_FALSE(result.Success) << "負の送信周期が受け入れられた";
    EXPECT_FALSE(result.ErrorMessage.empty()) << "拒否理由が示されていない";
}

// ---------------------------------------------------------------------------
// FR-002: 通信速度は 125k / 250k / 500k / 1M から選べ、省略時は 500 kbps。
// 根拠: spec.md FR-002、contracts/config-input-format.md（BitRateBps 任意・既定 500000）。
// ---------------------------------------------------------------------------
TEST(F001Edge, F001_FR002_EachAllowedBitRate_Accepted)
{
    RecordProperty("spec", "001:FR-002");
    for (long long rate : {125000LL, 250000LL, 500000LL, 1000000LL})
    {
        const ConfigLoadResult result = LoadFromText(
            "f001_fr002.canprj", "[Network]\nBitRateBps=" + std::to_string(rate) + "\n\n" + kNode1);
        ASSERT_TRUE(result.Success) << rate << " bps が拒否された: " << result.ErrorMessage;
        EXPECT_EQ(rate, result.Config.BitRateBps) << "通信速度";
    }
}

TEST(F001Edge, F001_FR002_BitRateOmitted_Defaults500k)
{
    RecordProperty("spec", "001:FR-002");
    const ConfigLoadResult result = LoadFromText("f001_fr002_default.canprj", kNode1);
    ASSERT_TRUE(result.Success) << result.ErrorMessage;
    EXPECT_EQ(500000LL, result.Config.BitRateBps) << "省略時の通信速度は 500 kbps";
}

// ---------------------------------------------------------------------------
// FR-005 / FR-015: 受信フィルタを設定したノードは、その ID だけを受信する。
//   行末の「; コメント」は値に含まれない（契約の構文例 `RxFilter=0x080,0x081  ; 省略時は全ID受信`）。
// FR-016: 受信処理時刻 = 受信完了時刻 + 受信処理遅延。
// 根拠: spec.md FR-005、FR-015、FR-016、contracts/config-input-format.md の構文例。
// ---------------------------------------------------------------------------
TEST(F001Edge, F001_FR005_RxFilterAndReceiveDelay_FilteredRowsAndDelayedProcess)
{
    RecordProperty("spec", "001:FR-005");
    RecordProperty("spec", "001:FR-016");
    const std::string text =
        "[Network]\nBitRateBps=500000\n\n"
        "[Node1]\nNodeId=1\n\n[Node1.Frame1]\nCanId=0x080\nDlc=8\nPeriodUs=10000\n\n"
        "[Node1.Frame2]\nCanId=0x081\nDlc=8\nPeriodUs=10000\nInitialOffsetUs=5000\n\n"
        "[Node2]\nNodeId=2\nRxFilter=0x080   ; 0x080 だけを受信\nReceiveDelayUs=40\n\n"
        "[Node3]\nNodeId=3\n";
    const ConfigLoadResult cfg = LoadFromText("f001_fr005.canprj", text);
    ASSERT_TRUE(cfg.Success) << cfg.ErrorMessage;

    const std::vector<LogRow> rows = RunNetwork(cfg.Config, 100000LL);
    int node2Rows = 0;
    int node3Rows081 = 0;
    for (const LogRow& r : rows)
    {
        if (r.RxNodeId == 2)
        {
            ++node2Rows;
            EXPECT_EQ(0x080u, r.CanId) << "ノード 2 は受信フィルタ外の ID を受信した";
            EXPECT_EQ(r.RxEndUs + 40, r.RxProcessUs) << "受信処理時刻 = 受信完了 + 40 us（FR-016）";
            EXPECT_EQ(r.RxProcessUs, r.TimestampUs) << "行の代表時刻は受信処理時刻（ログ契約）";
        }
        if (r.RxNodeId == 3)
        {
            EXPECT_EQ(r.RxEndUs, r.RxProcessUs) << "受信処理遅延 既定 0 us";
            if (r.CanId == 0x081) ++node3Rows081;
        }
    }
    EXPECT_EQ(10, node2Rows) << "ノード 2 は 0x080 を 100 ms で 10 回受信する";
    EXPECT_EQ(10, node3Rows081) << "フィルタ未指定のノード 3 は 0x081 も受信する";
}

// ---------------------------------------------------------------------------
// FR-017: 受信ノードがないフレームは送信側の情報だけで 1 行出す。代表時刻は送信完了時刻で、
//         受信ノードと受信の時刻は空欄。
// 根拠: spec.md FR-017、contracts/log-output-format.md（Timestamp_us・Rx_Node・Rx_* の説明）。
// ---------------------------------------------------------------------------
TEST(F001Edge, F001_FR017_NoReceiver_OneTxOnlyRowWithBlankRxColumns)
{
    RecordProperty("spec", "001:FR-017");
    Network net;
    net.BitRateBps = 500000;
    net.Nodes.push_back(MakeNode(1, 0x100, 8, 10000));
    const std::vector<LogRow> rows = RunNetwork(net, 30000LL);
    ASSERT_EQ(3u, rows.size()) << "30 ms・周期 10 ms で 3 回送信、受信ノードなしなので 1 回 1 行";
    for (const LogRow& r : rows)
    {
        EXPECT_EQ(-1, r.RxNodeId) << "受信ノードなし";
        EXPECT_EQ(r.TxEndUs, r.TimestampUs) << "代表時刻は送信完了時刻";
    }

    const std::wstring path = TempPath("f001_fr017.csv");
    ASSERT_TRUE(WriteLogCsv(rows, path));
    const std::vector<std::string> lines = SplitLines(ReadAll(path));
    DeleteFileW(path.c_str());
    ASSERT_EQ(4u, lines.size()) << "見出し + 3 行";
    const std::vector<std::string> f = SplitFields(lines[1]);
    ASSERT_EQ(15u, f.size()) << "列数";
    EXPECT_EQ("", f[4]) << "Rx_Node は空欄";
    EXPECT_EQ("", f[8]) << "Rx_Start は空欄";
    EXPECT_EQ("", f[9]) << "Rx_End は空欄";
    EXPECT_EQ("", f[10]) << "Rx_Process は空欄";
    EXPECT_EQ("0x100", f[1]) << "CAN_ID は 0x で始まる 3 桁の 16 進";
    EXPECT_EQ("SUCCESS", f[12]) << "Status";
}

// ---------------------------------------------------------------------------
// FR-011 / Edge Cases 1・2: バスが送信中で待っている間に同じフレームの次の周期の要求が来ると、
//   上書き回数が 1 増え、最初の送信要求時刻は保たれる。
// 根拠: spec.md FR-011、Edge Cases（1 つ目・2 つ目）。
// 構成（125 kbps、通常動作は 88 us から）: 0x100・0x200（DLC 8）が 0x300（DLC 0、周期 1500 us）と
//   同時に 88 us に要求する。DLC 8 のフレームは 1 本で 1 ms 近くかかる（8 us × 100 ビット超）ので、
//   0x300 は 2 本の後（88 + 2 ms 弱 > 1588 us）まで待ち、その間に 1588 us の要求が来る。
// ---------------------------------------------------------------------------
TEST(F001Edge, F001_FR011_NextPeriodWhileWaiting_UpdateCountedAndFirstRequestKept)
{
    RecordProperty("spec", "001:FR-011");
    Network net;
    net.BitRateBps = 125000;
    net.Nodes.push_back(MakeNode(1, 0x100, 8, 10000));
    net.Nodes.push_back(MakeNode(2, 0x200, 8, 10000));
    net.Nodes.push_back(MakeNode(3, 0x300, 0, 1500));
    const std::vector<LogRow> rows = RunNetwork(net, 5000LL);

    const LogRow* first300 = nullptr;
    for (const LogRow& r : rows)
    {
        if (r.CanId == 0x300 && (first300 == nullptr || r.TxStartUs < first300->TxStartUs)) first300 = &r;
    }
    ASSERT_NE(nullptr, first300) << "0x300 が一度も送信されなかった";
    EXPECT_GT(first300->TxStartUs, 1588) << "前提: 0x300 は次の周期の要求（1588 us）より後まで待つ";
    EXPECT_EQ(88, first300->TxRequestUs) << "最初の送信要求時刻（88 us）は変わらない";
    EXPECT_EQ(1, first300->UpdateCount) << "待っている間に 1 回上書きされた";
}

// ---------------------------------------------------------------------------
// FR-021: 指定したシミュレーション時間で終える。最初の送信要求がその時間より後のフレームは送信されない。
// 根拠: spec.md FR-021。最初の要求 = 22 us + 初回オフセット 8000 us > 実行時間 5000 us。
// ---------------------------------------------------------------------------
TEST(F001Edge, F001_FR021_FirstRequestAfterEnd_NotSent)
{
    RecordProperty("spec", "001:FR-021");
    Network net;
    net.BitRateBps = 500000;
    net.Nodes.push_back(MakeNode(1, 0x100, 8, 10000));
    net.Nodes.push_back(MakeNode(2, 0x200, 8, 10000));
    net.Nodes[1].Frames[0].InitialOffsetUs = 8000;
    const std::vector<LogRow> rows = RunNetwork(net, 5000LL);
    int rows100 = 0;
    for (const LogRow& r : rows)
    {
        EXPECT_NE(0x200u, r.CanId) << "実行時間より後に最初の要求が来るフレームが送信された";
        if (r.CanId == 0x100) ++rows100;
    }
    EXPECT_EQ(1, rows100) << "0x100 は 22 us の 1 回だけ送信される（受信ノード 1 つで 1 行）";
}

// ---------------------------------------------------------------------------
// Logger::WriteCsv は WriteLogCsv と同じ CSV を書く。書けないパスでは false を返す。
// 根拠: Logger.h のコメント（"Same CSV as Logger::WriteCsv"、"Returns false if the file could not be created"）。
// ---------------------------------------------------------------------------
TEST(F001Edge, F001_FR017_LoggerWriteCsv_SameAsWriteLogCsvAndFalseOnBadPath)
{
    RecordProperty("spec", "001:FR-017");
    Network net;
    net.BitRateBps = 500000;
    net.Nodes.push_back(MakeNode(1, 0x100, 8, 10000));
    net.Nodes.push_back(MakeNode(2, 0x200, 2, 20000));
    SimulationEngine engine(net, 100000LL);
    Logger logger;
    engine.Run(logger);

    const std::wstring a = TempPath("f001_logger_a.csv");
    const std::wstring b = TempPath("f001_logger_b.csv");
    ASSERT_TRUE(logger.WriteCsv(a));
    ASSERT_TRUE(WriteLogCsv(logger.Rows(), b));
    const std::string ca = ReadAll(a);
    const std::string cb = ReadAll(b);
    DeleteFileW(a.c_str());
    DeleteFileW(b.c_str());
    EXPECT_FALSE(ca.empty()) << "CSV が空";
    EXPECT_TRUE(ca == cb) << "Logger::WriteCsv と WriteLogCsv の出力が違う";

    const std::wstring bad = TempPath("f001_no_such_dir\\x\\log.csv");
    EXPECT_FALSE(logger.WriteCsv(bad)) << "存在しないフォルダーへの書き込みが成功扱いになった";
}

// ---------------------------------------------------------------------------
// FR-020: 集計を ID ごと・ノードごと・全体で CSV に出す。見出しの末尾は TxDiscard_Count,RxDiscard_Count で、
//   Node 行には破棄数、ID 行・ALL 行は空の値。
// 根拠: spec.md FR-020、contracts/log-output-format.md「集計サマリ出力」、
//        specs/008-frame-buffers/contracts/log-and-summary.md「集計」。
// ---------------------------------------------------------------------------
TEST(F001Edge, F001_FR020_WriteMetricsCsv_RowsPerGroupAndDiscardColumns)
{
    RecordProperty("spec", "001:FR-020");
    Network net;
    net.BitRateBps = 500000;
    net.Nodes.push_back(MakeNode(1, 0x100, 8, 10000));
    net.Nodes.push_back(MakeNode(2, 0x080, 8, 10000));
    const std::vector<LogRow> rows = RunNetwork(net, 100000LL);
    const std::vector<GroupMetrics> metrics = ComputeMetrics(rows, net);

    const std::wstring path = TempPath("f001_summary.csv");
    ASSERT_TRUE(WriteMetricsCsv(metrics, path));
    const std::vector<std::string> lines = SplitLines(ReadAll(path));
    DeleteFileW(path.c_str());

    ASSERT_EQ(metrics.size() + 1, lines.size()) << "見出し + グループごとに 1 行";
    const std::string suffix = ",TxDiscard_Count,RxDiscard_Count";
    ASSERT_GE(lines[0].size(), suffix.size());
    EXPECT_EQ(suffix, lines[0].substr(lines[0].size() - suffix.size())) << "見出しの末尾";

    const std::vector<std::string> header = SplitFields(lines[0]);
    std::set<std::string> types;
    for (std::size_t i = 1; i < lines.size(); ++i)
    {
        const std::vector<std::string> f = SplitFields(lines[i]);
        ASSERT_EQ(header.size(), f.size()) << i << " 行目の列数が見出しと違う";
        types.insert(f[0]);
        const std::string& tx = f[f.size() - 2];
        const std::string& rx = f[f.size() - 1];
        if (f[0] == "Node")
        {
            EXPECT_EQ("0", tx) << "Node 行の TxDiscard_Count";
            EXPECT_EQ("0", rx) << "Node 行の RxDiscard_Count";
        }
        else
        {
            EXPECT_EQ("", tx) << f[0] << " 行の TxDiscard_Count は空";
            EXPECT_EQ("", rx) << f[0] << " 行の RxDiscard_Count は空";
        }
    }
    EXPECT_EQ(1u, types.count("ID")) << "ID 行がある";
    EXPECT_EQ(1u, types.count("Node")) << "Node 行がある";
    EXPECT_EQ(1u, types.count("ALL")) << "ALL 行がある";
}
