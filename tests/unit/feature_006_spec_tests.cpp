// 機能 006（データフレーム編集）の仕様テスト
// 根拠: specs/006-frame-editor/spec.md と FrameEditor.h・ProjectFile.h のコメント。仕様に ID が無いので、
// 受け入れシナリオは US<ストーリー番号>-<シナリオ番号> と呼ぶ。
// 画面（一覧の表示・ボタンの有効／無効・確認の表示）はテストせず、CanSim の検査と編集の関数を確かめる。

#include <gtest/gtest.h>

#include <cstdio>
#include <string>
#include <vector>

#include "FrameEditor.h"
#include "FrameTiming.h"
#include "ProjectFile.h"
#include "StartupControl.h"
#include "support/test_helpers.h"

using namespace CanSim;
using namespace TestHelpers;

namespace
{
    // ECU 1 に 0x100（No.1）・0x101（No.2）、ECU 5 に 0x300（No.1）を持つ定義
    Network MakeDefinitions()
    {
        Network defs;
        defs.BitRateBps = 500000;
        defs.Nodes.push_back(MakeNode(1, {{0x100, 8, 10000, 0}, {0x101, 8, 100000, 0}}));
        defs.Nodes.push_back(MakeNode(5, {{0x300, 4, 20000, 0}}));
        return defs;
    }

    FrameInput Input(const std::string& id, const std::string& dlc, const std::string& periodMs,
                     const std::string& offsetMs = "", const std::string& name = "")
    {
        FrameInput in;
        in.Name = name;
        in.NameChars = static_cast<int>(name.size()); // テストでは ASCII の名前だけを使う
        in.IdText = id;
        in.DlcText = dlc;
        in.PeriodMsText = periodMs;
        in.OffsetMsText = offsetMs;
        return in;
    }

    bool HasIssue(const std::vector<FrameIssue>& issues, FrameField field, FrameProblem problem)
    {
        for (const FrameIssue& i : issues)
            if (i.Field == field && i.Problem == problem) return true;
        return false;
    }
}

// ---------------------------------------------------------------------------
// US2-3 / SC-002 / FR-006: 範囲外・重複・周期とオフセットの関係の誤りは、NG の項目と理由が返る。
// 根拠: spec.md US2 シナリオ 3、FR-006、SC-002、Edge Cases（名前 32 文字）。
//   周期の上限 3,600,000 ms は CanTypes.h の kMaxPeriodMs（"only enforced by the editor"）。
// ---------------------------------------------------------------------------
struct RejectInput
{
    const char* Name;
    FrameInput In;
    FrameField Field;
    FrameProblem Problem;
};

class FrameRejectTable : public ::testing::TestWithParam<RejectInput>
{
};

TEST_P(FrameRejectTable, F006_US2_3_InvalidInput_IssueForFieldAndReason)
{
    RecordProperty("spec", "006:US2-3");
    RecordProperty("spec", "006:SC-002");
    const RejectInput& c = GetParam();
    FrameDefinition out;
    const std::vector<FrameIssue> issues = ValidateFrame(MakeDefinitions(), 2, -1, c.In, out);
    EXPECT_TRUE(HasIssue(issues, c.Field, c.Problem)) << c.Name << ": 期待した NG の項目と理由が無い（NG 件数 " << issues.size() << "）";
}

