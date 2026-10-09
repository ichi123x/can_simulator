// 機能 025（解析・ログ表示メニュー。シミュレーションログの表示）の仕様テスト
// 根拠: specs/025-analysis-log-menus/spec.md と LogFileIndex.h のコメント。仕様に ID が無いので、
// 受け入れシナリオは US<ストーリー番号>-<シナリオ番号> と呼ぶ。
// メニュー・ウィンドウ・メッセージは画面側なのでテストせず、ログを読む LogFileIndex を確かめる。

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "LogFileIndex.h"
#include "support/test_helpers.h"

using namespace CanSim;
using namespace TestHelpers;

namespace
{
    // 1000 行のログ（CRLF）。k 行目は "line<k>,<k の 2 乗>"
    std::string ThousandLines()
    {
        std::string s;
        for (int k = 0; k < 1000; ++k) s += "line" + std::to_string(k) + "," + std::to_string(k * k) + "\r\n";
        return s;
    }
}

// ---------------------------------------------------------------------------
// US1-1 / SC-001 / FR-004: ファイルと同じ内容・同じ行の順で、最後の行まで読める（行数・先頭と最後の行が一致）。
//   索引の区切り（256 行ごと）をまたぐ範囲も正しく読める。行末の CR/LF は含まない。
// FR-007 / US1-3: 読んでもファイルは変わらない。
// 根拠: spec.md US1 シナリオ 1・3、SC-001、FR-004、FR-007、LogFileIndex.h。
// ---------------------------------------------------------------------------
TEST(F025Spec, F025_SC001_ThousandLines_SameContentAndUnchangedFile)
{
    RecordProperty("spec", "025:SC-001");
    RecordProperty("spec", "025:US1-1");
    RecordProperty("spec", "025:FR-007");
    const std::wstring path = TempPath("f025_sc001.log.csv");
    const std::string text = ThousandLines();
    WriteAll(path, text);

    LogFileIndex index;
    ASSERT_TRUE(index.Build(path));
    EXPECT_EQ(1000, index.LineCount()) << "行数";
    EXPECT_EQ(static_cast<int>(std::string("line999,998001").size()), index.MaxLineLength()) << "最長の行の文字数";

    const std::vector<std::string> head = index.ReadLines(0, 2);
    ASSERT_EQ(2u, head.size());
    EXPECT_EQ("line0,0", head[0]) << "先頭の行（CR/LF なし）";
    EXPECT_EQ("line1,1", head[1]);

    const std::vector<std::string> across = index.ReadLines(250, 12); // 256 行目の区切りをまたぐ
    ASSERT_EQ(12u, across.size());
    for (int i = 0; i < 12; ++i)
    {
        const int k = 250 + i;
        EXPECT_EQ("line" + std::to_string(k) + "," + std::to_string(k * k), across[i]) << k << " 行目";
    }
    const std::vector<std::string> last = index.ReadLines(999, 1);
    ASSERT_EQ(1u, last.size());
    EXPECT_EQ("line999,998001", last[0]) << "最後の行";

    EXPECT_EQ(text, ReadAll(path)) << "ログファイルが変わった";
    DeleteFileW(path.c_str());
}

// LogFileIndex.h: 範囲外は切る。最後の行に '\n' が無くても 1 行と数える。
TEST(F025Spec, F025_US1_1_ReadLines_ClippedAndLastLineWithoutNewline)
{
    RecordProperty("spec", "025:US1-1");
    const std::wstring path = TempPath("f025_clip.log.csv");
    WriteAll(path, "a\nbb\r\nccc");
    LogFileIndex index;
    ASSERT_TRUE(index.Build(path));
    DeleteFileW(path.c_str());
    // ファイルを消した後は読めない（ReadLines は毎回ファイルを開く）
    EXPECT_EQ(3, index.LineCount()) << "最後の行に改行が無くても 1 行";
    EXPECT_EQ(3, index.MaxLineLength());
    EXPECT_TRUE(index.ReadLines(0, 3).empty()) << "ファイルが読めなくなったら空";

    WriteAll(path, "a\nbb\r\nccc");
    LogFileIndex again;
    ASSERT_TRUE(again.Build(path));
    EXPECT_EQ((std::vector<std::string>{"bb", "ccc"}), again.ReadLines(1, 10)) << "末尾を超える分は切る";
    EXPECT_EQ((std::vector<std::string>{"a"}), again.ReadLines(-5, 6)) << "先頭より前は切る";
    EXPECT_TRUE(again.ReadLines(3, 1).empty()) << "全体が範囲外";
    DeleteFileW(path.c_str());
}

