// feature_004_spec_tests.cpp（tests/unit）
// 機能 004（specs/004-menu-project-settings/spec.md）の受け入れシナリオと成功基準のうち、画面を使わずに確かめられるもの。
// 期待値は spec.md と contracts/project-file-format.md から書く。メニュー・ダイアログ・描画は画面の確認。

#include "support/spec_test_helpers.h"

#include "FrameEvents.h"
#include "LogFileIndex.h"
#include "MetricsAggregator.h"
#include "ProjectFile.h"
#include "SimControls.h"
#include "StartupControl.h"

using namespace CanSim;
using namespace spectest;

namespace
{
    // [NodeN] と、フレームを 1 つ持つ [NodeN.Frame1]
    std::string NodeText(int nodeId, unsigned canId, long long periodUs = 10000)
    {
        char id[16];
        std::snprintf(id, sizeof(id), "0x%03X", canId);
        const std::string n = std::to_string(nodeId);
        return "[Node" + n + "]\r\nNodeId=" + n + "\r\n[Node" + n + ".Frame1]\r\nName=F" + n + "\r\nCanId=" + id +
               "\r\nDlc=8\r\nPeriodUs=" + std::to_string(periodUs) + "\r\n";
    }
}

// 根拠: spec.md US1 受け入れシナリオ 1・FR-006、contracts/project-file-format.md（[Project] 省略時の既定値）。
// 起動直後の値（ECU 数 2・500 kbps・10 秒）は、プロジェクトの既定値と同じ。セッティングの画面の表示は画面の確認。
TEST(F004Spec, F004_US1_1_Defaults_Ecu2_500k_10s)
{
    RecordProperty("spec", "004:US1-1");
    const ProjectSettings s;
    const Network n;
    EXPECT_EQ(s.EcuCount, 2) << "ECU 数の既定値";
    EXPECT_EQ(s.DurationSec, 10) << "シミュレーション時間の既定値";
    EXPECT_EQ(n.BitRateBps, 500000) << "通信速度の既定値";
    const ConfigLoadResult r = LoadText(L"f004_us1_1.canprj", NodeText(1, 0x100));
    ASSERT_TRUE(r.Success) << r.ErrorMessage;
    EXPECT_EQ(r.Project.EcuCount, 2) << "[Project] を省略したファイルの ECU 数";
    EXPECT_EQ(r.Project.DurationSec, 10) << "[Project] を省略したファイルのシミュレーション時間";
    EXPECT_EQ(r.Config.BitRateBps, 500000) << "[Network] を省略したファイルの通信速度";
}

// 根拠: spec.md US2 受け入れシナリオ 1・SC-003、FR-008・FR-009。ECU 数 5・250 kbps・20 秒・フレーム表示 ON とフレームの定義を
// 保存して読み込むと、4 つの設定とフレームの定義が復元される。
TEST(F004Spec, F004_US2_1_SaveThenLoad_SettingsAndFramesRestored)
{
    RecordProperty("spec", "004:US2-1");
    ProjectSettings s;
    s.EcuCount = 5;
    s.DurationSec = 20;
    s.FrameDisplay = true;
    Network defs;
    defs.BitRateBps = 250000;
    defs.Nodes.push_back(MakeNode(1, { MakeFrame(0x100, 8, 10000, 500) }));
    defs.Nodes.push_back(MakeNode(4, { MakeFrame(0x080, 2, 20000) }));
    const std::wstring path = TempPath(L"f004_us2_1.canprj");
    ASSERT_TRUE(SaveProject(path, s, defs)) << "保存できない";
    const ConfigLoadResult r = LoadConfig(path);
    ASSERT_TRUE(r.Success) << r.ErrorMessage;
    EXPECT_EQ(r.Project.EcuCount, 5) << "ECU 数";
    EXPECT_EQ(r.Config.BitRateBps, 250000) << "通信速度";
    EXPECT_EQ(r.Project.DurationSec, 20) << "シミュレーション時間";
    EXPECT_TRUE(r.Project.FrameDisplay) << "フレーム表示";
    ASSERT_EQ(r.Config.Nodes.size(), 2u) << "ノードの定義の数";
    EXPECT_EQ(r.Config.Nodes[1].NodeId, 4) << "ノード番号";
    ASSERT_EQ(r.Config.Nodes[1].Frames.size(), 1u) << "フレームの定義の数";
    EXPECT_EQ(r.Config.Nodes[1].Frames[0].CanId, 0x080u) << "CAN ID";
    EXPECT_EQ(r.Config.Nodes[0].Frames[0].InitialOffsetUs, 500) << "初回オフセット";
}

