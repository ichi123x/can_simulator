// feature_009_tests.cpp（tests）
// tools/cansim_runner/checks.cpp の CheckFeature009() から移した（機能 030）。
// 本体は、パスの取り方以外は変えていない。Check() のラベルには SC/FR の ID が残っている。

#include "test_helpers.h"

using namespace cansimtest;

TEST(Feature009, SC002_FR004to005)
{
    std::cout << "\n== 機能 009: ログ表示の行 ==\n";
    ConfigLoadResult cfg = LoadConfig(Widen(Join(SampleDir(), "sample_startup.canprj")));
    Check(cfg.Success, "sample_startup.canprj を読み込める");
    if (!cfg.Success)
        return;
    const Network net = BuildRunNetwork(cfg.Project, cfg.Config);
    const long long duration = 10 * 1000000LL;
    SimulationEngine engine(net, duration);
    Logger logger;
    engine.Run(logger);
    const std::vector<LogLine> lines = BuildLogLines(logger.Rows(), engine.GetNetwork(), duration);

    // (a) 起動記録の時刻に 18 行の状態行
    int states = 0, frames = 0;
    bool timesOk = true;
    for (const LogLine& l : lines)
    {
        if (l.Kind == LogLineKind::Frame)
        {
            ++frames;
            continue;
        }
        ++states;
        const Node* n = FindNode(engine.GetNetwork(), l.NodeId);
        const StartupState s = static_cast<StartupState>(l.Index);
        const long long expected = !n ? -2
            : s == StartupState::Initializing ? n->PowerOnTimeUs
            : s == StartupState::BusSyncDetecting ? n->InitDoneUs
            : n->NormalEntryUs;
        timesOk = timesOk && l.TimeUs == expected;
    }
    std::cout << "  状態行=" << states << " フレーム行=" << frames << " ログ行=" << logger.Rows().size() << "\n";
    Check(states == 18 && timesOk, "009 SC-002: 6 ECU x 3 状態の行が起動記録の時刻に並ぶ");

    // (b) 並び順: 時刻、状態がフレームより先、ノード番号
    bool ordered = true;
    for (std::size_t i = 1; i < lines.size(); ++i)
    {
        const LogLine& a = lines[i - 1];
        const LogLine& b = lines[i];
        if (a.TimeUs != b.TimeUs)
            ordered = ordered && a.TimeUs < b.TimeUs;
        else if (a.Kind != b.Kind)
            ordered = ordered && a.Kind == LogLineKind::State;
        else
            ordered = ordered && a.NodeId <= b.NodeId;
    }
    Check(ordered, "009 FR-005: 行は時刻順、同じ時刻なら状態が先、そのあとノード番号順");

    // (c) ログの 1 行につきフレーム行 1 行
    Check(frames == static_cast<int>(logger.Rows().size()), "009 FR-004: ログの 1 行（フレーム x 受信ノード）につきフレーム行が 1 行");

    // (d) 実行の終わりより後の遷移は出さない
    {
        const std::vector<LogLine> shortLines = BuildLogLines(std::vector<LogRow>(), engine.GetNetwork(), 25);
        bool within = true;
        for (const LogLine& l : shortLines)
            within = within && l.TimeUs <= 25;
        Check(within && !shortLines.empty() && shortLines.size() < 18,
            "009: シミュレーション時間より後の状態行は出さない");
    }

    // (e) 実行の終わりより後に電源が入るノードの行は無い
    {
        ConfigLoadResult late = LoadText("l_late.canprj",
            "[Project]\r\nEcuCount=2\r\n[Node2]\r\nNodeId=2\r\nPowerOnTimeUs=5000000\r\n");
        SimulationEngine e2(BuildRunNetwork(late.Project, late.Config), 1000000LL);
        Logger l2;
        e2.Run(l2);
        bool none2 = true;
        for (const LogLine& l : BuildLogLines(l2.Rows(), e2.GetNetwork(), 1000000LL))
            none2 = none2 && l.NodeId != 2;
        Check(late.Success && none2, "009: 実行の終わりより後に電源が入るノードの状態行は無い");
    }

    // (f) OVERWRITE の行もフレーム行
    {
        ConfigLoadResult bf = LoadConfig(Widen(Join(SampleDir(), "sample_buffer_full.canprj")));
        const Network bnet = BuildRunNetwork(bf.Project, bf.Config);
        SimulationEngine e3(bnet, 1000000LL);
        Logger l3;
        e3.Run(l3);
        const std::vector<LogLine> bl = BuildLogLines(l3.Rows(), e3.GetNetwork(), 1000000LL);
        int overwrite = 0;
        for (const LogLine& l : bl)
            if (l.Kind == LogLineKind::Frame && l3.Rows()[static_cast<std::size_t>(l.Index)].Status == kStatusOverwrite)
                ++overwrite;
        Check(overwrite > 0, "009: OVERWRITE の行もログ表示に出る");
    }
}
