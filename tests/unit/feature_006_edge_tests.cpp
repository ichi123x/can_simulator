// feature_006_edge_tests.cpp（tests/unit）
// 機能 006 の補足のテスト（/coverage）。受け入れシナリオ・成功基準とは別に、spec.md の FR・Edge Cases と contracts/ に根拠がある動きを確かめる。

#include "support/spec_test_helpers.h"

#include "FrameEditor.h"

using namespace CanSim;
using namespace spectest;

namespace
{
    FrameInput Input(const std::string& id, const std::string& dlc, const std::string& periodMs, const std::string& offsetMs = "")
    {
        FrameInput in;
        in.Name = "F";
        in.NameChars = 1;
        in.IdText = id;
        in.DlcText = dlc;
        in.PeriodMsText = periodMs;
        in.OffsetMsText = offsetMs;
        return in;
    }

    bool HasIssue(const std::vector<FrameIssue>& issues, FrameField field, FrameProblem problem)
    {
        for (const FrameIssue& i : issues)
            if (i.Field == field && i.Problem == problem)
                return true;
        return false;
    }
}

// 根拠: spec.md FR-006（NG の項目と理由を示す）、FrameEditor.h（理由: 空・数でない・範囲外 など）。空欄・数でない値は、その項目の
// 「空」「数でない」として NG になる。
TEST(F006Edge, F006_FR006_EmptyOrNotNumber_FieldAndReason)
{
    RecordProperty("spec", "006:FR-006");
    struct Case
    {
        FrameInput In;
        FrameField Field;
        FrameProblem Problem;
        const char* Name;
    };
    const Case cases[] = {
        { Input("0xZZ", "8", "10"), FrameField::Id, FrameProblem::NotANumber, "ID が 16 進でない" },
        { Input("0x123", "", "10"), FrameField::Dlc, FrameProblem::Empty, "DLC が空" },
        { Input("0x123", "8", ""), FrameField::Period, FrameProblem::Empty, "周期が空" },
        { Input("0x123", "8", "1.5"), FrameField::Period, FrameProblem::NotANumber, "周期が整数でない" },
        { Input("0x123", "8", "10", "x"), FrameField::Offset, FrameProblem::NotANumber, "オフセットが数でない" },
    };
    Network defs;
    defs.Nodes.push_back(MakeNode(1));
    for (const Case& c : cases)
    {
        FrameDefinition f;
        EXPECT_TRUE(HasIssue(ValidateFrame(defs, 1, -1, c.In, f), c.Field, c.Problem)) << c.Name;
    }
}

// 根拠: spec.md Edge Cases（フレーム名の上限は 32 文字）、contracts/project-file-format.md（InitialOffsetUs は PeriodUs 未満）。
// ファイルから読み込む場合も、33 文字以上の名前・周期以上のオフセットは理由を示して読み込まない。
TEST(F006Edge, F006_FR006_FileWithLongNameOrOffsetNotBelowPeriod_Rejected)
{
    RecordProperty("spec", "006:FR-006");
    const std::string head = "[Node1]\r\nNodeId=1\r\n[Node1.Frame1]\r\nCanId=0x100\r\nDlc=8\r\nPeriodUs=10000\r\n";
    const ConfigLoadResult longName = LoadText(L"f006e_name33.canprj", head + "Name=" + std::string(33, 'n') + "\r\n");
    EXPECT_FALSE(longName.Success) << "33 文字の名前が読み込めてしまった";
    const ConfigLoadResult name32 = LoadText(L"f006e_name32.canprj", head + "Name=" + std::string(32, 'n') + "\r\n");
    EXPECT_TRUE(name32.Success) << "32 文字の名前が読み込めない: " << name32.ErrorMessage;
    const ConfigLoadResult offset = LoadText(L"f006e_offset.canprj", head + "InitialOffsetUs=10000\r\n");
    EXPECT_FALSE(offset.Success) << "周期と同じオフセットが読み込めてしまった";
    EXPECT_FALSE(offset.ErrorMessage.empty());
}
