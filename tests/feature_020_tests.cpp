// feature_020_tests.cpp（tests）
// tools/cansim_runner/checks.cpp の CheckFeature020() から移した（機能 030）。
// 本体は、パスの取り方以外は変えていない。Check() のラベルには SC/FR の ID が残っている。

#include "test_helpers.h"

using namespace cansimtest;

TEST(Feature020, Checks)
{
    std::cout << "\n== 機能 020: イベント＆ログ表示のラベル ==\n";
    const int dpis[] = { 96, 120, 144 };
    bool logOk = true;
    for (int dpi : dpis)
    {
        const DiagramRect frame = ScaleLogFrameRect(dpi);
        const DiagramRect g = ScaleLogLabelRect(dpi);
        logOk = logOk && g.Top >= ScaleToDpi(kDiagramHeight, dpi) && g.Bottom <= frame.Top &&
            g.Left == frame.Left && g.Right <= frame.Right && g.Bottom - g.Top >= ScaleToDpi(16, dpi);
    }
    Check(logOk, "020: イベント＆ログ表示のラベルは、構成図とログ表示エリアの枠の間で、枠の左端に置かれる（DPI 96/120/144）");
}
