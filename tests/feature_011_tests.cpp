// feature_011_tests.cpp（tests）
// tools/cansim_runner/checks.cpp の CheckFeature011() から移した（機能 030）。
// 本体は、パスの取り方以外は変えていない。Check() のラベルには SC/FR の ID が残っている。

#include "test_helpers.h"

using namespace cansimtest;

TEST(Feature011, FR008to012)
{
    std::cout << "\n== 機能 011: 構成図の枠 ==\n";
    const int dpis[] = { 96, 120, 144 };
    bool inArea = true;
    bool clear = true;
    for (int n = 0; n <= kMaxEcuCount; ++n)
    {
        const BusDiagramLayout layout = ComputeBusDiagramLayout(n);
        for (int dpi : dpis)
        {
            const int w = (std::max)(1, ScaleToDpi(1, dpi));
            const int fl = ScaleToDpi(layout.Frame.Left, dpi);
            const int ft = ScaleToDpi(layout.Frame.Top, dpi);
            const int fr = ScaleToDpi(layout.Frame.Right, dpi);
            const int fb = ScaleToDpi(layout.Frame.Bottom, dpi);
            inArea = inArea && fl >= 0 && ft >= 0 && fr <= ScaleToDpi(kDiagramWidth, dpi) &&
                fb <= ScaleToDpi(kDiagramHeight, dpi);

            // 描くものはすべて、線から 1 px 以上内側になければならない:
            // [fl + w, fr - w) x [ft + w, fb - w) が空いている内側。
            auto inside = [&](int l, int t, int r, int b) {
                return ScaleToDpi(l, dpi) > fl + w && ScaleToDpi(t, dpi) > ft + w &&
                    ScaleToDpi(r, dpi) < fr - w && ScaleToDpi(b, dpi) < fb - w;
            };
            clear = clear && inside(layout.BusLine.Left, layout.BusLine.Top, layout.BusLine.Right, layout.BusLine.Bottom);
            // "CAN BUS": ダイアログが文字を描く枠。上の余白より下
            clear = clear && inside(kCaptionLeft, 76, kCaptionRight + 40, layout.BusLine.Top - 2);
            for (const EcuPlacement& ecu : layout.Ecus)
            {
                clear = clear && inside(ecu.Box.Left, ecu.Box.Top, ecu.Box.Right, ecu.Box.Bottom);
                clear = clear && inside(ecu.LineX, (std::min)(ecu.LineY1, ecu.LineY2), ecu.LineX + 1,
                    (std::max)(ecu.LineY1, ecu.LineY2));
            }
        }
    }
    Check(inArea, "011 FR-008: 枠は構成図の領域の内側にある（ECU 0..16、DPI 96/120/144）");
    Check(clear, "011 FR-010/FR-011: バス線・文字・ECU の箱・接続線は枠に触れない");

    // ログ表示エリアの枠: グレーの領域とスクロールバーの外側に、
    // 線の太さのあと 1 px 以上空けて、図の下に置く。
    bool logClear = true;
    bool sameAsBefore = true;
    for (int dpi : dpis)
    {
        const int w = (std::max)(1, ScaleToDpi(1, dpi));
        const DiagramRect view = ScaleLogViewRect(dpi);
        const DiagramRect frame = ScaleLogFrameRect(dpi);
        logClear = logClear && frame.Left + w < view.Left && frame.Top + w < view.Top &&
            frame.Right - w > view.Right && frame.Bottom - w > view.Bottom &&
            frame.Top > ScaleToDpi(kDiagramHeight, dpi);
        // 以前のダイアログと同じ位置（left / top + 換算後の大きさ）
        sameAsBefore = sameAsBefore && view.Left == ScaleToDpi(150, dpi) && view.Top == ScaleToDpi(260, dpi) &&
            view.Right == view.Left + ScaleToDpi(850, dpi) && view.Bottom == view.Top + ScaleToDpi(260, dpi);
    }
    Check(logClear, "011 FR-009/FR-011: ログ表示エリアの枠は、グレーの領域・スクロールバー・構成図に触れない（DPI 96/120/144）");
    Check(sameAsBefore, "011 FR-012: ログ表示エリアは 009 の位置を保つ（96 DPI で 150, 260, 850 x 260）");
}
