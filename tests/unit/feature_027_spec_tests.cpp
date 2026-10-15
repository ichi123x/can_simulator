// feature_027_spec_tests.cpp（tests/unit）
// 機能 027（specs/027-log-clear/spec.md）の受け入れシナリオと成功基準のうち、画面を使わずに確かめられるもの。
// 削除する結果ファイルの探し方と削除は LogFileIndex.h（ResultFilePaths・ExistingResultFiles・DeleteResultFiles）で確かめる。
// 確認・通知のメッセージ、ウィンドウを閉じること、ログ表示エリアを空にすることは画面の確認。

#include "support/spec_test_helpers.h"

#include "LogFileIndex.h"

#include <windows.h>

using namespace CanSim;
using namespace spectest;

namespace
{
    // 一時フォルダーに、プロジェクトファイルと結果ファイルを置く。results で置く結果ファイル（0 ログ・1 集計・2 起動記録）を選ぶ
    std::wstring Prepare(const std::wstring& name, std::vector<int> results)
    {
        const std::wstring project = TempPath(name + L".canprj");
        WriteText(project, "[Node1]\r\nNodeId=1\r\n");
        const std::vector<std::wstring> paths = ResultFilePaths(project);
        for (const std::wstring& p : paths)
        {
            SetFileAttributesW(p.c_str(), FILE_ATTRIBUTE_NORMAL);
            DeleteFileW(p.c_str());
        }
        for (int i : results)
            WriteText(paths[static_cast<std::size_t>(i)], "x\r\n");
        return project;
    }
}

// 根拠: spec.md US1 受け入れシナリオ 2、FR-002。削除の後は、そのプロジェクトのログファイルは無い（ログ表示ではログが無いことが通知される）。
TEST(F027Spec, F027_US1_2_AfterClear_NoLogLeft)
{
    RecordProperty("spec", "027:US1-2");
    const std::wstring project = Prepare(L"f027_us1_2", { 0, 1, 2 });
    const LogClearResult r = DeleteResultFiles(ExistingResultFiles(project));
    EXPECT_TRUE(r.Failed.empty()) << "削除できないファイルがある";
    EXPECT_FALSE(FileExists(LogPathFor(project))) << "ログファイルが残っている";
    EXPECT_TRUE(ExistingResultFiles(project).empty()) << "結果ファイルが残っている";
}

// 根拠: spec.md US1 受け入れシナリオ 3、FR-002・FR-009。シミュレーションで出力される 3 つのファイル（フレームログ・集計・起動記録）のうち、
// あるものをすべて削除する。プロジェクトファイル（.canprj）は削除しない。2 つだけあるときは、その 2 つだけが対象。
TEST(F027Spec, F027_US1_3_DeletesExistingResults_KeepsProjectFile)
{
    RecordProperty("spec", "027:US1-3");
    const std::wstring project = Prepare(L"f027_us1_3", { 0, 2 });
    const std::vector<std::wstring> paths = ResultFilePaths(project);
    const std::vector<std::wstring> existing = ExistingResultFiles(project);
    EXPECT_EQ(existing, (std::vector<std::wstring>{ paths[0], paths[2] })) << "あるものだけが対象になっていない";
    const LogClearResult r = DeleteResultFiles(existing);
    EXPECT_EQ(r.Deleted, existing) << "削除したファイル";
    EXPECT_TRUE(FileExists(project)) << "プロジェクトファイルが削除された";
}

// 根拠: spec.md US2 受け入れシナリオ 1、FR-005。プロジェクトファイルが無いときは、削除の対象が無く、何も削除しない。
TEST(F027Spec, F027_US2_1_NoProjectFile_NothingToDelete)
{
    RecordProperty("spec", "027:US2-1");
    EXPECT_TRUE(ExistingResultFiles(L"").empty()) << "プロジェクトファイルが無いのに対象がある";
}

