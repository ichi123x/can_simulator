// feature_029_edge_tests.cpp（tests）
// 読み込みの中断（ReadLogRecords の progress）、送信の集め方（CollectTransmissions）、
// グラフの目盛り（NiceTicks）の単体テスト。
// カバレッジで実行されていなかった行（最後の進み具合で中断、送信の時刻が無い行、時刻の順でない行、
// 範囲の無い目盛り）を通す。期待値は specs/029 の spec.md（改訂 1）・data-model.md と LogAnalysis.h のコメントに従う。

#include "test_helpers.h"

using namespace cansimtest;

// 最後の進み具合の呼び出しで false を返すと、読み込みは中断になり、結果は空になる
// （spec 改訂 1 B「中断」、LogAnalysis.h「called … at the end; when it returns false … records is emptied」）
TEST(Feature029Edge, Read_CancelAtEnd_EmptiesRecords)
{
    const std::string path = Join(TempDir(), "t029e_cancel.log.csv");
    ASSERT_TRUE(WriteTextFile(path,
        "CAN_ID,Tx_Node,Rx_Node,Tx_Request,Tx_Start,Tx_End,Rx_End,Status\r\n0x080,1,2,0,0,250,100,SUCCESS\r\n"));

    long long lastRead = -1, lastTotal = -2;
    auto cancel = [&](long long bytesRead, long long totalBytes) {
        lastRead = bytesRead;
        lastTotal = totalBytes;
        return false;
    };
    std::vector<LogRecord> records;
    std::string error;

    EXPECT_FALSE(ReadLogRecords(Widen(path), records, error, cancel));
    EXPECT_TRUE(records.empty()) << "中断したのに結果が残っている";
    EXPECT_EQ(error, kLogReadCancelled);
    EXPECT_EQ(lastRead, lastTotal) << "最後の呼び出しはファイル全体を読んだ後";
}

// 送信要求か送信開始の時刻が無い行は、送信として数えない（data-model「対象: … TxRequestUs >= 0、TxStartUs >= 0」）
TEST(Feature029Edge, Collect_RowWithoutTimes_IsExcluded)
{
    LogRecord ok;
    ok.CanId = 0x100;
    ok.TxNode = 1;
    ok.TxRequestUs = 0;
    ok.TxStartUs = 10;

    LogRecord noRequest = ok;
    noRequest.TxRequestUs = -1;
    noRequest.TxStartUs = 2000;

    LogRecord noStart = ok;
    noStart.TxRequestUs = 3000;
    noStart.TxStartUs = -1;

    long long overwrites = -1;
    const std::vector<Transmission> t = CollectTransmissions({ ok, noRequest, noStart }, overwrites);

    ASSERT_EQ(t.size(), 1u);
    EXPECT_EQ(t[0].TxStartUs, 10);
    EXPECT_EQ(overwrites, 0);
}

// 時刻の順に並んでいない行でも、結果は (CAN ID, 送信開始) の昇順になる（data-model「並び: (CanId, TxStartUs) の昇順」）
TEST(Feature029Edge, Collect_OutOfOrderRows_AreSorted)
{
    LogRecord later;
    later.CanId = 0x100;
    later.TxNode = 1;
    later.TxRequestUs = 2000;
    later.TxStartUs = 2010;

    LogRecord earlier = later;
    earlier.TxRequestUs = 1000;
    earlier.TxStartUs = 1010;

    long long overwrites = 0;
    const std::vector<Transmission> t = CollectTransmissions({ later, earlier }, overwrites);

    ASSERT_EQ(t.size(), 2u);
    EXPECT_EQ(t[0].TxStartUs, 1010);
    EXPECT_EQ(t[1].TxStartUs, 2010);
}

// 範囲が無い（hi <= lo）ときの目盛りは lo の 1 つだけ（LogAnalysis.h「{lo} when hi <= lo」）
TEST(Feature029Edge, NiceTicks_NoRange_IsLoOnly)
{
    EXPECT_EQ(NiceTicks(5.0, 5.0, 4), std::vector<double>({ 5.0 }));
    EXPECT_EQ(NiceTicks(5.0, 3.0, 4), std::vector<double>({ 5.0 }));
}
