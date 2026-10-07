// feature_027_tests.cpp（tests）
// tools/cansim_runner/checks.cpp の CheckFeature027() から移した（機能 030）。
// 本体は、パスの取り方以外は変えていない。Check() のラベルには SC/FR の ID が残っている。

#include "test_helpers.h"

using namespace cansimtest;

TEST(Feature027, FR002_FR005to006_FR009)
{
    std::cout << "\n== 機能 027: ログクリア ==\n";
    const std::vector<std::wstring> names = ResultFilePaths(L"C:\\x\\a.canprj");
    Check(names.size() == 3 && names[0] == L"C:\\x\\a.canprj.log.csv" && names[0] == LogPathFor(L"C:\\x\\a.canprj") &&
        names[1] == L"C:\\x\\a.canprj.summary.csv" && names[2] == L"C:\\x\\a.canprj.startup.csv",
        "027 FR-002: ResultFilePaths はプロジェクトの隣のフレームログ・集計・起動記録を、この順に返す");
    Check(ExistingResultFiles(L"").empty(), "027 FR-005: プロジェクトファイルが無ければ結果ファイルも無い");

    const std::wstring project = Widen(Join(TempDir(), "t027_clear.canprj"));
    const std::vector<std::wstring> paths = ResultFilePaths(project);
    auto touch = [](const std::wstring& path) { std::ofstream(path.c_str()) << "x\n"; };
    auto exists = [](const std::wstring& path) { struct _stat64 st; return _wstat64(path.c_str(), &st) == 0; };
    // 以前の実行の残り
    _wchmod(paths[2].c_str(), _S_IREAD | _S_IWRITE);
    for (const std::wstring& p : paths) _wremove(p.c_str());
    _wrmdir(paths[1].c_str());

    touch(project);
    touch(paths[0]);
    _wmkdir(paths[1].c_str()); // 集計と同じ名前のフォルダーは結果ファイルではない
    touch(paths[2]);
    const std::vector<std::wstring> existing = ExistingResultFiles(project);
    Check(existing.size() == 2 && existing[0] == paths[0] && existing[1] == paths[2],
        "027 FR-002: ExistingResultFiles は通常のファイルだけを順に返す（同じ名前のフォルダーは除く）");

    _wchmod(paths[2].c_str(), _S_IREAD);
    const LogClearResult result = DeleteResultFiles(existing);
    Check(result.Deleted.size() == 1 && result.Deleted[0] == paths[0] &&
        result.Failed.size() == 1 && result.Failed[0] == paths[2] &&
        !exists(paths[0]) && exists(paths[2]),
        "027 FR-006: DeleteResultFiles は消せるものを消し、読み取り専用のファイルは属性を変えずに失敗として報告する");
    Check(exists(project) && exists(paths[1]), "027 FR-009: プロジェクトファイルとほかのものには触れない");

    _wchmod(paths[2].c_str(), _S_IREAD | _S_IWRITE);
    const LogClearResult rest = DeleteResultFiles(ExistingResultFiles(project));
    Check(rest.Deleted.size() == 1 && rest.Failed.empty() && ExistingResultFiles(project).empty(),
        "027 FR-002: 書き込み可能にすれば消え、そのあと結果ファイルは残らない");
    _wrmdir(paths[1].c_str());
    _wremove(project.c_str());
}
