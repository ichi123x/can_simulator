// runner.cpp (tools/cansim_runner)
// Runs one simulation exactly the way MFCApplication1Dlg does, but from the
// command line and without MFC.
//
//   runner.exe <config path> <duration seconds> <output prefix>
//
// writes <output prefix>.log.csv, .summary.csv and .startup.csv.

#include "pch.h"
#include "ConfigLoader.h"
#include "SimulationEngine.h"
#include "Logger.h"
#include "MetricsAggregator.h"
#include "StartupControl.h"

#include <iostream>
#include <string>

using namespace CanSim;

static std::wstring Widen(const std::string& s)
{
    return std::wstring(s.begin(), s.end());
}

int main(int argc, char** argv)
{
    if (argc < 4)
    {
        std::cout << "usage: runner.exe <config path> <duration seconds> <output prefix>\n";
        return 2;
    }

    const std::wstring configPath = Widen(argv[1]);
    const long long seconds = std::stoll(argv[2]);
    const std::wstring prefix = Widen(argv[3]);

    ConfigLoadResult cfg = LoadConfig(configPath);
    if (!cfg.Success)
    {
        std::cout << "CONFIG ERROR: " << cfg.ErrorMessage << "\n";
        return 1;
    }

    SimulationEngine engine(cfg.Config, seconds * 1000000LL);
    Logger logger;
    engine.Run(logger);

    logger.WriteCsv(prefix + L".log.csv");
    WriteMetricsCsv(ComputeMetrics(logger.Rows(), cfg.Config), prefix + L".summary.csv");
    WriteStartupCsv(BuildStartupRecords(engine.GetNetwork()), prefix + L".startup.csv");

    std::cout << "rows=" << logger.Rows().size()
              << " nodes=" << cfg.Config.Nodes.size() << "\n";
    return 0;
}
