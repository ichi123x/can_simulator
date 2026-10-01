// checks.cpp (tools/cansim_runner)
// Automated verification of the success criteria of both features, driving the
// CanSim core directly (no MFC, no GUI).
//
//   checks.exe <SampleConfigs dir> <temp output dir>
//
// Exit code 0 when every check passes. See the quickstart.md of
// specs/001-can-simulation-core and specs/002-node-startup-control.

#include "pch.h"
#include "ConfigLoader.h"
#include "SimulationEngine.h"
#include "Logger.h"
#include "MetricsAggregator.h"
#include "StartupControl.h"
#include "FrameTiming.h"
#include "BusDiagramLayout.h"
#include "ProjectFile.h"
#include "FrameEvents.h"
#include "ReplayClock.h"
#include "LogLines.h"
#include "FrameEditor.h"
#include "SimControls.h"
#include "FlowEffect.h"

#include <climits>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

using namespace CanSim;

namespace
{
    int g_failures = 0;
    std::string g_sampleDir;
    std::string g_tempDir;

    void Check(bool ok, const std::string& what)
    {
        std::cout << (ok ? "[PASS] " : "[FAIL] ") << what << "\n";
        if (!ok)
            ++g_failures;
    }

    std::wstring Widen(const std::string& s)
    {
        return std::wstring(s.begin(), s.end());
    }

    std::string Join(const std::string& dir, const std::string& name)
    {
        if (dir.empty())
            return name;
        const char last = dir[dir.size() - 1];
        if (last == '\\' || last == '/')
            return dir + name;
        return dir + "\\" + name;
    }

    struct RunResult
    {
        bool Loaded = false;
        std::string Error;
        Network Config;   // as loaded
        Network EndState; // after the run (startup state, etc.)
        std::vector<LogRow> Rows;
    };

    RunResult RunConfigFile(const std::string& path, long long durationUs)
    {
        RunResult r;
        ConfigLoadResult cfg = LoadConfig(Widen(path));
        r.Loaded = cfg.Success;
        r.Error = cfg.ErrorMessage;
        if (!cfg.Success)
            return r;

        r.Config = cfg.Config;
        SimulationEngine engine(cfg.Config, durationUs);
        Logger logger;
        engine.Run(logger);
        r.Rows = logger.Rows();
        r.EndState = engine.GetNetwork();
        return r;
    }

    bool WriteTextFile(const std::string& path, const std::string& content)
    {
        std::ofstream f(path.c_str(), std::ios::binary);
        if (!f)
            return false;
        f << content;
        return true;
    }

    const Node* FindNode(const Network& net, int nodeId)
    {
        for (const Node& n : net.Nodes)
            if (n.NodeId == nodeId)
                return &n;
        return nullptr;
    }

    bool RowsEqual(const std::vector<LogRow>& a, const std::vector<LogRow>& b)
    {
        if (a.size() != b.size())
            return false;
        for (std::size_t i = 0; i < a.size(); ++i)
        {
            if (a[i].TimestampUs != b[i].TimestampUs || a[i].CanId != b[i].CanId ||
                a[i].Dlc != b[i].Dlc || a[i].TxNodeId != b[i].TxNodeId ||
                a[i].RxNodeId != b[i].RxNodeId || a[i].TxRequestUs != b[i].TxRequestUs ||
                a[i].TxStartUs != b[i].TxStartUs || a[i].TxEndUs != b[i].TxEndUs ||
                a[i].RxProcessUs != b[i].RxProcessUs || a[i].Status != b[i].Status ||
                a[i].RetryCount != b[i].RetryCount || a[i].UpdateCount != b[i].UpdateCount)
                return false;
        }
        return true;
    }

    // ---------- feature 001 ----------

    void CheckFeature001()
    {
        std::cout << "\n== feature 001: CAN simulation core ==\n";
        const std::string cfgPath = Join(g_sampleDir, "sample_appendix_a.canprj");
        const long long tenSeconds = 10 * 1000000LL;

        RunResult run1 = RunConfigFile(cfgPath, tenSeconds);
        Check(run1.Loaded, "sample_appendix_a.canprj loads");
        if (!run1.Loaded)
        {
            std::cout << "  error: " << run1.Error << "\n";
            return;
        }
        Check(run1.Config.Nodes.size() == 6, "sample_appendix_a.canprj has 6 nodes");
        Check(!run1.Rows.empty(), "US1 AS#1: the run produces log rows");

        // SC-001 reproducibility
        RunResult run2 = RunConfigFile(cfgPath, tenSeconds);
        Check(RowsEqual(run1.Rows, run2.Rows), "SC-001: two identical runs produce identical logs");

        // SC-002 / US2 AS#1: 0x080 wins arbitration against 0x100
        long long first080 = -1, first100 = -1;
        int retry100 = -1;
        bool sawLostStatus = false;
        for (const LogRow& row : run1.Rows)
        {
            if (row.CanId == 0x080 && first080 < 0)
                first080 = row.TxStartUs;
            if (row.CanId == 0x100 && first100 < 0)
            {
                first100 = row.TxStartUs;
                retry100 = row.RetryCount;
            }
            if (row.CanId == 0x100 && row.Status == "ARBITRATION_LOST_RETRY")
                sawLostStatus = true;
        }
        std::cout << "  0x080 first Tx_Start=" << first080
                  << " us, 0x100 first Tx_Start=" << first100
                  << " us, 0x100 Retry_Count=" << retry100 << "\n";
        Check(first080 >= 0 && first100 >= 0, "both 0x080 and 0x100 are transmitted");
        Check(first080 < first100, "SC-002: the lower CAN ID (0x080) transmits first");
        Check(retry100 >= 1, "US2 AS#1: 0x100 records Retry_Count >= 1");
        Check(sawLostStatus, "US2 AS#1: 0x100 records Status=ARBITRATION_LOST_RETRY");

        // With feature 002 in place every node needs 11 recessive bit times
        // (22 us at 500 kbps) before it may transmit, so the first frame of the
        // initial burst starts at 22 us instead of 0.
        Check(first080 == 162, "002 SC-006: 0x080 first Tx_Start is 140 us + 22 us startup = 162 us");

        // SC-004: transmit counts follow duration / period
        std::map<unsigned, long long> periods;
        for (const Node& n : run1.Config.Nodes)
            for (const FrameDefinition& fd : n.Frames)
                periods[fd.CanId] = fd.PeriodUs;
        std::map<unsigned, std::set<long long> > starts;
        for (const LogRow& row : run1.Rows)
            starts[row.CanId].insert(row.TxStartUs);
        for (std::map<unsigned, std::set<long long> >::const_iterator it = starts.begin();
             it != starts.end(); ++it)
        {
            const long long expected = tenSeconds / periods[it->first];
            const long long actual = static_cast<long long>(it->second.size());
            std::ostringstream os;
            os << "SC-004: CAN ID 0x" << std::hex << it->first << std::dec
               << " transmitted " << actual << " times (expected about " << expected << ")";
            Check(actual >= expected - 1 && actual <= expected + 1, os.str());
        }

        // SC-003: frame length follows 34 + 8*DLC unstuffed, + stuff bits + 10 fixed
        {
            std::vector<std::uint8_t> none;
            FrameTimingResult t0 = ComputeFrameTiming(0x000, 0, none, 500000);
            Check(t0.TotalBits - t0.StuffBitCount - 10 == 34,
                "SC-003: DLC=0 unstuffed SOF..CRC length is 34 bits");

            std::vector<std::uint8_t> data8(8, 0x00);
            FrameTimingResult t8 = ComputeFrameTiming(0x080, 8, data8, 500000);
            Check(t8.TotalBits == 34 + 64 + t8.StuffBitCount + 10,
                "SC-003: DLC=8 total bits = unstuffed + stuff bits + 10 fixed bits");
            Check(t8.StuffBitCount >= 0 && t8.StuffBitCount <= 24,
                "SC-003: DLC=8 stuff bit count stays within the worst case bound of 24");
        }

        // US1 AS#2 / SC-005: configuration rejection
        {
            std::ostringstream cfg;
            cfg << "[Network]\r\nBitRateBps=500000\r\n";
            for (int i = 1; i <= 17; ++i)
            {
                cfg << "[Node" << i << "]\r\nNodeId=" << i << "\r\n";
                cfg << "[Node" << i << ".Frame1]\r\nCanId=" << std::hex << (0x200 + i) << std::dec
                    << "\r\nDlc=0\r\nData=\r\nPeriodUs=1000\r\n";
            }
            const std::string path = Join(g_tempDir, "too_many_nodes.cfg");
            WriteTextFile(path, cfg.str());
            ConfigLoadResult r = LoadConfig(Widen(path));
            Check(!r.Success, "US1 AS#2 / SC-005: a 17 node configuration is rejected");
            if (!r.Success)
                std::cout << "  reason: " << r.ErrorMessage << "\n";
        }
        {
            std::ostringstream cfg;
            cfg << "[Network]\r\nBitRateBps=500000\r\n"
                << "[Node1]\r\nNodeId=1\r\n[Node1.Frame1]\r\nCanId=0x080\r\nDlc=0\r\nData=\r\nPeriodUs=1000\r\n"
                << "[Node2]\r\nNodeId=2\r\n[Node2.Frame1]\r\nCanId=0x080\r\nDlc=0\r\nData=\r\nPeriodUs=1000\r\n";
            const std::string path = Join(g_tempDir, "duplicate_id.cfg");
            WriteTextFile(path, cfg.str());
            ConfigLoadResult r = LoadConfig(Widen(path));
            Check(!r.Success, "FR-004: the same CAN ID on two nodes is rejected");
            if (!r.Success)
                std::cout << "  reason: " << r.ErrorMessage << "\n";
        }
    }

    // ---------- feature 002 ----------

