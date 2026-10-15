// feature_022_spec_tests.cpp（tests/unit）
// 機能 022（specs/022-ecu-name-display/spec.md）の受け入れシナリオと成功基準のうち、画面を使わずに確かめられるもの。
// 構成図に出す名前（FrameEditor.h の EcuLabelForDiagram など）、保存（ProjectFile.h）、箱の大きさと当たり判定（BusDiagramLayout.h）で
// 確かめる。入力欄・文字の大きさは画面の確認。

#include "support/spec_test_helpers.h"

#include "BusDiagramLayout.h"
#include "FrameEditor.h"
#include "ProjectFile.h"

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

// 根拠: spec.md US1 受け入れシナリオ 2、FR-003。名前「エンジン」（4 文字）は、構成図にそのまま表示される。
TEST(F022Spec, F022_US1_2_ShortName_ShownAsIs)
{
    RecordProperty("spec", "022:US1-2");
    EXPECT_EQ(EcuLabelForDiagram("エンジン"), "エンジン") << "10 文字以下の名前が変わった";
    EXPECT_EQ(CountUtf8Chars("エンジン"), 4) << "全角 1 文字を 1 と数えていない";
}

// 根拠: spec.md US1 受け入れシナリオ 3、FR-003。11 文字以上の名前（「エンジンコントロールユニット」14 文字）は、先頭 10 文字と「...」
// （「エンジンコントロール...」）。
TEST(F022Spec, F022_US1_3_LongName_FirstTenAndEllipsis)
{
    RecordProperty("spec", "022:US1-3");
    EXPECT_EQ(CountUtf8Chars("エンジンコントロールユニット"), 14);
    EXPECT_EQ(EcuLabelForDiagram("エンジンコントロールユニット"), "エンジンコントロール...") << "先頭 10 文字 + ... でない";
}

// 根拠: spec.md US1 受け入れシナリオ 4、Edge Cases（前後の空白は取り除く。空白だけの名前は名前なし）。名前が空白なら、
// 名前の行には何も出ない。
TEST(F022Spec, F022_US1_4_BlankName_NothingShown)
{
    RecordProperty("spec", "022:US1-4");
    EXPECT_EQ(EcuLabelForDiagram(""), "") << "空の名前";
    EXPECT_EQ(TrimEcuLabel("   "), "") << "空白だけの名前が名前なしにならない";
    EXPECT_EQ(TrimEcuLabel("　ブレーキ　"), "ブレーキ") << "前後の全角空白が取り除かれない";
    EXPECT_EQ(TrimEcuLabel(" \tメーター "), "メーター") << "前後の半角空白・タブが取り除かれない";
}

// 根拠: spec.md US1 受け入れシナリオ 5・SC-004、FR-006。名前を入力したプロジェクトを保存し、読み込み直すと、名前が保たれる。
// 名前の無い ECU は名前なしのまま。
TEST(F022Spec, F022_US1_5_NameSavedAndLoaded)
{
    RecordProperty("spec", "022:US1-5");
    Network defs;
    Node n1 = MakeNode(1);
    n1.Label = "エンジン";
    Node n2 = MakeNode(2);
    defs.Nodes = { n1, n2 };
    ProjectSettings s;
    const std::wstring path = TempPath(L"f022_us1_5.canprj");
    ASSERT_TRUE(SaveProject(path, s, defs));
    const ConfigLoadResult r = LoadConfig(path);
    ASSERT_TRUE(r.Success) << r.ErrorMessage;
    ASSERT_EQ(r.Config.Nodes.size(), 2u);
    EXPECT_EQ(r.Config.Nodes[0].Label, "エンジン") << "名前が保たれない";
    EXPECT_EQ(r.Config.Nodes[1].Label, "") << "名前の無い ECU に名前が付いた";
}

