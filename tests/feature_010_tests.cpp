// feature_010_tests.cpp（tests）
// tools/cansim_runner/checks.cpp の CheckFeature010() から移した（機能 030）。
// 本体は、パスの取り方以外は変えていない。Check() のラベルには SC/FR の ID が残っている。

#include "test_helpers.h"

using namespace cansimtest;

TEST(Feature010, SC001_SC004to005_FR005)
{
    std::cout << "\n== 機能 010: ECU の当たり判定 ==\n";
    const int dpis[] = { 96, 120, 144 };

    // (a) ScaleToDpi = (px * dpi + 48) / 96
    {
        bool same = true;
        for (int dpi : dpis)
            for (int px = 0; px <= kDiagramWidth; ++px)
                same = same && ScaleToDpi(px, dpi) == (px * dpi + 48) / 96;
        Check(same, "010 research 決定 2: ScaleToDpi = (px * dpi + 48) / 96");
    }

    // (b) 内側の角と中心はその ECU に当たり、1 px 外側は当たらない
    {
        bool inside = true;
        bool outside = true;
        for (int n = 1; n <= kMaxEcuCount; ++n)
        {
            const BusDiagramLayout layout = ComputeBusDiagramLayout(n);
            for (int dpi : dpis)
            {
                for (const EcuPlacement& ecu : layout.Ecus)
                {
                    const int l = ScaleToDpi(ecu.Box.Left, dpi);
                    const int t = ScaleToDpi(ecu.Box.Top, dpi);
                    const int r = ScaleToDpi(ecu.Box.Right, dpi);
                    const int b = ScaleToDpi(ecu.Box.Bottom, dpi);
                    const int k = ecu.Number;
                    inside = inside && FindEcuAt(layout, l, t, dpi) == k && FindEcuAt(layout, r - 1, t, dpi) == k &&
                        FindEcuAt(layout, l, b - 1, dpi) == k && FindEcuAt(layout, r - 1, b - 1, dpi) == k &&
                        FindEcuAt(layout, (l + r) / 2, (t + b) / 2, dpi) == k;
                    outside = outside && FindEcuAt(layout, l - 1, (t + b) / 2, dpi) != k &&
                        FindEcuAt(layout, (l + r) / 2, t - 1, dpi) != k &&
                        FindEcuAt(layout, r, (t + b) / 2, dpi) != k && FindEcuAt(layout, (l + r) / 2, b, dpi) != k;
                }
            }
        }
        Check(inside, "010 SC-001/SC-005: すべての箱の内側の角と中心がその ECU に当たる（ECU 1..16、DPI 96/120/144）");
        Check(outside, "010 SC-004: すべての箱の 1 px 外側はその ECU に当たらない");
    }

    // (c) バス線・文字・接続線・余白は何にも当たらない
    {
        const BusDiagramLayout layout = ComputeBusDiagramLayout(16);
        bool none = true;
        for (int dpi : dpis)
        {
            none = none && FindEcuAt(layout, ScaleToDpi(570, dpi), ScaleToDpi(kBusY, dpi), dpi) == 0;
            none = none && FindEcuAt(layout, ScaleToDpi(66, dpi), ScaleToDpi(93, dpi), dpi) == 0;
            none = none && FindEcuAt(layout, ScaleToDpi(1135, dpi), ScaleToDpi(210, dpi), dpi) == 0;
            for (const EcuPlacement& ecu : layout.Ecus)
                none = none && FindEcuAt(layout, ScaleToDpi(ecu.LineX, dpi),
                    ScaleToDpi((ecu.LineY1 + ecu.LineY2) / 2, dpi), dpi) == 0;
        }
        Check(none, "010 FR-005: バス線・文字・接続線・余白はどの ECU にも当たらない");
    }

    // (d) ECU が無いとき: 図のどこも当たらない
    {
        const BusDiagramLayout layout = ComputeBusDiagramLayout(0);
        bool none = true;
        for (int dpi : dpis)
            for (int y = 0; y < ScaleToDpi(kDiagramHeight, dpi); y += 10)
                for (int x = 0; x < ScaleToDpi(kDiagramWidth, dpi); x += 10)
                    none = none && FindEcuAt(layout, x, y, dpi) == 0;
        Check(none, "010: ECU が 0 個なら何にも当たらない");
    }

    // (e) ECU が N 個のとき、N を超える番号は返らない
    {
        bool within = true;
        for (int n = 0; n <= kMaxEcuCount; ++n)
        {
            const BusDiagramLayout layout = ComputeBusDiagramLayout(n);
            for (int y = 0; y < kDiagramHeight; y += 2)
                for (int x = 0; x < kDiagramWidth; x += 2)
                {
                    const int k = FindEcuAt(layout, x, y, 96);
                    within = within && k >= 0 && k <= n;
                }
        }
        Check(within, "010: ECU が N 個なら当たるのは ECU 1..N だけ");
    }
}
