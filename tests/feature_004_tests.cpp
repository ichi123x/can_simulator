// feature_004_tests.cpp（tests）
// tools/cansim_runner/checks.cpp の CheckFeature004() から移した（機能 030）。
// 本体は、パスの取り方以外は変えていない。Check() のラベルには SC/FR の ID が残っている。

#include "test_helpers.h"

using namespace cansimtest;

TEST(Feature004, SC003_SC008to009_FR005to006_FR010to011)
{
    std::cout << "\n== 機能 004: プロジェクトファイル / 実行用ネットワーク / フレームイベント ==\n";

    // [Project] の既定値と検査（FR-005, FR-006, FR-010）
    {
        ConfigLoadResult r = LoadText("p_default.canprj", "[Network]\r\nBitRateBps=250000\r\n");
        Check(r.Success && r.Project.EcuCount == 2 && r.Project.DurationSec == 10 &&
                  r.Project.FrameDisplay && r.Config.Nodes.empty() && r.Config.BitRateBps == 250000,
            "004 FR-006 / 021: [Project] の既定値は EcuCount=2, DurationSec=10, FrameDisplay=1（021）で、ノード 0 個も許される");

        const char* bad[] = { "EcuCount=17", "EcuCount=-1", "DurationSec=0", "DurationSec=601",
                              "FrameDisplay=2", "EcuCount=abc" };
        bool allRejected = true;
        for (const char* kv : bad)
        {
            ConfigLoadResult b = LoadText("p_bad.canprj", std::string("[Project]\r\n") + kv + "\r\n");
            allRejected = allRejected && !b.Success && b.ErrorMessage.find("[Project]") != std::string::npos;
        }
        Check(allRejected, "004 FR-010: 範囲外の [Project] の値は理由つきで拒否される");

        ConfigLoadResult edge = LoadText("p_edge.canprj",
            "[Project]\r\nEcuCount=16\r\nDurationSec=600\r\nFrameDisplay=1\r\n");
        Check(edge.Success && edge.Project.EcuCount == 16 && edge.Project.DurationSec == 600 &&
                  edge.Project.FrameDisplay,
            "004 FR-005: 境界値 EcuCount=16, DurationSec=600, FrameDisplay=1 は受け付けられる");
    }

    // 保存 -> 再読込みの往復（SC-003）
    {
        ConfigLoadResult src = LoadConfig(Widen(Join(SampleDir(), "sample_collision.canprj")));
        ProjectSettings s;
        s.EcuCount = 5;
        s.DurationSec = 20;
        s.FrameDisplay = true;
        src.Config.BitRateBps = 250000;
        const std::string path = Join(TempDir(), "roundtrip.canprj");
        const bool saved = SaveProject(Widen(path), s, src.Config);
        ConfigLoadResult back = LoadConfig(Widen(path));
        bool same = saved && back.Success && back.Project.EcuCount == 5 && back.Project.DurationSec == 20 &&
                    back.Project.FrameDisplay && back.Config.BitRateBps == 250000 &&
                    back.Config.Nodes.size() == src.Config.Nodes.size();
        for (std::size_t i = 0; same && i < src.Config.Nodes.size(); ++i)
        {
            const Node& a = src.Config.Nodes[i];
            const Node& b = back.Config.Nodes[i];
            same = a.NodeId == b.NodeId && a.Name == b.Name && a.RxFilterAll == b.RxFilterAll &&
                   a.RxFilter == b.RxFilter && a.ReceiveDelayUs == b.ReceiveDelayUs &&
                   a.PowerOnTimeUs == b.PowerOnTimeUs && a.InitDurationUs == b.InitDurationUs &&
                   a.Frames.size() == b.Frames.size();
            for (std::size_t f = 0; same && f < a.Frames.size(); ++f)
                same = a.Frames[f].CanId == b.Frames[f].CanId && a.Frames[f].Dlc == b.Frames[f].Dlc &&
                       a.Frames[f].Data == b.Frames[f].Data && a.Frames[f].PeriodUs == b.Frames[f].PeriodUs &&
                       a.Frames[f].InitialOffsetUs == b.Frames[f].InitialOffsetUs;
        }
        Check(same, "004 SC-003: 保存して読み直すと 4 つの設定とすべてのノード・フレームの定義が元に戻る");
    }

    // BuildRunNetwork（FR-011）
    {
        ConfigLoadResult src = LoadConfig(Widen(Join(SampleDir(), "sample_collision.canprj")));
        ProjectSettings s = src.Project;

        s.EcuCount = 5;
        Network up = BuildRunNetwork(s, src.Config);
        bool upOk = up.Nodes.size() == 5 && up.BitRateBps == src.Config.BitRateBps;
        for (int n = 1; upOk && n <= 5; ++n)
            upOk = up.Nodes[n - 1].NodeId == n;
        upOk = upOk && up.Nodes[3].Frames.empty() && up.Nodes[3].Name == "ECU4" && up.Nodes[2].Frames.size() == 1;
        Check(upOk, "004 FR-011: 定義のない ECU は（フレームなしで）補われ、1..EcuCount の番号になる");

        s.EcuCount = 2;
        Network down = BuildRunNetwork(s, src.Config);
        Check(down.Nodes.size() == 2 && FindNode(down, 3) == nullptr,
            "004 FR-011: EcuCount を超える定義（ECU 3）は実行されない");

        s.EcuCount = 0;
        Check(BuildRunNetwork(s, src.Config).Nodes.empty(), "004 FR-011: EcuCount=0 なら空のネットワークになる");
    }

    // ECU なし・フレームなしの実行はヘッダーだけの CSV を出す（US3 AS5）
    {
        ConfigLoadResult empty = LoadText("p_zero.canprj", "[Project]\r\nEcuCount=0\r\n");
        ConfigLoadResult noFrames = LoadText("p_noframes.canprj", "[Project]\r\nEcuCount=2\r\n");
        bool ok = empty.Success && noFrames.Success;
        std::vector<LogRow> rows0, rows2;
        ok = ok && RunProjectToCsv(Join(TempDir(), "p_zero.canprj"), Join(TempDir(), "p_zero"), 1000000LL, &rows0);
        ok = ok && RunProjectToCsv(Join(TempDir(), "p_noframes.canprj"), Join(TempDir(), "p_noframes"), 1000000LL, &rows2);
        ok = ok && rows0.empty() && rows2.empty() &&
             !ReadTextFile(Join(TempDir(), "p_zero.log.csv")).empty() &&
             !ReadTextFile(Join(TempDir(), "p_noframes.summary.csv")).empty();
        Check(ok, "004 US3 AS5: ECU 数 0 とフレームのない ECU の実行は、ヘッダーだけの CSV を書く");
    }

    // BuildFrameEvents（data-model.md）
    {
        std::vector<LogRow> rows;
        ConfigLoadResult a = LoadConfig(Widen(Join(SampleDir(), "sample_appendix_a.canprj")));
        SimulationEngine engine(BuildRunNetwork(a.Project, a.Config), 100000LL);
        Logger logger;
        engine.Run(logger);
        rows = logger.Rows();
        const std::vector<FrameEvent> events = BuildFrameEvents(rows);

        std::size_t rxTotal = 0;
        bool sorted = true, noSelf = true;
        for (std::size_t i = 0; i < events.size(); ++i)
        {
            for (int rx : events[i].RxNodeIds)
            {
                ++rxTotal;
                noSelf = noSelf && rx != events[i].TxNodeId;
            }
            if (i > 0)
                sorted = sorted && events[i - 1].TxStartUs <= events[i].TxStartUs;
        }
        std::size_t rowsWithRx = 0;
        for (const LogRow& r : rows)
            if (r.RxNodeId != -1)
                ++rowsWithRx;
        Check(!events.empty() && events.size() < rows.size() && rxTotal == rowsWithRx && sorted && noSelf,
            "004: BuildFrameEvents はフレームごとに行をまとめ、受信側をすべて残し、Tx_Start 順に並べる");

        std::vector<LogRow> single(1);
        single[0].TxNodeId = 1;
        single[0].RxNodeId = -1;
        single[0].TxStartUs = 5;
        const std::vector<FrameEvent> one = BuildFrameEvents(single);
        Check(one.size() == 1 && one[0].RxNodeIds.empty(),
            "004: 受信側のないフレームは、受信側なしのイベントになる");

        // イベント k の開始で止める（FR-018 / SC-008）: t = Tx_Start まで
        // 再実行すると、t までに完了したフレームだけが残る。
        const std::size_t k = events.size() / 2;
        const long long t = events[k].TxStartUs;
        SimulationEngine stopEngine(BuildRunNetwork(a.Project, a.Config), t);
        Logger stopLogger;
        stopEngine.Run(stopLogger);
        bool completedOnly = true;
        std::size_t expected = 0;
        for (const LogRow& r : rows)
            if (r.TxEndUs <= t)
                ++expected;
        for (const LogRow& r : stopLogger.Rows())
            completedOnly = completedOnly && r.TxEndUs <= t;
        std::cout << "  t=" << t << " us で停止: 行数=" << stopLogger.Rows().size()
                  << " 期待値=" << expected << "\n";
        Check(completedOnly && stopLogger.Rows().size() == expected,
            "004 SC-008: t で止めると、t までに送信が完了したフレームだけが残る");
    }

    // SC-009: サンプルの結果が基準と同じになる。機能 006 で結果が変わった
    // （データ 0xFF、送信要求を Normal に入った時点から数える）ので、基準は
    // 機能 006 以降のビルドで作ったものを使う。
    if (!BaselineDir().empty())
    {
        const char* samples[] = { "sample_appendix_a", "sample_collision", "sample_startup",
                                  "sample_startup_latejoin" };
        for (const char* name : samples)
        {
            const std::string prefix = Join(TempDir(), std::string("sc009_") + name);
            const bool ran = RunProjectToCsv(Join(SampleDir(), std::string(name) + ".canprj"), prefix,
                10 * 1000000LL);
            bool same = ran;
            const char* kinds[] = { ".log.csv", ".summary.csv", ".startup.csv" };
            for (const char* kind : kinds)
            {
                std::string before = ReadTextFile(Join(BaselineDir(), std::string(name) + kind));
                std::string now = NormalizeToBaseline(ReadTextFile(prefix + kind), before);
                // sample_collision には待機 ECU 2 が加わった（サンプル内のコメント参照）。
                if (std::string(name) == "sample_collision" && std::string(kind) == ".startup.csv")
                {
                    now = DropStartupRow(now, 2);
                    before = DropStartupRow(before, 2);
                }
                same = same && !before.empty() && now == before;
            }
            Check(same, std::string("004 SC-009: ") + name + ".canprj は基準 CSV と同じ結果になる（006 以降）");
        }
    }
    else
    {
        std::cout << "  （SC-009 は省略: 基準ディレクトリが指定されていない）\n";
    }
    if (BaselineDir().empty())
        std::cout << "[  注記   ] 基準 CSV との比較は省略しました（CANSIM_BASELINE_DIR を設定してください。tools/cansim_runner/README.md 参照）\n";
}
