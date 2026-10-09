// 機能 022（ECU ラベル名の表示）の仕様テスト
// 根拠: specs/022-ecu-name-display/spec.md（改訂: Name= ではなく Label= に保存）、FrameEditor.h、
//        CLAUDE.md（Label は空なら書かない、起動記録の Name とは別）。
// 入力欄・構成図の文字の描画は画面側なのでテストしない。

#include <gtest/gtest.h>

#include <string>

#include "BusDiagramLayout.h"
#include "FrameEditor.h"
#include "ProjectFile.h"
#include "StartupControl.h"
#include "support/test_helpers.h"

using namespace CanSim;
using namespace TestHelpers;

// ---------------------------------------------------------------------------
// FR-003 / SC-002: 11 文字以上の名前は先頭 10 文字と「...」、10 文字以下はそのまま。全角・半角とも 1 文字。
// 根拠: spec.md FR-003、SC-002、FrameEditor.h（EcuLabelForDiagram・CountUtf8Chars）。
// ---------------------------------------------------------------------------
TEST(F022Spec, F022_SC002_LongLabel_First10CharsAndEllipsis)
{
    RecordProperty("spec", "022:SC-002");
    EXPECT_EQ("Engine", EcuLabelForDiagram("Engine")) << "10 文字以下はそのまま";
    EXPECT_EQ("ABCDEFGHIJ", EcuLabelForDiagram("ABCDEFGHIJ")) << "ちょうど 10 文字";
    EXPECT_EQ("ABCDEFGHIJ...", EcuLabelForDiagram("ABCDEFGHIJK")) << "11 文字";
    EXPECT_EQ("ブレーキ/ESC", EcuLabelForDiagram("ブレーキ/ESC")) << "全角を含む 8 文字";
    EXPECT_EQ("あいうえおかきくけこ...", EcuLabelForDiagram("あいうえおかきくけこさ")) << "全角 11 文字";
    EXPECT_EQ("", EcuLabelForDiagram("")) << "空";
}

TEST(F022Spec, F022_SC002_CountUtf8Chars_FullAndHalfWidthCountOne)
{
    RecordProperty("spec", "022:SC-002");
    EXPECT_EQ(4, CountUtf8Chars("エンジン"));
    EXPECT_EQ(3, CountUtf8Chars("ECU"));
    EXPECT_EQ(5, CountUtf8Chars("メーターA"));
    EXPECT_EQ(0, CountUtf8Chars(""));
}

// FrameEditor.h / CLAUDE.md: 前後の空白（半角・タブ・全角 U+3000）を除き、32 文字まで。
TEST(F022Spec, F022_FR002_TrimAndLimitLabel)
{
    RecordProperty("spec", "022:FR-002");
    EXPECT_EQ("Engine ECU", TrimEcuLabel("  \tEngine ECU\t "));
    EXPECT_EQ("メーター", TrimEcuLabel("　メーター　")) << "全角空白も除く";
    EXPECT_EQ("", TrimEcuLabel("   "));
    const std::string forty(40, 'x');
    EXPECT_EQ(std::string(32, 'x'), LimitEcuLabel(forty)) << "32 文字まで";
    std::string jp;
    for (int i = 0; i < 33; ++i) jp += "あ";
    EXPECT_EQ(32, CountUtf8Chars(LimitEcuLabel(jp))) << "全角も 32 文字まで";
}

// ---------------------------------------------------------------------------
// FR-004 / SC-003: 箱は縦 48 px（96 dpi）で、16 ECU でも構成図の枠の中、バスと重ならない。
// 根拠: spec.md FR-004、SC-003、CLAUDE.md（上段 20〜68・下段 152〜200）、BusDiagramLayout.h（kEcuBoxHeight = 48）。
// ---------------------------------------------------------------------------
TEST(F022Spec, F022_SC003_TallerBoxes_InsideFrameClearOfBus)
{
    RecordProperty("spec", "022:SC-003");
    const BusDiagramLayout l = ComputeBusDiagramLayout(16);
    for (const EcuPlacement& e : l.Ecus)
    {
        EXPECT_EQ(48, e.Box.Bottom - e.Box.Top) << "ECU " << e.Number << " の箱の高さ";
        EXPECT_GT(e.Box.Top, l.Frame.Top) << "枠の中";
        EXPECT_LT(e.Box.Bottom, l.Frame.Bottom) << "枠の中";
        EXPECT_TRUE(e.Box.Bottom <= l.BusLine.Top || e.Box.Top >= l.BusLine.Bottom) << "バスと重ならない";
    }
}

// ---------------------------------------------------------------------------
// SC-004 / FR-006: ラベル名は保存・読込みで保たれる。空のラベルは書かない。
// FR-007: ラベル名は起動記録の CSV の名前（Name）に影響しない。
// 根拠: spec.md SC-004、FR-006（改訂で Label=）、FR-007、CLAUDE.md。
// ---------------------------------------------------------------------------
TEST(F022Spec, F022_SC004_LabelSavedAndLoaded_EmptyNotWritten)
{
    RecordProperty("spec", "022:SC-004");
    Network defs;
    Node a = MakeNode(1, {{0x100, 8, 10000, 0}});
    a.Label = "エンジン";
    Node b = MakeNode(2, {});
    b.Label = "";
    defs.Nodes = {a, b};
    ProjectSettings settings;
    settings.EcuCount = 2;
    const std::wstring path = TempPath("f022_sc004.canprj");
    ASSERT_TRUE(SaveProject(path, settings, defs));
    const std::string text = ReadAll(path);
    const ConfigLoadResult r = LoadConfig(path);
    DeleteFileW(path.c_str());
    ASSERT_TRUE(r.Success) << r.ErrorMessage;
    ASSERT_EQ(2u, r.Config.Nodes.size());
    EXPECT_EQ("エンジン", r.Config.Nodes[0].Label) << "ラベル名が保たれる";
    EXPECT_EQ("", r.Config.Nodes[1].Label);
    std::size_t count = 0;
    for (std::size_t p = text.find("Label="); p != std::string::npos; p = text.find("Label=", p + 1)) ++count;
    EXPECT_EQ(1u, count) << "空のラベルは書かない（Label= は 1 か所だけ）";
}

TEST(F022Spec, F022_FR007_Label_NotInStartupRecordName)
{
    RecordProperty("spec", "022:FR-007");
    Network net;
    net.BitRateBps = 500000;
    Node n = MakeNode(1, {});
    n.Name = "ECU1";
    n.Label = "メーター";
    net.Nodes.push_back(n);
    const std::vector<StartupRecord> recs = BuildStartupRecords(RunSim(net, 1000LL).After);
    ASSERT_EQ(1u, recs.size());
    EXPECT_EQ("ECU1", recs[0].NodeName) << "起動記録の名前は Name のまま（ラベル名ではない）";
}
