// feature_024_tests.cpp（tests）
// tools/cansim_runner/checks.cpp の CheckFeature024() から移した（機能 030）。
// 本体は、パスの取り方以外は変えていない。Check() のラベルには SC/FR の ID が残っている。

#include "test_helpers.h"

using namespace cansimtest;

TEST(Feature024, FR001to002_FR005)
{
    std::cout << "\n== 機能 024: タイトルのプロジェクトファイル名、起動時に渡すプロジェクト ==\n";
    // 実際のラベルは .rc の日本語の文字列。ここでは ASCII の代わりの文字列で並びを確かめる。
    Check(ComposeWindowTitle(L"CAN Bus Simulator", L"LABEL:", L"") == L"CAN Bus Simulator    LABEL:" &&
        ComposeWindowTitle(L"CAN Bus Simulator", L"LABEL:", L"C:\\work\\a.canprj") ==
            L"CAN Bus Simulator    LABEL:C:\\work\\a.canprj",
        "024 FR-001/FR-002: タイトル = アプリ名 + 半角空白 4 つ + ラベル + フルパス。プロジェクトが無いときはラベルで終わる");

    const std::wstring exe = L"MFCApplication1.exe";
    Check(FindStartupProjectArg({}).empty() && FindStartupProjectArg({ exe }).empty() &&
        FindStartupProjectArg({ exe, L"a.canprj" }) == L"a.canprj" &&
        FindStartupProjectArg({ exe, L"", L"a.canprj", L"b.canprj" }) == L"a.canprj",
        "024 FR-005: プログラムの次の最初の空でない引数が起動時のプロジェクト。後ろの引数は無視する");

    Check(HasProjectExtension(L"a.canprj") && HasProjectExtension(L"A.CANPRJ") &&
        HasProjectExtension(L"C:\\x\\b.CanPrj") && !HasProjectExtension(L"a.canprj.txt") &&
        !HasProjectExtension(L"README.md") && !HasProjectExtension(L"canprj") && !HasProjectExtension(L""),
        "024 FR-005: HasProjectExtension は大文字小文字を問わず .canprj を受け付け、ほかの拡張子は拒否する");
}
