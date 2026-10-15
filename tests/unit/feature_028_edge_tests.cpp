// feature_028_edge_tests.cpp（tests/unit）
// 機能 028・029 の補足のテスト（/coverage）。spec.md の Edge Cases・FR と LogAnalysis.h の約束に根拠がある動きを確かめる。

#include "support/spec_test_helpers.h"

#include "LogAnalysis.h"

using namespace CanSim;
using namespace spectest;

namespace
{
    const std::string kHeader = "Timestamp_us,CAN_ID,DLC,Tx_Node,Rx_Node,Tx_Request,Tx_Start,Tx_End,Rx_Start,Rx_End,"
                                "Rx_Process,Bus_Load_Pct,Status,Retry_Count,Update_Count\r\n";

    bool ReadText(const std::string& text, const std::wstring& name, std::vector<LogRecord>& records, std::string& error)
    {
        const std::wstring path = TempPath(name);
        WriteText(path, text);
        return ReadLogRecords(path, records, error);
    }

    LogRecord Rec(unsigned canId, int txNode, long long request, long long start, long long end, LogStatus status = LogStatus::Success)
    {
        LogRecord r;
        r.CanId = canId;
        r.TxNode = txNode;
        r.TxRequestUs = request;
        r.TxStartUs = start;
        r.TxEndUs = end;
        r.Status = status;
        return r;
    }
}

// 根拠: 028 spec.md US3 受け入れシナリオ 3（ログが読めない・形式が違うときは読めなかったことを通知）、LogAnalysis.h（読めなければ
// 行番号付きのエラー）。列が足りない行・CAN ID が読めない行・状態が読めない行・ヘッダーの無いファイルは読めない。
TEST(F028Edge, F028_US3_3_MalformedRows_ReadFails)
{
    RecordProperty("spec", "028:US3-3");
    const std::string ok = "268,0x080,8,1,2,22,22,268,22,268,268,4.9,SUCCESS,0,0\r\n";
    struct Case
    {
        const char* Name;
        std::string Text;
    };
    const Case cases[] = {
        { "too_few_columns", kHeader + "268,0x080,8\r\n" },
        { "bad_can_id", kHeader + "268,080,8,1,2,22,22,268,22,268,268,4.9,SUCCESS,0,0\r\n" },
        { "bad_can_id_char", kHeader + "268,0x0G0,8,1,2,22,22,268,22,268,268,4.9,SUCCESS,0,0\r\n" },
        { "bad_status", kHeader + "268,0x080,8,1,2,22,22,268,22,268,268,4.9,OK,0,0\r\n" },
        { "no_header", "" },
    };
    for (const Case& c : cases)
    {
        std::vector<LogRecord> records;
        std::string error;
        EXPECT_FALSE(ReadText(c.Text, Widen(std::string("f028e_") + c.Name + ".log.csv"), records, error)) << c.Name << ": 読めてしまった";
        EXPECT_FALSE(error.empty()) << c.Name << ": 理由が無い";
    }
    std::vector<LogRecord> records;
    std::string error;
    EXPECT_TRUE(ReadText(kHeader + ok, L"f028e_ok.log.csv", records, error)) << "正しい行が読めない: " << error;
}

// 根拠: 028 spec.md Edge Cases（受信の行が 1 件も無いログでは、受信完了時間の表は全体の行が件数 0 で、ヒストグラムは空の表示）、
// LogAnalysis.h（MakeHistogram は値が無ければ空）。
TEST(F028Edge, F028_EDGE_NoReceives_AllRowZeroAndEmptyHistogram)
{
    RecordProperty("spec", "028:Edge");
    Network net;
    net.BitRateBps = 500000;
    net.Nodes.push_back(MakeNode(1, { MakeFrame(0x100, 8, 10000) }));
    const std::wstring path = TempPath(L"f028e_noreceive.log.csv");
    ASSERT_TRUE(WriteLogCsv(RunSim(net, 50000LL), path));
    std::vector<LogRecord> records;
    std::string error;
    ASSERT_TRUE(ReadLogRecords(path, records, error)) << error;
    ASSERT_FALSE(records.empty()) << "前提: 送信の行が無い";
    const LatencyAnalysis a = AnalyzeLatency(records);
    ASSERT_EQ(a.Rows.size(), 1u) << "全体の行だけになっていない";
    EXPECT_EQ(a.Rows[0].Count, 0) << "全体の行の件数が 0 でない";
    EXPECT_TRUE(MakeHistogram(a.Rows[0].Values).Counts.empty()) << "ヒストグラムが空でない";
    EXPECT_EQ(AnalyzeBusLoad(records).TransmissionCount, 5) << "バス負荷は送信した行から計算する";
}

// 根拠: 028 spec.md FR-006（バスがフレームの送信に使われていた時間）、Edge Cases（破棄された送信は含めない）。OVERWRITE の行は
// バス負荷の送信に数えない。
TEST(F028Edge, F028_FR006_OverwriteRows_NotCountedInBusLoad)
{
    RecordProperty("spec", "028:FR-006");
    const std::vector<LogRecord> records = { Rec(0x100, 1, 0, 0, 246), Rec(0x101, 1, 0, -1, -1, LogStatus::Overwrite) };
    const BusLoadAnalysis b = AnalyzeBusLoad(records);
    EXPECT_EQ(b.TransmissionCount, 1) << "OVERWRITE の行が送信に数えられた";
}

