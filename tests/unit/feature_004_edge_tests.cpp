// feature_004_edge_tests.cpp（tests/unit）
// 機能 004 の補足のテスト（/coverage）。受け入れシナリオ・成功基準とは別に、spec.md の FR と contracts/ に根拠がある動きを確かめる。

#include "support/spec_test_helpers.h"

#include "ProjectFile.h"

using namespace CanSim;
using namespace spectest;

// 根拠: spec.md FR-010（形式不正は読み込まない）、contracts/project-file-format.md・001 の config-input-format.md（セクションは
// [Project]・[Network]・[NodeN]・[NodeN.FrameM]、値は整数）。形の崩れたセクション名・空の値・数でない値・桁あふれは読み込まない。
TEST(F004Edge, F004_FR010_MalformedSectionsAndValues_Rejected)
{
    RecordProperty("spec", "004:FR-010");
    const std::string frame = "[Node1.Frame1]\r\nCanId=0x100\r\nDlc=8\r\nPeriodUs=10000\r\n";
    const std::string cases[] = {
        "[Node]\r\nNodeId=1\r\n",
        "[NodeA]\r\nNodeId=1\r\n",
        "[Node1]\r\nNodeId=1\r\n[Node1.Frame]\r\nCanId=0x100\r\n",
        "[Node1]\r\nNodeId=1\r\n[Node1.FrameZ]\r\nCanId=0x100\r\n",
        "[Node1]\r\nNodeId=1\r\n[NodeX.Frame1]\r\nCanId=0x100\r\n",
        "[Node1]\r\nNodeId=1\r\n[Foo.Bar]\r\nCanId=0x100\r\n",
        "[Node1]\r\nNodeId=\r\n" + frame,
        "[Node1]\r\nNodeId=1x\r\n" + frame,
        "[Node1]\r\nNodeId=99999999999999999999999999\r\n" + frame,
        "[Node1]\r\nNodeId=1\r\nReceiveDelayUs=12abc\r\n" + frame,
    };
    int i = 0;
    for (const std::string& text : cases)
    {
        const ConfigLoadResult r = LoadText(L"f004e_fr010_" + std::to_wstring(i) + L".canprj", text);
        EXPECT_FALSE(r.Success) << i << " 番目の形の崩れたファイルが読み込めてしまった";
        EXPECT_FALSE(r.ErrorMessage.empty()) << i << " 番目: 理由が無い";
        ++i;
    }
}

// 根拠: spec.md SC-003（定義が空のまま保存した場合は、読み直せるようにフレームの無いノード（ECU 1〜ECU 数、ECU 数 0 なら ECU 1）が
// 加わる。実行結果も保存前と同じ）、contracts/project-file-format.md。
TEST(F004Edge, F004_SC003_NoDefinitions_SavedProjectReloads)
{
    RecordProperty("spec", "004:SC-003");
    for (int ecuCount : { 2, 0 })
    {
        ProjectSettings s;
        s.EcuCount = ecuCount;
        const Network empty;
        const std::wstring path = TempPath(L"f004e_sc003_nodefs.canprj");
        ASSERT_TRUE(SaveProject(path, s, empty)) << "保存できない（ECU 数 " << ecuCount << "）";
        const ConfigLoadResult r = LoadConfig(path);
        ASSERT_TRUE(r.Success) << "読み直せない（ECU 数 " << ecuCount << "）: " << r.ErrorMessage;
        const std::size_t expectedNodes = ecuCount == 0 ? 1u : static_cast<std::size_t>(ecuCount);
        ASSERT_EQ(r.Config.Nodes.size(), expectedNodes) << "加わったノードの数";
        for (const Node& n : r.Config.Nodes)
            EXPECT_TRUE(n.Frames.empty()) << "加わったノードにフレームがある";
        const Network before = BuildRunNetwork(s, empty);
        const Network after = BuildRunNetwork(r.Project, r.Config);
        ASSERT_EQ(after.Nodes.size(), before.Nodes.size()) << "実行用ネットワークのノード数が保存前と違う";
        for (std::size_t i = 0; i < before.Nodes.size(); ++i)
            EXPECT_EQ(after.Nodes[i].Name, before.Nodes[i].Name) << "実行用ネットワークのノード名が保存前と違う";
    }
}
