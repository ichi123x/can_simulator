// feature_025_edge_tests.cpp（tests/unit）
// 機能 025 の補足のテスト（/coverage）。LogFileIndex.h の約束（開けないファイル・空のファイル・改行で終わらない最後の行・範囲外の指定・
// 読めなくなったファイル）を確かめる。

#include "support/spec_test_helpers.h"

#include "LogFileIndex.h"

#include <windows.h>

using namespace CanSim;
using namespace spectest;

// 根拠: LogFileIndex.h（開けないファイルは false、空のファイルは 0 行で true）、spec.md FR-006（読めないときは読めなかったことがわかる）。
TEST(F025Edge, F025_FR006_MissingOrEmptyFile)
{
    RecordProperty("spec", "025:FR-006");
    LogFileIndex missing;
    EXPECT_FALSE(missing.Build(TempPath(L"f025e_no_such.log.csv"))) << "無いファイルで索引が作れた";
    EXPECT_EQ(missing.LineCount(), 0);
    const std::wstring emptyPath = TempPath(L"f025e_empty.log.csv");
    WriteText(emptyPath, "");
    LogFileIndex empty;
    EXPECT_TRUE(empty.Build(emptyPath)) << "空のファイルで索引が作れない";
    EXPECT_EQ(empty.LineCount(), 0) << "空のファイルの行数";
    EXPECT_TRUE(empty.ReadLines(0, 10).empty());
}

// 根拠: LogFileIndex.h（行は '\n' で区切り、'\n' の無い最後の行も 1 行。読み出す行に CR / LF は含めない。最長の行の文字数も CR / LF を除く）、
// spec.md FR-004（ファイルと同じ内容）。
TEST(F025Edge, F025_FR004_LastLineWithoutBreak_CountedWithoutCr)
{
    RecordProperty("spec", "025:FR-004");
    const std::wstring path = TempPath(L"f025e_lastline.log.csv");
    WriteText(path, "aa\r\nbbbbb\r\ncccc\r");
    LogFileIndex index;
    ASSERT_TRUE(index.Build(path));
    EXPECT_EQ(index.LineCount(), 3) << "改行で終わらない最後の行が数えられていない";
    EXPECT_EQ(index.MaxLineLength(), 5) << "最長の行の文字数（CR を除く）";
    EXPECT_EQ(index.ReadLines(0, 3), (std::vector<std::string>{ "aa", "bbbbb", "cccc" })) << "読み出した行";
}

// 根拠: LogFileIndex.h（読み出しは [first, first + count) を [0, 行数) に切る）。索引の間隔（256 行）のちょうど倍の行数で、
// 最後の行と、先頭より前から始まる指定を確かめる。
TEST(F025Edge, F025_FR004_RangesClippedAtBothEnds)
{
    RecordProperty("spec", "025:FR-004");
    std::string text;
    for (int i = 0; i < 2 * LogFileIndex::kIndexStride; ++i)
        text += "line" + std::to_string(i) + "\r\n";
    const std::wstring path = TempPath(L"f025e_ranges.log.csv");
    WriteText(path, text);
    LogFileIndex index;
    ASSERT_TRUE(index.Build(path));
    ASSERT_EQ(index.LineCount(), 2 * LogFileIndex::kIndexStride);
    EXPECT_EQ(index.ReadLines(2 * LogFileIndex::kIndexStride - 1, 5), (std::vector<std::string>{ "line511" })) << "最後の行";
    EXPECT_EQ(index.ReadLines(-2, 5), (std::vector<std::string>{ "line0", "line1", "line2" })) << "先頭より前から始まる指定";
    EXPECT_TRUE(index.ReadLines(-10, 5).empty()) << "全部が先頭より前の指定";
}

// 根拠: LogFileIndex.h（ファイルは読むたびに開き、読めなくなったら空を返す）。索引を作った後でファイルが消えたら、読み出しは空。
TEST(F025Edge, F025_FR006_FileGoneAfterIndex_ReadsNothing)
{
    RecordProperty("spec", "025:FR-006");
    const std::wstring path = TempPath(L"f025e_gone.log.csv");
    WriteText(path, "a\r\nb\r\n");
    LogFileIndex index;
    ASSERT_TRUE(index.Build(path));
    ASSERT_TRUE(DeleteFileW(path.c_str())) << "前提: ファイルを消せない";
    EXPECT_TRUE(index.ReadLines(0, 2).empty()) << "消えたファイルから行が読めた";
}
