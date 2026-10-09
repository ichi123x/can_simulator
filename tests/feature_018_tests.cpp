// feature_018_tests.cpp（tests）
// tools/cansim_runner/checks.cpp の CheckFeature018() から移した（機能 030）。
// 本体は、パスの取り方以外は変えていない。Check() のラベルには SC/FR の ID が残っている。

#include "test_helpers.h"

using namespace cansimtest;

TEST(Feature018, FR001_FR005)
{
    std::cout << "\n== 機能 018: 流れのエフェクト（改訂 1） ==\n";
    const BusDiagramLayout six = ComputeBusDiagramLayout(6);
    const int pairs[][2] = { { 1, 2 }, { 2, 1 }, { 1, 3 }, { 3, 1 }, { 6, 1 } };
    bool ends = true, onPath = true, forward = true;
    for (const auto& pr : pairs)
    {
        const EcuPlacement& s = six.Ecus[pr[0] - 1];
        const EcuPlacement& r = six.Ecus[pr[1] - 1];
        const FlowPoint a = FlowPointAt(s, r, 0.0);
        const FlowPoint b = FlowPointAt(s, r, 1.0);
        ends = ends && std::fabs(a.X - s.LineX) < 1e-6 && std::fabs(a.Y - s.LineY1) < 1e-6 &&
            std::fabs(b.X - r.LineX) < 1e-6 && std::fabs(b.Y - r.LineY1) < 1e-6;
        FlowPoint prev = a;
        double total = 0.0, travelled = -1.0;
        for (int i = 0; i <= 100; ++i)
        {
            const FlowPoint q = FlowPointAt(s, r, i / 100.0);
            onPath = onPath && (std::fabs(q.X - s.LineX) < 1e-6 || std::fabs(q.X - r.LineX) < 1e-6 ||
                std::fabs(q.Y - kBusY) < 1e-6);
            total += std::fabs(q.X - prev.X) + std::fabs(q.Y - prev.Y);
            forward = forward && total >= travelled;
            travelled = total;
            prev = q;
        }
        forward = forward && std::fabs(total - FlowPathLength(s, r)) < 1e-6;
    }
    Check(ends, "018 FR-001: 道筋は送信側の箱の縁から始まり、受信側の箱の縁で終わる");
    Check(onPath, "018 FR-005: 道筋のすべての点は、送信側の接続線・バス・受信側の接続線のどれかの上にある");
    Check(forward, "018 FR-001: f が増えると、点は道筋に沿って先へ進む");

    // 改訂 1 (A): 一定の間隔と速さ
    bool spacing = true, count = true, speed = true;
    const EcuPlacement& e1 = six.Ecus[0];
    const EcuPlacement& e6 = six.Ecus[5];
    const std::vector<std::pair<int, int>> one = { { 1, 6 } };
    const double length = FlowPathLength(e1, e6);
    for (long long t0 : { 0LL, 12345LL, 777777LL, 3000000LL })
    {
        const FlowEffect f = ComputeFlowEffect(six, one, t0);
        const double offset = FlowOffset(t0);
        count = count && offset >= 0.0 && offset < kFlowSpacing &&
            f.Particles.size() == static_cast<std::size_t>(std::floor((length - offset) / kFlowSpacing)) + 1;
        for (std::size_t k = 1; k < f.Particles.size(); ++k)
        {
            // 隣の粒は道筋に沿って kFlowSpacing だけ離れている。角でも
            // 直線のマンハッタン距離は道筋の距離と同じになる
            const double d = std::fabs(f.Particles[k].X - f.Particles[k - 1].X) +
                std::fabs(f.Particles[k].Y - f.Particles[k - 1].Y);
            spacing = spacing && std::fabs(d - kFlowSpacing) < 1e-6;
        }
        const long long dt = 25000; // 書き換え 1 回分
        double moved = FlowOffset(t0 + dt) - offset;
        if (moved < 0)
            moved += kFlowSpacing;
        speed = speed && std::fabs(moved - kFlowSpeed * 0.025) < 1e-6;
    }
    Check(count, "018 rev.1: 粒は道筋全体に offset + 16k の位置にある（個数 = floor((L - offset) / 16) + 1）");
    Check(spacing, "018 rev.1: 隣り合う粒は道筋の上で 16 px 離れている");
    Check(speed, "018 rev.1: 粒はどの道筋でも実時間で 150 px/秒（25 ms の書き換えごとに 3.75 px）で動く");

    // 組・ほかの ECU・送信元が 2 つのとき
    const std::vector<std::pair<int, int>> noPairs;
    bool pairsOk = ComputeFlowEffect(six, noPairs, 1000).Particles.empty() &&
        ComputeFlowEffect(six, noPairs, 1000).Glows.empty();
    const std::vector<std::pair<int, int>> both = { { 1, 3 }, { 3, 1 } };
    pairsOk = pairsOk && !ComputeFlowEffect(ComputeBusDiagramLayout(3), both, 0).Particles.empty();
    Check(pairsOk, "018 FR-001: 組が無ければ何も出ない。2 つの ECU が互いに送っても粒は出る");
    {
        std::vector<FrameEvent> events(2);
        events[0].TxStartUs = 0; events[0].TxEndUs = 200; events[0].TxNodeId = 1; events[0].RxNodeIds = { 2, 3 };
        events[1].TxStartUs = 300; events[1].TxEndUs = 500; events[1].TxNodeId = 3; events[1].RxNodeIds = { 1, 2 };
        ActiveFrameCursor cursor(events);
        const ActiveFrames a = cursor.Collect(0, 25000);
        const std::vector<std::pair<int, int>> expect = { { 1, 2 }, { 1, 3 }, { 3, 1 }, { 3, 2 } };
        Check(a.Flows == expect && a.Receivers == std::vector<int>{ 2 } && a.Senders == std::vector<int>{ 1, 3 },
            "018 FR-001: Collect は区間のすべての（送信, 受信）の組を残す。送信元どうしの組も含む");
    }
    bool noOthers = true;
    const std::vector<std::pair<int, int>> oneToThree = { { 1, 3 } };
    for (long long t0 = 0; t0 < 400000; t0 += 7919)
        for (const FlowPoint& q : ComputeFlowEffect(six, oneToThree, t0).Particles)
            for (int n : { 2, 4, 5, 6 })
            {
                const EcuPlacement& o = six.Ecus[n - 1];
                const double lo = (std::min)(static_cast<double>(o.LineY1), static_cast<double>(kBusY));
                const double hi = (std::max)(static_cast<double>(o.LineY1), static_cast<double>(kBusY));
                noOthers = noOthers && !(std::fabs(q.X - o.LineX) < 1e-6 && q.Y > lo + 1e-6 && q.Y < hi - 1e-6);
            }
    Check(noOthers, "018 FR-005: 受信しない ECU の接続線の上には粒が出ない");

    // 光: ECU ごとに 1 つで [0.05, 1] の範囲、受信側は送信側より半周期遅れる
    bool glow = true;
    for (long long t0 = 0; t0 < kFlowGlowPeriodUs; t0 += 50000)
    {
        const FlowEffect f = ComputeFlowEffect(six, oneToThree, t0);
        double sender = -1;
        bool hasReceiver = false;
        for (const FlowGlow& g : f.Glows)
        {
            glow = glow && g.Strength >= 0.05 - 1e-9 && g.Strength <= 1.0 + 1e-9;
            if (g.Ecu == 1 && g.Sender) sender = g.Strength;
            if (g.Ecu == 3 && !g.Sender) hasReceiver = true;
        }
        glow = glow && f.Glows.size() == 2 && sender >= 0 && hasReceiver;
        for (const FlowGlow& g : ComputeFlowEffect(six, oneToThree, t0 + kFlowGlowPeriodUs / 2).Glows)
            if (g.Ecu == 3)
                glow = glow && std::fabs(g.Strength - sender) < 1e-6;
    }
    Check(glow, "018 rev.1: 送信側・受信側ごとに光が 1 つで [0.05, 1] の範囲、受信側は送信側より半周期遅れる");

    // 改訂 1 (D): 道筋は 0.3 秒残る
    FlowPathHold hold;
    hold.Update({ { 1, 3 } }, 0);
    bool holdOk = hold.Current(300000).size() == 1;
    holdOk = holdOk && hold.Current(300001).empty();
    hold.Update({ { 1, 3 } }, 0);
    hold.Update({ { 1, 3 }, { 2, 4 } }, 200000);
    holdOk = holdOk && hold.Current(450000).size() == 2 && hold.Current(500001).empty();
    hold.Update({ { 5, 6 } }, 600000);
    hold.Clear();
    holdOk = holdOk && hold.Current(600000).empty();
    Check(holdOk, "018 rev.1: FlowPathHold は最後に見えてから 300 ms 道筋を残し、Clear で空になる");
}

// 速度の検査は実行時間で判定するので、別のテストに分けて ctest のラベル slow を付ける
// （tests/CMakeLists.txt）。Debug では遅くて閾値（1 秒）に収まらないので、Release で実行し、
// Debug では ctest -LE slow で外す。閾値は変えない。
TEST(Feature018Slow, SC005_EffectSpeed)
{
    const BusDiagramLayout sixteen = ComputeBusDiagramLayout(16);
    std::vector<std::pair<int, int>> many;
    for (int s = 1; s <= 8; ++s)
        for (int r = 9; r <= 16; ++r)
            many.emplace_back(s, r);
    std::size_t sink = 0;
    const auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < 1000; ++i)
        sink += ComputeFlowEffect(sixteen, many, i * 25000LL).Particles.size();
    const double sec = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    std::ostringstream what;
    what << "018 SC-005: 8 x 8 ECU の 1000 回のエフェクト計算が 1 秒未満（" << sec << " s, " << sink / 1000 << " 粒/回）";
    Check(sec < 1.0 && sink > 0, what.str());
}