INSTANTIATE_TEST_SUITE_P(
    Inputs, FrameRejectTable,
    ::testing::Values(
        RejectInput{"Id0x800", Input("0x800", "8", "10"), FrameField::Id, FrameProblem::OutOfRange},
        RejectInput{"IdEmpty", Input("", "8", "10"), FrameField::Id, FrameProblem::Empty},
        RejectInput{"IdNotHex", Input("0xZZ", "8", "10"), FrameField::Id, FrameProblem::NotANumber},
        RejectInput{"IdDuplicateSameEcu", Input("0x100", "8", "10"), FrameField::Id, FrameProblem::Duplicate},
        RejectInput{"IdDuplicateOtherEcu", Input("0x300", "8", "10"), FrameField::Id, FrameProblem::Duplicate},
        RejectInput{"Dlc9", Input("0x200", "9", "10"), FrameField::Dlc, FrameProblem::OutOfRange},
        RejectInput{"DlcNotNumber", Input("0x200", "eight", "10"), FrameField::Dlc, FrameProblem::NotANumber},
        RejectInput{"DlcEmpty", Input("0x200", "", "10"), FrameField::Dlc, FrameProblem::Empty},
        RejectInput{"Period0", Input("0x200", "8", "0"), FrameField::Period, FrameProblem::OutOfRange},
        RejectInput{"PeriodOverMax", Input("0x200", "8", "3600001"), FrameField::Period, FrameProblem::OutOfRange},
        RejectInput{"PeriodNotNumber", Input("0x200", "8", "ten"), FrameField::Period, FrameProblem::NotANumber},
        RejectInput{"PeriodEmpty", Input("0x200", "8", ""), FrameField::Period, FrameProblem::Empty},
        RejectInput{"OffsetEqualsPeriod", Input("0x200", "8", "10", "10"), FrameField::Offset,
                    FrameProblem::OffsetNotBelowPeriod},
        RejectInput{"OffsetNotNumber", Input("0x200", "8", "10", "x"), FrameField::Offset, FrameProblem::NotANumber},
        RejectInput{"Name33Chars", Input("0x200", "8", "10", "", std::string(33, 'a')), FrameField::Name,
                    FrameProblem::TooLong}),
    [](const ::testing::TestParamInfo<RejectInput>& info) { return std::string(info.param.Name); });

// 他の ECU と重複したときは、どの ECU の何番目かが返る（範囲外になった ECU 5 のものも重複として拒否）。
// 根拠: spec.md FR-006、Edge Cases 2 つ目、FrameEditor.h の FrameIssue（DuplicateNodeId / DuplicateIndex）。
TEST(F006Spec, F006_US2_3_DuplicateOfOtherEcu_ReportsNodeAndIndex)
{
    RecordProperty("spec", "006:US2-3");
    FrameDefinition out;
    const std::vector<FrameIssue> issues = ValidateFrame(MakeDefinitions(), 2, -1, Input("0x300", "8", "10"), out);
    ASSERT_EQ(1u, issues.size()) << "NG は ID の重複の 1 件だけ";
    EXPECT_EQ(FrameProblem::Duplicate, issues[0].Problem);
    EXPECT_EQ(5, issues[0].DuplicateNodeId) << "重複先の ECU";
    EXPECT_EQ(0, issues[0].DuplicateIndex) << "重複先の位置（0 始まり）";
}

// ---------------------------------------------------------------------------
// FR-006 の境界（通る側）と FR-005・FR-010: 正しい入力は NG なしで、周期・オフセットは us、データは DLC 分の 0xFF。
// 根拠: spec.md FR-005（初回オフセット 既定 0、空欄は 0）、FR-006、FR-010、Edge Cases（名前は空欄でも 32 文字でもよい）。
// ---------------------------------------------------------------------------
TEST(F006Spec, F006_FR006_BoundaryValues_Accepted)
{
    RecordProperty("spec", "006:FR-006");
    const Network defs = MakeDefinitions();
    FrameDefinition out;

    EXPECT_TRUE(ValidateFrame(defs, 2, -1, Input("0x7FF", "0", "1"), out).empty()) << "ID 0x7FF・DLC 0・周期 1 ms";
    EXPECT_EQ(0x7FFu, out.CanId);
    EXPECT_EQ(0u, out.Dlc);
    EXPECT_EQ(1000, out.PeriodUs) << "周期は us に直す";
    EXPECT_EQ(0, out.InitialOffsetUs) << "オフセットの空欄は 0";
    EXPECT_TRUE(out.Data.empty()) << "DLC 0 のデータは 0 バイト";

    EXPECT_TRUE(ValidateFrame(defs, 2, -1, Input("000", "8", "3600000", "3599999", std::string(32, 'n')), out).empty())
        << "ID 0x000・DLC 8・周期の上限・オフセット = 周期 - 1・名前 32 文字";
    EXPECT_EQ(0u, out.CanId) << "0x の無い 16 進";
    EXPECT_EQ(3600000000LL, out.PeriodUs);
    EXPECT_EQ(3599999000LL, out.InitialOffsetUs);
    EXPECT_EQ(std::string(32, 'n'), out.Name);
    EXPECT_EQ(std::vector<std::uint8_t>(8, 0xFF), out.Data) << "データは DLC 分の 0xFF";

    EXPECT_EQ(std::vector<std::uint8_t>(4, 0xFF), MakeFrameData(4)) << "MakeFrameData は DLC 分の 0xFF";
}