// 根拠: LogAnalysis.h（NiceTicks: lo〜hi を 1・2・5 × 10^m の間隔で、およそ approxCount の区間に分ける。hi <= lo なら {lo}）。
// 0〜100 を 5 区間なら、ちょうど間隔 20 になる。
TEST(F028Edge, F028_FR004_NiceTicks_RoundSteps)
{
    RecordProperty("spec", "028:FR-004");
    EXPECT_EQ(NiceTicks(0, 100, 5), (std::vector<double>{ 0, 20, 40, 60, 80, 100 })) << "0〜100・5 区間";
    EXPECT_EQ(NiceTicks(5, 5, 4), (std::vector<double>{ 5 })) << "hi = lo";
    EXPECT_EQ(NiceTicks(5, 1, 4), (std::vector<double>{ 5 })) << "hi < lo";
}

// 根拠: LogAnalysis.h（MaxPerColumn: 値が列より多いときは、各列に入る値の最大。そうでなければ値そのまま）、028 の CLAUDE.md の説明
// （区間が幅より多ければ列ごとの最大で間引く）。
TEST(F028Edge, F028_FR005_MaxPerColumn_KeepsPeaks)
{
    RecordProperty("spec", "028:FR-005");
    EXPECT_EQ(MaxPerColumn({ 1, 5, 2, 8, 3, 4 }, 3), (std::vector<double>{ 5, 8, 4 })) << "2 個ずつの列の最大";
    EXPECT_EQ(MaxPerColumn({ 1, 5 }, 3), (std::vector<double>{ 1, 5 })) << "値が列より少ないときはそのまま";
}

// 根拠: LogAnalysis.h（MinMaxPerColumn: [t0, t1] を列に分け、各列の最小と最大のずれを残す。点の無い列は Has = false）、
// 029 spec.md US2 受け入れシナリオ 4（点が画面の幅より多いときは列ごとにまとめる）。
TEST(F028Edge, F029_US2_4_MinMaxPerColumn_KeepsPeaksAndDips)
{
    RecordProperty("spec", "029:US2-4");
    std::vector<JitterPoint> points;
    const long long devs[] = { 3, -7, 2, 9, -1, 0, 0, 0, 0, 0 };
    for (int i = 0; i < 10; ++i)
    {
        JitterPoint p;
        p.TimeUs = i;
        p.DeviationUs = devs[i];
        points.push_back(p);
    }
    const std::vector<ColumnRange> cols = MinMaxPerColumn(points, 0, 19, 4); // 列の幅 5: 0〜4・5〜9・10〜14・15〜19
    ASSERT_EQ(cols.size(), 4u);
    EXPECT_TRUE(cols[0].Has);
    EXPECT_EQ(cols[0].MinUs, -7) << "最初の列の最小（谷）";
    EXPECT_EQ(cols[0].MaxUs, 9) << "最初の列の最大（山）";
    EXPECT_TRUE(cols[1].Has);
    EXPECT_EQ(cols[1].MinUs, 0);
    EXPECT_EQ(cols[1].MaxUs, 0);
    EXPECT_FALSE(cols[2].Has) << "点の無い列";
    EXPECT_FALSE(cols[3].Has) << "点の無い列";
    EXPECT_TRUE(MinMaxPerColumn(points, 0, 9, 0).empty()) << "列 0";
}

// 根拠: LogAnalysis.h（CollectTransmissions は (CanId, Tx_Start) の順に並べ、同じ送信は 1 回にまとめる）。行の順がばらばらの
// ログでも、並んだ送信の一覧になる。
TEST(F028Edge, F029_FR003_CollectTransmissions_SortedAndUnique)
{
    RecordProperty("spec", "029:FR-003");
    const std::vector<LogRecord> records = { Rec(0x100, 1, 2000, 2010, 2256), Rec(0x100, 1, 0, 5, 251),
                                             Rec(0x100, 1, 2000, 2010, 2256), Rec(0x080, 2, 1000, 1000, 1246) };
    long long overwrites = 0;
    const std::vector<Transmission> t = CollectTransmissions(records, overwrites);
    ASSERT_EQ(t.size(), 3u) << "同じ送信が 1 回にまとまっていない";
    EXPECT_EQ(t[0].CanId, 0x080u);
    EXPECT_EQ(t[1].TxStartUs, 5) << "0x100 の送信が送信開始の順に並んでいない";
    EXPECT_EQ(t[2].TxStartUs, 2010);
}

// 根拠: LogAnalysis.h（progress が false を返したら読むのをやめ、records を空にし、error を kLogReadCancelled にする。progress は
// 4 MB ごとと最後に呼ぶ）、029 spec.md の改訂 1（解析中の「中断」）。4 MB を超えるログの途中と、小さなログの最後で確かめる。
TEST(F028Edge, F029_REV1_ProgressFalse_ReadCancelled)
{
    RecordProperty("spec", "029:REV1");
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
        r.Status = "SUCCESS";
        rows.push_back(r);
    }
    const std::wstring big = TempPath(L"f028e_big.log.csv");
    ASSERT_TRUE(WriteLogCsv(rows, big));
    int calls = 0;
    std::vector<LogRecord> records;
    std::string error;
    EXPECT_FALSE(ReadLogRecords(big, records, error, [&calls](long long, long long) { ++calls; return false; }))
        << "途中で中断されない";
    EXPECT_EQ(calls, 1) << "最初の 4 MB の後で止まっていない";
    EXPECT_TRUE(records.empty()) << "中断しても行が残っている";
    EXPECT_EQ(error, kLogReadCancelled) << "中断の理由";

    const std::wstring small = TempPath(L"f028e_small.log.csv");
    ASSERT_TRUE(WriteLogCsv({ rows.front() }, small));
    error.clear();
    EXPECT_FALSE(ReadLogRecords(small, records, error, [](long long, long long) { return false; })) << "最後の呼び出しで中断されない";
    EXPECT_TRUE(records.empty());
    EXPECT_EQ(error, kLogReadCancelled);
}
