// feature_025_tests.cpp（tests）
// tools/cansim_runner/checks.cpp の CheckFeature025() から移した（機能 030）。
// 本体は、パスの取り方以外は変えていない。Check() のラベルには SC/FR の ID が残っている。

#include "test_helpers.h"

using namespace cansimtest;

TEST(Feature025, SC001_SC003)
{
    std::cout << "\n== 機能 025: シミュレーションログ表示（行の索引） ==\n";
    {
        const std::string empty = Join(TempDir(), "log025_empty.csv");
        WriteRaw(empty, "");
        LogFileIndex index;
        const bool built = index.Build(Widen(empty));
        LogFileIndex missing;
        Check(built && index.LineCount() == 0 && index.ReadLines(0, 10).empty() &&
            !missing.Build(Widen(Join(TempDir(), "log025_no_such_file.csv"))),
            "025: 空のログは 0 行。無いファイルでは索引を作れない");
    }

    // 長さの違う 1000 行。LF / CRLF / 最後に改行なし
    std::vector<std::string> lines;
    std::size_t longest = 0;
    for (int i = 0; i < 1000; ++i)
    {
        std::string s = "line," + std::to_string(i) + "," + std::string(static_cast<std::size_t>(i % 37), 'x');
        longest = (std::max)(longest, s.size());
        lines.push_back(s);
    }
    std::string lf, crlf;
    for (const std::string& s : lines)
    {
        lf += s + "\n";
        crlf += s + "\r\n";
    }
    const std::string noFinal = lf.substr(0, lf.size() - 1);
    bool same = true;
    bool clipped = true;
    const std::string names[] = { "log025_lf.csv", "log025_crlf.csv", "log025_nofinal.csv" };
    const std::string* texts[] = { &lf, &crlf, &noFinal };
    for (int k = 0; k < 3; ++k)
    {
        const std::string path = Join(TempDir(), names[k]);
        WriteRaw(path, *texts[k]);
        LogFileIndex index;
        same = same && index.Build(Widen(path)) && index.LineCount() == 1000 &&
            index.MaxLineLength() == static_cast<int>(longest);
        const std::vector<std::string> all = index.ReadLines(0, 1000);
        same = same && all == lines;
        const std::vector<std::string> a = index.ReadLines(255, 3);
        const std::vector<std::string> b = index.ReadLines(256, 1);
        const std::vector<std::string> c = index.ReadLines(998, 10);
        const std::vector<std::string> d = index.ReadLines(-5, 7);
        clipped = clipped && a.size() == 3 && a[0] == lines[255] && a[2] == lines[257] &&
            b.size() == 1 && b[0] == lines[256] &&
            c.size() == 2 && c[1] == lines[999] &&
            d.size() == 2 && d[0] == lines[0] && d[1] == lines[1] &&
            index.ReadLines(1000, 5).empty() && index.ReadLines(10, 0).empty();
    }
    Check(same, "025: LF・CRLF・最後の改行なしのどれも、同じ 1000 行（CR なし）と同じ最長行になる");
    Check(clipped, "025: 256 行ごとの区切りをまたぐ ReadLines と、両端での ReadLines は、ファイルの範囲に切られる");

    {
        // ちょうど 512 行: 終わりより先を指す区切りがあってはならない
        std::string text;
        for (int i = 0; i < 512; ++i)
            text += "r" + std::to_string(i) + "\n";
        const std::string path = Join(TempDir(), "log025_512.csv");
        WriteRaw(path, text);
        LogFileIndex index;
        const std::vector<std::string> tail = index.Build(Widen(path)) ? index.ReadLines(510, 5) : std::vector<std::string>();
        Check(index.LineCount() == 512 && tail.size() == 2 && tail[1] == "r511",
            "025: ちょうど区切りの位置で終わるファイルでも、最後の行を読める");
    }

    {
        // 実際のフレームログ（sample_16ecu、10 秒）
        const std::string prefix = Join(TempDir(), "log025_16ecu");
        bool ok = RunProjectToCsv(Join(SampleDir(), "sample_16ecu.canprj"), prefix, 10000000LL);
        const std::string path = prefix + ".log.csv";
        std::ifstream in(path.c_str());
        std::string line, firstLine, lastLine;
        long long count = 0;
        while (std::getline(in, line))
        {
            if (!line.empty() && line.back() == '\r')
                line.pop_back();
            if (count == 0)
                firstLine = line;
            lastLine = line;
            ++count;
        }
        LogFileIndex index;
        ok = ok && index.Build(Widen(path)) && index.LineCount() == count && count > 1000;
        const std::vector<std::string> head = index.ReadLines(0, 1);
        const std::vector<std::string> tail = index.ReadLines(count - 1, 1);
        ok = ok && head.size() == 1 && head[0] == firstLine && tail.size() == 1 && tail[0] == lastLine;
        std::cout << "  sample_16ecu 10 秒のログ: " << count << " 行\n";
        Check(ok, "025 SC-001: 実際のフレームログの索引は、ファイルと同じ行数・最初の行・最後の行になる");
    }

    {
        // 約 90 文字の 100 万行: 索引を作る時間（表示だけ）
        const std::string path = Join(TempDir(), "log025_big.csv");
        {
            std::ofstream out(path.c_str(), std::ios::binary);
            const std::string pad(80, 'y');
            for (int i = 0; i < 1000000; ++i)
                out << i << ',' << pad << "\r\n";
        }
        LogFileIndex index;
        const auto t0 = std::chrono::steady_clock::now();
        const bool built = index.Build(Widen(path));
        const auto t1 = std::chrono::steady_clock::now();
        const std::vector<std::string> tail = index.ReadLines(999990, 20);
        std::cout << "  100 万行の索引作成: "
                  << std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count() << " ms\n";
        Check(built && index.LineCount() == 1000000 && tail.size() == 10 &&
            tail[9] == "999999," + std::string(80, 'y'),
            "025 SC-003: 100 万行のログの索引を作れ、最後の行を読める");
        std::remove(path.c_str());
    }
}