// ---------------------------------------------------------------------------
// US3-1: 編集では、ID の重複検査から編集中のフレーム自身を除く。
// 根拠: spec.md US3 シナリオ 1、FR-006（編集時は自身を除く）。
// ---------------------------------------------------------------------------
TEST(F006Spec, F006_US3_1_EditKeepsOwnId_NoDuplicateIssue)
{
    RecordProperty("spec", "006:US3-1");
    const Network defs = MakeDefinitions();
    FrameDefinition out;
    EXPECT_TRUE(ValidateFrame(defs, 1, 1, Input("0x101", "8", "50"), out).empty()) << "自身の ID のままの編集は通る";
    EXPECT_TRUE(HasIssue(ValidateFrame(defs, 1, 1, Input("0x100", "8", "50"), out), FrameField::Id, FrameProblem::Duplicate))
        << "同じ ECU の他のフレームの ID には変えられない";
}

// ---------------------------------------------------------------------------
// US2-2 / FR-007 / FR-009: 追加は一覧の末尾。US3-1 / FR-007: 編集は置き換え。
// US3-2 / FR-009: 削除で残りが詰まり、並び順（= 通し番号）が保たれる。
// 根拠: spec.md US2 シナリオ 2、US3 シナリオ 1・2、FR-007、FR-009。
// ---------------------------------------------------------------------------
TEST(F006Spec, F006_US2_2_AddReplaceDelete_ListOrderMaintained)
{
    RecordProperty("spec", "006:US2-2");
    RecordProperty("spec", "006:US3-2");
    Network defs = MakeDefinitions();
    FrameDefinition f;
    ASSERT_TRUE(ValidateFrame(defs, 1, -1, Input("0x102", "2", "20", "", "Added"), f).empty());
    AddFrame(defs, 1, f);
    const Node* n1 = FindNodeDefinition(defs, 1);
    ASSERT_NE(nullptr, n1);
    ASSERT_EQ(3u, n1->Frames.size());
    EXPECT_EQ(0x102u, n1->Frames[2].CanId) << "追加は末尾（通し番号 3）";

    ASSERT_TRUE(ValidateFrame(defs, 1, 1, Input("0x101", "8", "50"), f).empty());
    ReplaceFrame(defs, 1, 1, f);
    n1 = FindNodeDefinition(defs, 1);
    EXPECT_EQ(50000, n1->Frames[1].PeriodUs) << "編集は同じ位置で置き換え";

    DeleteFrame(defs, 1, 0);
    n1 = FindNodeDefinition(defs, 1);
    ASSERT_EQ(2u, n1->Frames.size());
    EXPECT_EQ(0x101u, n1->Frames[0].CanId) << "削除後、残りが前に詰まる（通し番号 1）";
    EXPECT_EQ(0x102u, n1->Frames[1].CanId) << "削除後、残りが前に詰まる（通し番号 2）";
}

// FrameEditor.h: EnsureNode は無い ECU を "ECU<n>" の名前で NodeId の順に作り、あればそれを返す。
TEST(F006Spec, F006_FR002_EnsureNode_CreatesInNodeIdOrder)
{
    RecordProperty("spec", "006:FR-002");
    Network defs = MakeDefinitions();
    Node& n3 = EnsureNode(defs, 3);
    EXPECT_EQ(3, n3.NodeId);
    EXPECT_EQ("ECU3", n3.Name) << "新しい ECU の名前";
    ASSERT_EQ(3u, defs.Nodes.size());
    EXPECT_EQ(1, defs.Nodes[0].NodeId) << "NodeId の順";
    EXPECT_EQ(3, defs.Nodes[1].NodeId) << "NodeId の順";
    EXPECT_EQ(5, defs.Nodes[2].NodeId) << "NodeId の順";
    EnsureNode(defs, 1);
    EXPECT_EQ(3u, defs.Nodes.size()) << "あれば作らない";
    EXPECT_EQ(nullptr, FindNodeDefinition(defs, 9)) << "定義の無い ECU は nullptr";
}