// 根拠: spec.md US2 受け入れシナリオ 2、FR-005。削除の対象のファイルが 1 つも無いときは、対象が無い（ログが無いことを通知し、確認は出さない）。
TEST(F027Spec, F027_US2_2_NoResultFiles_NothingToDelete)
{
    RecordProperty("spec", "027:US2-2");
    const std::wstring project = Prepare(L"f027_us2_2", {});
    EXPECT_TRUE(ExistingResultFiles(project).empty()) << "結果ファイルが無いのに対象がある";
}

// 根拠: spec.md US2 受け入れシナリオ 3、FR-006、LogFileIndex.h（属性は変えないので、読み取り専用のファイルは削除できない側に入る）。
// 削除できないファイルがあったら、そのファイル名がわかる。削除できたファイルは削除されたままになる。
TEST(F027Spec, F027_US2_3_ReadOnlyFile_ReportedOthersDeleted)
{
    RecordProperty("spec", "027:US2-3");
    const std::wstring project = Prepare(L"f027_us2_3", { 0, 1, 2 });
    const std::vector<std::wstring> paths = ResultFilePaths(project);
    ASSERT_TRUE(SetFileAttributesW(paths[1].c_str(), FILE_ATTRIBUTE_READONLY)) << "前提: 読み取り専用にできない";
    const LogClearResult r = DeleteResultFiles(ExistingResultFiles(project));
    EXPECT_EQ(r.Failed, (std::vector<std::wstring>{ paths[1] })) << "削除できなかったファイルとして集計が示されない";
    EXPECT_EQ(r.Deleted, (std::vector<std::wstring>{ paths[0], paths[2] })) << "削除できたファイル";
    EXPECT_FALSE(FileExists(paths[0])) << "ログが削除されたままでない";
    EXPECT_FALSE(FileExists(paths[2])) << "起動記録が削除されたままでない";
    EXPECT_TRUE(FileExists(paths[1])) << "読み取り専用の集計が消えた";
    EXPECT_NE(GetFileAttributesW(paths[1].c_str()) & FILE_ATTRIBUTE_READONLY, 0u) << "読み取り専用の属性が変えられた";
    SetFileAttributesW(paths[1].c_str(), FILE_ATTRIBUTE_NORMAL); // 後片付け
    DeleteFileW(paths[1].c_str());
}

// 根拠: spec.md SC-001、FR-002。ログのあるプロジェクトで、1 回の削除で 3 つのファイルが削除される（ログ表示エリアは画面の確認）。
TEST(F027Spec, F027_SC001_OneClear_AllThreeDeleted)
{
    RecordProperty("spec", "027:SC-001");
    const std::wstring project = Prepare(L"f027_sc001", { 0, 1, 2 });
    const LogClearResult r = DeleteResultFiles(ExistingResultFiles(project));
    EXPECT_EQ(r.Deleted.size(), 3u) << "3 つのファイルが削除されない";
    EXPECT_TRUE(r.Failed.empty());
    for (const std::wstring& p : ResultFilePaths(project))
        EXPECT_FALSE(FileExists(p)) << "結果ファイルが残っている";
}

// 根拠: spec.md SC-002、FR-009。プロジェクトファイル・ほかのファイル（同じフォルダーの別のプロジェクトの結果、名前の似たファイル）は、
// どの場合でも削除・変更されない。
TEST(F027Spec, F027_SC002_ProjectAndOtherFiles_Untouched)
{
    RecordProperty("spec", "027:SC-002");
    const std::wstring project = Prepare(L"f027_sc002", { 0, 1, 2 });
    const std::wstring other = TempPath(L"f027_sc002_other.canprj.log.csv");
    const std::wstring similar = TempPath(L"f027_sc002.canprj.log.csv.bak");
    WriteText(other, "other\r\n");
    WriteText(similar, "similar\r\n");
    const std::string projectBefore = ReadText(project);
    DeleteResultFiles(ExistingResultFiles(project));
    EXPECT_EQ(ReadText(project), projectBefore) << "プロジェクトファイルが変わった";
    EXPECT_EQ(ReadText(other), "other\r\n") << "別のプロジェクトのログが削除・変更された";
    EXPECT_EQ(ReadText(similar), "similar\r\n") << "名前の似たファイルが削除・変更された";
}
