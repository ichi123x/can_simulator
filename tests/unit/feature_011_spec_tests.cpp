// feature_011_spec_tests.cpp（tests/unit）
// 機能 011（specs/011-ecu-feedback-frames/spec.md）の受け入れシナリオと成功基準のうち、画面を使わずに確かめられるもの。
// 2 つの枠の位置は BusDiagramLayout.h（構成図の Frame、ScaleLogViewRect・ScaleLogFrameRect）で確かめる。
// ECU のグレー表示（タイマー・描画）は画面の確認。

#include "support/spec_test_helpers.h"

#include "BusDiagramLayout.h"

using namespace CanSim;
using namespace spectest;

namespace
{
    DiagramRect Scale(const DiagramRect& r, int dpi)
    {
        DiagramRect s;
        s.Left = ScaleToDpi(r.Left, dpi);
        s.Top = ScaleToDpi(r.Top, dpi);
        s.Right = ScaleToDpi(r.Right, dpi);
        s.Bottom = ScaleToDpi(r.Bottom, dpi);
        return s;
    }

    // 中身 inner が、枠 frame の線（1 px、Left〜Right−1 の範囲の縁）に重ならずに内側にある
    bool StrictlyInside(const DiagramRect& inner, const DiagramRect& frame)
    {
        return inner.Left > frame.Left && inner.Top > frame.Top && inner.Right < frame.Right && inner.Bottom < frame.Bottom;
    }

    // 構成図の枠の内側にあるべき中身（ECU・バス線）を、指定の ECU 数で集める。「CAN BUS」の文字は縦の位置の定数が無いので、
    // 横の範囲（kCaptionLeft〜kCaptionRight）だけを別に確かめる
    std::vector<DiagramRect> DiagramContents(int ecuCount)
    {
        const BusDiagramLayout l = ComputeBusDiagramLayout(ecuCount);
        std::vector<DiagramRect> out = { l.BusLine };
        for (const EcuPlacement& e : l.Ecus)
            out.push_back(e.Box);
        return out;
    }
}

// 根拠: spec.md US3 受け入れシナリオ 1、FR-008・FR-009。構成図の領域の周囲と、ログ表示エリア（スクロールバーを含む）の周囲に、
// それぞれ枠がある（96 dpi）。構成図の中身は構成図の枠の内側、ログ表示エリアはログの枠の内側。
TEST(F011Spec, F011_US3_1_TwoFrames_AroundDiagramAndLogView)
{
    RecordProperty("spec", "011:US3-1");
    const BusDiagramLayout l = ComputeBusDiagramLayout(16);
    for (const DiagramRect& c : DiagramContents(16))
        EXPECT_TRUE(StrictlyInside(c, l.Frame)) << "構成図の中身が枠の内側にない";
    EXPECT_GT(kCaptionLeft, l.Frame.Left) << "「CAN BUS」の文字の左端が枠の内側にない";
    EXPECT_LT(kCaptionRight, l.Frame.Right) << "「CAN BUS」の文字の右端が枠の内側にない";
    EXPECT_TRUE(StrictlyInside(ScaleLogViewRect(96), ScaleLogFrameRect(96))) << "ログ表示エリアが枠の内側にない";
    EXPECT_LE(l.Frame.Bottom, ScaleLogFrameRect(96).Top) << "構成図の枠とログの枠が重なる";
}

// 根拠: spec.md US3 受け入れシナリオ 2、FR-010。ECU 数を 0〜16 に変えても、枠は常に同じ位置で、中身と重ならない。
TEST(F011Spec, F011_US3_2_EveryEcuCount_SameFrameNoOverlap)
{
    RecordProperty("spec", "011:US3-2");
    const DiagramRect ref = ComputeBusDiagramLayout(16).Frame;
    for (int n = 0; n <= 16; ++n)
    {
        const DiagramRect f = ComputeBusDiagramLayout(n).Frame;
        EXPECT_TRUE(f.Left == ref.Left && f.Top == ref.Top && f.Right == ref.Right && f.Bottom == ref.Bottom)
            << "ECU 数 " << n << " で構成図の枠の位置が変わった";
        for (const DiagramRect& c : DiagramContents(n))
            EXPECT_TRUE(StrictlyInside(c, f)) << "ECU 数 " << n << " で中身が枠に重なる";
    }
}

// 根拠: spec.md US3 受け入れシナリオ 3、FR-011。拡大率 100%・125%・150% で、枠は拡大率に合わせた位置で、中身と重ならない。
TEST(F011Spec, F011_US3_3_EachDpi_FramesScaledNoOverlap)
{
    RecordProperty("spec", "011:US3-3");
    for (int dpi : { 96, 120, 144 })
    {
        const DiagramRect frame = Scale(ComputeBusDiagramLayout(16).Frame, dpi);
        for (const DiagramRect& c : DiagramContents(16))
            EXPECT_TRUE(StrictlyInside(Scale(c, dpi), frame)) << dpi << " DPI: 構成図の中身が枠に重なる";
        EXPECT_TRUE(StrictlyInside(ScaleLogViewRect(dpi), ScaleLogFrameRect(dpi))) << dpi << " DPI: ログ表示エリアが枠に重なる";
    }
}

// 根拠: spec.md SC-005、FR-010・FR-011。拡大率 100%・125%・150%、ECU 数 0・6・16 のすべての組み合わせで、2 つの枠があり、中身と重ならない。
TEST(F011Spec, F011_SC005_DpiAndEcuCounts_FramesNeverOverlapContents)
{
    RecordProperty("spec", "011:SC-005");
    for (int dpi : { 96, 120, 144 })
    {
        for (int n : { 0, 6, 16 })
        {
            const DiagramRect frame = Scale(ComputeBusDiagramLayout(n).Frame, dpi);
            for (const DiagramRect& c : DiagramContents(n))
                EXPECT_TRUE(StrictlyInside(Scale(c, dpi), frame)) << dpi << " DPI・ECU 数 " << n << ": 構成図の中身が枠に重なる";
            EXPECT_TRUE(StrictlyInside(ScaleLogViewRect(dpi), ScaleLogFrameRect(dpi)))
                << dpi << " DPI・ECU 数 " << n << ": ログ表示エリアが枠に重なる";
            EXPECT_LE(frame.Bottom, ScaleLogFrameRect(dpi).Top) << dpi << " DPI・ECU 数 " << n << ": 2 つの枠が重なる";
        }
    }
}
