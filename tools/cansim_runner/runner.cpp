// runner.cpp (tools/cansim_runner)
// Runs one simulation exactly the way MFCApplication1Dlg does ("Simulation >
// Start"), but from the command line and without MFC.
//
//   runner.exe <project path (.canprj)> <output prefix> [duration seconds]
//
// The duration defaults to the project's DurationSec. The simulated network
// is built from the project's EcuCount (feature 004 FR-011).
// Writes <output prefix>.log.csv, .summary.csv and .startup.csv.

#include "pch.h"
#include "ConfigLoader.h"
#include "ProjectFile.h"
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
    if (argc < 3)
    {
        std::cout << "usage: runner.exe <project path (.canprj)> <output prefix> [duration seconds]\n";
        return 2;
    }

    const std::wstring projectPath = Widen(argv[1]);
    const std::wstring prefix = Widen(argv[2]);

    ConfigLoadResult project = LoadConfig(projectPath);
    if (!project.Success)
    {
        std::cout << "PROJECT ERROR: " << project.ErrorMessage << "\n";
        return 1;
    }
    const long long seconds = (argc >= 4) ? std::stoll(argv[3]) : project.Project.DurationSec;

    const Network network = BuildRunNetwork(project.Project, project.Config);
    SimulationEngine engine(network, seconds * 1000000LL);
    Logger logger;
    engine.Run(logger);

    logger.WriteCsv(prefix + L".log.csv");
    WriteMetricsCsv(ComputeMetrics(logger.Rows(), network), prefix + L".summary.csv");
    WriteStartupCsv(BuildStartupRecords(engine.GetNetwork()), prefix + L".startup.csv");

    std::cout << "rows=" << logger.Rows().size()
              << " ecus=" << network.Nodes.size() << "\n";
    return 0;
}
