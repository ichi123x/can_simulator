// feature_024_spec_tests.cpp（tests/unit）
// 機能 024（specs/024-project-path-title/spec.md）の受け入れシナリオと成功基準のうち、画面を使わずに確かめられるもの。
// タイトルの組み立て（ComposeWindowTitle）、起動時の引数の選び方（FindStartupProjectArg）、拡張子の判定（HasProjectExtension）は
// ProjectFile.h で確かめる。タイトルバーの更新・絶対パスへの変換・エラーの表示は画面の確認。

#include "support/spec_test_helpers.h"

#include "ProjectFile.h"

using namespace CanSim;
using namespace spectest;

namespace
{
    const std::wstring kApp = L"CAN Bus Simulator";
    const std::wstring kLabel = L"プロジェクト名：";
}

// 根拠: spec.md US1 受け入れシナリオ 1・SC-004、FR-001・FR-002、ProjectFile.h（appTitle + 半角空白 4 つ + label + path。
// パスが空ならラベルで終わる）。単体起動の直後は「CAN Bus Simulator」の右に「プロジェクト名：」のラベルだけがあり、その右は空白。
TEST(F024Spec, F024_US1_1_NoProject_TitleEndsWithLabel)
{
    RecordProperty("spec", "024:US1-1");
    EXPECT_EQ(ComposeWindowTitle(kApp, kLabel, L""), L"CAN Bus Simulator    プロジェクト名：") << "プロジェクトが無いときのタイトル";
}

// 根拠: spec.md US1 受け入れシナリオ 2、FR-001・FR-003。C:\work\a.canprj を読み込むと、ラベルの右にそのパスが出る。
TEST(F024Spec, F024_US1_2_Loaded_TitleShowsFullPath)
{
    RecordProperty("spec", "024:US1-2");
    EXPECT_EQ(ComposeWindowTitle(kApp, kLabel, L"C:\\work\\a.canprj"), L"CAN Bus Simulator    プロジェクト名：C:\\work\\a.canprj")
        << "読み込んだときのタイトル";
}

// 根拠: spec.md US2 受け入れシナリオ 1、FR-005、ProjectFile.h（args[0] はプログラム自身。その次の最初の空でない引数を開く。
// 後ろの引数は使わない）。sample_16ecu.canprj を引数にして起動すると、そのファイルを読み込み、構成図は 16 ECU になる。
TEST(F024Spec, F024_US2_1_StartupArgument_ProjectLoaded)
{
    RecordProperty("spec", "024:US2-1");
    const std::wstring sample = SampleConfigPath(L"sample_16ecu.canprj");
    EXPECT_EQ(FindStartupProjectArg({ L"MFCApplication1.exe", sample }), sample) << "最初の引数が選ばれない";
    EXPECT_EQ(FindStartupProjectArg({ L"MFCApplication1.exe", L"", sample, L"other.canprj" }), sample)
        << "空の引数を飛ばして最初の空でない引数が選ばれない";
    EXPECT_EQ(FindStartupProjectArg({ L"MFCApplication1.exe" }), L"") << "引数なしの単体起動で何かが選ばれた";
    EXPECT_EQ(FindStartupProjectArg({}), L"") << "引数の一覧が空";
    ASSERT_TRUE(HasProjectExtension(sample));
    const ConfigLoadResult r = LoadConfig(sample);
    ASSERT_TRUE(r.Success) << r.ErrorMessage;
    EXPECT_EQ(r.Project.EcuCount, 16) << "sample_16ecu の ECU 数";
}

// 根拠: spec.md US2 受け入れシナリオ 4、FR-005・FR-006。引数のファイルが存在しない・拡張子が .canprj でない・内容が不正のときは、
// メニューからの読込みと同じ検査で読み込まれない（エラーの表示と、単体起動と同じ状態で続けることは画面の確認）。
TEST(F024Spec, F024_US2_4_BadStartupArgument_NotLoaded)
{
    RecordProperty("spec", "024:US2-4");
    EXPECT_FALSE(LoadConfig(TempPath(L"f024_missing.canprj")).Success) << "存在しないファイルが読み込めた";
    EXPECT_FALSE(HasProjectExtension(L"C:\\work\\a.txt")) << "拡張子が違うファイルを受け付けた";
    const ConfigLoadResult bad = LoadText(L"f024_bad.canprj", "not a project\r\n");
    EXPECT_FALSE(bad.Success) << "不正な内容が読み込めた";
    EXPECT_FALSE(bad.ErrorMessage.empty()) << "理由が無い";
}