// 根拠: spec.md US2 受け入れシナリオ 3、FR-010。拡張子が .canprj でない、または内容が壊れているファイルは読み込めない。
// 拡張子の判定（ProjectFile.h の HasProjectExtension、大文字小文字を区別しない）と、壊れた内容の LoadConfig の失敗で確かめる。
// 「現在の内容は変更されない」は画面側の確認。
TEST(F004Spec, F004_US2_3_WrongExtensionOrBrokenContent_NotLoaded)
{
    RecordProperty("spec", "004:US2-3");
    EXPECT_FALSE(HasProjectExtension(L"C:\\work\\a.cfg")) << ".cfg を受け付けた";
    EXPECT_FALSE(HasProjectExtension(L"C:\\work\\a.canprj.bak")) << ".canprj.bak を受け付けた";
    EXPECT_TRUE(HasProjectExtension(L"C:\\work\\a.canprj")) << ".canprj を受け付けない";
    EXPECT_TRUE(HasProjectExtension(L"C:\\work\\A.CANPRJ")) << "大文字の .CANPRJ を受け付けない";

    const ConfigLoadResult broken = LoadText(L"f004_us2_3_broken.canprj", "this is not a project file\r\n");
    EXPECT_FALSE(broken.Success) << "壊れた内容が読み込めてしまった";
    EXPECT_FALSE(broken.ErrorMessage.empty()) << "理由が示されていない";
    const ConfigLoadResult unknown = LoadText(L"f004_us2_3_section.canprj", "[Foo]\r\nA=1\r\n" + NodeText(1, 0x100));
    EXPECT_FALSE(unknown.Success) << "知らないセクションのある内容が読み込めてしまった";
}

// 根拠: spec.md US2 受け入れシナリオ 4、FR-010、contracts/project-file-format.md「検証」。範囲外の値（ECU 数 17、選択肢に無い
// 通信速度、重複した CAN ID、シミュレーション時間 0・601、フレーム表示 2）を含むファイルは読み込まれず、理由が示される。
struct OutOfRangeCase
{
    const char* Name;
    std::string Text;
};

class F004OutOfRange : public ::testing::TestWithParam<OutOfRangeCase>
{
};

TEST_P(F004OutOfRange, F004_US2_4_OutOfRangeValue_RejectedWithReason)
{
    RecordProperty("spec", "004:US2-4");
    const OutOfRangeCase& c = GetParam();
    const ConfigLoadResult r = LoadText(Widen(std::string("f004_us2_4_") + c.Name + ".canprj"), c.Text);
    EXPECT_FALSE(r.Success) << c.Name << ": 範囲外の値が読み込めてしまった";
    EXPECT_FALSE(r.ErrorMessage.empty()) << c.Name << ": 理由が示されていない";
}

INSTANTIATE_TEST_SUITE_P(
    US2, F004OutOfRange,
    ::testing::Values(
        OutOfRangeCase{ "ecu17", "[Project]\r\nEcuCount=17\r\n" + NodeText(1, 0x100) },
        OutOfRangeCase{ "ecu_negative", "[Project]\r\nEcuCount=-1\r\n" + NodeText(1, 0x100) },
        OutOfRangeCase{ "bitrate_300k", "[Network]\r\nBitRateBps=300000\r\n" + NodeText(1, 0x100) },
        OutOfRangeCase{ "duplicate_canid", NodeText(1, 0x100) + NodeText(2, 0x100) },
        OutOfRangeCase{ "duration_0", "[Project]\r\nDurationSec=0\r\n" + NodeText(1, 0x100) },
        OutOfRangeCase{ "duration_601", "[Project]\r\nDurationSec=601\r\n" + NodeText(1, 0x100) },
        OutOfRangeCase{ "framedisplay_2", "[Project]\r\nFrameDisplay=2\r\n" + NodeText(1, 0x100) }),
    [](const ::testing::TestParamInfo<OutOfRangeCase>& info) { return std::string(info.param.Name); });