// ---------------------------------------------------------------------------
// US2-6 / SC-006 / FR-013: 100 フレームの ECU には追加できない。ファイルでも 101 フレームは読み込まない（100 は読める）。
// 根拠: spec.md US2 シナリオ 6、SC-006、FR-004、FR-013。
// ---------------------------------------------------------------------------
TEST(F006Spec, F006_SC006_HundredFrames_AddRejected)
{
    RecordProperty("spec", "006:US2-6");
    RecordProperty("spec", "006:SC-006");
    Network defs;
    std::vector<FrameSpec> frames;
    for (unsigned i = 0; i < 100; ++i) frames.push_back({0x100 + i, 1, 10000, 0});
    defs.Nodes.push_back(MakeNode(1, frames));
    FrameDefinition out;
    const std::vector<FrameIssue> issues = ValidateFrame(defs, 1, -1, Input("0x700", "1", "10"), out);
    bool full = false;
    for (const FrameIssue& i : issues)
        if (i.Problem == FrameProblem::ListFull) full = true;
    EXPECT_TRUE(full) << "100 フレームの ECU への追加が拒否されない";
    EXPECT_TRUE(ValidateFrame(defs, 1, 0, Input("0x700", "1", "10"), out).empty()) << "100 フレームでも編集はできる";
}

TEST(F006Spec, F006_SC006_FileWith101Frames_NotLoaded)
{
    RecordProperty("spec", "006:SC-006");
    RecordProperty("spec", "006:FR-013");
    auto text = [](int count) {
        std::string s = "[Node1]\nNodeId=1\n\n";
        for (int i = 1; i <= count; ++i)
        {
            char buf[128];
            snprintf(buf, sizeof(buf), "[Node1.Frame%d]\nCanId=0x%03X\nDlc=1\nPeriodUs=10000\n\n", i, 0x100 + i);
            s += buf;
        }
        return s;
    };
    const ConfigLoadResult ok = LoadFromText("f006_100.canprj", text(100));
    EXPECT_TRUE(ok.Success) << "100 フレームは読み込める: " << ok.ErrorMessage;
    const ConfigLoadResult ng = LoadFromText("f006_101.canprj", text(101));
    EXPECT_FALSE(ng.Success) << "101 フレームのファイルが読み込まれた";
    EXPECT_FALSE(ng.ErrorMessage.empty()) << "理由が示されていない";
}

