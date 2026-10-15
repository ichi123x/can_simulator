// console_utf8.cpp（tests/support）
// テストの日本語の出力を、Windows のコンソールで文字化けさせないための設定。
// ソースは UTF-8（/utf-8）で書いているので、コンソールの文字コードも UTF-8 にそろえる。
// tests/CMakeLists.txt の add_gtest が、すべてのテストの実行ファイルにこのファイルを入れる。
// テストはここには置かない。
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