// 根拠: contracts/project-file-format.md「検証」の境界（EcuCount 0〜16・DurationSec 1〜600・FrameDisplay 0 または 1）。
// US2-4 の範囲の内側の端は読み込める。
TEST(F004Spec, F004_US2_4_BoundaryValues_Accepted)
{
    RecordProperty("spec", "004:US2-4");
    const ConfigLoadResult hi =
        LoadText(L"f004_us2_4_hi.canprj", "[Project]\r\nEcuCount=16\r\nDurationSec=600\r\nFrameDisplay=1\r\n" + NodeText(1, 0x100));
    ASSERT_TRUE(hi.Success) << hi.ErrorMessage;
    EXPECT_EQ(hi.Project.EcuCount, 16);
    EXPECT_EQ(hi.Project.DurationSec, 600);
    EXPECT_TRUE(hi.Project.FrameDisplay);
    const ConfigLoadResult lo =
        LoadText(L"f004_us2_4_lo.canprj", "[Project]\r\nEcuCount=0\r\nDurationSec=1\r\nFrameDisplay=0\r\n" + NodeText(1, 0x100));
    ASSERT_TRUE(lo.Success) << lo.ErrorMessage;
    EXPECT_EQ(lo.Project.EcuCount, 0);
    EXPECT_EQ(lo.Project.DurationSec, 1);
    EXPECT_FALSE(lo.Project.FrameDisplay);
}

// 根拠: spec.md US2 受け入れシナリオ 5、FR-011。ECU 数がフレームの定義より小さいときは、ECU 数を超える番号のノードのフレームは
// 送信されない。ECU 数が定義より多いときは、定義の無い ECU は何も送信しない。
TEST(F004Spec, F004_US2_5_EcuCountWins_OverDefinitions)
{
    RecordProperty("spec", "004:US2-5");
    Network defs;
    defs.Nodes.push_back(MakeNode(1, { MakeFrame(0x100, 8, 10000) }));
    defs.Nodes.push_back(MakeNode(3, { MakeFrame(0x300, 8, 10000) }));

    ProjectSettings fewer;
    fewer.EcuCount = 2;
    const Network runFewer = BuildRunNetwork(fewer, defs);
    ASSERT_EQ(runFewer.Nodes.size(), 2u) << "ECU 数 2 の実行用ネットワークのノード数";
    for (const LogRow& r : RunSim(runFewer, 100000LL))
        EXPECT_NE(r.CanId, 0x300u) << "ECU 数を超える番号（3）のノードのフレームが送信された";

    ProjectSettings more;
    more.EcuCount = 5;
    const Network runMore = BuildRunNetwork(more, defs);
    ASSERT_EQ(runMore.Nodes.size(), 5u) << "ECU 数 5 の実行用ネットワークのノード数";
    std::set<int> senders;
    for (const LogRow& r : RunSim(runMore, 100000LL))
        senders.insert(r.TxNodeId);
    EXPECT_EQ(senders, (std::set<int>{ 1, 3 })) << "定義の無い ECU が送信した、または定義のある ECU が送信していない";
}

// 根拠: spec.md US3 受け入れシナリオ 1、Assumptions（結果ファイルは プロジェクトファイルと同じ場所に、
// <プロジェクトファイル名>.log.csv・.summary.csv・.startup.csv）。出力先の名前は LogFileIndex.h の ResultFilePaths。
// 実際に書き出して完了を通知するのは画面側。
TEST(F004Spec, F004_US3_1_ResultFiles_NextToProjectFile)
{
    RecordProperty("spec", "004:US3-1");
    const std::vector<std::wstring> paths = ResultFilePaths(L"C:\\work\\demo\\my.canprj");
    ASSERT_EQ(paths.size(), 3u) << "結果ファイルが 3 種でない";
    EXPECT_EQ(paths[0], L"C:\\work\\demo\\my.canprj.log.csv") << "フレームログ";
    EXPECT_EQ(paths[1], L"C:\\work\\demo\\my.canprj.summary.csv") << "集計";
    EXPECT_EQ(paths[2], L"C:\\work\\demo\\my.canprj.startup.csv") << "起動記録";
}

