// 機能 027（ログクリア）の仕様テスト
// 根拠: specs/027-log-clear/spec.md と LogFileIndex.h のコメント。仕様に ID が無いので、
// 受け入れシナリオは US<ストーリー番号>-<シナリオ番号> と呼ぶ。
// 確認・通知・ログ表示エリアの消去は画面側なのでテストせず、対象のファイルの決め方と削除を確かめる。

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "LogFileIndex.h"
#include "support/test_helpers.h"

using namespace CanSim;
using namespace TestHelpers;

namespace
{
    bool Exists(const std::wstring& path)
    {
        return GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES;
    }

    // %TEMP% の下に作業用のフォルダーを作る（前回の残りは消しておく）
    std::wstring MakeWorkDir(const std::string& name)
    {
        const std::wstring dir = TempPath(name);
        CreateDirectoryW(dir.c_str(), nullptr);
        return dir + L"\\";
    }
}

// ---------------------------------------------------------------------------
// US1-3 / FR-002: 対象はプロジェクトファイルの隣の 3 つ（フレームログ・集計・起動記録）、この順。
// 根拠: spec.md US1 シナリオ 3、FR-002、LogFileIndex.h（ResultFilePaths）、CLAUDE.md の出力ファイル名。
// ---------------------------------------------------------------------------
TEST(F027Spec, F027_FR002_ResultFilePaths_ThreeFilesInOrder)
{
    RecordProperty("spec", "027:FR-002");
    EXPECT_EQ((std::vector<std::wstring>{L"C:\\w\\a.canprj.log.csv", L"C:\\w\\a.canprj.summary.csv",
                                         L"C:\\w\\a.canprj.startup.csv"}),
              ResultFilePaths(L"C:\\w\\a.canprj"));
}

// ---------------------------------------------------------------------------
// US1-3 / US2-2 / FR-005: あるものだけが対象。1 つも無ければ空（ログが無い通知になる）。
//   同じ名前のフォルダーはファイルとして数えない。プロジェクトファイルが無い（空のパス）なら空。
// 根拠: spec.md US1 シナリオ 3、US2 シナリオ 1・2、FR-005、LogFileIndex.h（ExistingResultFiles）。
// ---------------------------------------------------------------------------
TEST(F027Spec, F027_US2_2_ExistingResultFiles_OnlyRegularFiles)
{
    RecordProperty("spec", "027:US2-2");
    RecordProperty("spec", "027:FR-005");
    const std::wstring dir = MakeWorkDir("f027_existing");
    const std::wstring project = dir + L"p.canprj";
    WriteAll(project, "[Project]\n");
    for (const std::wstring& p : ResultFilePaths(project)) { DeleteFileW(p.c_str()); RemoveDirectoryW(p.c_str()); }

    EXPECT_TRUE(ExistingResultFiles(project).empty()) << "1 つも無ければ空";
    EXPECT_TRUE(ExistingResultFiles(L"").empty()) << "プロジェクトファイルが無いときは空";

    const std::vector<std::wstring> all = ResultFilePaths(project);
    WriteAll(all[2], "startup");
    CreateDirectoryW(all[1].c_str(), nullptr); // 集計と同じ名前のフォルダー
    WriteAll(all[0], "log");
    EXPECT_EQ((std::vector<std::wstring>{all[0], all[2]}), ExistingResultFiles(project))
        << "あるファイルだけ、同じ順。フォルダーは数えない";

    RemoveDirectoryW(all[1].c_str());
    DeleteFileW(all[0].c_str());
    DeleteFileW(all[2].c_str());
    DeleteFileW(project.c_str());
    RemoveDirectoryW(dir.c_str());
}

// ---------------------------------------------------------------------------
// US1-3 / SC-001 / FR-009 / SC-002: 3 つを削除し、プロジェクトファイルと他のファイルは残る。
// US2-3 / FR-006 / Edge Cases: 読み取り専用のファイルは属性を変えずに「削除できなかった」側に入り、
//   削除できたものは削除されたまま。
// 根拠: spec.md US1 シナリオ 3、US2 シナリオ 3、FR-006、FR-009、SC-001、SC-002、Edge Cases（読み取り専用）、
//        LogFileIndex.h（DeleteResultFiles）。
// ---------------------------------------------------------------------------
TEST(F027Spec, F027_SC002_DeleteResultFiles_ProjectKeptReadOnlyFails)
{
    RecordProperty("spec", "027:SC-001");
    RecordProperty("spec", "027:SC-002");
    RecordProperty("spec", "027:US2-3");
    const std::wstring dir = MakeWorkDir("f027_delete");
    const std::wstring project = dir + L"p.canprj";
    const std::wstring other = dir + L"other.csv";
    WriteAll(project, "[Project]\n");
    WriteAll(other, "keep");
    const std::vector<std::wstring> files = ResultFilePaths(project);
    for (const std::wstring& p : files) { SetFileAttributesW(p.c_str(), FILE_ATTRIBUTE_NORMAL); WriteAll(p, "x"); }
    SetFileAttributesW(files[1].c_str(), FILE_ATTRIBUTE_READONLY);

    const LogClearResult r = DeleteResultFiles(ExistingResultFiles(project));
    EXPECT_EQ((std::vector<std::wstring>{files[0], files[2]}), r.Deleted) << "削除できたもの（順どおり）";
    EXPECT_EQ((std::vector<std::wstring>{files[1]}), r.Failed) << "読み取り専用は削除できなかった側";
    EXPECT_FALSE(Exists(files[0])) << "フレームログは削除された";
    EXPECT_FALSE(Exists(files[2])) << "起動記録は削除された";
    EXPECT_TRUE(Exists(files[1])) << "読み取り専用の集計は残る";
    EXPECT_NE(0u, GetFileAttributesW(files[1].c_str()) & FILE_ATTRIBUTE_READONLY) << "属性は変えない";
    EXPECT_EQ("[Project]\n", ReadAll(project)) << "プロジェクトファイルは変わらない";
    EXPECT_EQ("keep", ReadAll(other)) << "ほかのファイルは変わらない";

    SetFileAttributesW(files[1].c_str(), FILE_ATTRIBUTE_NORMAL);
    DeleteFileW(files[1].c_str());
    DeleteFileW(project.c_str());
    DeleteFileW(other.c_str());
    RemoveDirectoryW(dir.c_str());
}