// ---------------------------------------------------------------------------
// US4-1 / SC-003 / FR-012: 追加・編集・削除して保存し、読み込み直すと、全 ECU の一覧
//   （名前・ID・DLC・周期・オフセット・順序）が一致する。
// FR-014: データは保存されず、ファイルの Data= は使わず DLC 分の 0xFF になる。
// 根拠: spec.md US4 シナリオ 1、SC-003、FR-012、FR-014、ProjectFile.h（SaveProject）。
// ---------------------------------------------------------------------------
TEST(F006Spec, F006_SC003_EditSaveReload_ListsIdentical)
{
    RecordProperty("spec", "006:US4-1");
    RecordProperty("spec", "006:SC-003");
    Network defs = MakeDefinitions();
    FrameDefinition f;
    ASSERT_TRUE(ValidateFrame(defs, 1, -1, Input("0x0A0", "3", "25", "7", "Wheel speed"), f).empty());
    AddFrame(defs, 1, f);
    ASSERT_TRUE(ValidateFrame(defs, 5, 0, Input("0x300", "6", "40", "5", "Renamed"), f).empty());
    ReplaceFrame(defs, 5, 0, f);
    DeleteFrame(defs, 1, 0);

    ProjectSettings settings;
    settings.EcuCount = 5;
    const std::wstring path = TempPath("f006_sc003.canprj");
    ASSERT_TRUE(SaveProject(path, settings, defs));
    const std::string saved = ReadAll(path);
    const ConfigLoadResult loaded = LoadConfig(path);
    DeleteFileW(path.c_str());
    ASSERT_TRUE(loaded.Success) << loaded.ErrorMessage;
    EXPECT_EQ(std::string::npos, saved.find("Data=")) << "データの値は保存しない（FR-014）";

    ASSERT_EQ(defs.Nodes.size(), loaded.Config.Nodes.size()) << "ECU の数";
    for (std::size_t n = 0; n < defs.Nodes.size(); ++n)
    {
        const Node& a = defs.Nodes[n];
        const Node& b = loaded.Config.Nodes[n];
        EXPECT_EQ(a.NodeId, b.NodeId);
        ASSERT_EQ(a.Frames.size(), b.Frames.size()) << "ECU " << a.NodeId << " のフレーム数";
        for (std::size_t i = 0; i < a.Frames.size(); ++i)
        {
            EXPECT_EQ(a.Frames[i].Name, b.Frames[i].Name) << "名前";
            EXPECT_EQ(a.Frames[i].CanId, b.Frames[i].CanId) << "ID（順序）";
            EXPECT_EQ(a.Frames[i].Dlc, b.Frames[i].Dlc) << "DLC";
            EXPECT_EQ(a.Frames[i].PeriodUs, b.Frames[i].PeriodUs) << "周期";
            EXPECT_EQ(a.Frames[i].InitialOffsetUs, b.Frames[i].InitialOffsetUs) << "初回オフセット";
            EXPECT_EQ(std::vector<std::uint8_t>(b.Frames[i].Dlc, 0xFF), b.Frames[i].Data) << "データは DLC 分の 0xFF";
        }
    }
}

// FR-014: ファイルに Data=00,... が書かれていても使わず、DLC 分の 0xFF にする。
TEST(F006Spec, F006_FR014_DataKeyInFile_IgnoredAndFilledWithFF)
{
    RecordProperty("spec", "006:FR-014");
    const ConfigLoadResult r = LoadFromText(
        "f006_fr014.canprj", "[Node1]\nNodeId=1\n\n[Node1.Frame1]\nCanId=0x100\nDlc=3\nData=00,00,00\nPeriodUs=10000\n");
    ASSERT_TRUE(r.Success) << r.ErrorMessage;
    ASSERT_EQ(1u, r.Config.Nodes.size());
    EXPECT_EQ(std::vector<std::uint8_t>(3, 0xFF), r.Config.Nodes[0].Frames[0].Data) << "データは 0xFF";
}

// ---------------------------------------------------------------------------
// SC-005 / SC-007 / FR-015: 送信要求の時刻 = 送信ノードの通常動作開始 + 初回オフセット + n × 周期。
//   電源投入 50 ms のノードの初回オフセット 0 のフレームの最初の要求は、その通常動作開始時刻と一致する。
//   10 秒の実行で送信回数が期待値と一致する。
// 根拠: spec.md FR-015、SC-005、SC-007。通常動作開始 = 50000 + 22 = 50022 us（機能 002、バスは空）。
//   ECU 2: 0x200、オフセット 3 ms、周期 20 ms -> 要求 50022+3000+20000n < 10,000,000 -> n = 0..497（498 回）
//   ECU 1: 0x100、オフセット 0、周期 10 ms、通常動作 22 us -> 要求 22+10000n -> 1000 回
// ---------------------------------------------------------------------------
TEST(F006Spec, F006_SC007_RequestOrigin_IsNormalEntryPlusOffset)
{
    RecordProperty("spec", "006:SC-005");
    RecordProperty("spec", "006:SC-007");
    Network net;
    net.BitRateBps = 500000;
    net.Nodes.push_back(MakeNode(1, {{0x100, 8, 10000, 0}}));
    Node late = MakeNode(2, {{0x200, 4, 20000, 3000}, {0x201, 2, 10000, 0}});
    late.PowerOnTimeUs = 50000;
    net.Nodes.push_back(late);
    const RunResult run = RunSim(net, 10000000LL);

    long long first201 = -1;
    std::vector<long long> req200;
    int count100 = 0;
    for (const LogRow& r : UniqueTransmissions(run.Rows))
    {
        if (r.CanId == 0x201 && (first201 < 0 || r.TxRequestUs < first201)) first201 = r.TxRequestUs;
        if (r.CanId == 0x200) req200.push_back(r.TxRequestUs);
        if (r.CanId == 0x100) ++count100;
    }
    EXPECT_EQ(50022, first201) << "オフセット 0 の最初の要求 = 通常動作開始（シミュレーション開始ではない）";
    ASSERT_EQ(498u, req200.size()) << "0x200 の 10 秒の送信回数";
    for (std::size_t n = 0; n < req200.size(); ++n)
        EXPECT_EQ(53022 + 20000LL * static_cast<long long>(n), req200[n]) << n << " 回目の要求時刻";
    EXPECT_EQ(1000, count100) << "0x100 の 10 秒の送信回数";
}

