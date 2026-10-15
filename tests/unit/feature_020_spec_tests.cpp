// feature_020_spec_tests.cpp（tests/unit）
// 機能 020（specs/020-area-labels/spec.md）の受け入れシナリオと成功基準のうち、画面を使わずに確かめられるもの。
// ログ表示エリアのラベルの位置は BusDiagramLayout.h の ScaleLogLabelRect で確かめる。文字・色は画面の確認。
// 構成図の左上の「イベント表示」のラベルは、ユーザーの指示で外された（CLAUDE.md の機能 020）ため、ここでは扱わない。

#include "support/spec_test_helpers.h"

#include "BusDiagramLayout.h"

using namespace CanSim;
using namespace spectest;

namespace
{
    bool Overlaps(const DiagramRect& a, const DiagramRect& b)
    {
        return a.Left < b.Right && b.Left < a.Right && a.Top < b.Bottom && b.Top < a.Bottom;
    }

    DiagramRect Scale(const DiagramRect& r, int dpi)
    {
        DiagramRect s;
        s.Left = ScaleToDpi(r.Left, dpi);
        s.Top = ScaleToDpi(r.Top, dpi);
        s.Right = ScaleToDpi(r.Right, dpi);
        s.Bottom = ScaleToDpi(r.Bottom, dpi);
        return s;
    }
}

// 根拠: spec.md US1 受け入れシナリオ 2、FR-002。ログ表示エリアの枠のすぐ上の左端にラベルがあり、ログの行・スクロールバー
// （ログ表示エリア）とは重ならない（96 dpi）。
TEST(F020Spec, F020_US1_2_LogLabel_JustAboveFrameAtLeft)
{
    RecordProperty("spec", "020:US1-2");
    const DiagramRect label = ScaleLogLabelRect(96);
    const DiagramRect frame = ScaleLogFrameRect(96);
    EXPECT_EQ(label.Left, frame.Left) << "ラベルの左端が枠の左端と揃っていない";
    EXPECT_LE(label.Bottom, frame.Top) << "ラベルが枠の上にない";
    EXPECT_LE(frame.Top - label.Bottom, 8) << "ラベルが枠のすぐ上にない";
    EXPECT_FALSE(Overlaps(label, ScaleLogViewRect(96))) << "ラベルがログ表示エリアと重なる";
}

// 根拠: spec.md SC-001、FR-003。ラベルが、どの DPI（96/120/144）・ECU 数（0〜16）でも、ほかの描画物（ログ表示エリア・その枠・
// 構成図の ECU・バス線・構成図の枠）と重ならない。
TEST(F020Spec, F020_SC001_LogLabel_NoOverlapAtAnyDpiOrEcuCount)
{
    RecordProperty("spec", "020:SC-001");
    for (int dpi : { 96, 120, 144 })
    {
        const DiagramRect label = ScaleLogLabelRect(dpi);
        EXPECT_FALSE(Overlaps(label, ScaleLogViewRect(dpi))) << dpi << " DPI: ログ表示エリアと重なる";
        EXPECT_FALSE(Overlaps(label, ScaleLogFrameRect(dpi))) << dpi << " DPI: ログの枠と重なる";
        for (int n = 0; n <= 16; ++n)
        {
            const BusDiagramLayout l = ComputeBusDiagramLayout(n);
            EXPECT_FALSE(Overlaps(label, Scale(l.BusLine, dpi))) << dpi << " DPI・ECU 数 " << n << ": バス線と重なる";
            EXPECT_GE(label.Top, ScaleToDpi(l.Frame.Bottom, dpi)) << dpi << " DPI・ECU 数 " << n << ": 構成図の枠と重なる";
            for (const EcuPlacement& e : l.Ecus)
                EXPECT_FALSE(Overlaps(label, Scale(e.Box, dpi))) << dpi << " DPI・ECU 数 " << n << ": ECU " << e.Number << " と重なる";
        }
    }
}
