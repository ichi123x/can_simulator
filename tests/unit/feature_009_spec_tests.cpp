// feature_009_spec_tests.cpp（tests/unit）
// 機能 009（specs/009-log-view/spec.md）の受け入れシナリオと成功基準のうち、画面を使わずに確かめられるもの。
// ログ表示エリアに出す行の作り方（LogLines.h の BuildLogLines）を確かめる。描画・スクロール・スライドバーは画面の確認。

#include "support/spec_test_helpers.h"

#include "LogLines.h"
#include "ProjectFile.h"
#include "StartupControl.h"

#include <map>

using namespace CanSim;
using namespace spectest;

namespace
{
    Node StartupNode(int nodeId, long long powerOnUs, long long initUs, const std::vector<FrameDefinition>& frames = {})
    {
        Node n = MakeNode(nodeId, frames);
        n.PowerOnTimeUs = powerOnUs;
        n.InitDurationUs = initUs;
        return n;
    }

    struct Built
    {
        std::vector<LogRow> Rows;
        Network After;
        std::vector<LogLine> Lines;
    };

    Built Build(const Network& net, long long durationUs)
    {
        Built b;
        b.Rows = RunSim(net, durationUs, &b.After);
        b.Lines = BuildLogLines(b.Rows, b.After, durationUs);
        return b;
    }
}

// 根拠: spec.md US1 受け入れシナリオ 2、FR-003。ノードの状態が遷移する（電源投入 = 初期化中、初期化完了 = バス同期検出中、
// 通常動作の開始）と、その時刻に、ノード番号と遷移後の状態の行が追加される。手計算（500 kbps、バスは空）:
// ノード 2 は 1,000 µs に電源投入、300 µs で初期化完了（1,300 µs）、22 µs で通常動作（1,322 µs）。
TEST(F009Spec, F009_US1_2_StateTransitions_OneLineEachAtItsTime)
{
    RecordProperty("spec", "009:US1-2");
    Network net;
    net.BitRateBps = 500000;
    net.Nodes.push_back(StartupNode(1, 0, 0));
    net.Nodes.push_back(StartupNode(2, 1000, 300));
    const Built b = Build(net, 10000LL);
    std::vector<std::pair<long long, int>> node2; // (時刻, 状態)
    for (const LogLine& l : b.Lines)
        if (l.Kind == LogLineKind::State && l.NodeId == 2)
            node2.push_back({ l.TimeUs, l.Index });
    ASSERT_EQ(node2.size(), 3u) << "ノード 2 の状態遷移の行が 3 つでない";
    EXPECT_EQ(node2[0], std::make_pair(1000LL, static_cast<int>(StartupState::Initializing))) << "電源投入（初期化中）";
    EXPECT_EQ(node2[1], std::make_pair(1300LL, static_cast<int>(StartupState::BusSyncDetecting))) << "初期化完了";
    EXPECT_EQ(node2[2], std::make_pair(1322LL, static_cast<int>(StartupState::Normal))) << "通常動作の開始";
}

// 根拠: spec.md US1 受け入れシナリオ 3、FR-004。送信が完了した（またはバッファ満杯で捨てられた）フレームは、CSV のログと同じ単位
// （1 フレーム × 受信ノード）で 1 行ずつ追加され、行からフレームログの行（送信元・ID・DLC・状態）がわかる。
TEST(F009Spec, F009_US1_3_FrameLines_OnePerLogRow)
{
    RecordProperty("spec", "009:US1-3");
    Network net;
    net.BitRateBps = 500000;
    net.Nodes.push_back(MakeNode(1, { MakeFrame(0x100, 8, 2000) }));
    net.Nodes.push_back(MakeNode(2, { MakeFrame(0x080, 2, 3000) }));
    net.Nodes.push_back(MakeNode(3));
    const Built b = Build(net, 20000LL);
    std::set<int> indexes;
    for (const LogLine& l : b.Lines)
    {
        if (l.Kind != LogLineKind::Frame)
            continue;
        ASSERT_GE(l.Index, 0);
        ASSERT_LT(l.Index, static_cast<int>(b.Rows.size()));
        const LogRow& r = b.Rows[static_cast<std::size_t>(l.Index)];
        EXPECT_EQ(l.NodeId, r.TxNodeId) << "行の送信元がフレームログの行と違う";
        indexes.insert(l.Index);
    }
    EXPECT_EQ(indexes.size(), b.Rows.size()) << "フレームの行が、フレームログの行と 1 対 1 でない";
}

