// feature_025_spec_tests.cpp（tests/unit）
// 機能 025（specs/025-analysis-log-menus/spec.md）の受け入れシナリオと成功基準のうち、画面を使わずに確かめられるもの。
// ログの表示の中身は LogFileIndex.h（索引と、見えている行の読み出し）、ログのパスは LogPathFor で確かめる。
// メニュー・ウィンドウ・メッセージ・スクロールは画面の確認。

#include "support/spec_test_helpers.h"

#include "LogFileIndex.h"

using namespace CanSim;
using namespace spectest;

namespace
{
    // 実際の実行のログの CSV を書き、その中身（バイト列）を返す
    std::string WriteRunLog(const std::wstring& path, long long durationUs)
    {
        Network net;
        net.BitRateBps = 500000;
        net.Nodes.push_back(MakeNode(1, { MakeFrame(0x100, 8, 1000) }));
        net.Nodes.push_back(MakeNode(2, { MakeFrame(0x080, 2, 700, 100) }));
        net.Nodes.push_back(MakeNode(3));
        EXPECT_TRUE(WriteLogCsv(RunSim(net, durationUs), path));
        return ReadText(path);
    }
}

// 根拠: spec.md US1 受け入れシナリオ 1・SC-001、FR-004。ログの内容は、ファイルと同じ（行の順・区切りのカンマを含む）で表示される。
// 索引から読み出した全行が、ファイルを "\r\n" で区切った行と 1 文字も違わない（索引の間隔 256 行をまたぐ数の行で確かめる）。
TEST(F025Spec, F025_US1_1_ViewerLines_IdenticalToFile)
{
    RecordProperty("spec", "025:US1-1");
    const std::wstring path = TempPath(L"f025_us1_1.canprj.log.csv");
    const std::vector<std::string> fileLines = SplitLines(WriteRunLog(path, 300000LL));
    ASSERT_GT(fileLines.size(), 3u * LogFileIndex::kIndexStride) << "前提: 行が索引の間隔をまたがない";
    LogFileIndex index;
    ASSERT_TRUE(index.Build(path)) << "索引を作れない";
    ASSERT_EQ(index.LineCount(), static_cast<long long>(fileLines.size())) << "行数がファイルと違う";
    const std::vector<std::string> all = index.ReadLines(0, static_cast<int>(fileLines.size()));
    ASSERT_EQ(all.size(), fileLines.size());
    for (std::size_t i = 0; i < all.size(); ++i)
        ASSERT_EQ(all[i], fileLines[i]) << i << " 行目がファイルと違う";
    // 途中から（索引の境目の前後）読み出しても同じ
    const std::vector<std::string> mid = index.ReadLines(LogFileIndex::kIndexStride - 2, 5);
    ASSERT_EQ(mid.size(), 5u);
    for (int k = 0; k < 5; ++k)
        EXPECT_EQ(mid[static_cast<std::size_t>(k)], fileLines[static_cast<std::size_t>(LogFileIndex::kIndexStride - 2 + k)])
            << "索引の境目の前後の行";
}

// 根拠: spec.md US1 受け入れシナリオ 2・3、FR-007。表示しても（索引を作って読み出しても）ログファイルは変更されない。
TEST(F025Spec, F025_US1_3_Viewing_DoesNotChangeFile)
{
    RecordProperty("spec", "025:US1-3");
    const std::wstring path = TempPath(L"f025_us1_3.canprj.log.csv");
    const std::string before = WriteRunLog(path, 50000LL);
    LogFileIndex index;
    ASSERT_TRUE(index.Build(path));
    index.ReadLines(0, 100);
    index.ReadLines(index.LineCount() - 3, 10);
    EXPECT_EQ(ReadText(path), before) << "表示でログファイルが変わった";
}

// 根拠: spec.md US1 受け入れシナリオ 4、FR-003（表示するのはそのプロジェクトのフレームログ <プロジェクトファイル名>.log.csv）。
// 別のプロジェクトを読み込むと、新しいプロジェクトのログを探す。
TEST(F025Spec, F025_US1_4_LogPath_FollowsCurrentProject)
{
    RecordProperty("spec", "025:US1-4");
    EXPECT_EQ(LogPathFor(L"C:\\work\\a.canprj"), L"C:\\work\\a.canprj.log.csv") << "プロジェクト a のログ";
    EXPECT_EQ(LogPathFor(L"D:\\data\\b.canprj"), L"D:\\data\\b.canprj.log.csv") << "プロジェクト b のログ";
}

// 根拠: spec.md SC-001、FR-004。実行後のログは、行数・先頭と最後の行がファイルと一致する。
TEST(F025Spec, F025_SC001_LineCountFirstAndLast_MatchFile)
{
    RecordProperty("spec", "025:SC-001");
    const std::wstring path = TempPath(L"f025_sc001.canprj.log.csv");
    const std::vector<std::string> fileLines = SplitLines(WriteRunLog(path, 200000LL));
    LogFileIndex index;
    ASSERT_TRUE(index.Build(path));
    ASSERT_EQ(index.LineCount(), static_cast<long long>(fileLines.size())) << "行数";
    EXPECT_EQ(index.ReadLines(0, 1).front(), fileLines.front()) << "先頭の行（ヘッダー）";
    EXPECT_EQ(index.ReadLines(index.LineCount() - 1, 1).front(), fileLines.back()) << "最後の行";
    EXPECT_TRUE(index.ReadLines(index.LineCount(), 5).empty()) << "最後より後の行が読み出せた";
}