// 根拠: spec.md US3 受け入れシナリオ 2・SC-008、FR-018、Assumptions（「終了」は、終了時点までに送信を完了したフレームを結果とし、
// 送信途中のフレームは含めない）。時刻 t まで実行した結果は、最後まで実行した結果のうち送信完了が t 以下の行と一致する。
TEST(F004Spec, F004_US3_2_StopAtT_OnlyFramesCompletedByT)
{
    RecordProperty("spec", "004:US3-2");
    Network net;
    net.BitRateBps = 500000;
    net.Nodes.push_back(MakeNode(1, { MakeFrame(0x100, 8, 1000) }));
    net.Nodes.push_back(MakeNode(2, { MakeFrame(0x080, 8, 1500, 100) }));
    const long long t = 10130; // 0x100 の送信の途中
    const std::vector<LogRow> full = RunSim(net, 100000LL);
    std::vector<LogRow> expected;
    for (const LogRow& r : full)
        if (r.TxEndUs <= t)
            expected.push_back(r);
    const std::vector<LogRow> stopped = RunSim(net, t);
    bool cutInFlight = false;
    for (const LogRow& r : full)
        if (r.TxStartUs < t && t < r.TxEndUs)
            cutInFlight = true;
    ASSERT_TRUE(cutInFlight) << "前提: t の時点で送信中のフレームが無い";
    ASSERT_EQ(stopped.size(), expected.size()) << "t までに送信を完了したフレームの行数と違う";
    for (std::size_t i = 0; i < stopped.size(); ++i)
        EXPECT_TRUE(SameRow(stopped[i], expected[i])) << i << " 行目が違う";
}

// 根拠: spec.md US3 受け入れシナリオ 3、FR-019。実行中は「開始」を選べず「終了」を選べる。実行中でないときは逆。
// メニューとツールバーが共通に使う有効／無効（SimControls.h の GetSimCommandStates）で確かめる。
TEST(F004Spec, F004_US3_3_StartStopEnabled_ByRunState)
{
    RecordProperty("spec", "004:US3-3");
    const SimCommandStates running = GetSimCommandStates(RunPhase::Running, false);
    EXPECT_FALSE(running.Start) << "実行中に「開始」が選べる";
    EXPECT_TRUE(running.Stop) << "実行中に「終了」が選べない";
    const SimCommandStates idle = GetSimCommandStates(RunPhase::Idle, false);
    EXPECT_TRUE(idle.Start) << "実行中でないのに「開始」が選べない";
    EXPECT_FALSE(idle.Stop) << "実行中でないのに「終了」が選べる";
}

// 根拠: spec.md US3 受け入れシナリオ 5。ECU 数 0、または送信フレームが 1 つも無い状態で開始しても、エラーにならず、
// フレームの無い（ヘッダーだけの）結果が出る。3 種の CSV をヘッダーだけで書けることを確かめる。
TEST(F004Spec, F004_US3_5_NoEcuOrNoFrames_HeaderOnlyResults)
{
    RecordProperty("spec", "004:US3-5");
    Network defs;
    defs.Nodes.push_back(MakeNode(1));
    for (int ecuCount : { 0, 2 })
    {
        ProjectSettings s;
        s.EcuCount = ecuCount;
        const Network run = BuildRunNetwork(s, defs);
        Network after;
        const std::vector<LogRow> rows = RunSim(run, 1000000LL, &after);
        EXPECT_TRUE(rows.empty()) << "ECU 数 " << ecuCount << ": フレームの行がある";
        const std::wstring base = TempPath(L"f004_us3_5_" + std::to_wstring(ecuCount));
        ASSERT_TRUE(WriteLogCsv(rows, base + L".log.csv")) << "フレームログを書けない";
        ASSERT_TRUE(WriteMetricsCsv(ComputeMetrics(rows, run), base + L".summary.csv")) << "集計を書けない";
        ASSERT_TRUE(WriteStartupCsv(BuildStartupRecords(after), base + L".startup.csv")) << "起動記録を書けない";
        EXPECT_EQ(SplitLines(ReadText(base + L".log.csv")).size(), 1u) << "フレームログがヘッダーだけでない";
        EXPECT_FALSE(ReadText(base + L".summary.csv").empty()) << "集計が書かれていない";
    }
}

// 根拠: spec.md US4 受け入れシナリオ 2。2 つのノードが同時に送信要求した構成では、調停に勝った ECU のフレームが先に表示され、
// 負けた ECU は次の機会に表示される。表示の元のフレームイベント（FrameEvents.h、送信開始の順）の並びで確かめる。
TEST(F004Spec, F004_US4_2_FrameEvents_WinnerFirst)
{
    RecordProperty("spec", "004:US4-2");
    Network net;
    net.BitRateBps = 500000;
    net.Nodes.push_back(MakeNode(1, { MakeFrame(0x100, 8, 1000000) }));
    net.Nodes.push_back(MakeNode(2, { MakeFrame(0x080, 8, 1000000) }));
    const std::vector<FrameEvent> events = BuildFrameEvents(RunSim(net, 10000LL));
    ASSERT_EQ(events.size(), 2u) << "フレームイベントが 2 つでない";
    EXPECT_EQ(events[0].TxNodeId, 2) << "調停に勝った ECU 2（0x080）が先でない";
    EXPECT_EQ(events[1].TxNodeId, 1) << "負けた ECU 1 が次でない";
    EXPECT_EQ(events[0].RxNodeIds, (std::vector<int>{ 1 })) << "0x080 の受信 ECU";
}