// 根拠: spec.md US1 受け入れシナリオ 6、FR-004・FR-005。箱は名前の行の分だけ縦に大きく（96 dpi で 48 px、BusDiagramLayout.h）、
// ダブルクリックの当たり判定は大きくなった箱の全体（名前の行のあたりを含む）で働く。
TEST(F022Spec, F022_US1_6_TallerBox_HitTestCoversNameRow)
{
    RecordProperty("spec", "022:US1-6");
    const BusDiagramLayout l = ComputeBusDiagramLayout(16);
    for (const EcuPlacement& e : l.Ecus)
    {
        EXPECT_EQ(e.Box.Bottom - e.Box.Top, kEcuBoxHeight) << "ECU " << e.Number << " の箱の高さ";
        const int cx = (e.Box.Left + e.Box.Right) / 2;
        EXPECT_EQ(FindEcuAt(l, cx, e.Box.Bottom - 6, 96), e.Number) << "ECU " << e.Number << " の名前の行のあたり";
        EXPECT_EQ(FindEcuAt(l, cx, e.Box.Top + 6, 96), e.Number) << "ECU " << e.Number << " の「ECU nn」の行のあたり";
    }
    EXPECT_EQ(kEcuBoxHeight, 48) << "箱の高さが 48 px でない";
}

// 根拠: spec.md SC-002、FR-003。11 文字以上の名前はすべて「先頭 10 文字 + ...」、10 文字以下はそのまま（境目の 10・11 文字と、
// 入力できる最大の 32 文字、半角・全角で確かめる）。
TEST(F022Spec, F022_SC002_LengthBoundary_TenKeptElevenCut)
{
    RecordProperty("spec", "022:SC-002");
    EXPECT_EQ(EcuLabelForDiagram("ABCDEFGHIJ"), "ABCDEFGHIJ") << "10 文字";
    EXPECT_EQ(EcuLabelForDiagram("ABCDEFGHIJK"), "ABCDEFGHIJ...") << "11 文字";
    EXPECT_EQ(EcuLabelForDiagram(std::string(32, 'x')), std::string(10, 'x') + "...") << "32 文字";
    EXPECT_EQ(EcuLabelForDiagram("あいうえおかきくけこ"), "あいうえおかきくけこ") << "全角 10 文字";
    EXPECT_EQ(EcuLabelForDiagram("あいうえおかきくけこさ"), "あいうえおかきくけこ...") << "全角 11 文字";
    EXPECT_EQ(CountUtf8Chars(LimitEcuLabel(std::string(40, 'y'))), 32) << "入力できるのは 32 文字まで";
}

// 根拠: spec.md SC-003、FR-004。ECU 数 16・DPI 96/120/144 で、箱が重ならず、構成図の枠の中に収まり、バスとも重ならない。
TEST(F022Spec, F022_SC003_SixteenEcus_BoxesInsideFrameNoOverlap)
{
    RecordProperty("spec", "022:SC-003");
    const BusDiagramLayout l = ComputeBusDiagramLayout(16);
    for (int dpi : { 96, 120, 144 })
    {
        const DiagramRect frame = Scale(l.Frame, dpi);
        const DiagramRect bus = Scale(l.BusLine, dpi);
        for (std::size_t i = 0; i < l.Ecus.size(); ++i)
        {
            const DiagramRect b = Scale(l.Ecus[i].Box, dpi);
            EXPECT_TRUE(b.Left > frame.Left && b.Top > frame.Top && b.Right < frame.Right && b.Bottom < frame.Bottom)
                << dpi << " DPI: ECU " << i + 1 << " が枠の中に収まらない";
            EXPECT_FALSE(Overlaps(b, bus)) << dpi << " DPI: ECU " << i + 1 << " がバスと重なる";
            for (std::size_t j = i + 1; j < l.Ecus.size(); ++j)
                EXPECT_FALSE(Overlaps(b, Scale(l.Ecus[j].Box, dpi))) << dpi << " DPI: ECU " << i + 1 << " と " << j + 1 << " が重なる";
        }
    }
}
