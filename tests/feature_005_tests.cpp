// feature_005_tests.cpp（tests）
// tools/cansim_runner/checks.cpp の CheckFeature005() から移した（機能 030）。
// 本体は、パスの取り方以外は変えていない。Check() のラベルには SC/FR の ID が残っている。

#include "test_helpers.h"

using namespace cansimtest;

TEST(Feature005, SC004_FR002to003_FR009)
{
    std::cout << "\n== 機能 005: スピード / 再生の時計 ==\n";

    // [Project] のスピード（FR-001, FR-002）
    {
        ConfigLoadResult def = LoadText("s_default.canprj", "[Project]\r\nEcuCount=2\r\n[Node1]\r\nNodeId=1\r\n");
        Check(def.Success && def.Project.Speed == 0, "005 FR-002: Speed が無いときの既定値は 0");

        struct Case { const char* Text; int Expected; };
        const Case ok[] = { { "+3", 3 }, { "-3", -3 }, { "10", 10 }, { "-10", -10 }, { "0", 0 } };
        bool accepted = true;
        for (const Case& c : ok)
        {
            ConfigLoadResult r = LoadText("s_ok.canprj", std::string("[Project]\r\nSpeed=") + c.Text + "\r\n[Node1]\r\nNodeId=1\r\n");
            accepted = accepted && r.Success && r.Project.Speed == c.Expected;
        }
        Check(accepted, "005 FR-002: Speed の +3 / -3 / 10 / -10 / 0 は受け付けられる（026: 範囲は -10..10）");

        const char* bad[] = { "11", "-14", "1.5", "abc", "", "+", "--1" };
        bool rejected = true;
        for (const char* text : bad)
        {
            ConfigLoadResult r = LoadText("s_bad.canprj", std::string("[Project]\r\nSpeed=") + text + "\r\n");
            rejected = rejected && !r.Success && r.ErrorMessage.find("Speed") != std::string::npos;
        }
        Check(rejected, "005 FR-002: Speed の 11 / -14 / 1.5 / abc / 空 は理由つきで拒否される");

        ProjectSettings s;
        s.Speed = -10;
        const std::string path = Join(TempDir(), "s_roundtrip.canprj");
        ConfigLoadResult back;
        // ノード 0 個は読込みエラー（001 FR-001）なので、ノードを 1 つ持たせて保存する
        Network oneNode;
        Node node;
        node.NodeId = 1;
        oneNode.Nodes.push_back(node);
        const bool saved = SaveProject(Widen(path), s, oneNode);
        back = LoadConfig(Widen(path));
        Check(saved && back.Success && back.Project.Speed == -10, "005 FR-002: Speed は保存して読み直しても保たれる");
    }

    // SimTimeAtUs（FR-003）
    {
        const long long e = 1000000; // 実時間の 1 秒
        Check(SimTimeAtUs(e, 0) == e && SimTimeAtUs(e, 1) == 2 * e && SimTimeAtUs(e, -1) == e / 2 &&
                  SimTimeAtUs(e, -10) == e / 1024 && SimTimeAtUs(e, -13) == e / 8192 && // 式そのものは kMinSpeed より下でも成り立つ
                  SimTimeAtUs(e, 9) == 512 * e && SimTimeAtUs(0, 5) == 0,
            "005 FR-003: シミュレーション時間 = 実時間 x 2^スピード（0, +1, -1, -10, -13, +9）");
        Check(SimTimeAtUs(LLONG_MAX / 2, 9) == LLONG_MAX, "005: SimTimeAtUs はオーバーフローせずに飽和する");
    }

    // ActiveFrameCursor（FR-009）
    {
        std::vector<FrameEvent> ev(3);
        ev[0].TxStartUs = 100; ev[0].TxEndUs = 200; ev[0].TxNodeId = 1; ev[0].RxNodeIds = { 2, 3 };
        ev[1].TxStartUs = 300; ev[1].TxEndUs = 400; ev[1].TxNodeId = 2; ev[1].RxNodeIds = { 1, 3 };
        ev[2].TxStartUs = 1000; ev[2].TxEndUs = 1100; ev[2].TxNodeId = 3; ev[2].RxNodeIds = { 1, 2 };

        ActiveFrameCursor cursor(ev);
        ActiveFrames a = cursor.Collect(0, 500);
        Check(a.BusBusy && a.Senders == std::vector<int>({ 1, 2 }) && a.Receivers == std::vector<int>({ 3 }),
            "005 FR-009: 1 回の書き換えに 2 フレーム -> 送信元は両方。送信もした ECU は送信元として示される");
        ActiveFrames idle = cursor.Collect(500, 900);
        Check(!idle.BusBusy && idle.Senders.empty() && idle.Receivers.empty(),
            "005 FR-009: 何も送られない区間では何も強調しない");
        ActiveFrames third = cursor.Collect(900, 1050);
        Check(third.Senders == std::vector<int>({ 3 }) && third.Receivers == std::vector<int>({ 1, 2 }),
            "005 FR-009: 区間の終わりでまだバス上にあるフレームは強調される");
        ActiveFrames still = cursor.Collect(1050, 1060);
        Check(still.Senders == std::vector<int>({ 3 }), "005 SC-004: 長いフレームは複数回の書き換えにわたって強調され続ける");
    }

    // どの書き換え間隔でも、すべてのフレームが少なくとも 1 回は強調される
    {
        ConfigLoadResult a = LoadConfig(Widen(Join(SampleDir(), "sample_appendix_a.canprj")));
        SimulationEngine engine(BuildRunNetwork(a.Project, a.Config), 1000000LL);
        Logger logger;
        engine.Run(logger);
        const std::vector<FrameEvent> events = BuildFrameEvents(logger.Rows());

        const long long steps[] = { 16000, 1000 };
        bool allSeen = !events.empty();
        for (long long step : steps)
        {
            std::map<std::pair<long long, int>, bool> seen;
            ActiveFrameCursor cursor(events);
            std::size_t next = 0;
            for (long long t = step; t <= 1000000 + step; t += step)
            {
                ActiveFrames f = cursor.Collect(t - step, t);
                // この区間と重なるイベントのうち、送信元が表示されるものすべてに印を付ける
                while (next < events.size() && events[next].TxStartUs <= t)
                {
                    const bool shown = std::find(f.Senders.begin(), f.Senders.end(), events[next].TxNodeId) != f.Senders.end();
                    if (shown)
                        seen[std::make_pair(events[next].TxStartUs, events[next].TxNodeId)] = true;
                    ++next;
                }
            }
            allSeen = allSeen && seen.size() == events.size();
        }
        Check(allSeen, "005 FR-009: 書き換え間隔 16 ms と 1 ms のどちらでも、すべてのフレームが少なくとも 1 回は強調される");
    }
}