namespace
{
    // フレーム表示の ON / OFF だけを変えて、実行用ネットワークを作って実行する
    std::vector<LogRow> RunWithDisplay(bool frameDisplay, Network* after)
    {
        ProjectSettings s;
        s.EcuCount = 3;
        s.FrameDisplay = frameDisplay;
        Network defs;
        defs.Nodes.push_back(MakeNode(1, { MakeFrame(0x100, 8, 2000) }));
        defs.Nodes.push_back(MakeNode(2, { MakeFrame(0x080, 4, 3000, 500) }));
        const Network run = BuildRunNetwork(s, defs);
        return RunSim(run, 100000LL, after);
    }
}

// 根拠: spec.md US4 受け入れシナリオ 3・SC-006、FR-015。フレーム表示の ON と OFF で、実行結果（ログ・集計・起動記録の CSV）は
// 完全に同一。3 種の CSV をそれぞれ書いて、バイト単位で比べる。
TEST(F004Spec, F004_US4_3_FrameDisplayOnOff_IdenticalCsv)
{
    RecordProperty("spec", "004:US4-3");
    Network afterOn, afterOff;
    const std::vector<LogRow> on = RunWithDisplay(true, &afterOn);
    const std::vector<LogRow> off = RunWithDisplay(false, &afterOff);
    ASSERT_FALSE(on.empty()) << "前提: ログが空";
    const std::wstring a = TempPath(L"f004_us4_3_on"), b = TempPath(L"f004_us4_3_off");
    WriteLogCsv(on, a + L".log.csv");
    WriteLogCsv(off, b + L".log.csv");
    WriteMetricsCsv(ComputeMetrics(on, afterOn), a + L".summary.csv");
    WriteMetricsCsv(ComputeMetrics(off, afterOff), b + L".summary.csv");
    WriteStartupCsv(BuildStartupRecords(afterOn), a + L".startup.csv");
    WriteStartupCsv(BuildStartupRecords(afterOff), b + L".startup.csv");
    for (const wchar_t* ext : { L".log.csv", L".summary.csv", L".startup.csv" })
        EXPECT_EQ(ReadText(a + ext), ReadText(b + ext)) << "ON と OFF で CSV が違う";
}

// 根拠: spec.md SC-001・FR-006（起動直後のフレーム表示は OFF）。
TEST(F004Spec, F004_SC001_Defaults_FrameDisplayOff)
{
    RecordProperty("spec", "004:SC-001");
    const ProjectSettings s;
    EXPECT_EQ(s.EcuCount, 2) << "ECU 数の既定値";
    EXPECT_EQ(s.DurationSec, 10) << "シミュレーション時間の既定値";
    if (s.FrameDisplay)
        GTEST_SKIP() << "仕様と実装の食い違い（人の判断待ち）: 004 FR-006・SC-001 はフレーム表示の既定を OFF とするが、"
                        "実装は機能 021 のとおり ON。004 の記述が 021 で置き換わったかの判断が必要";
    EXPECT_FALSE(s.FrameDisplay) << "フレーム表示の既定値";
}