    void CheckFeature002()
    {
        std::cout << "\n== feature 002: node startup control ==\n";
        const std::string startupCfg = Join(g_sampleDir, "sample_startup.canprj");

        // SC-001 / SC-003: power-on + init + 11 bit times (22 us at 500 kbps)
        RunResult run = RunConfigFile(startupCfg, 1000000LL);
        Check(run.Loaded, "sample_startup.canprj loads");
        if (!run.Loaded)
        {
            std::cout << "  error: " << run.Error << "\n";
            return;
        }
        bool allMatch = true;
        for (const Node& n : run.EndState.Nodes)
        {
            const long long expected = n.PowerOnTimeUs + n.InitDurationUs + 22;
            if (n.NormalEntryUs != expected)
            {
                allMatch = false;
                std::cout << "  node " << n.NodeId << " Normal_Entry=" << n.NormalEntryUs
                          << " expected " << expected << "\n";
            }
        }
        Check(allMatch, "SC-001 / SC-003: every Normal_Entry_us equals power-on + init + 22 us");

        // SC-002: nothing is ever transmitted before the sending node is Normal
        bool beforeNormal = false;
        for (const LogRow& row : run.Rows)
        {
            const Node* tx = FindNode(run.EndState, row.TxNodeId);
            if (tx && (tx->NormalEntryUs < 0 || row.TxStartUs < tx->NormalEntryUs))
                beforeNormal = true;
        }
        Check(!beforeNormal, "SC-002: no frame starts before its node reaches Normal");

        // FR-010: a request raised during startup keeps its original request time
        {
            const Node* late = FindNode(run.EndState, 6);
            long long firstRequest = -1, firstStart = -1;
            for (const LogRow& row : run.Rows)
            {
                if (row.TxNodeId == 6)
                {
                    firstRequest = row.TxRequestUs;
                    firstStart = row.TxStartUs;
                    break;
                }
            }
            std::cout << "  node6 first frame: Tx_Request=" << firstRequest
                      << " Tx_Start=" << firstStart << "\n";
            // Feature 006 FR-015 (domain spec FR-020a): requests are counted from
            // the Normal entry, so node 6 (power-on 50 ms) raises its first
            // request at Normal entry instead of holding one raised at 0.
            Check(late != nullptr && firstRequest == late->NormalEntryUs && firstStart == late->NormalEntryUs,
                "006 FR-015 (was 002 FR-010): the first request of a late node is raised at its Normal entry");
        }

        // SC-005: reproducibility including the startup records
        {
            RunResult again = RunConfigFile(startupCfg, 1000000LL);
            bool same = RowsEqual(run.Rows, again.Rows);
            const std::vector<StartupRecord> a = BuildStartupRecords(run.EndState);
            const std::vector<StartupRecord> b = BuildStartupRecords(again.EndState);
            if (a.size() != b.size())
                same = false;
            else
                for (std::size_t i = 0; i < a.size(); ++i)
                    if (a[i].NormalEntryUs != b[i].NormalEntryUs ||
                        a[i].InitDoneUs != b[i].InitDoneUs ||
                        a[i].ReachedNormal != b[i].ReachedNormal)
                        same = false;
            Check(same, "SC-005: two identical runs produce identical logs and startup records");
        }

        // FR-013: a node powered on after the run never reaches Normal and stays silent
        {
            std::ifstream src(startupCfg.c_str(), std::ios::binary);
            std::ostringstream buf;
            buf << src.rdbuf();
            std::string text = buf.str();
            const std::string needle = "PowerOnTimeUs=50000";
            const std::size_t pos = text.find(needle);
            if (pos != std::string::npos)
                text = text.substr(0, pos) + "PowerOnTimeUs=2000000" +
                       text.substr(pos + needle.size());
            const std::string path = Join(g_tempDir, "late_node.cfg");
            WriteTextFile(path, text);

            RunResult late = RunConfigFile(path, 1000000LL);
            const Node* n6 = FindNode(late.EndState, 6);
            int txRows = 0, rxRows = 0;
            for (const LogRow& row : late.Rows)
            {
                if (row.TxNodeId == 6) ++txRows;
                if (row.RxNodeId == 6) ++rxRows;
            }
            Check(n6 != nullptr && n6->Startup != StartupState::Normal && n6->NormalEntryUs < 0,
                "FR-013: a node powered on after the run never reaches Normal");
            Check(txRows == 0 && rxRows == 0,
                "FR-008 / FR-009: a node that never reaches Normal neither sends nor receives");
        }

        // SC-004 / FR-007: joining a bus that is already busy
        {
            const std::string path = Join(g_sampleDir, "sample_startup_latejoin.canprj");
            RunResult lj = RunConfigFile(path, 1000000LL);
            Check(lj.Loaded, "sample_startup_latejoin.canprj loads");
            if (lj.Loaded)
            {
                const Node* a = FindNode(lj.EndState, 1);
                const Node* b = FindNode(lj.EndState, 2);
                const Node* c = FindNode(lj.EndState, 3);
                std::cout << "  Normal_Entry: node1=" << (a ? a->NormalEntryUs : -1)
                          << " node2=" << (b ? b->NormalEntryUs : -1)
                          << " node3=" << (c ? c->NormalEntryUs : -1) << "\n";
                Check(a && a->NormalEntryUs == 22, "node 1 syncs on an idle bus at 22 us");
                Check(b && b->NormalEntryUs == 280,
                    "SC-004: a node joining mid-frame syncs at the intermission end (280 us)");
                // Feature 006: node 1's second request moved from 300 to 322 us
                // (Normal 22 + period 300) and node 3 now powers on at 305 us, so
                // its window (305 + 22 = 327) is cut by the frame 322 -> 574 and
                // it syncs at 574 - 16 + 22 = 580 us.
                Check(c && c->NormalEntryUs == 580,
                    "FR-007: a node interrupted by a new frame restarts and syncs at 580 us");
            }
        }

        // Feature 006 FR-015: no request is raised before the node is Normal, so
        // nothing piles up while it waits for startup (the 002 case of 5
        // overwrites during startup no longer occurs).
        {
            std::ifstream src(startupCfg.c_str(), std::ios::binary);
            std::ostringstream buf;
            buf << src.rdbuf();
            std::string text = buf.str();
            const std::size_t node6 = text.find("[Node6.Frame1]");
            if (node6 != std::string::npos)
            {
                const std::size_t period = text.find("PeriodUs=100000", node6);
                if (period != std::string::npos)
                    text = text.substr(0, period) + "PeriodUs=10000" +
                           text.substr(period + std::string("PeriodUs=100000").size());
            }
            const std::string path = Join(g_tempDir, "overwrite.cfg");
            WriteTextFile(path, text);

            RunResult ow = RunConfigFile(path, 1000000LL);
            int firstOverwrite = -1;
            for (const LogRow& row : ow.Rows)
            {
                if (row.TxNodeId == 6)
                {
                    firstOverwrite = row.UpdateCount;
                    break;
                }
            }
            std::cout << "  node6 first frame Overwrite_Count=" << firstOverwrite << "\n";
            Check(firstOverwrite == 0,
                "006 FR-015: a node waiting for startup has no request yet, so Overwrite_Count is 0");
        }
    }
    // ---- feature 003: bus diagram layout ----
    bool InsideDiagram(const DiagramRect& r)
    {
        return r.Left >= 0 && r.Top >= 0 && r.Right <= kDiagramWidth && r.Bottom <= kDiagramHeight;
    }

    void CheckFeature003()
    {
        std::cout << "\n== feature 003: bus diagram layout ==\n";

        const int counts[] = { 0, 1, 6, 16 };
        bool countsOk = true;
        for (int n : counts)
            countsOk = countsOk && ComputeBusDiagramLayout(n).Ecus.size() == static_cast<std::size_t>(n);
        Check(countsOk, "003 SC-002: ECU count 0/1/6/16 matches the number of placed ECUs");

        const BusDiagramLayout full = ComputeBusDiagramLayout(16);
        bool sideOk = true, pitchOk = true, insideOk = InsideDiagram(full.BusLine), clearOk = true;
        for (std::size_t i = 0; i < full.Ecus.size(); ++i)
        {
            const EcuPlacement& e = full.Ecus[i];
            sideOk = sideOk && e.Number == static_cast<int>(i) + 1 && e.OnTop == (e.Number % 2 == 1) &&
                     (e.OnTop ? e.Box.Bottom <= full.BusLine.Top : e.Box.Top >= full.BusLine.Bottom);
            if (i > 0)
                pitchOk = pitchOk && e.LineX - full.Ecus[i - 1].LineX == kEcuPitch;
            insideOk = insideOk && InsideDiagram(e.Box);
            clearOk = clearOk && e.LineX > kCaptionRight;
        }
        Check(sideOk, "003 FR-005: odd ECUs above the bus, even ECUs below, in number order");
        Check(pitchOk, "003 FR-009: ECU center pitch is a constant 56 px");
        Check(full.Ecus.front().LineX == 150 && full.Ecus.back().LineX == 990,
            "003 FR-011: ECU 01 stub at x=150 and ECU 16 stub at x=990 (140 px margin at both bus ends)");
        Check(full.Ecus.front().LineX - full.BusLine.Left == full.BusLine.Right - full.Ecus.back().LineX,
            "003 FR-011: the bus margin is symmetric");
        Check(clearOk, "003 SC-006: every stub line is right of the CAN BUS caption (x > 118)");
        Check(insideOk, "003 SC-001: bus and all 16 ECU boxes fit inside the 1140x216 diagram");

        bool stableOk = true;
        for (int n = 1; n <= 16; ++n)
        {
            const BusDiagramLayout part = ComputeBusDiagramLayout(n);
            for (std::size_t i = 0; i < part.Ecus.size(); ++i)
            {
                const EcuPlacement& a = part.Ecus[i];
                const EcuPlacement& b = full.Ecus[i];
                stableOk = stableOk && a.LineX == b.LineX && a.Box.Left == b.Box.Left &&
                           a.Box.Top == b.Box.Top && a.OnTop == b.OnTop;
            }
        }
        Check(stableOk, "003 FR-009: ECU n keeps its position whatever the ECU count is");

        Check(ComputeBusDiagramLayout(-1).Ecus.empty() && ComputeBusDiagramLayout(17).Ecus.size() == 16,
            "003: ECU count is clamped to 0..16 (-1 -> 0, 17 -> 16)");
    }

    // ---- feature 004: .canprj project, run network, frame events ----
    std::string g_baselineDir; // optional: CSVs produced before feature 004 (SC-009)

    std::string ReadTextFile(const std::string& path)
    {
        std::ifstream f(path.c_str(), std::ios::binary);
        std::ostringstream buf;
        buf << f.rdbuf();
        return buf.str();
    }

    // Makes a CSV written by this build comparable with a baseline written
    // before feature 008: the log column Overwrite_Count became Update_Count
    // and the summary gained the last two columns TxDiscard_Count and
    // RxDiscard_Count. Nothing changes when the baseline is already 008+.
    std::string NormalizeToBaseline(const std::string& now, const std::string& before)
    {
        std::string out = now;
        if (before.find("Overwrite_Count") != std::string::npos)
        {
            const std::size_t pos = out.find("Update_Count");
            if (pos != std::string::npos)
                out.replace(pos, std::string("Update_Count").size(), "Overwrite_Count");
        }
        if (out.find("TxDiscard_Count") != std::string::npos && before.find("TxDiscard_Count") == std::string::npos)
        {
            std::istringstream in(out);
            std::string line, stripped;
            while (std::getline(in, line))
            {
                const bool cr = !line.empty() && line.back() == '\r';
                if (cr)
                    line.pop_back();
                for (int k = 0; k < 2; ++k)
                {
                    const std::size_t comma = line.rfind(',');
                    if (comma != std::string::npos)
                        line.erase(comma);
                }
                stripped += line + (cr ? "\r\n" : "\n");
            }
            out = stripped;
        }
        return out;
    }