// 根拠: spec.md US1 受け入れシナリオ 4・FR-005、Edge Cases（同じ時刻はノード番号の小さい順）、LogLines.h（同じ時刻では状態遷移が先）。
// 行は時刻の昇順で、同じ時刻なら状態遷移が先、次にノード番号の小さい順。
TEST(F009Spec, F009_US1_4_Lines_TimeAscendingStateFirstThenNode)
{
    RecordProperty("spec", "009:US1-4");
    Network net;
    net.BitRateBps = 500000;
    net.Nodes.push_back(StartupNode(3, 0, 0, { MakeFrame(0x300, 8, 1000) }));
    net.Nodes.push_back(StartupNode(1, 0, 0, { MakeFrame(0x100, 8, 1500) }));
    net.Nodes.push_back(StartupNode(2, 500, 0, { MakeFrame(0x200, 1, 700) }));
    const Built b = Build(net, 20000LL);
    ASSERT_GT(b.Lines.size(), 10u) << "前提: 行が少ない";
    for (std::size_t i = 1; i < b.Lines.size(); ++i)
    {
        const LogLine& p = b.Lines[i - 1];
        const LogLine& c = b.Lines[i];
        ASSERT_LE(p.TimeUs, c.TimeUs) << i << " 行目で時刻が戻った";
        if (p.TimeUs == c.TimeUs && p.Kind == c.Kind)
            EXPECT_LE(p.NodeId, c.NodeId) << i << " 行目: 同じ時刻・同じ種類でノード番号の順でない";
        if (p.TimeUs == c.TimeUs)
            EXPECT_FALSE(p.Kind == LogLineKind::Frame && c.Kind == LogLineKind::State)
                << i << " 行目: 同じ時刻でフレームの行が状態遷移の行より先";
    }
}

// 根拠: spec.md US1 受け入れシナリオ 6、FR-006。フレーム表示が OFF でも、ON のときと同じ行になる（行はフレーム表示の設定に
// 影響されない）。設定だけを変えた実行用ネットワークから作った行が一致することで確かめる。
TEST(F009Spec, F009_US1_6_FrameDisplayOnOff_SameLines)
{
    RecordProperty("spec", "009:US1-6");
    Network defs;
    defs.Nodes.push_back(MakeNode(1, { MakeFrame(0x100, 8, 2000) }));
    defs.Nodes.push_back(MakeNode(2, { MakeFrame(0x080, 2, 3000) }));
    std::vector<std::vector<LogLine>> results;
    for (bool display : { true, false })
    {
        ProjectSettings s;
        s.EcuCount = 2;
        s.FrameDisplay = display;
        results.push_back(Build(BuildRunNetwork(s, defs), 20000LL).Lines);
    }
    ASSERT_EQ(results[0].size(), results[1].size()) << "行数が違う";
    for (std::size_t i = 0; i < results[0].size(); ++i)
    {
        const LogLine& a = results[0][i];
        const LogLine& b = results[1][i];
        EXPECT_TRUE(a.TimeUs == b.TimeUs && a.Kind == b.Kind && a.NodeId == b.NodeId && a.Index == b.Index)
            << i << " 行目が違う";
    }
}