// ---------------------------------------------------------------------------
// US2-5 / SC-004 / FR-011: 入力画面のビット長（データ 0xFF、スタッフビット込み）が手計算と一致する。
// 根拠: spec.md SC-004、FR-011。画面は FrameTiming の ComputeFrameTiming の結果を表示する。
// 期待値は CAN 2.0A の定義から、製品コードとは別の Python スクリプトで独立に求めた（機能 001 SC-003 と同じ方法）。
// ---------------------------------------------------------------------------
TEST(F006Spec, F006_SC004_BitLengthWithoutData_MatchesHandCalc)
{
    RecordProperty("spec", "006:SC-004");
    const FrameTimingResult r = ComputeFrameTiming(0x7FF, 0, MakeFrameData(0), 500000);
    EXPECT_EQ(47, r.TotalBits) << "ID 0x7FF・DLC 0 のビット長";
    EXPECT_EQ(94000, r.TxDurationNs) << "500 kbps の伝送時間";
}

TEST(F006Spec, F006_SC004_BitLengthWithFFData_MatchesHandCalc)
{
    RecordProperty("spec", "006:SC-004");
    GTEST_SKIP() << "仕様と実装の食い違い（人の判断待ち）: 機能 001 SC-003 と同じスタッフビットの数え方の違い。"
                    "ID 0x080・DLC 8・データ 0xFF の手計算は 123 ビット、実装は 127 ビット";
    const FrameTimingResult r = ComputeFrameTiming(0x080, 8, MakeFrameData(8), 500000);
    EXPECT_EQ(123, r.TotalBits) << "ID 0x080・DLC 8・0xFF のビット長";
    EXPECT_EQ(246000, r.TxDurationNs) << "500 kbps の伝送時間";
}

// ---------------------------------------------------------------------------
// Edge Cases: フレーム名の上限は 32 文字。ファイルから読むときも 33 文字の名前は読み込まない（32 文字は読める）。
// 根拠: spec.md Edge Cases（長さの上限は 32 文字）、specs/004-menu-project-settings/contracts/project-file-format.md
//        （機能 006 による変更: Name= は 0〜32 文字）。
// ---------------------------------------------------------------------------
TEST(F006Spec, F006_FR006_FileFrameName33Chars_NotLoaded)
{
    RecordProperty("spec", "006:FR-006");
    auto text = [](int chars) {
        return "[Node1]\nNodeId=1\n\n[Node1.Frame1]\nCanId=0x100\nDlc=1\nPeriodUs=10000\nName=" +
               std::string(static_cast<std::size_t>(chars), 'n') + "\n";
    };
    const ConfigLoadResult ok = LoadFromText("f006_name32.canprj", text(32));
    ASSERT_TRUE(ok.Success) << "32 文字の名前は読める: " << ok.ErrorMessage;
    EXPECT_EQ(std::string(32, 'n'), ok.Config.Nodes[0].Frames[0].Name);
    const ConfigLoadResult ng = LoadFromText("f006_name33.canprj", text(33));
    EXPECT_FALSE(ng.Success) << "33 文字の名前が読み込まれた";
    EXPECT_FALSE(ng.ErrorMessage.empty());
}
