// feature_010_spec_tests.cpp（tests/unit）
// 機能 010（specs/010-ecu-doubleclick-edit/spec.md）の受け入れシナリオと成功基準のうち、画面を使わずに確かめられるもの。
// ダブルクリックが効く範囲は BusDiagramLayout.h の FindEcuAt（描画と同じ DPI の換算）、編集中の「開始」は SimControls.h、
// 定義の無い ECU への追加は FrameEditor.h で確かめる。ウィンドウの開閉・切り替え・時間は画面の確認。

#include "support/spec_test_helpers.h"

#include "BusDiagramLayout.h"
#include "FrameEditor.h"
#include "SimControls.h"

using namespace CanSim;
using namespace spectest;

namespace
{
    // ECU の箱の、換算後の範囲 [Left, Right) × [Top, Bottom)
    DiagramRect ScaledBox(const EcuPlacement& e, int dpi)
    {
        DiagramRect r;
        r.Left = ScaleToDpi(e.Box.Left, dpi);
        r.Top = ScaleToDpi(e.Box.Top, dpi);
        r.Right = ScaleToDpi(e.Box.Right, dpi);
        r.Bottom = ScaleToDpi(e.Box.Bottom, dpi);
        return r;
    }
}

// 根拠: spec.md US1 受け入れシナリオ 3、FR-002・FR-016（機能 006: 定義の無い ECU を作る）。送信フレームを持たない ECU
// （プロジェクトに定義が無い ECU を含む）を選んで「追加」でフレームを登録できる。
TEST(F010Spec, F010_US1_3_EcuWithoutDefinition_CanAddFrame)
{
    RecordProperty("spec", "010:US1-3");
    Network defs;
    defs.Nodes.push_back(MakeNode(1, { MakeFrame(0x100, 8, 10000) }));
    ASSERT_EQ(FindNodeDefinition(defs, 5), nullptr) << "前提: ECU 5 に定義がある";
    FrameInput in;
    in.Name = "New";
    in.NameChars = 3;
    in.IdText = "0x155";
    in.DlcText = "2";
    in.PeriodMsText = "20";
    FrameDefinition f;
    ASSERT_TRUE(ValidateFrame(defs, 5, -1, in, f).empty()) << "定義の無い ECU への追加が NG になった";
    AddFrame(defs, 5, f);
    const Node* n5 = FindNodeDefinition(defs, 5);
    ASSERT_NE(n5, nullptr) << "ECU 5 の定義が作られない";
    ASSERT_EQ(n5->Frames.size(), 1u) << "ECU 5 に追加されない";
    EXPECT_EQ(n5->Frames[0].CanId, 0x155u);
}

// 根拠: spec.md US2 受け入れシナリオ 1、FR-005。バス線・「CAN BUS」の文字・接続線・構成図の余白をダブルクリックしても何も開かない
// （ECU の箱の外は FindEcuAt が 0）。ログ表示エリアは構成図の外。
TEST(F010Spec, F010_US2_1_NonBoxAreas_NoEcu)
{
    RecordProperty("spec", "010:US2-1");
    const BusDiagramLayout l = ComputeBusDiagramLayout(16);
    const int dpi = 96;
    EXPECT_EQ(FindEcuAt(l, (l.BusLine.Left + l.BusLine.Right) / 2, (l.BusLine.Top + l.BusLine.Bottom) / 2, dpi), 0) << "バス線";
    EXPECT_EQ(FindEcuAt(l, (kCaptionLeft + kCaptionRight) / 2, kBusY - 10, dpi), 0) << "「CAN BUS」の文字";
    const EcuPlacement& e = l.Ecus[2];
    EXPECT_EQ(FindEcuAt(l, e.LineX, (e.LineY1 + e.LineY2) / 2, dpi), 0) << "接続線";
    EXPECT_EQ(FindEcuAt(l, 5, 5, dpi), 0) << "構成図の左上の余白";
    EXPECT_EQ(FindEcuAt(l, kLogViewLeft + 10, kLogViewTop + 10, dpi), 0) << "ログ表示エリア";
}

// 根拠: spec.md US2 受け入れシナリオ 4。ECU 数 0 の構成図（バスのみ）では、どこをダブルクリックしても何も開かない。
TEST(F010Spec, F010_US2_4_ZeroEcus_NothingAnywhere)
{
    RecordProperty("spec", "010:US2-4");
    const BusDiagramLayout l = ComputeBusDiagramLayout(0);
    for (int y = 0; y < kDiagramHeight; y += 4)
        for (int x = 0; x < kDiagramWidth; x += 4)
            ASSERT_EQ(FindEcuAt(l, x, y, 96), 0) << "(" << x << ", " << y << ") で ECU になった";
}