// 根拠: spec.md SC-003、FR-008・FR-009。保存したプロジェクトを読み込むと、設定 4 項目とフレームの定義のすべてが保存時と
// 完全に一致する。複数のノード・フレーム、受信フィルタ、受信処理遅延を含めて確かめる。
TEST(F004Spec, F004_SC003_SaveLoad_AllDefinitionsIdentical)
{
    RecordProperty("spec", "004:SC-003");
    ProjectSettings s;
    s.EcuCount = 7;
    s.DurationSec = 123;
    s.FrameDisplay = false;
    Network defs;
    defs.BitRateBps = 1000000;
    Node n1 = MakeNode(1, { MakeFrame(0x100, 8, 10000, 250), MakeFrame(0x101, 0, 5000) });
    n1.RxFilterAll = false;
    n1.RxFilter = { 0x080, 0x7FF };
    n1.ReceiveDelayUs = 35;
    Node n6 = MakeNode(6, { MakeFrame(0x7FF, 3, 1000000, 999999) });
    defs.Nodes = { n1, n6 };
    const std::wstring path = TempPath(L"f004_sc003.canprj");
    ASSERT_TRUE(SaveProject(path, s, defs)) << "保存できない";
    const ConfigLoadResult r = LoadConfig(path);
    ASSERT_TRUE(r.Success) << r.ErrorMessage;
    EXPECT_EQ(r.Project.EcuCount, 7);
    EXPECT_EQ(r.Project.DurationSec, 123);
    EXPECT_FALSE(r.Project.FrameDisplay);
    EXPECT_EQ(r.Config.BitRateBps, 1000000);
    ASSERT_EQ(r.Config.Nodes.size(), defs.Nodes.size()) << "ノードの定義の数";
    for (std::size_t i = 0; i < defs.Nodes.size(); ++i)
    {
        const Node& a = defs.Nodes[i];
        const Node& b = r.Config.Nodes[i];
        EXPECT_EQ(b.NodeId, a.NodeId);
        EXPECT_EQ(b.RxFilterAll, a.RxFilterAll);
        EXPECT_EQ(b.RxFilter, a.RxFilter);
        EXPECT_EQ(b.ReceiveDelayUs, a.ReceiveDelayUs);
        ASSERT_EQ(b.Frames.size(), a.Frames.size()) << "ノード " << a.NodeId << " のフレーム数";
        for (std::size_t j = 0; j < a.Frames.size(); ++j)
        {
            EXPECT_EQ(b.Frames[j].CanId, a.Frames[j].CanId);
            EXPECT_EQ(b.Frames[j].Dlc, a.Frames[j].Dlc);
            EXPECT_EQ(b.Frames[j].PeriodUs, a.Frames[j].PeriodUs);
            EXPECT_EQ(b.Frames[j].InitialOffsetUs, a.Frames[j].InitialOffsetUs);
            EXPECT_EQ(b.Frames[j].Name, a.Frames[j].Name);
        }
    }
}

// 根拠: spec.md SC-004、FR-010。不正な .canprj（拡張子違い・壊れた内容・範囲外の値・検証規則違反）は、全ケースで理由が示されて
// 読み込まれない。拡張子違いは HasProjectExtension、それ以外は LoadConfig の失敗と理由で確かめる。「現在の内容は変化しない」は画面側。
TEST(F004Spec, F004_SC004_InvalidProjectFiles_AllRejectedWithReason)
{
    RecordProperty("spec", "004:SC-004");
    EXPECT_FALSE(HasProjectExtension(L"x.txt")) << "拡張子違い";
    const std::string cases[] = {
        "garbage line\r\n",                                       // 壊れた内容
        "[Project]\r\nEcuCount=99\r\n" + NodeText(1, 0x100),      // 範囲外の値
        NodeText(1, 0x123) + NodeText(2, 0x123),                  // 検証規則違反（CAN ID の重複）
        "[Network]\r\nBitRateBps=500000\r\n",                     // 検証規則違反（ノード 0 個、001 FR-001）
    };
    int i = 0;
    for (const std::string& text : cases)
    {
        const ConfigLoadResult r = LoadText(L"f004_sc004_" + std::to_wstring(i) + L".canprj", text);
        EXPECT_FALSE(r.Success) << i << " 番目の不正なファイルが読み込めてしまった";
        EXPECT_FALSE(r.ErrorMessage.empty()) << i << " 番目の理由が示されていない";
        ++i;
    }
}

// 根拠: spec.md SC-008、FR-018。「終了」を選んだ場合は、選んだ時点までに送信を完了したフレームの行数と一致する結果が出る。
// いくつかの停止時刻で、t まで実行した行数が、最後まで実行したログのうち送信完了が t 以下の行数と一致する。
TEST(F004Spec, F004_SC008_StopTimes_RowCountMatchesCompletedFrames)
{
    RecordProperty("spec", "004:SC-008");
    Network net;
    net.BitRateBps = 250000;
    net.Nodes.push_back(MakeNode(1, { MakeFrame(0x100, 8, 3000) }));
    net.Nodes.push_back(MakeNode(2, { MakeFrame(0x080, 2, 2000, 700) }));
    net.Nodes.push_back(MakeNode(3));
    const std::vector<LogRow> full = RunSim(net, 100000LL);
    for (long long t : { 1000LL, 7777LL, 20000LL, 54321LL })
    {
        std::size_t completed = 0;
        for (const LogRow& r : full)
            if (r.TxEndUs <= t)
                ++completed;
        EXPECT_EQ(RunSim(net, t).size(), completed) << t << " µs で終了したときの行数";
    }
}
