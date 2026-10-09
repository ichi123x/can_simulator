// 機能 024（タイトルのプロジェクトファイル名と、起動時の引数）の仕様テスト
// 根拠: specs/024-project-path-title/spec.md と ProjectFile.h のコメント。仕様に ID が無いので、
// 受け入れシナリオは US<ストーリー番号>-<シナリオ番号> と呼ぶ。
// タイトルバーの表示そのもの・起動の流れ（絶対パスへの変換・エラー表示）は画面側なのでテストしない。

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "ProjectFile.h"

using namespace CanSim;

// ---------------------------------------------------------------------------
// FR-001 / US1-2: タイトル = アプリ名 + 区切り（半角空白 4 つ）+ ラベル + フルパス。
// FR-002 / SC-004 / US1-1: プロジェクトファイルが無いときはラベルで終わる。
// 根拠: spec.md FR-001、FR-002、SC-004、ProjectFile.h（ComposeWindowTitle: four spaces）。
// ---------------------------------------------------------------------------
TEST(F024Spec, F024_FR001_TitleWithPath_AppSpacesLabelPath)
{
    RecordProperty("spec", "024:FR-001");
    EXPECT_EQ(std::wstring(L"CAN Bus Simulator    Project: C:\\work\\a.canprj"),
              ComposeWindowTitle(L"CAN Bus Simulator", L"Project: ", L"C:\\work\\a.canprj"))
        << "アプリ名 + 半角空白 4 つ + ラベル + パス";
}

TEST(F024Spec, F024_SC004_NoProjectFile_TitleEndsWithLabel)
{
    RecordProperty("spec", "024:SC-004");
    EXPECT_EQ(std::wstring(L"CAN Bus Simulator    Project: "), ComposeWindowTitle(L"CAN Bus Simulator", L"Project: ", L""))
        << "パスが空ならラベルで終わる";
}

// ---------------------------------------------------------------------------
// FR-005 / Edge Cases: 起動時の引数は、プログラムの次の最初の空でない引数を使い、2 つ目以降は無視する。
// 根拠: spec.md FR-005、Edge Cases（引数が 2 つ以上なら最初の 1 つだけ）、ProjectFile.h（FindStartupProjectArg）。
// ---------------------------------------------------------------------------
TEST(F024Spec, F024_FR005_StartupArg_FirstNonEmptyAfterProgram)
{
    RecordProperty("spec", "024:FR-005");
    EXPECT_EQ(std::wstring(L"a.canprj"), FindStartupProjectArg({L"app.exe", L"a.canprj", L"b.canprj"}))
        << "最初の 1 つだけ";
    EXPECT_EQ(std::wstring(L"b.canprj"), FindStartupProjectArg({L"app.exe", L"", L"b.canprj"})) << "空の引数は飛ばす";
    EXPECT_EQ(std::wstring(), FindStartupProjectArg({L"app.exe"})) << "引数なし（単体起動）";
    EXPECT_EQ(std::wstring(), FindStartupProjectArg({})) << "引数の一覧が空";
}

// FR-005 / US2-4: 拡張子が .canprj でなければ読み込まない（メニューの読込みと同じ検査）。大文字小文字は区別しない。
// 根拠: spec.md FR-005、US2 シナリオ 4、ProjectFile.h（HasProjectExtension: ASCII case-insensitive）。
TEST(F024Spec, F024_US2_4_ProjectExtension_CaseInsensitive)
{
    RecordProperty("spec", "024:US2-4");
    EXPECT_TRUE(HasProjectExtension(L"C:\\work\\a.canprj"));
    EXPECT_TRUE(HasProjectExtension(L"C:\\work\\A.CANPRJ")) << "大文字";
    EXPECT_TRUE(HasProjectExtension(L"b.CanPrj")) << "混在";
    EXPECT_FALSE(HasProjectExtension(L"C:\\work\\a.cfg")) << "別の拡張子";
    EXPECT_FALSE(HasProjectExtension(L"C:\\work\\a.canprj.bak")) << "途中にあるだけ";
    EXPECT_FALSE(HasProjectExtension(L"canprj")) << "ドットが無い";
    EXPECT_FALSE(HasProjectExtension(L"")) << "空";
}
