// feature_007_tests.cpp（tests）
// tools/cansim_runner/checks.cpp の CheckFeature007() から移した（機能 030）。
// 本体は、パスの取り方以外は変えていない。Check() のラベルには SC/FR の ID が残っている。

#include "test_helpers.h"

using namespace cansimtest;

TEST(Feature007, SC003to004)
{
    std::cout << "\n== 機能 007: バスがアイドルになる前のインターミッション ==\n";
    const std::string text = ReadTextFile(Join(SampleDir(), "sample_collision.canprj"));
    const long long rates[] = { 125000, 250000, 500000, 1000000 };
    for (long long rate : rates)
    {
        std::string t2 = text;
        const std::size_t pos = t2.find("BitRateBps=500000");
        if (pos != std::string::npos)
            t2 = t2.substr(0, pos) + "BitRateBps=" + std::to_string(rate) + t2.substr(pos + 17);
        const std::string name = "itm_" + std::to_string(rate) + ".canprj";
        ConfigLoadResult cfg = LoadText(name, t2);
        if (!cfg.Success)
        {
            Check(false, "007: sample_collision を読み込める（" + std::to_string(rate) + " bps）");
            continue;
        }
        const Network net = BuildRunNetwork(cfg.Project, cfg.Config);
        SimulationEngine engine(net, 1000000LL);
        Logger logger;
        engine.Run(logger);

        // 送信されたフレームごとに 1 件、Tx_Start 順
        std::map<long long, LogRow> frames;
        for (const LogRow& r : logger.Rows())
            frames.insert(std::make_pair(r.TxStartUs, r));

        const long long itmUs = 3 * 1000000LL / rate; // 3 ビット時間
        int backToBack = 0;
        bool gapOk = true, durationOk = true;
        const LogRow* prev = nullptr;
        for (const auto& entry : frames)
        {
            const LogRow& cur = entry.second;
            const FrameTimingResult timing = ComputeFrameTiming(cur.CanId, cur.Dlc, MakeFrameData(cur.Dlc), rate);
            durationOk = durationOk && (cur.TxEndUs - cur.TxStartUs) * 1000 == timing.TxDurationNs;
            if (prev != nullptr)
            {
                const long long gap = cur.TxStartUs - prev->TxEndUs;
                if (cur.TxRequestUs <= prev->TxEndUs + itmUs)
                {
                    ++backToBack;
                    gapOk = gapOk && gap == itmUs;
                }
                else
                {
                    gapOk = gapOk && gap >= itmUs;
                }
            }
            prev = &cur;
        }
        std::cout << "  " << rate << " bps: フレーム数=" << frames.size() << " 連続送信=" << backToBack
                  << " ITM=" << itmUs << " us\n";
        Check(gapOk && backToBack > 0,
            "007 SC-003: " + std::to_string(rate) + " bps で、待っているフレームがあるとき次の SOF は EOF のちょうど 3 ビット時間後に始まる");
        Check(durationOk,
            "007 SC-004: " + std::to_string(rate) + " bps で、Tx_End - Tx_Start はフレームだけの長さ（インターミッションを含まない）");
    }
}
