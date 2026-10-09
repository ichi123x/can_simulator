// feature_028_edge_tests.cpp（tests）
// ログの読み込み（ReadLogRecords）と受信完了時間（AnalyzeLatency）の単体テスト。
// カバレッジで実行されていなかった行（ヘッダーの末尾のカンマ、空のファイル、列の不足、
// 0x の無い・16 進でない CAN_ID、"-" だけの数値、改行の無い最後の行の誤り、受信の無いログ）を通す。
// 期待値は specs/028 の spec.md・data-model.md と specs/001 の contracts/log-output-format.md に従う。
// LogAnalysis.cpp の次の行はテストしない（2026-10-09 判断）。
//   125 行目 大文字の 16 進（0x7FF）  ログは小文字で書かれ（Logger.cpp）、仕様に大文字の扱いが無い
//   262・263 行目 読み込みの失敗      開けたファイルが途中で読めなくなる場合で、テストで起こせない

#include "test_helpers.h"

using namespace cansimtest;

namespace
{
    const std::string kHeader = "CAN_ID,Tx_Node,Rx_Node,Tx_Request,Tx_Start,Tx_End,Rx_End,Status";

    // ログを一時フォルダーに書き、読み込んだ結果を返す
    bool ReadText(const std::string& fileName, const std::string& text, std::vector<LogRecord>& records,
        std::string& error)
    {
        const std::string path = Join(TempDir(), fileName);
        EXPECT_TRUE(WriteTextFile(path, text)) << "ログを書けること";
        return ReadLogRecords(Widen(path), records, error);
    }
}

// ヘッダーの末尾にカンマ（空の余分な列）があっても、列は名前で探すので読める（data-model「列は名前で探す」）
TEST(Feature028Edge, Read_HeaderWithTrailingComma_IsRead)
{
    std::vector<LogRecord> records;
    std::string error;
    const bool ok = ReadText("t028e_trailing.log.csv",
        kHeader + ",\r\n0x080,1,2,0,0,250,100,SUCCESS,\r\n", records, error);

    EXPECT_TRUE(ok) << error;
    ASSERT_EQ(records.size(), 1u);
    EXPECT_EQ(records[0].CanId, 0x080u);
    EXPECT_EQ(records[0].RxEndUs, 100);
}

// 空のファイルはヘッダーが無いので、1 行目の誤りになる（data-model「1 行目がヘッダー」）
TEST(Feature028Edge, Read_EmptyFile_IsLine1Error)
{
    std::vector<LogRecord> records;
    std::string error;

    EXPECT_FALSE(ReadText("t028e_empty.log.csv", "", records, error));
    EXPECT_EQ(error.find("line 1:"), 0u) << error;
}

// データの行の列が足りないときは、その行番号の誤りになる（spec US3-3「形式が違う」）
TEST(Feature028Edge, Read_TooFewColumns_IsLineError)
{
    std::vector<LogRecord> records;
    std::string error;

    EXPECT_FALSE(ReadText("t028e_few.log.csv", kHeader + "\r\n0x080,1,2\r\n", records, error));
    EXPECT_EQ(error.find("line 2:"), 0u) << error;
}

// CAN_ID は 0x で始まる 16 進（log-output-format「例: 0x080」）。0x が無い・16 進でない文字は誤り
TEST(Feature028Edge, Read_BadCanId_IsLineError)
{
    const char* ids[] = { "080", "0x08G" };
    for (const char* id : ids)
    {
        std::vector<LogRecord> records;
        std::string error;
        const std::string text = kHeader + "\r\n" + id + ",1,2,0,0,250,100,SUCCESS\r\n";

        EXPECT_FALSE(ReadText("t028e_canid.log.csv", text, records, error)) << "CAN_ID " << id;
        EXPECT_EQ(error.find("line 2:"), 0u) << "CAN_ID " << id << ": " << error;
    }
}

// "-" だけの欄は数値にならないので誤り（data-model「数値にならない欄があれば、行番号付きで失敗」）
TEST(Feature028Edge, Read_LoneMinusSign_IsLineError)
{
    std::vector<LogRecord> records;
    std::string error;

    EXPECT_FALSE(ReadText("t028e_minus.log.csv", kHeader + "\r\n0x080,1,2,-,0,250,100,SUCCESS\r\n", records, error));
    EXPECT_EQ(error.find("line 2:"), 0u) << error;
}

// 改行の無い最後の行も同じ規則で調べ、誤りならその行番号になる（同上）
TEST(Feature028Edge, Read_BadLastLineWithoutNewline_IsLineError)
{
    std::vector<LogRecord> records;
    std::string error;
    const std::string text = kHeader + "\r\n0x080,1,2,0,0,250,100,SUCCESS\r\n0x080,1,2,abc,0,250,100,SUCCESS";

    EXPECT_FALSE(ReadText("t028e_last.log.csv", text, records, error));
    EXPECT_EQ(error.find("line 3:"), 0u) << error;
}

// 受信の行が 1 件も無いログは、全体の行だけで件数 0、最小・平均・最大・P95 は 0
// （spec Edge Cases「受信の行が 1 件も無いログ」、data-model「対象は RxNode > 0」「件数 0 の全体の行では … 0」）
TEST(Feature028Edge, Latency_NoReceives_AllRowIsZero)
{
    LogRecord overwrite;
    overwrite.CanId = 0x100;
    overwrite.TxNode = 1;
    overwrite.TxRequestUs = 500;
    overwrite.Status = LogStatus::Overwrite;

    LogRecord noReceiver; // 送信したが受信 ECU の無い行（Rx_Node が空欄）
    noReceiver.CanId = 0x080;
    noReceiver.TxNode = 1;
    noReceiver.RxNode = 0;
    noReceiver.TxRequestUs = 0;
    noReceiver.TxStartUs = 0;
    noReceiver.TxEndUs = 250;

    const LatencyAnalysis lat = AnalyzeLatency({ overwrite, noReceiver });

    ASSERT_EQ(lat.Rows.size(), 1u) << "全体の行だけになる";
    EXPECT_EQ(lat.Rows[0].Count, 0);
    EXPECT_EQ(lat.Rows[0].MinUs, 0);
    EXPECT_EQ(lat.Rows[0].MaxUs, 0);
    EXPECT_EQ(lat.Rows[0].MeanUs, 0.0);
    EXPECT_EQ(lat.Rows[0].P95Us, 0);
    EXPECT_EQ(lat.OverwriteCount, 1);
}
