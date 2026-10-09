// feature_002_startupcsv_tests.cpp（tests）
// 起動記録 CSV（WriteStartupCsv）の単体テスト。
// カバレッジで実行されていなかった行（名前の引用符処理、未到達の空欄、書き込み失敗）を通す。
// 期待値は specs/002 の contracts/startup-log-format.md に従う。

#include "test_helpers.h"

using namespace cansimtest;

namespace
{
    const std::string kHeader =
        "Node_Id,Node_Name,Power_On_us,Init_Done_us,Normal_Entry_us,Reached_Normal\r\n";

    // 1 ノード分の記録を CSV に書き、ファイルの中身を返す
    std::string WriteOne(const StartupRecord& r, const std::string& fileName)
    {
        const std::string path = Join(TempDir(), fileName);
        EXPECT_TRUE(WriteStartupCsv({ r }, Widen(path))) << "CSV を書けること";
        return ReadTextFile(path);
    }
}

// 名前にカンマがあるとき、名前全体を二重引用符で囲む
TEST(Feature002StartupCsv, F002_NameWithComma_IsQuoted)
{
    StartupRecord r;
    r.NodeId = 1;
    r.NodeName = "ECU1, Engine";
    r.PowerOnUs = 0;
    r.InitDoneUs = 0;
    r.NormalEntryUs = 22;
    r.ReachedNormal = true;

    EXPECT_EQ(WriteOne(r, "startup_comma.csv"),
              kHeader + R"(1,"ECU1, Engine",0,0,22,TRUE)" + "\r\n")
        << "カンマを含む名前が二重引用符で囲まれていない";
}

// 名前に " があるとき、" を 2 つ重ねたうえで全体を囲む
TEST(Feature002StartupCsv, F002_NameWithQuote_IsEscaped)
{
    StartupRecord r;
    r.NodeId = 2;
    r.NodeName = R"(ECU "A")";
    r.PowerOnUs = 0;
    r.InitDoneUs = 0;
    r.NormalEntryUs = 22;
    r.ReachedNormal = true;

    EXPECT_EQ(WriteOne(r, "startup_quote.csv"),
              kHeader + R"(2,"ECU ""A""",0,0,22,TRUE)" + "\r\n")
        << "名前の中の \" が 2 つに重ねられていない";
}

// 通常動作に到達しなかったノードは、時刻の欄を空にして FALSE を書く
TEST(Feature002StartupCsv, F002_NotReached_TimesAreEmpty)
{
    StartupRecord r;          // InitDoneUs と NormalEntryUs は -1（未到達）のまま
    r.NodeId = 3;
    r.NodeName = "ECU3";
    r.PowerOnUs = 9999000;

    EXPECT_EQ(WriteOne(r, "startup_notreached.csv"),
              kHeader + "3,ECU3,9999000,,,FALSE\r\n")
        << "未到達の時刻が空欄になっていない";
}

// 書き込めない場所（存在しないフォルダー）を指定したら false を返す
TEST(Feature002StartupCsv, F002_BadPath_ReturnsFalse)
{
    const std::string path = Join(TempDir(), "no_such_folder/startup.csv");
    EXPECT_FALSE(WriteStartupCsv({}, Widen(path)))
        << "存在しないフォルダーなのに、書き込めたことになっている";
}