// 根拠: spec.md US3 受け入れシナリオ 6・SC-007、FR-013。編集ウィンドウが開いている間（切り替えの待ちを含む）は「開始」を選べない。
// 閉じると選べる。
TEST(F010Spec, F010_US3_6_WhileEditing_StartDisabled)
{
    RecordProperty("spec", "010:US3-6");
    EXPECT_FALSE(GetSimCommandStates(RunPhase::Idle, true).Start) << "編集中に「開始」が選べる";
    EXPECT_TRUE(GetSimCommandStates(RunPhase::Idle, false).Start) << "閉じた後に「開始」が選べない";
}

// 根拠: spec.md SC-002、FR-002・FR-004。ECU 数 1〜16 のそれぞれで、表示されているすべての ECU の箱をダブルクリックすると
// その番号の ECU になる（取り違え 0 件）。箱の中の何か所かで確かめる。
TEST(F010Spec, F010_SC002_EveryCountEveryBox_RightEcu)
{
    RecordProperty("spec", "010:SC-002");
    for (int n = 1; n <= 16; ++n)
    {
        const BusDiagramLayout l = ComputeBusDiagramLayout(n);
        for (const EcuPlacement& e : l.Ecus)
        {
            const DiagramRect b = ScaledBox(e, 96);
            const int cx = (b.Left + b.Right) / 2, cy = (b.Top + b.Bottom) / 2;
            EXPECT_EQ(FindEcuAt(l, cx, cy, 96), e.Number) << "ECU 数 " << n << " の ECU " << e.Number << " の真ん中";
            EXPECT_EQ(FindEcuAt(l, b.Left + 2, b.Top + 2, 96), e.Number) << "ECU 数 " << n << " の ECU " << e.Number << " の左上";
            EXPECT_EQ(FindEcuAt(l, b.Right - 3, b.Bottom - 3, 96), e.Number) << "ECU 数 " << n << " の ECU " << e.Number << " の右下";
        }
    }
}

// 根拠: spec.md SC-003、FR-005・FR-006。ECU の箱以外の場所へのダブルクリックで開く回数は 0 回。構成図の全体を調べ、ECU の箱の外で
// ECU になる点が無い（実行中のダブルクリックは画面の処理なので、画面の確認）。
TEST(F010Spec, F010_SC003_OutsideBoxes_NeverAnEcu)
{
    RecordProperty("spec", "010:SC-003");
    const BusDiagramLayout l = ComputeBusDiagramLayout(16);
    for (int y = 0; y < kDiagramHeight; ++y)
    {
        for (int x = 0; x < kDiagramWidth; x += 2)
        {
            bool inBox = false;
            for (const EcuPlacement& e : l.Ecus)
            {
                const DiagramRect b = ScaledBox(e, 96);
                if (x >= b.Left && x < b.Right && y >= b.Top && y < b.Bottom)
                    inBox = true;
            }
            if (!inBox)
                ASSERT_EQ(FindEcuAt(l, x, y, 96), 0) << "(" << x << ", " << y << ") は箱の外なのに ECU になった";
        }
    }
}

// 根拠: spec.md SC-005、FR-004。画面の拡大率 100%・125%・150%（96・120・144 dpi）のいずれでも、ECU の箱の縁の内側 1 px では開き、
// 外側 1 px では開かない。
TEST(F010Spec, F010_SC005_EachDpi_InnerEdgeOpensOuterEdgeNot)
{
    RecordProperty("spec", "010:SC-005");
    const BusDiagramLayout l = ComputeBusDiagramLayout(16);
    for (int dpi : { 96, 120, 144 })
    {
        for (const EcuPlacement& e : l.Ecus)
        {
            const DiagramRect b = ScaledBox(e, dpi);
            const int cx = (b.Left + b.Right) / 2, cy = (b.Top + b.Bottom) / 2;
            EXPECT_EQ(FindEcuAt(l, b.Left, cy, dpi), e.Number) << dpi << " DPI・ECU " << e.Number << ": 左の縁の内側";
            EXPECT_EQ(FindEcuAt(l, b.Right - 1, cy, dpi), e.Number) << dpi << " DPI・ECU " << e.Number << ": 右の縁の内側";
            EXPECT_EQ(FindEcuAt(l, cx, b.Top, dpi), e.Number) << dpi << " DPI・ECU " << e.Number << ": 上の縁の内側";
            EXPECT_EQ(FindEcuAt(l, cx, b.Bottom - 1, dpi), e.Number) << dpi << " DPI・ECU " << e.Number << ": 下の縁の内側";
            EXPECT_EQ(FindEcuAt(l, b.Left - 1, cy, dpi), 0) << dpi << " DPI・ECU " << e.Number << ": 左の縁の外側";
            EXPECT_EQ(FindEcuAt(l, b.Right, cy, dpi), 0) << dpi << " DPI・ECU " << e.Number << ": 右の縁の外側";
            EXPECT_EQ(FindEcuAt(l, cx, b.Top - 1, dpi), 0) << dpi << " DPI・ECU " << e.Number << ": 上の縁の外側";
            EXPECT_EQ(FindEcuAt(l, cx, b.Bottom, dpi), 0) << dpi << " DPI・ECU " << e.Number << ": 下の縁の外側";
        }
    }
}
