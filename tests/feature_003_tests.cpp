// feature_003_tests.cpp（tests）
// tools/cansim_runner/checks.cpp の CheckFeature003() から移した（機能 030）。
// 本体は、パスの取り方以外は変えていない。Check() のラベルには SC/FR の ID が残っている。

#include "test_helpers.h"

using namespace cansimtest;

TEST(Feature003, SC001to002_SC006_FR005_FR009_FR011)
{
    std::cout << "\n== 機能 003: バス構成図のレイアウト ==\n";

    const int counts[] = { 0, 1, 6, 16 };
    bool countsOk = true;
    for (int n : counts)
        countsOk = countsOk && ComputeBusDiagramLayout(n).Ecus.size() == static_cast<std::size_t>(n);
    Check(countsOk, "003 SC-002: ECU 数 0/1/6/16 が配置された ECU の数と一致する");

    const BusDiagramLayout full = ComputeBusDiagramLayout(16);
    bool sideOk = true, pitchOk = true, insideOk = InsideDiagram(full.BusLine), clearOk = true;
    for (std::size_t i = 0; i < full.Ecus.size(); ++i)
    {
        const EcuPlacement& e = full.Ecus[i];
        sideOk = sideOk && e.Number == static_cast<int>(i) + 1 && e.OnTop == (e.Number % 2 == 1) &&
                 (e.OnTop ? e.Box.Bottom <= full.BusLine.Top : e.Box.Top >= full.BusLine.Bottom);
        if (i > 0)
            pitchOk = pitchOk && e.LineX - full.Ecus[i - 1].LineX == kEcuPitch;
        insideOk = insideOk && InsideDiagram(e.Box);
        clearOk = clearOk && e.LineX > kCaptionRight;
    }
    Check(sideOk, "003 FR-005: 奇数番の ECU はバスの上、偶数番は下に、番号順に並ぶ");
    Check(pitchOk, "003 FR-009: ECU の中心間隔は一定の 56 px");
    Check(full.Ecus.front().LineX == 150 && full.Ecus.back().LineX == 990,
        "003 FR-011: ECU 01 の接続線は x=150、ECU 16 は x=990（バスの両端に 140 px の余白）");
    Check(full.Ecus.front().LineX - full.BusLine.Left == full.BusLine.Right - full.Ecus.back().LineX,
        "003 FR-011: バスの余白は左右対称");
    Check(clearOk, "003 SC-006: すべての接続線が CAN BUS の文字の右側にある（x > 118）");
    Check(insideOk, "003 SC-001: バスと 16 個の ECU の箱が 1140x216 の図に収まる");

    bool stableOk = true;
    for (int n = 1; n <= 16; ++n)
    {
        const BusDiagramLayout part = ComputeBusDiagramLayout(n);
        for (std::size_t i = 0; i < part.Ecus.size(); ++i)
        {
            const EcuPlacement& a = part.Ecus[i];
            const EcuPlacement& b = full.Ecus[i];
            stableOk = stableOk && a.LineX == b.LineX && a.Box.Left == b.Box.Left &&
                       a.Box.Top == b.Box.Top && a.OnTop == b.OnTop;
        }
    }
    Check(stableOk, "003 FR-009: ECU n の位置は ECU 数によらず変わらない");

    Check(ComputeBusDiagramLayout(-1).Ecus.empty() && ComputeBusDiagramLayout(17).Ecus.size() == 16,
        "003: ECU 数は 0〜16 に丸められる（-1 -> 0、17 -> 16）");
}