// ---------------------------------------------------------------------------
// Edge Cases: 見出しの行だけのログは、その 1 行を表示する（「ログが無い」とは扱わない）。空のファイルは 0 行。
// US2-3 / FR-006: 開けないファイルは失敗（「ログが無い」と区別する）。
// 根拠: spec.md Edge Cases（ログが空）、US2 シナリオ 3、FR-006、LogFileIndex.h（Build）。
// ---------------------------------------------------------------------------
TEST(F025Spec, F025_US2_3_HeaderOnlyEmptyAndMissing_Distinguished)
{
    RecordProperty("spec", "025:US2-3");
    const std::wstring header = TempPath("f025_header.log.csv");
    WriteAll(header, "Timestamp_us,CAN_ID\r\n");
    LogFileIndex h;
    EXPECT_TRUE(h.Build(header));
    EXPECT_EQ(1, h.LineCount()) << "見出しの行だけのログは 1 行";
    EXPECT_EQ((std::vector<std::string>{"Timestamp_us,CAN_ID"}), h.ReadLines(0, 5));
    DeleteFileW(header.c_str());

    const std::wstring empty = TempPath("f025_empty.log.csv");
    WriteAll(empty, "");
    LogFileIndex e;
    EXPECT_TRUE(e.Build(empty)) << "空のファイルは成功（0 行）";
    EXPECT_EQ(0, e.LineCount());
    DeleteFileW(empty.c_str());

    LogFileIndex m;
    EXPECT_FALSE(m.Build(TempPath("f025_no_such.log.csv"))) << "無いファイルは失敗";
    EXPECT_EQ(0, m.LineCount());
}

// ---------------------------------------------------------------------------
// FR-003 / US1-4: 表示するログは、現在のプロジェクトのフレームログ "<プロジェクトファイル名>.log.csv"。
// 根拠: spec.md FR-003、US1 シナリオ 4、LogFileIndex.h（LogPathFor）。
// ---------------------------------------------------------------------------
TEST(F025Spec, F025_FR003_LogPathFor_ProjectNamePlusLogCsv)
{
    RecordProperty("spec", "025:FR-003");
    EXPECT_EQ(std::wstring(L"C:\\work\\a.canprj.log.csv"), LogPathFor(L"C:\\work\\a.canprj"));
    EXPECT_EQ(std::wstring(L"C:\\work\\b.canprj.log.csv"), LogPathFor(L"C:\\work\\b.canprj"))
        << "別のプロジェクトは別のログ";
}

// LogFileIndex.h: 行数が索引の区切り（256 行）のちょうど倍数で、最後が改行で終わるファイルも、
//   行数と最後の行が正しい。数 0 を頼むと空。
TEST(F025Spec, F025_SC001_ExactMultipleOfStride_LastLineReadable)
{
    RecordProperty("spec", "025:SC-001");
    std::string text;
    for (int k = 0; k < 512; ++k) text += "row" + std::to_string(k) + "\n";
    const std::wstring path = TempPath("f025_512.log.csv");
    WriteAll(path, text);
    LogFileIndex index;
    ASSERT_TRUE(index.Build(path));
    EXPECT_EQ(512, index.LineCount()) << "行数";
    EXPECT_EQ((std::vector<std::string>{"row510", "row511"}), index.ReadLines(510, 5)) << "最後の 2 行";
    EXPECT_EQ((std::vector<std::string>{"row256"}), index.ReadLines(256, 1)) << "区切りの行";
    EXPECT_TRUE(index.ReadLines(512, 1).empty()) << "最後の後は無い";
    EXPECT_TRUE(index.ReadLines(10, 0).empty()) << "数 0";
    DeleteFileW(path.c_str());
}