    // Runs a project the way the dialog does (Simulation > Start) and writes
    // the three CSVs to <prefix>.log.csv / .summary.csv / .startup.csv.
    bool RunProjectToCsv(const std::string& projectPath, const std::string& prefix,
        long long durationUs, std::vector<LogRow>* rowsOut = nullptr)
    {
        ConfigLoadResult project = LoadConfig(Widen(projectPath));
        if (!project.Success)
            return false;
        const Network network = BuildRunNetwork(project.Project, project.Config);
        SimulationEngine engine(network, durationUs);
        Logger logger;
        engine.Run(logger);
        const std::wstring p = Widen(prefix);
        const bool ok = logger.WriteCsv(p + L".log.csv") &&
            WriteMetricsCsv(ComputeMetrics(logger.Rows(), network), p + L".summary.csv") &&
            WriteStartupCsv(BuildStartupRecords(engine.GetNetwork()), p + L".startup.csv");
        if (rowsOut)
            *rowsOut = logger.Rows();
        return ok;
    }

    // Drops the lines of a startup CSV that start with "<nodeId>,".
    std::string DropStartupRow(const std::string& csv, int nodeId)
    {
        std::istringstream in(csv);
        std::string line, out;
        const std::string prefix = std::to_string(nodeId) + ",";
        while (std::getline(in, line))
        {
            if (line.compare(0, prefix.size(), prefix) == 0)
                continue;
            out += line + "\n";
        }
        return out;
    }

    ConfigLoadResult LoadText(const std::string& name, const std::string& text)
    {
        const std::string path = Join(g_tempDir, name);
        WriteTextFile(path, text);
        return LoadConfig(Widen(path));
    }

    void CheckFeature004()
    {
        std::cout << "\n== feature 004: project file / run network / frame events ==\n";

        // Defaults and validation of [Project] (FR-005, FR-006, FR-010)
        {
            ConfigLoadResult r = LoadText("p_default.canprj", "[Network]\r\nBitRateBps=250000\r\n");
            Check(r.Success && r.Project.EcuCount == 2 && r.Project.DurationSec == 10 &&
                      !r.Project.FrameDisplay && r.Config.Nodes.empty() && r.Config.BitRateBps == 250000,
                "004 FR-006: [Project] defaults are EcuCount=2, DurationSec=10, FrameDisplay=0; zero nodes allowed");

            const char* bad[] = { "EcuCount=17", "EcuCount=-1", "DurationSec=0", "DurationSec=601",
                                  "FrameDisplay=2", "EcuCount=abc" };
            bool allRejected = true;
            for (const char* kv : bad)
            {
                ConfigLoadResult b = LoadText("p_bad.canprj", std::string("[Project]\r\n") + kv + "\r\n");
                allRejected = allRejected && !b.Success && b.ErrorMessage.find("[Project]") != std::string::npos;
            }
            Check(allRejected, "004 FR-010: out-of-range [Project] values are rejected with a reason");

            ConfigLoadResult edge = LoadText("p_edge.canprj",
                "[Project]\r\nEcuCount=16\r\nDurationSec=600\r\nFrameDisplay=1\r\n");
            Check(edge.Success && edge.Project.EcuCount == 16 && edge.Project.DurationSec == 600 &&
                      edge.Project.FrameDisplay,
                "004 FR-005: boundary values EcuCount=16, DurationSec=600, FrameDisplay=1 are accepted");
        }

        // Save -> reload round trip (SC-003)
        {
            ConfigLoadResult src = LoadConfig(Widen(Join(g_sampleDir, "sample_collision.canprj")));
            ProjectSettings s;
            s.EcuCount = 5;
            s.DurationSec = 20;
            s.FrameDisplay = true;
            src.Config.BitRateBps = 250000;
            const std::string path = Join(g_tempDir, "roundtrip.canprj");
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
            Check(same, "004 SC-003: save then reload restores the 4 settings and every node/frame definition");
        }

        // BuildRunNetwork (FR-011)
        {
            ConfigLoadResult src = LoadConfig(Widen(Join(g_sampleDir, "sample_collision.canprj")));
            ProjectSettings s = src.Project;

            s.EcuCount = 5;
            Network up = BuildRunNetwork(s, src.Config);
            bool upOk = up.Nodes.size() == 5 && up.BitRateBps == src.Config.BitRateBps;
            for (int n = 1; upOk && n <= 5; ++n)
                upOk = up.Nodes[n - 1].NodeId == n;
            upOk = upOk && up.Nodes[3].Frames.empty() && up.Nodes[3].Name == "ECU4" && up.Nodes[2].Frames.size() == 1;
            Check(upOk, "004 FR-011: ECUs without a definition are added (no frames), numbered 1..EcuCount");

            s.EcuCount = 2;
            Network down = BuildRunNetwork(s, src.Config);
            Check(down.Nodes.size() == 2 && FindNode(down, 3) == nullptr,
                "004 FR-011: definitions above EcuCount (ECU 3) are not simulated");

            s.EcuCount = 0;
            Check(BuildRunNetwork(s, src.Config).Nodes.empty(), "004 FR-011: EcuCount=0 gives an empty network");
        }

        // Runs with no ECU / no frames produce header-only CSVs (US3 AS5)
        {
            ConfigLoadResult empty = LoadText("p_zero.canprj", "[Project]\r\nEcuCount=0\r\n");
            ConfigLoadResult noFrames = LoadText("p_noframes.canprj", "[Project]\r\nEcuCount=2\r\n");
            bool ok = empty.Success && noFrames.Success;
            std::vector<LogRow> rows0, rows2;
            ok = ok && RunProjectToCsv(Join(g_tempDir, "p_zero.canprj"), Join(g_tempDir, "p_zero"), 1000000LL, &rows0);
            ok = ok && RunProjectToCsv(Join(g_tempDir, "p_noframes.canprj"), Join(g_tempDir, "p_noframes"), 1000000LL, &rows2);
            ok = ok && rows0.empty() && rows2.empty() &&
                 !ReadTextFile(Join(g_tempDir, "p_zero.log.csv")).empty() &&
                 !ReadTextFile(Join(g_tempDir, "p_noframes.summary.csv")).empty();
            Check(ok, "004 US3 AS5: ECU count 0 and ECUs without frames run and write header-only CSVs");
        }

        // BuildFrameEvents (data-model.md)
        {
            std::vector<LogRow> rows;
            ConfigLoadResult a = LoadConfig(Widen(Join(g_sampleDir, "sample_appendix_a.canprj")));
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
                "004: BuildFrameEvents merges rows per frame, keeps every receiver and sorts by Tx_Start");

            std::vector<LogRow> single(1);
            single[0].TxNodeId = 1;
            single[0].RxNodeId = -1;
            single[0].TxStartUs = 5;
            const std::vector<FrameEvent> one = BuildFrameEvents(single);
            Check(one.size() == 1 && one[0].RxNodeIds.empty(),
                "004: a frame without receivers becomes an event with no receiver");

            // Stop at the start of event k (FR-018 / SC-008): re-running up to
            // t = Tx_Start keeps exactly the frames completed by t.
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
            std::cout << "  stop at t=" << t << " us: rows=" << stopLogger.Rows().size()
                      << " expected=" << expected << "\n";
            Check(completedOnly && stopLogger.Rows().size() == expected,
                "004 SC-008: stopping at t keeps exactly the frames whose transmission completed by t");
        }