// 根拠: spec.md SC-002。sample_startup（6 ECU）を実行すると、各 ECU の「電源投入」「初期化完了」「通常動作」の 3 つの状態遷移
// （計 18 行）が、起動記録の時刻と一致する時刻で表示される。
TEST(F009Spec, F009_SC002_SampleStartup_EighteenStateLinesMatchRecords)
{
    RecordProperty("spec", "009:SC-002");
    const ConfigLoadResult r = LoadConfig(SampleConfigPath(L"sample_startup.canprj"));
    ASSERT_TRUE(r.Success) << "sample_startup を読めない: " << r.ErrorMessage;
    const Network run = BuildRunNetwork(r.Project, r.Config);
    ASSERT_EQ(run.Nodes.size(), 6u) << "前提: sample_startup が 6 ECU でない";
    const long long durationUs = r.Project.DurationSec * 1000000LL;
    const Built b = Build(run, durationUs);
    std::map<int, std::vector<LogLine>> byNode;
    for (const LogLine& l : b.Lines)
        if (l.Kind == LogLineKind::State)
            byNode[l.NodeId].push_back(l);
    int total = 0;
    for (const StartupRecord& rec : BuildStartupRecords(b.After))
    {
        const std::vector<LogLine>& lines = byNode[rec.NodeId];
        ASSERT_EQ(lines.size(), 3u) << "ECU " << rec.NodeId << " の状態遷移の行が 3 つでない";
        EXPECT_EQ(lines[0].TimeUs, rec.PowerOnUs) << "ECU " << rec.NodeId << " の電源投入";
        EXPECT_EQ(lines[1].TimeUs, rec.InitDoneUs) << "ECU " << rec.NodeId << " の初期化完了";
        EXPECT_EQ(lines[2].TimeUs, rec.NormalEntryUs) << "ECU " << rec.NodeId << " の通常動作";
        total += static_cast<int>(lines.size());
    }
    EXPECT_EQ(total, 18) << "状態遷移の行の合計が 18 でない";
}

// 根拠: spec.md Edge Cases（シミュレーション時間が短く、電源投入前に終わる ECU は、電源投入前の行が出ない）、LogLines.h。
// シミュレーション時間より後の遷移・到達しなかった遷移の行は出ない。
TEST(F009Spec, F009_US1_2_NotReachedTransitions_NoLines)
{
    RecordProperty("spec", "009:US1-2");
    Network net;
    net.BitRateBps = 500000;
    net.Nodes.push_back(StartupNode(1, 0, 0));
    net.Nodes.push_back(StartupNode(2, 5000, 0));   // 時間内に電源投入・通常動作
    net.Nodes.push_back(StartupNode(3, 50000, 0));  // 時間の後に電源投入
    const Built b = Build(net, 10000LL);
    for (const LogLine& l : b.Lines)
    {
        EXPECT_NE(l.NodeId, 3) << "電源投入前に終わった ECU 3 の行が出た";
        EXPECT_LE(l.TimeUs, 10000) << "シミュレーション時間より後の行が出た";
    }
}

// 根拠: spec.md SC-008、FR-011。ログ表示エリアの行を作っても、同じ構成の実行結果（CSV）は変わらない。
TEST(F009Spec, F009_SC008_BuildingLines_DoesNotChangeCsv)
{
    RecordProperty("spec", "009:SC-008");
    Network net;
    net.BitRateBps = 500000;
    net.Nodes.push_back(MakeNode(1, { MakeFrame(0x100, 8, 2000) }));
    net.Nodes.push_back(MakeNode(2, { MakeFrame(0x080, 2, 3000) }));
    const std::vector<LogRow> plain = RunSim(net, 20000LL);
    const Built withLines = Build(net, 20000LL);
    const std::wstring a = TempPath(L"f009_sc008_a.log.csv"), b = TempPath(L"f009_sc008_b.log.csv");
    WriteLogCsv(plain, a);
    WriteLogCsv(withLines.Rows, b);
    EXPECT_EQ(ReadText(a), ReadText(b)) << "行を作った実行で CSV が変わった";
}
