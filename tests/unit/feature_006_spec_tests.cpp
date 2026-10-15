// feature_006_spec_tests.cpp（tests/unit）
// 機能 006（specs/006-frame-editor/spec.md）の受け入れシナリオと成功基準のうち、画面を使わずに確かめられるもの。
// 入力の検査・追加・編集・削除は FrameEditor.h、保存は ProjectFile.h、送信要求の時刻はシミュレーションで確かめる。

#include "support/spec_test_helpers.h"

#include "FrameEditor.h"
#include "FrameTiming.h"
#include "ProjectFile.h"

using namespace CanSim;
using namespace spectest;

namespace
{
    FrameInput Input(const std::string& id, const std::string& dlc, const std::string& periodMs,
        const std::string& offsetMs = "", const std::string& name = "F")
    {
        FrameInput in;
        in.Name = name;
        in.NameChars = static_cast<int>(name.size());
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

    // ノード 1 に 0x100、範囲外になった ECU 9 に 0x200 を持つ定義
    Network BaseDefinitions()
    {
        Network defs;
        defs.Nodes.push_back(MakeNode(1, { MakeFrame(0x100, 8, 10000) }));
        defs.Nodes.push_back(MakeNode(9, { MakeFrame(0x200, 8, 10000) }));
        return defs;
    }
}

// 根拠: spec.md US2 受け入れシナリオ 2、FR-007・FR-009・FR-010。正しい値で OK すると一覧の末尾に登録され、通し番号
// （一覧の並び順）が振られる。データは DLC 分の 0xFF、周期とオフセットは ms で入れて µs で持つ。
TEST(F006Spec, F006_US2_2_ValidInput_AppendedAtEnd)
{
    RecordProperty("spec", "006:US2-2");
    Network defs = BaseDefinitions();
    FrameDefinition f;
    const std::vector<FrameIssue> issues = ValidateFrame(defs, 1, -1, Input("0x123", "3", "20", "5", "Brake"), f);
    ASSERT_TRUE(issues.empty()) << "正しい入力が NG になった";
    AddFrame(defs, 1, f);
    const Node* n = FindNodeDefinition(defs, 1);
    ASSERT_NE(n, nullptr);
    ASSERT_EQ(n->Frames.size(), 2u) << "一覧の数";
    const FrameDefinition& added = n->Frames[1];
    EXPECT_EQ(added.Name, "Brake") << "末尾（通し番号 2）にフレーム名";
    EXPECT_EQ(added.CanId, 0x123u) << "ID";
    EXPECT_EQ(added.Dlc, 3u) << "DLC";
    EXPECT_EQ(added.PeriodUs, 20000) << "送信周期（20 ms）";
    EXPECT_EQ(added.InitialOffsetUs, 5000) << "初回オフセット（5 ms）";
    EXPECT_EQ(added.Data, (std::vector<std::uint8_t>{ 0xFF, 0xFF, 0xFF })) << "データが DLC 分の 0xFF でない";
}

// 根拠: spec.md US2 受け入れシナリオ 3・SC-002、FR-006、Edge Cases（他のノードの ID は範囲外になった ECU のものも重複）。
// 範囲外の ID（0x800 以上）・DLC（9 以上）・重複する ID・1 ms 未満の周期・周期以上のオフセット・33 文字以上の名前は NG で、
// NG の項目と理由が示される。
struct InvalidInputCase
{
    const char* Name;
    FrameInput In;
    FrameField Field;
    FrameProblem Problem;
};

class F006InvalidInput : public ::testing::TestWithParam<InvalidInputCase>
{
};

TEST_P(F006InvalidInput, F006_US2_3_InvalidInput_RejectedWithFieldAndReason)
{
    RecordProperty("spec", "006:US2-3");
    const InvalidInputCase& c = GetParam();
    FrameDefinition f;
    const std::vector<FrameIssue> issues = ValidateFrame(BaseDefinitions(), 1, -1, c.In, f);
    EXPECT_FALSE(issues.empty()) << c.Name << ": NG にならない";
    EXPECT_TRUE(HasIssue(issues, c.Field, c.Problem)) << c.Name << ": NG の項目と理由が違う";
}

INSTANTIATE_TEST_SUITE_P(
    US2, F006InvalidInput,
    ::testing::Values(
        InvalidInputCase{ "id_0x800", Input("0x800", "8", "10"), FrameField::Id, FrameProblem::OutOfRange },
        InvalidInputCase{ "dlc_9", Input("0x123", "9", "10"), FrameField::Dlc, FrameProblem::OutOfRange },
        InvalidInputCase{ "id_duplicate_same_node", Input("0x100", "8", "10"), FrameField::Id, FrameProblem::Duplicate },
        InvalidInputCase{ "id_duplicate_hidden_ecu", Input("0x200", "8", "10"), FrameField::Id, FrameProblem::Duplicate },
        InvalidInputCase{ "period_0", Input("0x123", "8", "0"), FrameField::Period, FrameProblem::OutOfRange },
        InvalidInputCase{ "period_over_1h", Input("0x123", "8", "3600001"), FrameField::Period, FrameProblem::OutOfRange },
        InvalidInputCase{ "offset_equal_period", Input("0x123", "8", "10", "10"), FrameField::Offset,
                          FrameProblem::OffsetNotBelowPeriod },
        InvalidInputCase{ "name_33_chars", Input("0x123", "8", "10", "", std::string(33, 'a')), FrameField::Name,
                          FrameProblem::TooLong },
        InvalidInputCase{ "id_empty", Input("", "8", "10"), FrameField::Id, FrameProblem::Empty },
        InvalidInputCase{ "dlc_not_number", Input("0x123", "x", "10"), FrameField::Dlc, FrameProblem::NotANumber }),
    [](const ::testing::TestParamInfo<InvalidInputCase>& info) { return std::string(info.param.Name); });

// 根拠: spec.md US2 受け入れシナリオ 6・SC-006、FR-004・FR-013（1 ノード最大 100 フレーム）。一覧が 100 フレームのノードには
// 追加できない（検査が ListFull を返す。「追加」ボタンの無効化は画面の確認）。
TEST(F006Spec, F006_US2_6_HundredFrames_CannotAdd)
{
    RecordProperty("spec", "006:US2-6");
    Network defs;
    std::vector<FrameDefinition> frames;
    for (unsigned i = 0; i < 100; ++i)
        frames.push_back(MakeFrame(0x100 + i, 8, 10000));
    defs.Nodes.push_back(MakeNode(1, frames));
    FrameDefinition f;
    const std::vector<FrameIssue> issues = ValidateFrame(defs, 1, -1, Input("0x700", "8", "10"), f);
    EXPECT_FALSE(issues.empty()) << "100 フレームのノードに追加できてしまう";
    bool listFull = false;
    for (const FrameIssue& i : issues)
        if (i.Problem == FrameProblem::ListFull)
            listFull = true;
    EXPECT_TRUE(listFull) << "一覧が満杯であることが理由になっていない";
}

// 根拠: spec.md US3 受け入れシナリオ 1、FR-006・FR-007（ID の重複検査では編集中のフレーム自身を除く。検査に通れば置き換える）。
TEST(F006Spec, F006_US3_1_Edit_SameIdAllowedAndReplaced)
{
    RecordProperty("spec", "006:US3-1");
    Network defs = BaseDefinitions();
    FrameDefinition f;
    const std::vector<FrameIssue> issues = ValidateFrame(defs, 1, 0, Input("0x100", "2", "50", "", "Renamed"), f);
    ASSERT_TRUE(issues.empty()) << "編集中のフレーム自身の ID が重複として NG になった";
    ReplaceFrame(defs, 1, 0, f);
    const Node* n = FindNodeDefinition(defs, 1);
    ASSERT_EQ(n->Frames.size(), 1u) << "編集で数が変わった";
    EXPECT_EQ(n->Frames[0].Name, "Renamed");
    EXPECT_EQ(n->Frames[0].Dlc, 2u);
    EXPECT_EQ(n->Frames[0].PeriodUs, 50000);

    // 他のノードの ID への変更は、編集でも重複として NG
    EXPECT_FALSE(ValidateFrame(defs, 1, 0, Input("0x200", "2", "50"), f).empty()) << "他のノードの ID に変更できてしまう";
}

// 根拠: spec.md US3 受け入れシナリオ 2、FR-008・FR-009。了承して削除すると、残りの通し番号は 1 から振り直される
// （通し番号は一覧の並び順）。確認の表示は画面の確認。
TEST(F006Spec, F006_US3_2_Delete_RemainingRenumbered)
{
    RecordProperty("spec", "006:US3-2");
    Network defs;
    defs.Nodes.push_back(MakeNode(1, { MakeFrame(0x101, 1, 1000), MakeFrame(0x102, 1, 1000), MakeFrame(0x103, 1, 1000) }));
    DeleteFrame(defs, 1, 0);
    const Node* n = FindNodeDefinition(defs, 1);
    ASSERT_EQ(n->Frames.size(), 2u) << "削除されていない";
    EXPECT_EQ(n->Frames[0].CanId, 0x102u) << "通し番号 1 が元の 2 番目でない";
    EXPECT_EQ(n->Frames[1].CanId, 0x103u) << "通し番号 2 が元の 3 番目でない";
}

// 根拠: spec.md US4 受け入れシナリオ 1・SC-003、FR-012。追加・編集・削除の後に保存して読み込み直すと、各ノードの一覧
// （フレーム名・ID・DLC・周期・初回オフセット・並び順）が保存前と一致する。
TEST(F006Spec, F006_US4_1_EditSaveLoad_ListsIdentical)
{
    RecordProperty("spec", "006:US4-1");
    Network defs = BaseDefinitions();
    FrameDefinition f;
    ASSERT_TRUE(ValidateFrame(defs, 1, -1, Input("0x0A0", "4", "25", "3", "Speed"), f).empty());
    AddFrame(defs, 1, f);
    ASSERT_TRUE(ValidateFrame(defs, 3, -1, Input("0x300", "0", "1000", "999", "Door"), f).empty());
    AddFrame(defs, 3, f); // 定義の無かった ECU 3 には定義が作られる
    ASSERT_TRUE(ValidateFrame(defs, 1, 0, Input("0x100", "6", "15", "", "Engine"), f).empty());
    ReplaceFrame(defs, 1, 0, f);
    DeleteFrame(defs, 9, 0);

    ProjectSettings s;
    s.EcuCount = 9;
    const std::wstring path = TempPath(L"f006_us4_1.canprj");
    ASSERT_TRUE(SaveProject(path, s, defs)) << "保存できない";
    const ConfigLoadResult r = LoadConfig(path);
    ASSERT_TRUE(r.Success) << r.ErrorMessage;
    for (const Node& before : defs.Nodes)
    {
        const Node* after = FindNodeDefinition(r.Config, before.NodeId);
        ASSERT_NE(after, nullptr) << "ノード " << before.NodeId << " が復元されない";
        ASSERT_EQ(after->Frames.size(), before.Frames.size()) << "ノード " << before.NodeId << " の一覧の数";
        for (std::size_t i = 0; i < before.Frames.size(); ++i)
        {
            EXPECT_EQ(after->Frames[i].Name, before.Frames[i].Name) << "フレーム名";
            EXPECT_EQ(after->Frames[i].CanId, before.Frames[i].CanId) << "ID";
            EXPECT_EQ(after->Frames[i].Dlc, before.Frames[i].Dlc) << "DLC";
            EXPECT_EQ(after->Frames[i].PeriodUs, before.Frames[i].PeriodUs) << "周期";
            EXPECT_EQ(after->Frames[i].InitialOffsetUs, before.Frames[i].InitialOffsetUs) << "初回オフセット";
        }
    }
}

// 根拠: spec.md US4 受け入れシナリオ 2、FR-012。編集した状態でシミュレーションを開始すると、編集後のフレーム定義で送信される。
TEST(F006Spec, F006_US4_2_EditedDefinitions_UsedInRun)
{
    RecordProperty("spec", "006:US4-2");
    Network defs = BaseDefinitions();
    FrameDefinition f;
    ASSERT_TRUE(ValidateFrame(defs, 1, 0, Input("0x111", "1", "5"), f).empty());
    ReplaceFrame(defs, 1, 0, f);
    ProjectSettings s;
    s.EcuCount = 2;
    const std::vector<LogRow> rows = RunSim(BuildRunNetwork(s, defs), 20000LL);
    ASSERT_FALSE(rows.empty()) << "送信が無い";
    for (const LogRow& r : rows)
    {
        EXPECT_EQ(r.CanId, 0x111u) << "編集前の ID で送信された";
        EXPECT_EQ(r.Dlc, 1u) << "編集前の DLC で送信された";
    }
}

// 根拠: spec.md SC-004、FR-011。入力画面のビット長は、同じ ID・DLC・データ 0xFF の手計算のスタッフビット込みのビット長と一致する。
// 手計算の値は機能 031 の spec.md（0x080・DLC 8 は 123 ビット・500 kbps で 246 µs、0x7FF・DLC 0 は 47 ビット・94 µs）。
// 表示は ComputeFrameTiming（データは MakeFrameData）の値なので、その値で確かめる。
TEST(F006Spec, F006_SC004_BitLength_MatchesHandCalc)
{
    RecordProperty("spec", "006:SC-004");
    const FrameTimingResult a = ComputeFrameTiming(0x080, 8, MakeFrameData(8), 500000);
    EXPECT_EQ(a.TotalBits, 123) << "0x080・DLC 8 のビット長";
    EXPECT_EQ(a.TxDurationNs, 246000) << "0x080・DLC 8 の伝送時間";
    const FrameTimingResult b = ComputeFrameTiming(0x7FF, 0, MakeFrameData(0), 500000);
    EXPECT_EQ(b.TotalBits, 47) << "0x7FF・DLC 0 のビット長";
    EXPECT_EQ(b.TxDurationNs, 94000) << "0x7FF・DLC 0 の伝送時間";
    EXPECT_EQ(MakeFrameData(5), (std::vector<std::uint8_t>{ 0xFF, 0xFF, 0xFF, 0xFF, 0xFF })) << "データが 0xFF でない";
}

// 根拠: spec.md SC-005、FR-015。追加したフレームは「送信ノードの通常動作開始時刻 + 初回オフセット + n × 周期」に送信要求され、
// 10 秒の実行で送信回数が期待値と一致する。手計算: 通常動作開始 22 µs、周期 30 ms・オフセット 7 ms →
// 要求は 7,022 + 30,000 n µs。10 秒（10,000,000 µs）までの回数は ceil((10,000,000 − 7,022) / 30,000) = 334 回。
TEST(F006Spec, F006_SC005_RequestTimes_NormalPlusOffsetPlusPeriods)
{
    RecordProperty("spec", "006:SC-005");
    Network defs;
    FrameDefinition f;
    ASSERT_TRUE(ValidateFrame(defs, 1, -1, Input("0x321", "8", "30", "7"), f).empty());
    AddFrame(defs, 1, f);
    ProjectSettings s;
    s.EcuCount = 2;
    const std::vector<LogRow> rows = RunSim(BuildRunNetwork(s, defs), 10000000LL);
    const std::vector<LogRow> tx = Transmissions(rows, 0x321);
    ASSERT_EQ(tx.size(), 334u) << "10 秒の送信回数";
    for (std::size_t n = 0; n < tx.size(); ++n)
        EXPECT_EQ(tx[n].TxRequestUs, 7022 + 30000LL * static_cast<long long>(n)) << n << " 回目の送信要求時刻";
}

// 根拠: spec.md SC-006、FR-013。101 フレームのノードを含むファイルは、理由を示して読み込まれない（100 は読める）。
TEST(F006Spec, F006_SC006_FileWith101Frames_Rejected)
{
    RecordProperty("spec", "006:SC-006");
    auto makeText = [](int count) {
        std::string text = "[Node1]\r\nNodeId=1\r\n";
        for (int i = 1; i <= count; ++i)
        {
            char id[16];
            std::snprintf(id, sizeof(id), "0x%03X", 0x100 + i);
            text += "[Node1.Frame" + std::to_string(i) + "]\r\nCanId=" + id + "\r\nDlc=1\r\nPeriodUs=100000\r\n";
        }
        return text;
    };
    EXPECT_TRUE(LoadText(L"f006_sc006_100.canprj", makeText(100)).Success) << "100 フレームのノードが読み込めない";
    const ConfigLoadResult r = LoadText(L"f006_sc006_101.canprj", makeText(101));
    EXPECT_FALSE(r.Success) << "101 フレームのノードが読み込めてしまった";
    EXPECT_FALSE(r.ErrorMessage.empty()) << "理由が示されていない";
}

// 根拠: spec.md SC-007、FR-015。電源投入時刻 50 ms のノードの、初回オフセット 0 のフレームの最初の送信要求時刻は、
// そのノードの通常動作開始時刻（バスが空なので 50,000 + 22 = 50,022 µs）と一致する（シミュレーション開始時刻ではない）。
TEST(F006Spec, F006_SC007_LatePowerOn_FirstRequestAtNormalEntry)
{
    RecordProperty("spec", "006:SC-007");
    Network net;
    net.BitRateBps = 500000;
    Node late = MakeNode(1, { MakeFrame(0x100, 8, 10000) });
    late.PowerOnTimeUs = 50000;
    net.Nodes.push_back(late);
    net.Nodes.push_back(MakeNode(2));
    Network after;
    const std::vector<LogRow> rows = RunSim(net, 200000LL, &after);
    const std::vector<LogRow> tx = Transmissions(rows, 0x100);
    ASSERT_FALSE(tx.empty()) << "送信が無い";
    EXPECT_EQ(after.Nodes[0].NormalEntryUs, 50022) << "通常動作開始時刻";
    EXPECT_EQ(tx.front().TxRequestUs, 50022) << "最初の送信要求時刻が通常動作開始時刻でない";
}