        // SC-009: the samples give the same results as a baseline. Feature 006
        // changed the results (0xFF data, requests counted from the Normal
        // entry), so the baseline must be produced by feature 006 or later.
        if (!g_baselineDir.empty())
        {
            const char* samples[] = { "sample_appendix_a", "sample_collision", "sample_startup",
                                      "sample_startup_latejoin" };
            for (const char* name : samples)
            {
                const std::string prefix = Join(g_tempDir, std::string("sc009_") + name);
                const bool ran = RunProjectToCsv(Join(g_sampleDir, std::string(name) + ".canprj"), prefix,
                    10 * 1000000LL);
                bool same = ran;
                const char* kinds[] = { ".log.csv", ".summary.csv", ".startup.csv" };
                for (const char* kind : kinds)
                {
                    std::string before = ReadTextFile(Join(g_baselineDir, std::string(name) + kind));
                    std::string now = NormalizeToBaseline(ReadTextFile(prefix + kind), before);
                    // sample_collision gained the idle ECU 2 (see the comment in the sample).
                    if (std::string(name) == "sample_collision" && std::string(kind) == ".startup.csv")
                    {
                        now = DropStartupRow(now, 2);
                        before = DropStartupRow(before, 2);
                    }
                    same = same && !before.empty() && now == before;
                }
                Check(same, std::string("004 SC-009: ") + name + ".canprj reproduces the baseline CSVs (006 or later)");
            }
        }
        else
        {
            std::cout << "  (SC-009 skipped: no baseline directory given)\n";
        }
    }

    // ---- feature 005: speed setting and replay clock ----
    void CheckFeature005()
    {
        std::cout << "\n== feature 005: speed / replay clock ==\n";

        // Speed in [Project] (FR-001, FR-002)
        {
            ConfigLoadResult def = LoadText("s_default.canprj", "[Project]\r\nEcuCount=2\r\n");
            Check(def.Success && def.Project.Speed == 0, "005 FR-002: Speed defaults to 0 when absent");

            struct Case { const char* Text; int Expected; };
            const Case ok[] = { { "+3", 3 }, { "-3", -3 }, { "10", 10 }, { "-13", -13 }, { "0", 0 } };
            bool accepted = true;
            for (const Case& c : ok)
            {
                ConfigLoadResult r = LoadText("s_ok.canprj", std::string("[Project]\r\nSpeed=") + c.Text + "\r\n");
                accepted = accepted && r.Success && r.Project.Speed == c.Expected;
            }
            Check(accepted, "005 FR-002: Speed +3 / -3 / 10 / -13 / 0 are accepted");

            const char* bad[] = { "11", "-14", "1.5", "abc", "", "+", "--1" };
            bool rejected = true;
            for (const char* text : bad)
            {
                ConfigLoadResult r = LoadText("s_bad.canprj", std::string("[Project]\r\nSpeed=") + text + "\r\n");
                rejected = rejected && !r.Success && r.ErrorMessage.find("Speed") != std::string::npos;
            }
            Check(rejected, "005 FR-002: Speed 11 / -14 / 1.5 / abc / empty are rejected with a reason");

            ProjectSettings s;
            s.Speed = -13;
            const std::string path = Join(g_tempDir, "s_roundtrip.canprj");
            ConfigLoadResult back;
            const bool saved = SaveProject(Widen(path), s, Network());
            back = LoadConfig(Widen(path));
            Check(saved && back.Success && back.Project.Speed == -13, "005 FR-002: Speed survives save then reload");
        }

        // SimTimeAtUs (FR-003)
        {
            const long long e = 1000000; // 1 s of real time
            Check(SimTimeAtUs(e, 0) == e && SimTimeAtUs(e, 1) == 2 * e && SimTimeAtUs(e, -1) == e / 2 &&
                      SimTimeAtUs(e, -10) == e / 1024 && SimTimeAtUs(e, -13) == e / 8192 &&
                      SimTimeAtUs(e, 9) == 512 * e && SimTimeAtUs(0, 5) == 0,
                "005 FR-003: simulated time = real time x 2^speed (0, +1, -1, -10, -13, +9)");
            Check(SimTimeAtUs(LLONG_MAX / 2, 9) == LLONG_MAX, "005: SimTimeAtUs saturates instead of overflowing");
        }

        // ActiveFrameCursor (FR-009)
        {
            std::vector<FrameEvent> ev(3);
            ev[0].TxStartUs = 100; ev[0].TxEndUs = 200; ev[0].TxNodeId = 1; ev[0].RxNodeIds = { 2, 3 };
            ev[1].TxStartUs = 300; ev[1].TxEndUs = 400; ev[1].TxNodeId = 2; ev[1].RxNodeIds = { 1, 3 };
            ev[2].TxStartUs = 1000; ev[2].TxEndUs = 1100; ev[2].TxNodeId = 3; ev[2].RxNodeIds = { 1, 2 };

            ActiveFrameCursor cursor(ev);
            ActiveFrames a = cursor.Collect(0, 500);
            Check(a.BusBusy && a.Senders == std::vector<int>({ 1, 2 }) && a.Receivers == std::vector<int>({ 3 }),
                "005 FR-009: two frames in one refresh -> both senders; an ECU that also sent is shown as sender");
            ActiveFrames idle = cursor.Collect(500, 900);
            Check(!idle.BusBusy && idle.Senders.empty() && idle.Receivers.empty(),
                "005 FR-009: an idle interval highlights nothing");
            ActiveFrames third = cursor.Collect(900, 1050);
            Check(third.Senders == std::vector<int>({ 3 }) && third.Receivers == std::vector<int>({ 1, 2 }),
                "005 FR-009: a frame still on the bus at the end of the interval is highlighted");
            ActiveFrames still = cursor.Collect(1050, 1060);
            Check(still.Senders == std::vector<int>({ 3 }), "005 SC-004: a long frame stays highlighted across refreshes");
        }

        // Every frame is highlighted at least once, whatever the refresh step is
        {
            ConfigLoadResult a = LoadConfig(Widen(Join(g_sampleDir, "sample_appendix_a.canprj")));
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
                    // mark every event overlapping this interval whose sender is shown
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
            Check(allSeen, "005 FR-009: at 16 ms and 1 ms refresh steps every frame is highlighted at least once");
        }
    }

    // ---- feature 006: data frame editor ----
    FrameInput MakeInput(const char* id, const char* dlc, const char* periodMs, const char* offsetMs,
        const char* name = "f")
    {
        FrameInput in;
        in.Name = name;
        in.NameChars = static_cast<int>(std::string(name).size());
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

    void CheckFeature006()
    {
        std::cout << "\n== feature 006: data frame editor ==\n";

        ConfigLoadResult a = LoadConfig(Widen(Join(g_sampleDir, "sample_appendix_a.canprj")));
        Check(a.Success, "sample_appendix_a.canprj loads");
        if (!a.Success)
            return;

        // Reading: Data is always Dlc x 0xFF, names are read (FR-014, FR-012)
        {
            bool allFF = true;
            for (const Node& n : a.Config.Nodes)
                for (const FrameDefinition& f : n.Frames)
                    allFF = allFF && f.Data == MakeFrameData(f.Dlc);
            const Node* ecu1 = FindNode(a.Config, 1);
            Check(allFF, "006 FR-014: every frame's data is Dlc bytes of 0xFF");
            Check(ecu1 && ecu1->Frames.size() == 2 && !ecu1->Frames[0].Name.empty(),
                "006 FR-012: frame names are read from the project");

            ConfigLoadResult old = LoadText("f_olddata.canprj",
                "[Node1]\r\nNodeId=1\r\n[Node1.Frame1]\r\nCanId=0x10\r\nDlc=2\r\nData=00,01\r\nPeriodUs=1000\r\n");
            Check(old.Success && old.Config.Nodes[0].Frames[0].Data == MakeFrameData(2),
                "006 FR-014: an old Data=00,01 line is ignored and the data becomes 0xFF 0xFF");
        }

        // Reading: rejections (FR-013, offset < period, name length)
        {
            std::ostringstream big;
            big << "[Node1]\r\nNodeId=1\r\n";
            for (int i = 1; i <= 101; ++i)
                big << "[Node1.Frame" << i << "]\r\nCanId=0x" << std::hex << (0x100 + i) << std::dec
                    << "\r\nDlc=0\r\nPeriodUs=1000\r\n";
            ConfigLoadResult r101 = LoadText("f_101.canprj", big.str());
            Check(!r101.Success, "006 FR-013: a node with 101 frames is rejected");

            ConfigLoadResult off = LoadText("f_offset.canprj",
                "[Node1]\r\nNodeId=1\r\n[Node1.Frame1]\r\nCanId=0x10\r\nDlc=0\r\nPeriodUs=1000\r\nInitialOffsetUs=1000\r\n");
            Check(!off.Success, "006: InitialOffsetUs equal to PeriodUs is rejected");

            ConfigLoadResult longName = LoadText("f_name.canprj",
                "[Node1]\r\nNodeId=1\r\n[Node1.Frame1]\r\nName=abcdefghijabcdefghijabcdefghijabc\r\nCanId=0x10\r\nDlc=0\r\nPeriodUs=1000\r\n");
            Check(!longName.Success, "006: a frame name of 33 characters is rejected");
        }

        // ValidateFrame (FR-006)
        {
            FrameDefinition out;
            std::vector<FrameIssue> ok = ValidateFrame(a.Config, 1, -1, MakeInput("0x200", "4", "20", ""), out);
            Check(ok.empty() && out.CanId == 0x200 && out.Dlc == 4 && out.PeriodUs == 20000 &&
                      out.InitialOffsetUs == 0 && out.Data == MakeFrameData(4),
                "006 FR-006: a valid entry passes (empty offset = 0, ms -> us, data 0xFF)");
            Check(ValidateFrame(a.Config, 1, -1, MakeInput("200", "4", "20", "5"), out).empty() && out.InitialOffsetUs == 5000,
                "006 FR-006: the ID may be typed without 0x");

            Check(HasIssue(ValidateFrame(a.Config, 1, -1, MakeInput("0x800", "4", "20", ""), out), FrameField::Id, FrameProblem::OutOfRange),
                "006 FR-006: ID 0x800 is out of range");
            Check(HasIssue(ValidateFrame(a.Config, 1, -1, MakeInput("xyz", "4", "20", ""), out), FrameField::Id, FrameProblem::NotANumber),
                "006 FR-006: ID 'xyz' is not a number");
            Check(HasIssue(ValidateFrame(a.Config, 1, -1, MakeInput("0x200", "9", "20", ""), out), FrameField::Dlc, FrameProblem::OutOfRange),
                "006 FR-006: DLC 9 is out of range");
            Check(HasIssue(ValidateFrame(a.Config, 1, -1, MakeInput("0x200", "4", "0", ""), out), FrameField::Period, FrameProblem::OutOfRange),
                "006 FR-006: period 0 ms is out of range");
            Check(HasIssue(ValidateFrame(a.Config, 1, -1, MakeInput("0x200", "4", "20", "20"), out), FrameField::Offset, FrameProblem::OffsetNotBelowPeriod),
                "006 FR-006: offset equal to the period is rejected");

            FrameInput longName = MakeInput("0x200", "4", "20", "");
            longName.NameChars = 33;
            Check(HasIssue(ValidateFrame(a.Config, 1, -1, longName, out), FrameField::Name, FrameProblem::TooLong),
                "006 FR-006: a 33 character name is rejected");

            // 0x080 belongs to ECU3
            std::vector<FrameIssue> dup = ValidateFrame(a.Config, 1, -1, MakeInput("0x080", "4", "20", ""), out);
            Check(HasIssue(dup, FrameField::Id, FrameProblem::Duplicate) && dup[0].DuplicateNodeId == 3,
                "006 FR-006: an ID used by another node is a duplicate (reports the node)");
            // editing ECU1's first frame keeps its own ID
            Check(ValidateFrame(a.Config, 1, 0, MakeInput("0x100", "8", "10", ""), out).empty(),
                "006 FR-006: editing a frame does not collide with itself");

            // an ECU beyond the ECU count still owns its IDs
            Network defs = a.Config;
            AddFrame(defs, 16, out);  // 0x100 is ECU1's; use a fresh frame instead
            defs.Nodes.back().Frames.back().CanId = 0x7F0;
            Check(HasIssue(ValidateFrame(defs, 1, -1, MakeInput("0x7F0", "0", "10", ""), out), FrameField::Id, FrameProblem::Duplicate),
                "006: IDs of ECUs outside the ECU count are also checked for duplicates");
        }

        // AddFrame / ReplaceFrame / DeleteFrame / EnsureNode (FR-007..FR-009)
        {
            Network defs = a.Config;
            FrameDefinition f;
            ValidateFrame(defs, 7, -1, MakeInput("0x700", "1", "50", "", "new"), f);
            AddFrame(defs, 7, f);
            const Node* n7 = FindNode(defs, 7);
            bool ordered = true;
            for (std::size_t i = 1; i < defs.Nodes.size(); ++i)
                ordered = ordered && defs.Nodes[i - 1].NodeId < defs.Nodes[i].NodeId;
            Check(n7 && n7->Name == "ECU7" && n7->Frames.size() == 1 && ordered,
                "006: adding to an ECU without a definition creates it in NodeId order");

            FrameDefinition g = f;
            g.CanId = 0x701;
            AddFrame(defs, 7, g);
            FrameDefinition h = f;
            h.PeriodUs = 70000;
            ReplaceFrame(defs, 7, 0, h);
            DeleteFrame(defs, 7, 0);
            n7 = FindNode(defs, 7);
            Check(n7 && n7->Frames.size() == 1 && n7->Frames[0].CanId == 0x701,
                "006 FR-009: after deleting No.1 the remaining frame becomes No.1");

            Network full;
            for (int i = 0; i < kMaxFramesPerNode; ++i)
            {
                FrameDefinition x;
                x.CanId = static_cast<unsigned>(i);
                x.PeriodUs = 1000;
                AddFrame(full, 1, x);
            }
            Check(HasIssue(ValidateFrame(full, 1, -1, MakeInput("0x700", "0", "10", ""), f), FrameField::Name, FrameProblem::ListFull),
                "006 FR-013: the 101st frame cannot be added");
        }

        // Save -> reload keeps names/order and writes no Data= (SC-003)
        {
            Network defs = a.Config;
            FrameDefinition f;
            ValidateFrame(defs, 2, -1, MakeInput("0x210", "3", "25", "7", "added"), f);
            AddFrame(defs, 2, f);
            ProjectSettings s = a.Project;
            const std::string path = Join(g_tempDir, "f_roundtrip.canprj");
            SaveProject(Widen(path), s, defs);
            const std::string text = ReadTextFile(path);
            ConfigLoadResult back = LoadConfig(Widen(path));
            bool same = back.Success && back.Config.Nodes.size() == defs.Nodes.size();
            for (std::size_t n = 0; same && n < defs.Nodes.size(); ++n)
            {
                const Node& x = defs.Nodes[n];
                const Node& y = back.Config.Nodes[n];
                same = x.Frames.size() == y.Frames.size();
                for (std::size_t i = 0; same && i < x.Frames.size(); ++i)
                    same = x.Frames[i].Name == y.Frames[i].Name && x.Frames[i].CanId == y.Frames[i].CanId &&
                           x.Frames[i].Dlc == y.Frames[i].Dlc && x.Frames[i].PeriodUs == y.Frames[i].PeriodUs &&
                           x.Frames[i].InitialOffsetUs == y.Frames[i].InitialOffsetUs;
            }
            Check(same && text.find("Data=") == std::string::npos,
                "006 SC-003: save then reload keeps every list (names, order) and writes no Data= line");
        }

        // First request = Normal entry + offset (FR-015, SC-005, SC-007)
        {
            ConfigLoadResult late = LoadText("f_late.canprj",
                "[Project]\r\nEcuCount=1\r\n[Node1]\r\nNodeId=1\r\nPowerOnTimeUs=50000\r\n"
                "[Node1.Frame1]\r\nCanId=0x100\r\nDlc=0\r\nPeriodUs=20000\r\n"
                "[Node1.Frame2]\r\nCanId=0x101\r\nDlc=0\r\nPeriodUs=20000\r\nInitialOffsetUs=5000\r\n");
            SimulationEngine engine(BuildRunNetwork(late.Project, late.Config), 200000LL);
            Logger logger;
            engine.Run(logger);
            const Node* n = FindNode(engine.GetNetwork(), 1);
            std::vector<long long> req100, req101;
            for (const LogRow& r : logger.Rows())
                (r.CanId == 0x100 ? req100 : req101).push_back(r.TxRequestUs);
            const long long normal = n ? n->NormalEntryUs : -1;
            std::cout << "  Normal_Entry=" << normal << " first 0x100=" << (req100.empty() ? -1 : req100[0])
                      << " first 0x101=" << (req101.empty() ? -1 : req101[0]) << "\n";
            Check(late.Success && normal == 50022 && !req100.empty() && req100[0] == normal,
                "006 SC-007: offset 0 -> the first request is at the Normal entry (50 ms + 22 us)");
            Check(!req101.empty() && req101[0] == normal + 5000 && req101.size() > 1 && req101[1] == normal + 25000,
                "006 FR-015: offset 5 ms -> Normal + 5 ms, then every period (+20 ms)");
            // (200 ms - 50.022 ms) / 20 ms -> 8 requests each
            Check(req100.size() == 8 && req101.size() == 8, "006 SC-005: request count follows the period after the Normal entry");
        }
    }
    // ---- feature 007: intermission before bus idle (domain spec FR-021/FR-022) ----
    void CheckFeature007()
    {
        std::cout << "\n== feature 007: intermission before bus idle ==\n";
        const std::string text = ReadTextFile(Join(g_sampleDir, "sample_collision.canprj"));
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
                Check(false, "007: sample_collision loads at " + std::to_string(rate) + " bps");
                continue;
            }
            const Network net = BuildRunNetwork(cfg.Project, cfg.Config);
            SimulationEngine engine(net, 1000000LL);
            Logger logger;
            engine.Run(logger);

            // one entry per transmitted frame, in Tx_Start order
            std::map<long long, LogRow> frames;
            for (const LogRow& r : logger.Rows())
                frames.insert(std::make_pair(r.TxStartUs, r));

            const long long itmUs = 3 * 1000000LL / rate; // 3 bit times
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
            std::cout << "  " << rate << " bps: frames=" << frames.size() << " back-to-back=" << backToBack
                      << " ITM=" << itmUs << " us\n";
            Check(gapOk && backToBack > 0,
                "007 SC-003: at " + std::to_string(rate) + " bps the next SOF starts exactly 3 bit times after EOF when frames are waiting");
            Check(durationOk,
                "007 SC-004: at " + std::to_string(rate) + " bps Tx_End - Tx_Start is the frame alone (no intermission)");
        }
    }

    // ---- feature 008: transmit / receive frame buffers (domain spec 3.7a) ----
    void CheckFeature008()
    {
        std::cout << "\n== feature 008: frame buffers ==\n";
        const std::string path = Join(g_sampleDir, "sample_buffer_full.canprj");
        ConfigLoadResult cfg = LoadConfig(Widen(path));
        Check(cfg.Success, "sample_buffer_full.canprj loads");
        if (!cfg.Success)
            return;

        const Network net = BuildRunNetwork(cfg.Project, cfg.Config);
        SimulationEngine engine(net, 10 * 1000000LL);
        Logger logger;
        engine.Run(logger);
        const std::vector<LogRow>& rows = logger.Rows();
        const Node* ecu1 = FindNode(engine.GetNetwork(), 1);
        const Node* ecu2 = FindNode(engine.GetNetwork(), 2);

        // --- transmit side (US1) ---
        std::vector<const LogRow*> txDropped, rxDropped;
        for (const LogRow& r : rows)
        {
            if (r.Status != kStatusOverwrite)
                continue;
            (r.TxStartUs < 0 ? txDropped : rxDropped).push_back(&r);
        }
        bool idsOk = true, blanksOk = true, timesOk = true;
        const long long normal1 = ecu1 ? ecu1->NormalEntryUs : -1;
        for (const LogRow* r : txDropped)
        {
            idsOk = idsOk && r->TxNodeId == 1 && r->CanId >= 0x110 && r->CanId <= 0x113;
            blanksOk = blanksOk && r->RxNodeId == -1 && r->TxEndUs < 0 && r->RxStartUs < 0 && r->RxEndUs < 0 &&
                       r->RxProcessUs < 0 && r->TimestampUs == r->TxRequestUs;
            timesOk = timesOk && (r->TxRequestUs - normal1) % 100000 == 0;
        }
        std::cout << "  transmit-side OVERWRITE rows=" << txDropped.size()
                  << " ECU1 TxDiscardCount=" << (ecu1 ? ecu1->TxDiscardCount : -1) << "\n";
        Check(txDropped.size() == 400 && idsOk,
            "008 SC-001: every 100 ms the 4 highest IDs of ECU1 (0x110..0x113) are dropped -> 400 rows");
        Check(blanksOk && timesOk, "008 FR-004: a dropped request has blank Rx_Node / Tx / Rx times at its request time");
        Check(ecu1 && ecu1->TxDiscardCount == 400, "008 FR-004: ECU1 counts 400 transmit-side discards");

        // at most 16 frames of ECU1 hold a transmit buffer (request .. EOF);
        // at every dropped request all 16 are in use
        std::map<std::pair<long long, unsigned>, std::pair<long long, long long>> tx1; // (start, id) -> (request, end)
        for (const LogRow& r : rows)
            if (r.TxNodeId == 1 && r.TxStartUs >= 0)
                tx1[std::make_pair(r.TxStartUs, r.CanId)] = std::make_pair(r.TxRequestUs, r.TxEndUs);
        bool fullAtDrop = true;
        for (const LogRow* r : txDropped)
        {
            int inUse = 0;
            for (const auto& kv : tx1)
                if (kv.second.first <= r->TxRequestUs && r->TxRequestUs < kv.second.second)
                    ++inUse;
            fullAtDrop = fullAtDrop && inUse == kTxBufferCount;
        }
        Check(fullAtDrop, "008 SC-002: each dropped transmit request found exactly 16 frames holding ECU1's buffers");

        // --- receive side (US2) ---
        bool onlyEcu2 = true, rxBlank = true;
        for (const LogRow* r : rxDropped)
        {
            onlyEcu2 = onlyEcu2 && r->RxNodeId == 2;
            rxBlank = rxBlank && r->RxProcessUs < 0 && r->RxEndUs >= 0 && r->TimestampUs == r->RxEndUs;
        }
        std::cout << "  receive-side OVERWRITE rows=" << rxDropped.size()
                  << " ECU2 RxDiscardCount=" << (ecu2 ? ecu2->RxDiscardCount : -1) << "\n";
        Check(!rxDropped.empty() && onlyEcu2 && rxBlank,
            "008 FR-006: ECU2 (2 ms receive delay) drops frames; the rows have Status OVERWRITE and no Rx_Process");
        Check(ecu2 && ecu2->RxDiscardCount == static_cast<int>(rxDropped.size()),
            "008 SC-003: ECU2's receive-side discard count equals its OVERWRITE rows");

        // receive FIFO occupancy: frames accepted by a node occupy [Rx_End, Rx_Process)
        bool rxAtMost8 = true, fullAtRxDrop = true;
        for (int nodeId = 1; nodeId <= 3; ++nodeId)
        {
            std::vector<std::pair<long long, long long>> held;
            for (const LogRow& r : rows)
                if (r.RxNodeId == nodeId && r.RxProcessUs >= 0)
                    held.push_back(std::make_pair(r.RxEndUs, r.RxProcessUs));
            for (const LogRow& r : rows)
            {
                if (r.RxNodeId != nodeId || r.RxEndUs < 0)
                    continue;
                int inUse = 0;
                for (const auto& h : held)
                    if (h.first < r.RxEndUs && r.RxEndUs < h.second)
                        ++inUse; // frames still waiting when this one arrives
                if (r.RxProcessUs >= 0)
                    rxAtMost8 = rxAtMost8 && inUse < kRxBufferCount;
                else
                    fullAtRxDrop = fullAtRxDrop && inUse == kRxBufferCount;
            }
        }
        Check(rxAtMost8 && fullAtRxDrop, "008 SC-002: a receive FIFO never holds more than 8; a drop happens only when 8 are held");

        // reproducibility (SC-006)
        {
            SimulationEngine again(net, 10 * 1000000LL);
            Logger logger2;
            again.Run(logger2);
            Check(RowsEqual(rows, logger2.Rows()), "008 SC-006: two runs give identical logs including OVERWRITE rows");
        }

        // summary: per-node discard counts, OVERWRITE excluded from statistics (FR-009, FR-010)
        {
            const std::vector<GroupMetrics> metrics = ComputeMetrics(rows, net);
            bool found1 = false, found2 = false, idCountOk = true;
            for (const GroupMetrics& g : metrics)
            {
                if (g.GroupType == "Node" && g.GroupKey == "1")
                    found1 = g.HasDiscards && g.TxDiscardCount == 400 && g.RxDiscardCount == 0;
                if (g.GroupType == "Node" && g.GroupKey == "2")
                    found2 = g.HasDiscards && g.RxDiscardCount == static_cast<int>(rxDropped.size());
                if (g.GroupType == "ID" && g.GroupKey == "0x113")
                    idCountOk = g.TotalLatency.Count == 0 && g.QueueingDelay.Count == 0;
            }
            Check(found1 && found2, "008 FR-009: the summary shows TxDiscard_Count 400 for ECU1 and ECU2's RxDiscard_Count");
            Check(idCountOk, "008 FR-010: OVERWRITE rows are not counted in the statistics (0x113 is never sent)");
        }

        // receive delay 0 -> no receive-side drop (FR-095 / SC-004)
        {
            const char* samples[] = { "sample_appendix_a", "sample_collision", "sample_startup",
                                      "sample_startup_latejoin", "sample_16ecu" };
            bool none = true;
            for (const char* name : samples)
            {
                std::vector<LogRow> r;
                RunProjectToCsv(Join(g_sampleDir, std::string(name) + ".canprj"), Join(g_tempDir, std::string("b8_") + name),
                    10 * 1000000LL, &r);
                for (const LogRow& row : r)
                    none = none && row.Status != kStatusOverwrite;
            }
            Check(none, "008 SC-004: the existing samples (receive delay 0, <= 16 waiting) have no OVERWRITE row");
        }

        // SC-005: sample_16ecu against the baseline (the other 4 samples are compared by 004 SC-009)
        if (!g_baselineDir.empty())
        {
            const std::string prefix = Join(g_tempDir, "b8_sample_16ecu");
            bool same = true;
            const char* kinds[] = { ".log.csv", ".summary.csv", ".startup.csv" };
            for (const char* kind : kinds)
            {
                const std::string before = ReadTextFile(Join(g_baselineDir, std::string("sample_16ecu") + kind));
                const std::string now = NormalizeToBaseline(ReadTextFile(prefix + kind), before);
                same = same && !before.empty() && now == before;
            }
            Check(same, "008 SC-005: sample_16ecu matches the baseline apart from the renamed / added columns");
        }
    }

    // ---- feature 009: rows of the log view ----
    void CheckFeature009()
    {
        std::cout << "\n== feature 009: log view rows ==\n";
        ConfigLoadResult cfg = LoadConfig(Widen(Join(g_sampleDir, "sample_startup.canprj")));
        Check(cfg.Success, "sample_startup.canprj loads");
        if (!cfg.Success)
            return;
        const Network net = BuildRunNetwork(cfg.Project, cfg.Config);
        const long long duration = 10 * 1000000LL;
        SimulationEngine engine(net, duration);
        Logger logger;
        engine.Run(logger);
        const std::vector<LogLine> lines = BuildLogLines(logger.Rows(), engine.GetNetwork(), duration);

        // (a) 18 state rows at the startup record times
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
        std::cout << "  state rows=" << states << " frame rows=" << frames << " log rows=" << logger.Rows().size() << "\n";
        Check(states == 18 && timesOk, "009 SC-002: 6 ECUs x 3 state rows at the startup record times");

        // (b) order: time, states before frames, node id
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
        Check(ordered, "009 FR-005: rows are in time order, states first at the same time, then by node id");

        // (c) one frame row per log row
        Check(frames == static_cast<int>(logger.Rows().size()), "009 FR-004: one frame row per log row (frame x receiving node)");

        // (d) transitions after the end of the run are left out
        {
            const std::vector<LogLine> shortLines = BuildLogLines(std::vector<LogRow>(), engine.GetNetwork(), 25);
            bool within = true;
            for (const LogLine& l : shortLines)
                within = within && l.TimeUs <= 25;
            Check(within && !shortLines.empty() && shortLines.size() < 18,
                "009: state rows later than the run duration are left out");
        }

        // (e) a node powered on after the run has no row
        {
            ConfigLoadResult late = LoadText("l_late.canprj",
                "[Project]\r\nEcuCount=2\r\n[Node2]\r\nNodeId=2\r\nPowerOnTimeUs=5000000\r\n");
            SimulationEngine e2(BuildRunNetwork(late.Project, late.Config), 1000000LL);
            Logger l2;
            e2.Run(l2);
            bool none2 = true;
            for (const LogLine& l : BuildLogLines(l2.Rows(), e2.GetNetwork(), 1000000LL))
                none2 = none2 && l.NodeId != 2;
            Check(late.Success && none2, "009: a node powered on after the run has no state row");
        }

        // (f) OVERWRITE rows are frame rows too
        {
            ConfigLoadResult bf = LoadConfig(Widen(Join(g_sampleDir, "sample_buffer_full.canprj")));
            const Network bnet = BuildRunNetwork(bf.Project, bf.Config);
            SimulationEngine e3(bnet, 1000000LL);
            Logger l3;
            e3.Run(l3);
            const std::vector<LogLine> bl = BuildLogLines(l3.Rows(), e3.GetNetwork(), 1000000LL);
            int overwrite = 0;
            for (const LogLine& l : bl)
                if (l.Kind == LogLineKind::Frame && l3.Rows()[static_cast<std::size_t>(l.Index)].Status == kStatusOverwrite)
                    ++overwrite;
            Check(overwrite > 0, "009: OVERWRITE rows appear in the log view");
        }
    }

    // ---- feature 010: ECU hit test on the bus diagram ----
    void CheckFeature010()
    {
        std::cout << "\n== feature 010: ECU hit test ==\n";
        const int dpis[] = { 96, 120, 144 };

        // (a) ScaleToDpi = (px * dpi + 48) / 96
        {
            bool same = true;
            for (int dpi : dpis)
                for (int px = 0; px <= kDiagramWidth; ++px)
                    same = same && ScaleToDpi(px, dpi) == (px * dpi + 48) / 96;
            Check(same, "010 research decision 2: ScaleToDpi = (px * dpi + 48) / 96");
        }

        // (b) inside corners and center hit the ECU, 1 px outside does not
        {
            bool inside = true;
            bool outside = true;
            for (int n = 1; n <= kMaxEcuCount; ++n)
            {
                const BusDiagramLayout layout = ComputeBusDiagramLayout(n);
                for (int dpi : dpis)
                {
                    for (const EcuPlacement& ecu : layout.Ecus)
                    {
                        const int l = ScaleToDpi(ecu.Box.Left, dpi);
                        const int t = ScaleToDpi(ecu.Box.Top, dpi);
                        const int r = ScaleToDpi(ecu.Box.Right, dpi);
                        const int b = ScaleToDpi(ecu.Box.Bottom, dpi);
                        const int k = ecu.Number;
                        inside = inside && FindEcuAt(layout, l, t, dpi) == k && FindEcuAt(layout, r - 1, t, dpi) == k &&
                            FindEcuAt(layout, l, b - 1, dpi) == k && FindEcuAt(layout, r - 1, b - 1, dpi) == k &&
                            FindEcuAt(layout, (l + r) / 2, (t + b) / 2, dpi) == k;
                        outside = outside && FindEcuAt(layout, l - 1, (t + b) / 2, dpi) != k &&
                            FindEcuAt(layout, (l + r) / 2, t - 1, dpi) != k &&
                            FindEcuAt(layout, r, (t + b) / 2, dpi) != k && FindEcuAt(layout, (l + r) / 2, b, dpi) != k;
                    }
                }
            }
            Check(inside, "010 SC-001/SC-005: inner corners and center of every box hit that ECU (ECU 1..16, DPI 96/120/144)");
            Check(outside, "010 SC-004: 1 px outside every box does not hit that ECU");
        }

        // (c) bus line, caption, stub lines and the margin hit nothing
        {
            const BusDiagramLayout layout = ComputeBusDiagramLayout(16);
            bool none = true;
            for (int dpi : dpis)
            {
                none = none && FindEcuAt(layout, ScaleToDpi(570, dpi), ScaleToDpi(kBusY, dpi), dpi) == 0;
                none = none && FindEcuAt(layout, ScaleToDpi(66, dpi), ScaleToDpi(93, dpi), dpi) == 0;
                none = none && FindEcuAt(layout, ScaleToDpi(1135, dpi), ScaleToDpi(210, dpi), dpi) == 0;
                for (const EcuPlacement& ecu : layout.Ecus)
                    none = none && FindEcuAt(layout, ScaleToDpi(ecu.LineX, dpi),
                        ScaleToDpi((ecu.LineY1 + ecu.LineY2) / 2, dpi), dpi) == 0;
            }
            Check(none, "010 FR-005: bus line, caption, stub lines and the margin hit no ECU");
        }

        // (d) no ECU: nothing is hit anywhere in the diagram
        {
            const BusDiagramLayout layout = ComputeBusDiagramLayout(0);
            bool none = true;
            for (int dpi : dpis)
                for (int y = 0; y < ScaleToDpi(kDiagramHeight, dpi); y += 10)
                    for (int x = 0; x < ScaleToDpi(kDiagramWidth, dpi); x += 10)
                        none = none && FindEcuAt(layout, x, y, dpi) == 0;
            Check(none, "010: with 0 ECUs nothing is hit");
        }

        // (e) with N ECUs no number above N is returned
        {
            bool within = true;
            for (int n = 0; n <= kMaxEcuCount; ++n)
            {
                const BusDiagramLayout layout = ComputeBusDiagramLayout(n);
                for (int y = 0; y < kDiagramHeight; y += 2)
                    for (int x = 0; x < kDiagramWidth; x += 2)
                    {
                        const int k = FindEcuAt(layout, x, y, 96);
                        within = within && k >= 0 && k <= n;
                    }
            }
            Check(within, "010: with N ECUs only ECU 1..N can be hit");
        }
    }

    // ---- feature 011: border around the bus diagram ----
    void CheckFeature011()
    {
        std::cout << "\n== feature 011: diagram border ==\n";
        const int dpis[] = { 96, 120, 144 };
        bool inArea = true;
        bool clear = true;
        for (int n = 0; n <= kMaxEcuCount; ++n)
        {
            const BusDiagramLayout layout = ComputeBusDiagramLayout(n);
            for (int dpi : dpis)
            {
                const int w = (std::max)(1, ScaleToDpi(1, dpi));
                const int fl = ScaleToDpi(layout.Frame.Left, dpi);
                const int ft = ScaleToDpi(layout.Frame.Top, dpi);
                const int fr = ScaleToDpi(layout.Frame.Right, dpi);
                const int fb = ScaleToDpi(layout.Frame.Bottom, dpi);
                inArea = inArea && fl >= 0 && ft >= 0 && fr <= ScaleToDpi(kDiagramWidth, dpi) &&
                    fb <= ScaleToDpi(kDiagramHeight, dpi);

                // Everything drawn must be at least 1 px inside the line:
                // [fl + w, fr - w) x [ft + w, fb - w) is the free inside.
                auto inside = [&](int l, int t, int r, int b) {
                    return ScaleToDpi(l, dpi) > fl + w && ScaleToDpi(t, dpi) > ft + w &&
                        ScaleToDpi(r, dpi) < fr - w && ScaleToDpi(b, dpi) < fb - w;
                };
                clear = clear && inside(layout.BusLine.Left, layout.BusLine.Top, layout.BusLine.Right, layout.BusLine.Bottom);
                // "CAN BUS": the text box the dialog draws it in, below its top margin
                clear = clear && inside(kCaptionLeft, 76, kCaptionRight + 40, layout.BusLine.Top - 2);
                for (const EcuPlacement& ecu : layout.Ecus)
                {
                    clear = clear && inside(ecu.Box.Left, ecu.Box.Top, ecu.Box.Right, ecu.Box.Bottom);
                    clear = clear && inside(ecu.LineX, (std::min)(ecu.LineY1, ecu.LineY2), ecu.LineX + 1,
                        (std::max)(ecu.LineY1, ecu.LineY2));
                }
            }
        }
        Check(inArea, "011 FR-008: the border is inside the diagram area (ECU 0..16, DPI 96/120/144)");
        Check(clear, "011 FR-010/FR-011: bus line, caption, ECU boxes and stub lines stay clear of the border");

        // Log view border: outside the grey area and scroll bar by at least
        // 1 px after the line width, and below the diagram.
        bool logClear = true;
        bool sameAsBefore = true;
        for (int dpi : dpis)
        {
            const int w = (std::max)(1, ScaleToDpi(1, dpi));
            const DiagramRect view = ScaleLogViewRect(dpi);
            const DiagramRect frame = ScaleLogFrameRect(dpi);
            logClear = logClear && frame.Left + w < view.Left && frame.Top + w < view.Top &&
                frame.Right - w > view.Right && frame.Bottom - w > view.Bottom &&
                frame.Top > ScaleToDpi(kDiagramHeight, dpi);
            // Same place as the dialog used before (left / top + scaled size)
            sameAsBefore = sameAsBefore && view.Left == ScaleToDpi(150, dpi) && view.Top == ScaleToDpi(260, dpi) &&
                view.Right == view.Left + ScaleToDpi(850, dpi) && view.Bottom == view.Top + ScaleToDpi(260, dpi);
        }
        Check(logClear, "011 FR-009/FR-011: the log view border is clear of the grey area, the scroll bar and the diagram (DPI 96/120/144)");
        Check(sameAsBefore, "011 FR-012: the log view keeps its 009 position (150, 260, 850 x 260 at 96 DPI)");
    }
    void CheckFeature014()
    {
        std::cout << "\n== feature 014: simulation commands and toolbar ==\n";

        // FR-004: Start / Pause / Stop for every phase, with and without the
        // frame editor open.
        struct Row { RunPhase Phase; bool Editing; bool Start; bool Pause; bool Stop; };
        const Row rows[] = {
            { RunPhase::Idle, false, true, false, false },
            { RunPhase::Idle, true, false, false, false },
            { RunPhase::Running, false, false, true, true },
            { RunPhase::Running, true, false, true, true },
            { RunPhase::Paused, false, true, false, true },
            { RunPhase::Paused, true, true, false, true },
        };
        bool table = true;
        for (const Row& r : rows)
        {
            const SimCommandStates s = GetSimCommandStates(r.Phase, r.Editing);
            table = table && s.Start == r.Start && s.Pause == r.Pause && s.Stop == r.Stop;
        }
        Check(table, "014 FR-004: Start / Pause / Stop states match the table for Idle, Running and Paused");

        // Toolbar geometry at DPI 96/120/144
        const int dpis[] = { 96, 120, 144 };
        bool inBand = true;
        bool ordered = true;
        bool partsInside = true;
        bool hit = true;
        for (int dpi : dpis)
        {
            const int band = ScaleToDpi(kToolbarHeight, dpi);
            for (int i = 0; i < kSimButtonCount; ++i)
            {
                const DiagramRect r = SimButtonRect(i, dpi);
                const DiagramRect box = SimButtonBoxRect(i, dpi);
                const DiagramRect label = SimButtonLabelRect(i, dpi);
                inBand = inBand && r.Left >= 0 && r.Top >= 0 && r.Bottom <= band && r.Left < r.Right && r.Top < r.Bottom;
                if (i > 0)
                    ordered = ordered && SimButtonRect(i - 1, dpi).Right < r.Left;
                partsInside = partsInside &&
                    box.Left >= r.Left && box.Right <= r.Right && box.Top >= r.Top && box.Bottom <= label.Top &&
                    label.Left >= r.Left && label.Right <= r.Right && label.Bottom <= r.Bottom;
                hit = hit && FindSimButtonAt(r.Left, r.Top, dpi) == i &&
                    FindSimButtonAt(r.Right - 1, r.Bottom - 1, dpi) == i &&
                    FindSimButtonAt(r.Right, r.Top, dpi) != i && FindSimButtonAt(r.Left, r.Bottom, dpi) == -1 &&
                    FindSimButtonAt(r.Left - 1, r.Top, dpi) != i;
                if (i > 0)
                    hit = hit && FindSimButtonAt(SimButtonRect(i - 1, dpi).Right, r.Top, dpi) == -1;
            }
            hit = hit && FindSimButtonAt(0, 0, dpi) == -1 && FindSimButtonAt(10, band + 5, dpi) == -1;
        }
        Check(inBand, "014 FR-002/FR-009: the three buttons stay inside the toolbar band, above the moved diagram (DPI 96/120/144)");
        Check(ordered, "014 FR-002: Start, Pause and Stop are placed left to right without overlapping");
        Check(partsInside, "014 FR-002: box and name of each button are inside its hit area, the name under the box");
        Check(hit, "014 FR-003: FindSimButtonAt agrees with the button rectangles [Left, Right) x [Top, Bottom)");

        // FR-006: resumed time
        bool same = true;
        const long long elapsed[] = { 0, 1, 999, 1000000, 37000001 };
        const int speeds[] = { -13, 0, 5 };
        for (long long e : elapsed)
            for (int s : speeds)
                same = same && ResumedSimTimeUs(0, e, s) == SimTimeAtUs(e, s);
        Check(same, "014 FR-006: with base 0 the resumed time equals the 005 clock");

        bool split = true;
        const long long pairs[][2] = { { 0, 5 }, { 3000000, 7000000 }, { 1, 1 }, { 123456, 654321 }, { 9999999, 1 } };
        for (const auto& p : pairs)
            split = split && ResumedSimTimeUs(ResumedSimTimeUs(0, p[0], 0), p[1], 0) == SimTimeAtUs(p[0] + p[1], 0);
        Check(split, "014 SC-003: pausing and resuming (speed 0) gives the same time as running without a pause");
        Check(ResumedSimTimeUs(LLONG_MAX - 1, 1000000, 0) == LLONG_MAX && ResumedSimTimeUs(-5, 10, 0) == 10,
            "014: the resumed time saturates and treats a negative base as 0");
    }
    void CheckFeature015()
    {
        std::cout << "\n== feature 015: speed slider ==\n";
        const int dpis[] = { 96, 120, 144 };
        bool even = true, placed = true, labels = true, thumbs = true, nearest = true, hit = true;
        for (int dpi : dpis)
        {
            const int pitch = ScaleToDpi(kSpeedTickPitch, dpi);
            for (int s = kMinSpeed + 1; s <= kMaxSpeed; ++s)
            {
                const int d = SpeedTickX(s, dpi) - SpeedTickX(s - 1, dpi);
                even = even && d >= pitch - 1 && d <= pitch + 1;
                labels = labels && SpeedLabelRect(s - 1, dpi).Right <= SpeedLabelRect(s, dpi).Left;
            }
            const int band = ScaleToDpi(kToolbarHeight, dpi);
            const int hitLeft = SpeedTickX(kMinSpeed, dpi) - ScaleToDpi(kSpeedTickPitch / 2, dpi);
            placed = placed && hitLeft > SimButtonRect(2, dpi).Right &&
                SpeedLabelRect(kMaxSpeed, dpi).Right <= ScaleToDpi(kDiagramWidth, dpi) &&
                SpeedLabelRect(kMaxSpeed, dpi).Bottom <= band && ScaleToDpi(kSpeedHitBottom, dpi) <= band &&
                SpeedThumbRect(kMaxSpeed, dpi).Bottom <= band;
            for (int s = kMinSpeed; s <= kMaxSpeed; ++s)
            {
                const DiagramRect th = SpeedThumbRect(s, dpi);
                const int x = SpeedTickX(s, dpi);
                thumbs = thumbs && th.Left <= x && x < th.Right;
                nearest = nearest && SpeedAtX(x, dpi) == s;
                if (s < kMaxSpeed)
                {
                    const int next = SpeedTickX(s + 1, dpi);
                    nearest = nearest && SpeedAtX(x + (next - x) / 2 - 1, dpi) == s &&
                        SpeedAtX(x + (next - x) / 2 + 2, dpi) == s + 1;
                }
                const DiagramRect lb = SpeedLabelRect(s, dpi);
                hit = hit && IsOnSpeedSlider(x, (th.Top + th.Bottom) / 2, dpi) &&
                    IsOnSpeedSlider(x, (lb.Top + lb.Bottom) / 2, dpi);
            }
            nearest = nearest && SpeedAtX(0, dpi) == kMinSpeed && SpeedAtX(ScaleToDpi(2000, dpi), dpi) == kMaxSpeed;
            const DiagramRect stop = SimButtonRect(2, dpi);
            hit = hit && !IsOnSpeedSlider((stop.Left + stop.Right) / 2, (stop.Top + stop.Bottom) / 2, dpi) &&
                !IsOnSpeedSlider(SpeedTickX(0, dpi), band + 1, dpi) &&
                !IsOnSpeedSlider(ScaleToDpi(kSpeedSliderLeft - kSpeedTickPitch / 2, dpi) - 1, ScaleToDpi(20, dpi), dpi) &&
                IsOnSpeedSlider(ScaleToDpi(kSpeedSliderLeft - kSpeedTickPitch / 2, dpi), ScaleToDpi(20, dpi), dpi);
        }
        Check(even, "015 FR-002: 24 ticks from -13 to +10, evenly spaced (DPI 96/120/144)");
        Check(placed, "015 FR-002/FR-012: the slider is right of the Stop button and inside the toolbar band and the window width");
        Check(labels, "015 FR-004: the tick values do not overlap");
        Check(thumbs, "015 FR-003: the thumb is drawn on the tick of the speed");
        Check(nearest, "015 FR-005/SC-001: SpeedAtX snaps to the nearest tick and to the end values outside");
        Check(hit, "015 FR-005: IsOnSpeedSlider covers thumb, track and values but not the buttons or below the band");

        struct Row { RunPhase Phase; int Speed; SpeedChangeAction Action; };
        const Row rows[] = {
            { RunPhase::Idle, -13, SpeedChangeAction::None }, { RunPhase::Idle, 0, SpeedChangeAction::None },
            { RunPhase::Idle, 9, SpeedChangeAction::None }, { RunPhase::Idle, 10, SpeedChangeAction::None },
            { RunPhase::Running, -13, SpeedChangeAction::Rebase }, { RunPhase::Running, 0, SpeedChangeAction::Rebase },
            { RunPhase::Running, 9, SpeedChangeAction::Rebase }, { RunPhase::Running, 10, SpeedChangeAction::FinishNow },
            { RunPhase::Paused, -13, SpeedChangeAction::None }, { RunPhase::Paused, 0, SpeedChangeAction::None },
            { RunPhase::Paused, 9, SpeedChangeAction::None }, { RunPhase::Paused, 10, SpeedChangeAction::FinishNow },
        };
        bool actions = true;
        for (const Row& r : rows)
            actions = actions && GetSpeedChangeAction(r.Phase, r.Speed) == r.Action;
        Check(actions, "015 FR-007..FR-009: a speed change rebases while running, waits while paused, and +10 finishes at once");
    }
    void CheckFeature017()
    {
        std::cout << "\n== feature 017: frame display check box and simulated time ==\n";
        const int dpis[] = { 96, 120, 144 };
        bool order = true, inside = true, toggle = true;
        for (int dpi : dpis)
        {
            const int band = ScaleToDpi(kToolbarHeight, dpi);
            const int hitLeft = ScaleToDpi(kFrameDisplayHitLeft, dpi);
            const int hitRight = ScaleToDpi(kFrameDisplayHitRight, dpi);
            const int sliderLeft = SpeedTickX(kMinSpeed, dpi) - ScaleToDpi(kSpeedTickPitch / 2, dpi);
            const int sliderRight = SpeedTickX(kMaxSpeed, dpi) + ScaleToDpi(kSpeedTickPitch / 2, dpi);
            const DiagramRect area = SimTimeAreaRect(dpi);
            order = order && SimButtonRect(2, dpi).Right < hitLeft && hitRight <= sliderLeft && sliderRight <= area.Left;

            const DiagramRect box = FrameDisplayBoxRect(dpi);
            const DiagramRect label = FrameDisplayLabelRect(dpi);
            const DiagramRect value = SimTimeValueRect(dpi);
            const DiagramRect unit = SimTimeUnitRect(dpi);
            const DiagramRect caption = SimTimeCaptionRect(dpi);
            inside = inside && area.Right <= ScaleToDpi(kDiagramWidth, dpi) && area.Bottom <= band &&
                box.Left >= hitLeft && box.Right <= label.Left && label.Right <= hitRight &&
                box.Top >= ScaleToDpi(kFrameDisplayHitTop, dpi) && label.Bottom <= ScaleToDpi(kFrameDisplayHitBottom, dpi) &&
                ScaleToDpi(kFrameDisplayHitBottom, dpi) <= band &&
                value.Right <= unit.Left && caption.Bottom <= value.Top && unit.Right <= area.Right;

            const DiagramRect stop = SimButtonRect(2, dpi);
            toggle = toggle && IsOnFrameDisplayToggle((box.Left + box.Right) / 2, (box.Top + box.Bottom) / 2, dpi) &&
                IsOnFrameDisplayToggle((label.Left + label.Right) / 2, (label.Top + label.Bottom) / 2, dpi) &&
                !IsOnFrameDisplayToggle((stop.Left + stop.Right) / 2, (stop.Top + stop.Bottom) / 2, dpi) &&
                !IsOnFrameDisplayToggle(SpeedTickX(kMinSpeed, dpi), ScaleToDpi(20, dpi), dpi) &&
                !IsOnFrameDisplayToggle((box.Left + box.Right) / 2, band + 1, dpi);
        }
        Check(order, "017 FR-002/FR-006: buttons, check box, slider and simulated time are placed left to right without overlap (DPI 96/120/144)");
        Check(inside, "017: everything stays inside the toolbar band and the window width; box and label inside the hit area");
        Check(toggle, "017 FR-003: IsOnFrameDisplayToggle covers the box and the label only");
        Check(FormatSimTimeUs(0) == "0" && FormatSimTimeUs(999) == "999" && FormatSimTimeUs(1000) == "1,000" &&
            FormatSimTimeUs(1234567) == "1,234,567" && FormatSimTimeUs(10000000) == "10,000,000" &&
            FormatSimTimeUs(600000000) == "600,000,000" && FormatSimTimeUs(-5) == "0",
            "017 FR-007: FormatSimTimeUs groups the digits by three");
    }
    void CheckFeature018()
    {
        std::cout << "\n== feature 018: flow animation ==\n";
        bool phaseOk = FlowPhase(0) == 0.0 && FlowPhase(300000) == 0.5 && FlowPhase(600000) == 0.0 && FlowPhase(-1) == 0.0;
        for (long long us = 0; us < 3000000; us += 3001)
        {
            const double p = FlowPhase(us);
            phaseOk = phaseOk && p >= 0.0 && p < 1.0;
        }
        Check(phaseOk, "018 FR-002: FlowPhase repeats every 600 ms of real time and stays in [0, 1)");

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
            double travelled = -1.0;
            FlowPoint prev = a;
            double total = 0.0;
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
        Check(ends, "018 FR-001: a path starts at the sender box edge and ends at the receiver box edge");
        Check(onPath, "018 FR-005: every point of a path is on the sender stub, the bus or the receiver stub");
        Check(forward, "018 FR-001: particles move forward along the path as f grows");

        ActiveFrames active;
        active.Senders = { 1 };
        active.Receivers = { 3, 5 };
        active.BusBusy = true;
        active.Flows = { { 1, 3 }, { 1, 5 } };
        const FlowEffect e = ComputeFlowEffect(six, active, 0.0);
        bool countOk = e.Particles.size() == 6;
        ActiveFrames none = active;
        none.Flows.clear();
        countOk = countOk && ComputeFlowEffect(six, none, 0.3).Particles.empty() && ComputeFlowEffect(six, none, 0.3).Glows.empty();
        none = active;
        none.BusBusy = false;
        countOk = countOk && ComputeFlowEffect(six, none, 0.3).Particles.empty() && ComputeFlowEffect(six, none, 0.3).Glows.empty();
        Check(countOk, "018 FR-001/FR-005: 3 particles per (sender, receiver) pair; nothing when the bus is idle or there is no pair");

        // Collect keeps the pairs even when both ECUs send in the same interval
        // (they are then both red and Receivers is empty).
        {
            std::vector<FrameEvent> events(2);
            events[0].TxStartUs = 0; events[0].TxEndUs = 200; events[0].TxNodeId = 1; events[0].RxNodeIds = { 2, 3 };
            events[1].TxStartUs = 300; events[1].TxEndUs = 500; events[1].TxNodeId = 3; events[1].RxNodeIds = { 1, 2 };
            ActiveFrameCursor cursor(events);
            const ActiveFrames a = cursor.Collect(0, 25000);
            const std::vector<std::pair<int, int>> expect = { { 1, 2 }, { 1, 3 }, { 3, 1 }, { 3, 2 } };
            Check(a.Flows == expect && a.Receivers == std::vector<int>{ 2 } && a.Senders == std::vector<int>{ 1, 3 },
                "018 FR-001: Collect keeps every (sender, receiver) pair of the interval, also between two senders");
            ActiveFrames both;
            both.Senders = { 1, 3 };
            both.BusBusy = true;
            both.Flows = { { 1, 3 }, { 3, 1 } };
            Check(ComputeFlowEffect(ComputeBusDiagramLayout(3), both, 0.5).Particles.size() == 6,
                "018 FR-001: two ECUs sending to each other in one refresh still get particles");
        }

        ActiveFrames one;
        one.Senders = { 1 };
        one.Receivers = { 3 };
        one.BusBusy = true;
        one.Flows = { { 1, 3 } };
        bool noOthers = true;
        for (int i = 0; i < 50; ++i)
        {
            const FlowEffect f = ComputeFlowEffect(six, one, i / 50.0);
            for (const FlowPoint& q : f.Particles)
                for (int n : { 2, 4, 5, 6 })
                {
                    const EcuPlacement& o = six.Ecus[n - 1];
                    const double lo = (std::min)(static_cast<double>(o.LineY1), static_cast<double>(kBusY));
                    const double hi = (std::max)(static_cast<double>(o.LineY1), static_cast<double>(kBusY));
                    noOthers = noOthers && !(std::fabs(q.X - o.LineX) < 1e-6 && q.Y > lo + 1e-6 && q.Y < hi - 1e-6);
                }
        }
        Check(noOthers, "018 FR-005: no particle on the stub line of an ECU that does not receive");

        bool glowOk = false;
        for (const FlowGlow& g : e.Glows)
            if (g.Ecu == 1 && g.Sender && std::fabs(g.Strength - 1.0) < 1e-9)
                glowOk = true;
        for (int i = 0; i < 50; ++i)
            for (const FlowGlow& g : ComputeFlowEffect(six, active, i / 50.0).Glows)
                glowOk = glowOk && g.Strength > 0.0 && g.Strength <= 1.0;
        Check(glowOk, "018 FR-003: the sender glows fully when a particle leaves; glow strengths stay in (0, 1]");

        const BusDiagramLayout sixteen = ComputeBusDiagramLayout(16);
        ActiveFrames many;
        for (int n = 1; n <= 8; ++n) many.Senders.push_back(n);
        for (int n = 9; n <= 16; ++n) many.Receivers.push_back(n);
        many.BusBusy = true;
        for (int s = 1; s <= 8; ++s)
            for (int r = 9; r <= 16; ++r)
                many.Flows.emplace_back(s, r);
        std::size_t sink = 0;
        const auto t0 = std::chrono::steady_clock::now();
        for (int i = 0; i < 1000; ++i)
            sink += ComputeFlowEffect(sixteen, many, (i % 600) / 600.0).Particles.size();
        const double sec = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        std::ostringstream what;
        what << "018 SC-005: 1000 effects for 8 x 8 ECUs take less than 1 s (" << sec << " s)";
        Check(sec < 1.0 && sink == 1000u * 8 * 8 * 3, what.str());
    }
}

int main(int argc, char** argv)
{
    if (argc < 3)
    {
        std::cout << "usage: checks.exe <SampleConfigs dir> <temp output dir> [baseline dir]\n";
        return 2;
    }
    g_sampleDir = argv[1];
    g_tempDir = argv[2];
    if (argc >= 4)
        g_baselineDir = argv[3];

    CheckFeature001();
    CheckFeature002();
    CheckFeature003();
    CheckFeature004();
    CheckFeature005();
    CheckFeature006();
    CheckFeature007();
    CheckFeature008();
    CheckFeature009();
    CheckFeature010();
    CheckFeature011();
    CheckFeature014();
    CheckFeature015();
    CheckFeature017();
    CheckFeature018();

    std::cout << "\n" << g_failures << " check(s) failed.\n";
    return g_failures == 0 ? 0 : 1;
}
