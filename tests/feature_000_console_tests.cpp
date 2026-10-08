// feature_000_console_tests.cpp（tests）
// テストの日本語の出力を、Windows のコンソールで文字化けさせないための設定。
// ソースは UTF-8（/utf-8）で書いているので、コンソールの文字コードも UTF-8 にそろえる。
// テストはここには置かない（名前は CMake が拾う形 feature_*_tests.cpp に合わせている）。

#ifdef _WIN32
#include <windows.h>

namespace
{
    // main より前に 1 回だけ実行される
    const bool kConsoleUtf8 = []() {
        SetConsoleOutputCP(CP_UTF8);
        return true;
    }();
}
#endif