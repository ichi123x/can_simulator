// check_support.h（tests/support）
// Check() は tools/cansim_runner/checks.cpp と同じ呼び出しの形を保つので、機能ごとのテストは
// そのまま移せる。失敗したチェックは、ラベルを表示する GoogleTest の失敗になる。
// 環境変数 CANSIM_TEST_VERBOSE=1 を設定すると、通ったチェックも checks.exe と同じ形の
// "[PASS] <ラベル>" で表示するので、2 つの出力を項目ごとに突き合わせられる。
#pragma once

#include <gtest/gtest.h>

#include <cstdlib>
#include <iostream>
#include <string>

namespace cansimtest
{
    inline bool VerboseChecks()
    {
        static const bool verbose = std::getenv("CANSIM_TEST_VERBOSE") != nullptr;
        return verbose;
    }

    inline void Check(bool ok, const std::string& what)
    {
        if (VerboseChecks())
            std::cout << (ok ? "[PASS] " : "[FAIL] ") << what << "\n";
        EXPECT_TRUE(ok) << what;
    }
}
