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
#include "LogFileIndex.h"
#include "LogAnalysis.h"

#include <climits>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <direct.h>
#include <io.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <cstdlib>
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
        // Feature 031: 0x050 (DLC 2, data 0xFF) is 65 bits = 130 us with standard bit stuffing (was 67 bits).
        Check(first080 == 158, "002 SC-006: 0x080 first Tx_Start is 22 us startup + 130 us (0x050) + 6 us intermission = 158 us");

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
                // Feature 031: 0x100 (DLC 8, data 0xFF) is 123 bits = 246 us (was 126 bits), so the frame
                // sent at 22 us ends at 268 us and the intermission ends at 274 us.
                Check(b && b->NormalEntryUs == 274,
                    "SC-004: a node joining mid-frame syncs at the intermission end (274 us)");
                // Feature 006: node 1's second request moved from 300 to 322 us
                // (Normal 22 + period 300) and node 3 now powers on at 305 us, so
                // its window (305 + 22 = 327) is cut by the frame 322 -> 568 and
                // it syncs at 568 - 16 + 22 = 574 us (feature 031: the frame is 246 us long; it was 322 -> 574 before).
                Check(c && c->NormalEntryUs == 574,
                    "FR-007: a node interrupted by a new frame restarts and syncs at 574 us");
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
                      r.Project.FrameDisplay && r.Config.Nodes.empty() && r.Config.BitRateBps == 250000,
                "004 FR-006 / 021: [Project] defaults are EcuCount=2, DurationSec=10, FrameDisplay=1 (021); zero nodes allowed");

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
            const Case ok[] = { { "+3", 3 }, { "-3", -3 }, { "10", 10 }, { "-10", -10 }, { "0", 0 } };
            bool accepted = true;
            for (const Case& c : ok)
            {
                ConfigLoadResult r = LoadText("s_ok.canprj", std::string("[Project]\r\nSpeed=") + c.Text + "\r\n");
                accepted = accepted && r.Success && r.Project.Speed == c.Expected;
            }
            Check(accepted, "005 FR-002: Speed +3 / -3 / 10 / -10 / 0 are accepted (026: range -10..10)");

            const char* bad[] = { "11", "-14", "1.5", "abc", "", "+", "--1" };
            bool rejected = true;
            for (const char* text : bad)
            {
                ConfigLoadResult r = LoadText("s_bad.canprj", std::string("[Project]\r\nSpeed=") + text + "\r\n");
                rejected = rejected && !r.Success && r.ErrorMessage.find("Speed") != std::string::npos;
            }
            Check(rejected, "005 FR-002: Speed 11 / -14 / 1.5 / abc / empty are rejected with a reason");

            ProjectSettings s;
            s.Speed = -10;
            const std::string path = Join(g_tempDir, "s_roundtrip.canprj");
            ConfigLoadResult back;
            const bool saved = SaveProject(Widen(path), s, Network());
            back = LoadConfig(Widen(path));
            Check(saved && back.Success && back.Project.Speed == -10, "005 FR-002: Speed survives save then reload");
        }

        // SimTimeAtUs (FR-003)
        {
            const long long e = 1000000; // 1 s of real time
            Check(SimTimeAtUs(e, 0) == e && SimTimeAtUs(e, 1) == 2 * e && SimTimeAtUs(e, -1) == e / 2 &&
                      SimTimeAtUs(e, -10) == e / 1024 && SimTimeAtUs(e, -13) == e / 8192 && // the formula itself also holds below kMinSpeed
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
        const int speeds[] = { kMinSpeed, 0, 5 };
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
                // 019: only the labels shown for the current value must not overlap
                for (int cur = kMinSpeed; cur <= kMaxSpeed; ++cur)
                    for (int prev = kMinSpeed; prev < s; ++prev)
                        if (IsSpeedLabelShown(prev, cur) && IsSpeedLabelShown(s, cur))
                            labels = labels && SpeedLabelRect(prev, dpi).Right <= SpeedLabelRect(s, dpi).Left;
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
        Check(even, "015 FR-002 / 026: 21 ticks from -10 to +10, evenly spaced (DPI 96/120/144)");
        Check(placed, "015 FR-002/FR-012: the slider is right of the Stop button and inside the toolbar band and the window width");
        Check(labels, "015 FR-004 / 019 FR-004: the shown tick values do not overlap for any current value");
        Check(thumbs, "015 FR-003: the thumb is drawn on the tick of the speed");
        Check(nearest, "015 FR-005/SC-001: SpeedAtX snaps to the nearest tick and to the end values outside");
        Check(hit, "015 FR-005: IsOnSpeedSlider covers thumb, track and values but not the buttons or below the band");

        struct Row { RunPhase Phase; int Speed; SpeedChangeAction Action; };
        const Row rows[] = {
            { RunPhase::Idle, kMinSpeed, SpeedChangeAction::None }, { RunPhase::Idle, 0, SpeedChangeAction::None },
            { RunPhase::Idle, 9, SpeedChangeAction::None }, { RunPhase::Idle, 10, SpeedChangeAction::None },
            { RunPhase::Running, kMinSpeed, SpeedChangeAction::Rebase }, { RunPhase::Running, 0, SpeedChangeAction::Rebase },
            { RunPhase::Running, 9, SpeedChangeAction::Rebase }, { RunPhase::Running, 10, SpeedChangeAction::FinishNow },
            { RunPhase::Paused, kMinSpeed, SpeedChangeAction::None }, { RunPhase::Paused, 0, SpeedChangeAction::None },
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
            // 019: the simulated time comes before the "Speed:" caption and the slider
            order = order && SimButtonRect(2, dpi).Right < hitLeft && hitRight <= area.Left &&
                area.Right <= SpeedCaptionRect(dpi).Left && SpeedCaptionRect(dpi).Right <= sliderLeft &&
                sliderRight <= ScaleToDpi(kDiagramWidth, dpi);

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
        Check(order, "017 / 019 FR-001: buttons, check box, simulated time, Speed: and slider are placed left to right without overlap (DPI 96/120/144)");
        Check(inside, "017: everything stays inside the toolbar band and the window width; box and label inside the hit area");
        Check(toggle, "017 FR-003: IsOnFrameDisplayToggle covers the box and the label only");
        Check(FormatSimTimeUs(0) == "0" && FormatSimTimeUs(999) == "999" && FormatSimTimeUs(1000) == "1,000" &&
            FormatSimTimeUs(1234567) == "1,234,567" && FormatSimTimeUs(10000000) == "10,000,000" &&
            FormatSimTimeUs(600000000) == "600,000,000" && FormatSimTimeUs(-5) == "0",
            "017 FR-007: FormatSimTimeUs groups the digits by three");
    }
    void CheckFeature018()
    {
        std::cout << "\n== feature 018: flow animation (revision 1) ==\n";
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
        Check(ends, "018 FR-001: a path starts at the sender box edge and ends at the receiver box edge");
        Check(onPath, "018 FR-005: every point of a path is on the sender stub, the bus or the receiver stub");
        Check(forward, "018 FR-001: points move forward along the path as f grows");

        // Revision 1 (A): constant spacing and speed
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
                // neighbours are kFlowSpacing apart along the path; on a corner the
                // straight Manhattan distance is the same as the path distance
                const double d = std::fabs(f.Particles[k].X - f.Particles[k - 1].X) +
                    std::fabs(f.Particles[k].Y - f.Particles[k - 1].Y);
                spacing = spacing && std::fabs(d - kFlowSpacing) < 1e-6;
            }
            const long long dt = 25000; // one refresh
            double moved = FlowOffset(t0 + dt) - offset;
            if (moved < 0)
                moved += kFlowSpacing;
            speed = speed && std::fabs(moved - kFlowSpeed * 0.025) < 1e-6;
        }
        Check(count, "018 rev.1: particles at offset + 16k along the whole path (count = floor((L - offset) / 16) + 1)");
        Check(spacing, "018 rev.1: neighbouring particles are 16 px apart on the path");
        Check(speed, "018 rev.1: particles move 150 px/s of real time (3.75 px per 25 ms refresh) on every path");

        // Pairs, other ECUs, two senders
        const std::vector<std::pair<int, int>> noPairs;
        bool pairsOk = ComputeFlowEffect(six, noPairs, 1000).Particles.empty() &&
            ComputeFlowEffect(six, noPairs, 1000).Glows.empty();
        const std::vector<std::pair<int, int>> both = { { 1, 3 }, { 3, 1 } };
        pairsOk = pairsOk && !ComputeFlowEffect(ComputeBusDiagramLayout(3), both, 0).Particles.empty();
        Check(pairsOk, "018 FR-001: nothing without a pair; two ECUs sending to each other still get particles");
        {
            std::vector<FrameEvent> events(2);
            events[0].TxStartUs = 0; events[0].TxEndUs = 200; events[0].TxNodeId = 1; events[0].RxNodeIds = { 2, 3 };
            events[1].TxStartUs = 300; events[1].TxEndUs = 500; events[1].TxNodeId = 3; events[1].RxNodeIds = { 1, 2 };
            ActiveFrameCursor cursor(events);
            const ActiveFrames a = cursor.Collect(0, 25000);
            const std::vector<std::pair<int, int>> expect = { { 1, 2 }, { 1, 3 }, { 3, 1 }, { 3, 2 } };
            Check(a.Flows == expect && a.Receivers == std::vector<int>{ 2 } && a.Senders == std::vector<int>{ 1, 3 },
                "018 FR-001: Collect keeps every (sender, receiver) pair of the interval, also between two senders");
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
        Check(noOthers, "018 FR-005: no particle on the stub line of an ECU that does not receive");

        // Glow: one per ECU, in [0.05, 1], the receiver half a period after the sender
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
        Check(glow, "018 rev.1: one glow per sender / receiver in [0.05, 1], the receiver half a period after the sender");

        // Revision 1 (D): paths stay 0.3 s
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
        Check(holdOk, "018 rev.1: FlowPathHold keeps a path 300 ms after it was last seen, Clear empties it");

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
        what << "018 SC-005: 1000 effects for 8 x 8 ECUs take less than 1 s (" << sec << " s, " << sink / 1000 << " particles each)";
        Check(sec < 1.0 && sink > 0, what.str());
    }
    void CheckFeature019()
    {
        std::cout << "\n== feature 019: compact toolbar ==\n";
        bool half = true;
        for (int i = 0; i < kSimButtonCount; ++i)
        {
            const DiagramRect box = SimButtonBoxRect(i, 96);
            half = half && box.Right - box.Left == 24 && box.Bottom - box.Top == 18;
        }
        half = half && SpeedTickX(kMinSpeed + 1, 96) - SpeedTickX(kMinSpeed, 96) == 13 &&
            SpeedThumbRect(0, 96).Right - SpeedThumbRect(0, 96).Left <= 5;
        Check(half, "019 FR-002/FR-003: buttons 24 x 18 and slider ticks 13 apart (half size, 96 DPI)");

        auto shown = [](int current) {
            std::vector<int> v;
            for (int s = kMinSpeed; s <= kMaxSpeed; ++s)
                if (IsSpeedLabelShown(s, current))
                    v.push_back(s);
            return v;
        };
        Check(shown(0) == std::vector<int>{ -10, -5, 0, 5, 10 } &&
            shown(-9) == std::vector<int>{ -9, -5, 0, 5, 10 } &&
            shown(9) == std::vector<int>{ -10, -5, 0, 5, 9 } &&
            shown(3) == std::vector<int>{ -10, -5, 0, 3, 10 },
            "019 FR-004: every 5th value and the current one; 5th values within 2 ticks of it are hidden");

        bool caption = true;
        const int dpis[] = { 96, 120, 144 };
        for (int dpi : dpis)
        {
            const DiagramRect c = SpeedCaptionRect(dpi);
            caption = caption && c.Right <= SpeedTickX(kMinSpeed, dpi) - ScaleToDpi(kSpeedTickPitch / 2, dpi) &&
                c.Left >= SimTimeAreaRect(dpi).Right && c.Bottom <= ScaleToDpi(kToolbarHeight, dpi) &&
                !IsOnSpeedSlider((c.Left + c.Right) / 2, (c.Top + c.Bottom) / 2, dpi);
        }
        Check(caption, "019 FR-005: the Speed: caption is between the simulated time and the slider, outside the slider hit area");
    }
    void CheckFeature020()
    {
        std::cout << "\n== feature 020: event & log view label ==\n";
        const int dpis[] = { 96, 120, 144 };
        bool logOk = true;
        for (int dpi : dpis)
        {
            const DiagramRect frame = ScaleLogFrameRect(dpi);
            const DiagramRect g = ScaleLogLabelRect(dpi);
            logOk = logOk && g.Top >= ScaleToDpi(kDiagramHeight, dpi) && g.Bottom <= frame.Top &&
                g.Left == frame.Left && g.Right <= frame.Right && g.Bottom - g.Top >= ScaleToDpi(16, dpi);
        }
        Check(logOk, "020: the event & log view label is between the diagram and the log view border, at its left edge (DPI 96/120/144)");
    }
    void CheckFeature022()
    {
        std::cout << "\n== feature 022: ECU label ==\n";
        // UTF-8 test strings written as bytes (this file stays ASCII)
        const std::string engine = "\xE3\x82\xA8\xE3\x83\xB3\xE3\x82\xB8\xE3\x83\xB3";           // 4 characters
        const std::string ten = "0123456789";
        const std::string eleven = "0123456789A";
        std::string longJp;                                                                       // 14 characters
        for (int i = 0; i < 14; ++i)
            longJp += "\xE3\x81\x82";
        std::string firstTenJp;
        for (int i = 0; i < 10; ++i)
            firstTenJp += "\xE3\x81\x82";
        Check(CountUtf8Chars(engine) == 4 && CountUtf8Chars("ABC") == 3 && CountUtf8Chars("") == 0 && CountUtf8Chars(longJp) == 14,
            "022: CountUtf8Chars counts full and half width characters as 1");
        Check(EcuLabelForDiagram(engine) == engine && EcuLabelForDiagram(ten) == ten &&
            EcuLabelForDiagram(eleven) == ten + "..." && EcuLabelForDiagram(longJp) == firstTenJp + "..." &&
            EcuLabelForDiagram("") == "",
            "022 FR-003: up to 10 characters as they are, longer labels as the first 10 characters and ...");
        Check(TrimEcuLabel("  " + engine + "\t") == engine && TrimEcuLabel("\xE3\x80\x80" + engine + "\xE3\x80\x80") == engine &&
            TrimEcuLabel(" \xE3\x80\x80 ") == "" && TrimEcuLabel("a b") == "a b",
            "022: TrimEcuLabel removes leading / trailing spaces, tabs and ideographic spaces only");
        std::string thirtyThree;
        for (int i = 0; i < 33; ++i)
            thirtyThree += "\xE3\x81\x82";
        Check(CountUtf8Chars(LimitEcuLabel(thirtyThree)) == 32 && LimitEcuLabel(engine) == engine,
            "022: LimitEcuLabel keeps the first 32 characters");

        // Label= round trip; not written when empty; missing -> empty
        {
            ProjectSettings settings;
            Network net;
            net.BitRateBps = 500000;
            Node a;
            a.NodeId = 1;
            a.Name = "ECU1";
            a.Label = engine;
            Node b;
            b.NodeId = 2;
            b.Name = "ECU2";
            net.Nodes.push_back(a);
            net.Nodes.push_back(b);
            const std::string path = Join(g_tempDir, "label_roundtrip.canprj");
            const bool saved = SaveProject(Widen(path), settings, net);
            std::ifstream in(path, std::ios::binary);
            std::stringstream ss;
            ss << in.rdbuf();
            const std::string text = ss.str();
            std::size_t labels = 0;
            for (std::size_t p = text.find("Label="); p != std::string::npos; p = text.find("Label=", p + 1))
                ++labels;
            const ConfigLoadResult back = LoadConfig(Widen(path));
            Check(saved && labels == 1 && back.Success && back.Config.Nodes.size() == 2 &&
                back.Config.Nodes[0].Label == engine && back.Config.Nodes[1].Label.empty() &&
                back.Config.Nodes[0].Name == "ECU1",
                "022 FR-006: Label= is saved only when set and read back; Name is unchanged");
            ConfigLoadResult old = LoadText("label_missing.canprj", "[Node1]\r\nNodeId=1\r\nName=X\r\n");
            Check(old.Success && old.Config.Nodes.size() == 1 && old.Config.Nodes[0].Label.empty(),
                "022: a file without Label= gives an empty label");
        }

        // Taller boxes stay inside the border and clear of the bus
        bool boxes = kEcuBoxHeight == 48;
        for (int n = 0; n <= kMaxEcuCount; ++n)
        {
            const BusDiagramLayout layout = ComputeBusDiagramLayout(n);
            for (const EcuPlacement& e : layout.Ecus)
            {
                boxes = boxes && e.Box.Bottom - e.Box.Top == 48 && e.Box.Top > layout.Frame.Top && e.Box.Bottom < layout.Frame.Bottom;
                boxes = boxes && (e.OnTop ? (e.Box.Top == 20 && e.Box.Bottom == 68) : (e.Box.Top == 152 && e.Box.Bottom == 200));
            }
        }
        Check(boxes, "022 FR-004: ECU boxes are 48 high (upper 20..68, lower 152..200), inside the diagram border");
    }

    void CheckFeature023()
    {
        std::cout << "\n== feature 023: Save / Load toolbar buttons ==\n";
        const int dpis[] = { 96, 120, 144 };
        bool lefts = true;
        bool ordered = true;
        bool hit = true;
        bool aliases = true;
        bool placed = true;
        const int expectedLeft[kToolbarButtonCount] = { 8, 36, 92, 120, 148 };
        for (int i = 0; i < kToolbarButtonCount; ++i)
        {
            const DiagramRect box = ToolbarButtonBoxRect(i, 96);
            lefts = lefts && box.Left == expectedLeft[i] && box.Right - box.Left == 24 && box.Bottom - box.Top == 18;
        }
        for (int dpi : dpis)
        {
            const int band = ScaleToDpi(kToolbarHeight, dpi);
            for (int i = 0; i < kToolbarButtonCount; ++i)
            {
                const DiagramRect r = ToolbarButtonRect(i, dpi);
                if (i > 0)
                    ordered = ordered && ToolbarButtonRect(i - 1, dpi).Right < r.Left;
                hit = hit && FindToolbarButtonAt(r.Left, r.Top, dpi) == i &&
                    FindToolbarButtonAt(r.Right - 1, r.Bottom - 1, dpi) == i &&
                    FindToolbarButtonAt(r.Right, r.Top, dpi) != i && FindToolbarButtonAt(r.Left, r.Bottom, dpi) == -1 &&
                    FindToolbarButtonAt(r.Left - 1, r.Top, dpi) != i && r.Bottom <= band;
            }
            // one free button pitch between Load and Start
            const DiagramRect load = ToolbarButtonRect(static_cast<int>(ToolbarButton::Load), dpi);
            const DiagramRect start = ToolbarButtonRect(static_cast<int>(ToolbarButton::Start), dpi);
            ordered = ordered && start.Left - load.Right >= ScaleToDpi(kSimButtonPitch, dpi);
            hit = hit && FindToolbarButtonAt(ScaleToDpi(76, dpi), (load.Top + load.Bottom) / 2, dpi) == -1 &&
                FindToolbarButtonAt(ScaleToDpi(10, dpi), band + 5, dpi) == -1;

            for (int i = 0; i < kSimButtonCount; ++i)
            {
                const DiagramRect a = SimButtonRect(i, dpi);
                const DiagramRect b = ToolbarButtonRect(i + kFileButtonCount, dpi);
                aliases = aliases && a.Left == b.Left && a.Right == b.Right && a.Top == b.Top && a.Bottom == b.Bottom &&
                    FindSimButtonAt((a.Left + a.Right) / 2, (a.Top + a.Bottom) / 2, dpi) == i;
            }
            for (int i = 0; i < kFileButtonCount; ++i)
            {
                const DiagramRect r = ToolbarButtonRect(i, dpi);
                aliases = aliases && FindSimButtonAt((r.Left + r.Right) / 2, (r.Top + r.Bottom) / 2, dpi) == -1;
            }

            placed = placed && SimButtonRect(2, dpi).Right < ScaleToDpi(kFrameDisplayHitLeft, dpi) &&
                SpeedLabelRect(kMaxSpeed, dpi).Right <= ScaleToDpi(kDiagramWidth, dpi) &&
                SpeedLabelRect(kMaxSpeed, dpi).Bottom <= band;
        }
        Check(lefts, "023 FR-001/FR-002: Save, Load, Start, Pause, Stop boxes at x 8, 36, 92, 120, 148 (24 x 18, 96 DPI)");
        Check(ordered, "023 FR-001: buttons left to right without overlap, one button pitch free between Load and Start (DPI 96/120/144)");
        Check(hit, "023 FR-007: FindToolbarButtonAt agrees with [Left, Right) x [Top, Bottom); -1 on the free slot and below the band");
        Check(aliases, "023: SimButtonRect(i) == ToolbarButtonRect(i + 2); FindSimButtonAt is -1 on Save / Load");
        Check(placed, "023: the check box is right of Stop and the +10 label still fits the band and the window width");

        bool states = true;
        const RunPhase phases[] = { RunPhase::Idle, RunPhase::Running, RunPhase::Paused };
        for (RunPhase phase : phases)
            for (int editing = 0; editing < 2; ++editing)
            {
                const SimCommandStates s = GetSimCommandStates(phase, editing != 0);
                const bool file = phase == RunPhase::Idle;
                states = states &&
                    IsToolbarButtonEnabled(static_cast<int>(ToolbarButton::Save), phase, editing != 0) == file &&
                    IsToolbarButtonEnabled(static_cast<int>(ToolbarButton::Load), phase, editing != 0) == file &&
                    IsToolbarButtonEnabled(static_cast<int>(ToolbarButton::Start), phase, editing != 0) == s.Start &&
                    IsToolbarButtonEnabled(static_cast<int>(ToolbarButton::Pause), phase, editing != 0) == s.Pause &&
                    IsToolbarButtonEnabled(static_cast<int>(ToolbarButton::Stop), phase, editing != 0) == s.Stop;
            }
        Check(states, "023 FR-006: Save / Load only while Idle (also while editing); Start / Pause / Stop as GetSimCommandStates");

        // Revision 1 A: the check box and its label share the vertical centre of the button boxes.
        bool aligned = true;
        for (int dpi : dpis)
        {
            const DiagramRect stop = ToolbarButtonBoxRect(static_cast<int>(ToolbarButton::Stop), dpi);
            const DiagramRect box = FrameDisplayBoxRect(dpi);
            const DiagramRect label = FrameDisplayLabelRect(dpi);
            const int stopMid = stop.Top + stop.Bottom;   // doubled centres keep the half pixel
            const int boxMid = box.Top + box.Bottom;
            const int labelMid = label.Top + label.Bottom;
            aligned = aligned && std::abs(boxMid - stopMid) <= 2 && std::abs(labelMid - stopMid) <= 2 &&
                box.Top >= ScaleToDpi(kFrameDisplayHitTop, dpi) && label.Top >= ScaleToDpi(kFrameDisplayHitTop, dpi) &&
                box.Bottom <= ScaleToDpi(kToolbarHeight, dpi);
        }
        Check(aligned, "023 revision 1 A: check box and label centred on the Stop box within 1 px, inside the hit area (DPI 96/120/144)");
    }

    void CheckFeature024()
    {
        std::cout << "\n== feature 024: project file name in the title, project given at startup ==\n";
        // The real label is Japanese text in the .rc; an ASCII stand-in checks the layout.
        Check(ComposeWindowTitle(L"CAN Bus Simulator", L"LABEL:", L"") == L"CAN Bus Simulator    LABEL:" &&
            ComposeWindowTitle(L"CAN Bus Simulator", L"LABEL:", L"C:\\work\\a.canprj") ==
                L"CAN Bus Simulator    LABEL:C:\\work\\a.canprj",
            "024 FR-001/FR-002: title = app title + 4 spaces + label + full path; the label ends it without a project");

        const std::wstring exe = L"MFCApplication1.exe";
        Check(FindStartupProjectArg({}).empty() && FindStartupProjectArg({ exe }).empty() &&
            FindStartupProjectArg({ exe, L"a.canprj" }) == L"a.canprj" &&
            FindStartupProjectArg({ exe, L"", L"a.canprj", L"b.canprj" }) == L"a.canprj",
            "024 FR-005: the first non-empty argument after the program is the startup project; later ones are ignored");

        Check(HasProjectExtension(L"a.canprj") && HasProjectExtension(L"A.CANPRJ") &&
            HasProjectExtension(L"C:\\x\\b.CanPrj") && !HasProjectExtension(L"a.canprj.txt") &&
            !HasProjectExtension(L"README.md") && !HasProjectExtension(L"canprj") && !HasProjectExtension(L""),
            "024 FR-005: HasProjectExtension accepts .canprj in any case and rejects other extensions");
    }
    // Writes text as is (binary) for the log index checks.
    void WriteRaw(const std::string& path, const std::string& text)
    {
        std::ofstream out(path.c_str(), std::ios::binary);
        out << text;
    }

    void CheckFeature025()
    {
        std::cout << "\n== feature 025: simulation log viewer (line index) ==\n";
        {
            const std::string empty = Join(g_tempDir, "log025_empty.csv");
            WriteRaw(empty, "");
            LogFileIndex index;
            const bool built = index.Build(Widen(empty));
            LogFileIndex missing;
            Check(built && index.LineCount() == 0 && index.ReadLines(0, 10).empty() &&
                !missing.Build(Widen(Join(g_tempDir, "log025_no_such_file.csv"))),
                "025: an empty log is 0 lines; a missing file cannot be built");
        }

        // 1000 lines of different lengths, LF / CRLF / no final newline
        std::vector<std::string> lines;
        std::size_t longest = 0;
        for (int i = 0; i < 1000; ++i)
        {
            std::string s = "line," + std::to_string(i) + "," + std::string(static_cast<std::size_t>(i % 37), 'x');
            longest = (std::max)(longest, s.size());
            lines.push_back(s);
        }
        std::string lf, crlf;
        for (const std::string& s : lines)
        {
            lf += s + "\n";
            crlf += s + "\r\n";
        }
        const std::string noFinal = lf.substr(0, lf.size() - 1);
        bool same = true;
        bool clipped = true;
        const std::string names[] = { "log025_lf.csv", "log025_crlf.csv", "log025_nofinal.csv" };
        const std::string* texts[] = { &lf, &crlf, &noFinal };
        for (int k = 0; k < 3; ++k)
        {
            const std::string path = Join(g_tempDir, names[k]);
            WriteRaw(path, *texts[k]);
            LogFileIndex index;
            same = same && index.Build(Widen(path)) && index.LineCount() == 1000 &&
                index.MaxLineLength() == static_cast<int>(longest);
            const std::vector<std::string> all = index.ReadLines(0, 1000);
            same = same && all == lines;
            const std::vector<std::string> a = index.ReadLines(255, 3);
            const std::vector<std::string> b = index.ReadLines(256, 1);
            const std::vector<std::string> c = index.ReadLines(998, 10);
            const std::vector<std::string> d = index.ReadLines(-5, 7);
            clipped = clipped && a.size() == 3 && a[0] == lines[255] && a[2] == lines[257] &&
                b.size() == 1 && b[0] == lines[256] &&
                c.size() == 2 && c[1] == lines[999] &&
                d.size() == 2 && d[0] == lines[0] && d[1] == lines[1] &&
                index.ReadLines(1000, 5).empty() && index.ReadLines(10, 0).empty();
        }
        Check(same, "025: LF, CRLF and a missing final newline give the same 1000 lines (no CR) and the same longest line");
        Check(clipped, "025: ReadLines across the 256-line checkpoints and at both ends is clipped to the file");

        {
            // exactly 512 lines: no checkpoint may point past the end
            std::string text;
            for (int i = 0; i < 512; ++i)
                text += "r" + std::to_string(i) + "\n";
            const std::string path = Join(g_tempDir, "log025_512.csv");
            WriteRaw(path, text);
            LogFileIndex index;
            const std::vector<std::string> tail = index.Build(Widen(path)) ? index.ReadLines(510, 5) : std::vector<std::string>();
            Check(index.LineCount() == 512 && tail.size() == 2 && tail[1] == "r511",
                "025: a file ending exactly on a checkpoint boundary reads its last lines");
        }

        {
            // a real frame log (sample_16ecu, 10 s)
            const std::string prefix = Join(g_tempDir, "log025_16ecu");
            bool ok = RunProjectToCsv(Join(g_sampleDir, "sample_16ecu.canprj"), prefix, 10000000LL);
            const std::string path = prefix + ".log.csv";
            std::ifstream in(path.c_str());
            std::string line, firstLine, lastLine;
            long long count = 0;
            while (std::getline(in, line))
            {
                if (!line.empty() && line.back() == '\r')
                    line.pop_back();
                if (count == 0)
                    firstLine = line;
                lastLine = line;
                ++count;
            }
            LogFileIndex index;
            ok = ok && index.Build(Widen(path)) && index.LineCount() == count && count > 1000;
            const std::vector<std::string> head = index.ReadLines(0, 1);
            const std::vector<std::string> tail = index.ReadLines(count - 1, 1);
            ok = ok && head.size() == 1 && head[0] == firstLine && tail.size() == 1 && tail[0] == lastLine;
            std::cout << "  sample_16ecu 10 s log: " << count << " lines\n";
            Check(ok, "025 SC-001: the index of a real frame log has the same line count, first and last line as the file");
        }

        {
            // 1,000,000 lines of about 90 characters: time to build (reported only)
            const std::string path = Join(g_tempDir, "log025_big.csv");
            {
                std::ofstream out(path.c_str(), std::ios::binary);
                const std::string pad(80, 'y');
                for (int i = 0; i < 1000000; ++i)
                    out << i << ',' << pad << "\r\n";
            }
            LogFileIndex index;
            const auto t0 = std::chrono::steady_clock::now();
            const bool built = index.Build(Widen(path));
            const auto t1 = std::chrono::steady_clock::now();
            const std::vector<std::string> tail = index.ReadLines(999990, 20);
            std::cout << "  build of 1,000,000 lines: "
                      << std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count() << " ms\n";
            Check(built && index.LineCount() == 1000000 && tail.size() == 10 &&
                tail[9] == "999999," + std::string(80, 'y'),
                "025 SC-003: a 1,000,000-line log is indexed and its last lines are read");
            std::remove(path.c_str());
        }
    }
    void CheckFeature026()
    {
        std::cout << "\n== feature 026: speed range -10..+10 ==\n";
        const int dpis[] = { 96, 120, 144 };
        bool ticks = kMinSpeed == -10 && kMaxSpeed == 10 && SpeedTickX(-10, 96) == 604 && SpeedTickX(10, 96) == 864 &&
            kMaxSpeed - kMinSpeed + 1 == 21;
        bool fits = true;
        for (int dpi : dpis)
        {
            ticks = ticks && SpeedAtX(0, dpi) == -10 && SpeedAtX(ScaleToDpi(2000, dpi), dpi) == 10 &&
                SpeedTickX(-13, dpi) == SpeedTickX(-10, dpi); // values below the range are drawn at -10
            const int band = ScaleToDpi(kToolbarHeight, dpi);
            fits = fits && SpeedLabelRect(10, dpi).Right <= ScaleToDpi(kDiagramWidth, dpi) &&
                SpeedLabelRect(10, dpi).Bottom <= band &&
                SpeedCaptionRect(dpi).Right <= SpeedTickX(-10, dpi) - ScaleToDpi(kSpeedTickPitch / 2, dpi);
        }
        Check(ticks, "026 FR-004: 21 ticks, -10 at x 604 and +10 at x 864 (96 DPI); SpeedAtX stops at -10 / +10");
        Check(fits, "026 SC-003: the +10 label and the Speed: caption stay inside the band (DPI 96/120/144)");

        struct Case { const char* Text; int Expected; };
        const Case legacy[] = { { "-13", -10 }, { "-12", -10 }, { "-11", -10 }, { "-10", -10 }, { "10", 10 } };
        bool read = true;
        for (const Case& c : legacy)
        {
            ConfigLoadResult r = LoadText("s026_ok.canprj", std::string("[Project]\r\nSpeed=") + c.Text + "\r\n");
            read = read && r.Success && r.Project.Speed == c.Expected;
        }
        Check(read, "026 FR-005/SC-004: Speed -13 / -12 / -11 of older files load as -10; -10 and 10 as they are");

        bool rejected = true;
        const char* bad[] = { "-14", "11" };
        for (const char* text : bad)
        {
            ConfigLoadResult r = LoadText("s026_bad.canprj", std::string("[Project]\r\nSpeed=") + text + "\r\n");
            rejected = rejected && !r.Success && r.ErrorMessage.find("-10 and 10") != std::string::npos;
        }
        Check(rejected, "026 FR-005: Speed -14 and 11 are rejected and the message names -10 and 10");
    }

    void CheckFeature027()
    {
        std::cout << "\n== feature 027: clear log ==\n";
        const std::vector<std::wstring> names = ResultFilePaths(L"C:\\x\\a.canprj");
        Check(names.size() == 3 && names[0] == L"C:\\x\\a.canprj.log.csv" && names[0] == LogPathFor(L"C:\\x\\a.canprj") &&
            names[1] == L"C:\\x\\a.canprj.summary.csv" && names[2] == L"C:\\x\\a.canprj.startup.csv",
            "027 FR-002: ResultFilePaths is the frame log, summary and startup record next to the project, in this order");
        Check(ExistingResultFiles(L"").empty(), "027 FR-005: no result files without a project file");

        const std::wstring project = Widen(Join(g_tempDir, "t027_clear.canprj"));
        const std::vector<std::wstring> paths = ResultFilePaths(project);
        auto touch = [](const std::wstring& path) { std::ofstream(path.c_str()) << "x\n"; };
        auto exists = [](const std::wstring& path) { struct _stat64 st; return _wstat64(path.c_str(), &st) == 0; };
        // leftovers of an earlier run
        _wchmod(paths[2].c_str(), _S_IREAD | _S_IWRITE);
        for (const std::wstring& p : paths) _wremove(p.c_str());
        _wrmdir(paths[1].c_str());

        touch(project);
        touch(paths[0]);
        _wmkdir(paths[1].c_str()); // a folder with the summary's name is not a result file
        touch(paths[2]);
        const std::vector<std::wstring> existing = ExistingResultFiles(project);
        Check(existing.size() == 2 && existing[0] == paths[0] && existing[1] == paths[2],
            "027 FR-002: ExistingResultFiles lists the regular files only, in order (a folder of the same name is left out)");

        _wchmod(paths[2].c_str(), _S_IREAD);
        const LogClearResult result = DeleteResultFiles(existing);
        Check(result.Deleted.size() == 1 && result.Deleted[0] == paths[0] &&
            result.Failed.size() == 1 && result.Failed[0] == paths[2] &&
            !exists(paths[0]) && exists(paths[2]),
            "027 FR-006: DeleteResultFiles deletes what it can and reports the read-only file as failed, without changing it");
        Check(exists(project) && exists(paths[1]), "027 FR-009: the project file and other entries are not touched");

        _wchmod(paths[2].c_str(), _S_IREAD | _S_IWRITE);
        const LogClearResult rest = DeleteResultFiles(ExistingResultFiles(project));
        Check(rest.Deleted.size() == 1 && rest.Failed.empty() && ExistingResultFiles(project).empty(),
            "027 FR-002: once writable it is deleted, and then no result files remain");
        _wrmdir(paths[1].c_str());
        _wremove(project.c_str());
    }

    void CheckFeature028()
    {
        std::cout << "\n== feature 028: latency and bus load analysis ==\n";
        auto writeLog = [](const std::string& name, const std::string& text) {
            const std::string path = Join(g_tempDir, name);
            std::ofstream(path.c_str(), std::ios::binary) << text;
            return Widen(path);
        };
        std::vector<LogRecord> records;
        std::string error;

        // Reading: columns found by name (shuffled order, an extra column), empty cells
        std::string latencyLog = "Status,Extra,CAN_ID,Tx_Node,Rx_Node,Tx_Request,Tx_Start,Tx_End,Rx_End\r\n"
            "SUCCESS,x,0x080,1,2,0,0,250,100\r\n"
            "SUCCESS,x,0x080,1,3,0,0,250,200\r\n"
            "OVERWRITE,x,0x100,1,,500,,,\r\n";
        for (int k = 1; k <= 20; ++k)
        {
            std::ostringstream row;
            row << (k % 2 ? "SUCCESS" : "ARBITRATION_LOST_RETRY") << ",x,0x200,2,1," << 1000 * k << ',' << 1000 * k << ','
                << 1000 * k + 250 << ',' << 1000 * k + k << "\r\n";
            latencyLog += row.str();
        }
        latencyLog += "\r\n"; // an empty line is skipped
        const bool read = ReadLogRecords(writeLog("t028_latency.log.csv", latencyLog), records, error);
        Check(read && records.size() == 23 && records[2].Status == LogStatus::Overwrite && records[2].RxNode == 0 &&
            records[2].TxStartUs == -1 && records[0].CanId == 0x080 && records[3].Status == LogStatus::Success &&
            records[4].Status == LogStatus::RetrySuccess,
            "028 FR-002: ReadLogRecords finds the columns by name; empty cells give RxNode 0 and time -1");

        bool rejected = true;
        const char* bad[] = {
            "CAN_ID,Tx_Node,Rx_Node,Tx_Request,Tx_Start,Tx_End,Status\r\n",                                // no Rx_End
            "CAN_ID,Tx_Node,Rx_Node,Tx_Request,Tx_Start,Tx_End,Rx_End,Status\r\n0x080,1,2,0,0,250,100,FOO\r\n", // bad status
            "CAN_ID,Tx_Node,Rx_Node,Tx_Request,Tx_Start,Tx_End,Rx_End,Status\r\n0x080,1,2,abc,0,250,100,SUCCESS\r\n",
        };
        const char* lines[] = { "line 1:", "line 2:", "line 2:" };
        for (int i = 0; i < 3; ++i)
        {
            std::vector<LogRecord> r;
            std::string e;
            rejected = rejected && !ReadLogRecords(writeLog("t028_bad.log.csv", bad[i]), r, e) && e.find(lines[i]) == 0;
        }
        std::vector<LogRecord> none;
        rejected = rejected && !ReadLogRecords(Widen(Join(g_tempDir, "t028_missing.log.csv")), none, error);
        Check(rejected, "028 US3: a missing column, a bad Status or number, or a missing file is an error with the line number");

        // Latency table
        const LatencyAnalysis lat = AnalyzeLatency(records);
        const bool rowsOk = lat.Rows.size() == 4 &&
            lat.Rows[0].TxNode == 0 && lat.Rows[1].TxNode == 1 && lat.Rows[1].RxNode == 2 &&
            lat.Rows[2].TxNode == 1 && lat.Rows[2].RxNode == 3 && lat.Rows[3].TxNode == 2 && lat.Rows[3].RxNode == 1;
        Check(rowsOk && lat.OverwriteCount == 1,
            "028 FR-003 (Q1): rows are all, then (sender, receiver) pairs in order; OVERWRITE is counted apart");
        Check(rowsOk && lat.Rows[3].Count == 20 && lat.Rows[3].MinUs == 1 && lat.Rows[3].MaxUs == 20 &&
            std::fabs(lat.Rows[3].MeanUs - 10.5) < 1e-9 && lat.Rows[3].P95Us == 19 &&
            lat.Rows[1].Count == 1 && lat.Rows[1].MinUs == 100 && lat.Rows[1].P95Us == 100 &&
            lat.Rows[0].Count == 22 && lat.Rows[0].MinUs == 1 && lat.Rows[0].MaxUs == 200 &&
            std::fabs(lat.Rows[0].MeanUs - 510.0 / 22.0) < 1e-9 && lat.Rows[0].P95Us == 100,
            "028 FR-003 (Q2): Rx_End - Tx_Request; count, min, mean, max and nearest-rank P95 match the hand calculation");

        // Histogram
        bool hist = MakeHistogram({}).Counts.empty();
        const Histogram same = MakeHistogram({ 7, 7, 7, 7, 7 });
        hist = hist && same.Counts.size() == 1 && same.Counts[0] == 5 && same.StartUs == 7;
        std::vector<long long> thousand;
        for (int v = 1; v <= 1000; ++v)
            thousand.push_back(v);
        const Histogram h = MakeHistogram(thousand);
        long long w = h.WidthUs;
        while (w % 10 == 0)
            w /= 10;
        long long sum = 0;
        for (long long c : h.Counts)
            sum += c;
        hist = hist && (w == 1 || w == 2 || w == 5) && h.Counts.size() >= 10 && h.Counts.size() <= 50 && sum == 1000 &&
            h.StartUs <= 1 && h.StartUs + h.WidthUs * static_cast<long long>(h.Counts.size()) > 1000;
        Check(hist, "028 FR-004: histogram bins have a round width, 10..50 bins, and the counts add up (empty / one value too)");

        // Bus load: one transmission [0, 250), one seen by two receivers [9900, 10150)
        std::vector<LogRecord> busRecords;
        const std::string busLog = "CAN_ID,Tx_Node,Rx_Node,Tx_Request,Tx_Start,Tx_End,Rx_End,Status\r\n"
            "0x080,1,2,0,0,250,250,SUCCESS\r\n"
            "0x100,2,1,9800,9900,10150,10150,ARBITRATION_LOST_RETRY\r\n"
            "0x100,2,3,9800,9900,10150,10150,ARBITRATION_LOST_RETRY\r\n"
            "0x300,3,,9000,,,,OVERWRITE\r\n";
        const bool busRead = ReadLogRecords(writeLog("t028_bus.log.csv", busLog), busRecords, error);
        const BusLoadAnalysis bus = AnalyzeBusLoad(busRecords);
        Check(busRead && bus.TransmissionCount == 2 && bus.RangeUs == 20000 && bus.SectionPct.size() == 2 &&
            std::fabs(bus.SectionPct[0] - 3.5) < 1e-9 && std::fabs(bus.SectionPct[1] - 1.5) < 1e-9 &&
            std::fabs(bus.AveragePct - 2.5) < 1e-9 && std::fabs(bus.MaxPct - 3.5) < 1e-9 && bus.MaxSection == 0,
            "028 FR-006: a transmission seen by two receivers counts once, split at the 10 ms border; average = busy / range");
        Check(bus.Ecus.size() == 2 && bus.Ecus[0].Node == 1 && bus.Ecus[1].Node == 2 && bus.Ecus[1].FrameCount == 1 &&
            std::fabs(bus.Ecus[0].UsagePct + bus.Ecus[1].UsagePct - bus.AveragePct) < 1e-9,
            "028 SC-003: per-ECU usage adds up to the average load");

        // Graph helpers
        const std::vector<double> ticks = NiceTicks(0, 100, 5);
        const std::vector<double> cols = MaxPerColumn({ 1, 5, 2, 8, 3, 1 }, 3);
        Check(ticks == std::vector<double>({ 0, 20, 40, 60, 80, 100 }) && cols == std::vector<double>({ 5, 8, 3 }) &&
            MaxPerColumn({ 1, 2 }, 3) == std::vector<double>({ 1, 2 }),
            "028: NiceTicks gives round ticks; MaxPerColumn keeps the peak of each column");

        // 16 ECU vehicle sample, 5 s
        const std::string prefix = Join(g_tempDir, "t028_vehicle16");
        std::vector<LogRow> rows;
        bool sample = RunProjectToCsv(Join(g_sampleDir, "sample_vehicle_16ecu.canprj"), prefix, 5000000LL, &rows);
        std::vector<LogRecord> vehicle;
        sample = sample && ReadLogRecords(Widen(prefix) + L".log.csv", vehicle, error);
        const LatencyAnalysis vl = AnalyzeLatency(vehicle);
        long long receives = 0;
        for (const LogRecord& r : vehicle)
            if (r.RxNode > 0 && r.Status != LogStatus::Overwrite)
                ++receives;
        bool counts = sample && !vl.Rows.empty() && vl.Rows[0].Count == receives && receives > 0;
        long long pairSum = 0;
        for (std::size_t i = 0; counts && i < vl.Rows.size(); ++i)
        {
            const Histogram hh = MakeHistogram(vl.Rows[i].Values);
            long long s = 0;
            for (long long c : hh.Counts)
                s += c;
            counts = counts && s == vl.Rows[i].Count;
            if (i > 0)
                pairSum += vl.Rows[i].Count;
        }
        counts = counts && pairSum == receives;
        const BusLoadAnalysis vb = AnalyzeBusLoad(vehicle);
        double ecuSum = 0.0, sectionMean = 0.0;
        for (const EcuBusUsage& e : vb.Ecus)
            ecuSum += e.UsagePct;
        for (double p : vb.SectionPct)
            sectionMean += p;
        sectionMean /= static_cast<double>(vb.SectionPct.size());
        std::ostringstream what;
        what << "028 SC-001/SC-002/SC-003: 16 ECU vehicle sample (" << receives << " receives, average load "
             << vb.AveragePct << " %): counts, histogram totals and load sums agree";
        Check(counts && vb.AveragePct > 0.0 && std::fabs(ecuSum - vb.AveragePct) < 1e-6 &&
            std::fabs(sectionMean - vb.AveragePct) < 1e-6, what.str());
    }

    void CheckFeature029()
    {
        std::cout << "\n== feature 029: transmit wait time and transmit jitter ==\n";
        auto readLog = [](const std::string& name, const std::string& text, std::vector<LogRecord>& records) {
            const std::string path = Join(g_tempDir, name);
            std::ofstream(path.c_str(), std::ios::binary) << text;
            std::string error;
            return ReadLogRecords(Widen(path), records, error);
        };
        const std::string header = "CAN_ID,Tx_Node,Rx_Node,Tx_Request,Tx_Start,Tx_End,Rx_End,Status\r\n";

        // One transmission seen by two receivers, another one, and a discarded row
        std::vector<LogRecord> rec;
        bool ok = readLog("t029_collect.log.csv", header +
            "0x200,2,1,100,150,400,400,SUCCESS\r\n"
            "0x100,1,2,0,0,250,250,SUCCESS\r\n"
            "0x100,1,3,0,0,250,250,SUCCESS\r\n"
            "0x300,3,,500,,,,OVERWRITE\r\n", rec);
        long long overwrites = 0;
        const std::vector<Transmission> tx = CollectTransmissions(rec, overwrites);
        Check(ok && tx.size() == 2 && overwrites == 1 && tx[0].CanId == 0x100 && tx[1].CanId == 0x200 && tx[1].TxStartUs == 150,
            "029 R1: one transmission per (Tx_Node, CAN_ID, Tx_Start), sorted by CAN ID; OVERWRITE counted apart");

        // Wait time: 0x100 waits 0 / 10 / 20, 0x200 waits 5
        std::vector<LogRecord> waitRec;
        ok = readLog("t029_wait.log.csv", header +
            "0x200,2,1,1000,1005,1250,1250,SUCCESS\r\n"
            "0x100,1,2,0,0,250,250,SUCCESS\r\n"
            "0x100,1,3,0,0,250,250,SUCCESS\r\n"
            "0x100,1,2,10000,10010,10260,10260,ARBITRATION_LOST_RETRY\r\n"
            "0x100,1,2,20000,20020,20270,20270,SUCCESS\r\n"
            "0x300,3,,500,,,,OVERWRITE\r\n", waitRec);
        const TxWaitAnalysis wait = AnalyzeTxWait(waitRec);
        const bool waitRows = ok && wait.Rows.size() == 3 && wait.Rows[0].All && wait.Rows[1].CanId == 0x100 &&
            wait.Rows[1].TxNode == 1 && wait.Rows[2].CanId == 0x200 && wait.OverwriteCount == 1;
        Check(waitRows && wait.Rows[1].Count == 3 && wait.Rows[1].MinUs == 0 && wait.Rows[1].MaxUs == 20 &&
            std::fabs(wait.Rows[1].MeanUs - 10.0) < 1e-9 && wait.Rows[1].P95Us == 20 &&
            wait.Rows[2].Count == 1 && wait.Rows[2].MinUs == 5 &&
            wait.Rows[0].Count == 4 && wait.Rows[0].Count == wait.Rows[1].Count + wait.Rows[2].Count &&
            std::fabs(wait.Rows[0].MeanUs - 35.0 / 4.0) < 1e-9,
            "029 FR-003/SC-002: Tx_Start - Tx_Request per transmission; all, then CAN ID rows; counts add up");

        // Jitter, period 10 ms: starts 0 / 10050 / 20000 / 30120 -> +50, -50, +120
        std::vector<LogRecord> jitRec;
        ok = readLog("t029_jitter.log.csv", header +
            "0x100,1,2,0,0,250,250,SUCCESS\r\n"
            "0x100,1,2,10000,10050,10300,10300,SUCCESS\r\n"
            "0x100,1,3,10000,10050,10300,10300,SUCCESS\r\n"
            "0x100,1,2,20000,20000,20250,20250,SUCCESS\r\n"
            "0x100,1,2,30000,30120,30370,30370,ARBITRATION_LOST_RETRY\r\n"
            "0x200,2,1,0,30,280,280,SUCCESS\r\n"                         // one transmission lost:
            "0x200,2,1,20000,20010,20260,20260,SUCCESS\r\n"              // requests 20 ms apart -> -20
            "0x300,3,1,500,500,750,750,SUCCESS\r\n", jitRec);           // sent once
        const JitterAnalysis jit = AnalyzeJitter(jitRec);
        const double mean = (50.0 - 50.0 + 120.0) / 3.0;
        const double sd = std::sqrt(((50 - mean) * (50 - mean) + (-50 - mean) * (-50 - mean) + (120 - mean) * (120 - mean)) / 3.0);
        const bool jitOk = ok && jit.Rows.size() == 3 &&
            jit.Rows[0].CanId == 0x100 && jit.Rows[0].PeriodUs == 10000 && jit.Rows[0].Count == 3 &&
            jit.Rows[0].Points.size() == 3 && jit.Rows[0].Points[0].DeviationUs == 50 && jit.Rows[0].Points[0].TimeUs == 10050 &&
            jit.Rows[0].Points[1].DeviationUs == -50 && jit.Rows[0].Points[2].DeviationUs == 120 &&
            jit.Rows[0].MinUs == -50 && jit.Rows[0].MaxUs == 120 && std::fabs(jit.Rows[0].StdDevUs - sd) < 1e-9;
        Check(jitOk, "029 FR-005: jitter = start interval - request interval; period, min, max and population SD by hand");
        Check(ok && jit.Rows.size() == 3 && jit.Rows[1].Count == 1 && jit.Rows[1].Points[0].DeviationUs == -20 &&
            jit.Rows[1].PeriodUs == 20000 && jit.Rows[2].Count == 0 && jit.Rows[2].PeriodUs == 0 && jit.Rows[2].Points.empty(),
            "029 Edge Cases: a transmission lost to a full buffer is not a period of jitter; a frame sent once has no jitter");

        // Min / max per column
        const std::vector<JitterPoint> pts = { { 0, 5 }, { 10, -3 }, { 20, 7 }, { 90, 1 } };
        const std::vector<ColumnRange> c = MinMaxPerColumn(pts, 0, 99, 4);
        Check(c.size() == 4 && c[0].Has && c[0].MinUs == -3 && c[0].MaxUs == 7 && !c[1].Has && !c[2].Has &&
            c[3].Has && c[3].MinUs == 1 && c[3].MaxUs == 1 && MinMaxPerColumn(pts, 0, 99, 0).empty(),
            "029 R5: MinMaxPerColumn keeps both the dip and the peak of each column");

        // 16 ECU vehicle sample, 5 s: counts, and the jitter of the summary CSV
        const std::string prefix = Join(g_tempDir, "t029_vehicle16");
        bool sample = RunProjectToCsv(Join(g_sampleDir, "sample_vehicle_16ecu.canprj"), prefix, 5000000LL);
        std::vector<LogRecord> vehicle;
        std::string error;
        sample = sample && ReadLogRecords(Widen(prefix) + L".log.csv", vehicle, error);
        const TxWaitAnalysis vw = AnalyzeTxWait(vehicle);
        const BusLoadAnalysis vb = AnalyzeBusLoad(vehicle);
        bool counts = sample && !vw.Rows.empty() && vw.Rows[0].Count == vb.TransmissionCount && vb.TransmissionCount > 0;
        long long idSum = 0;
        for (std::size_t i = 0; counts && i < vw.Rows.size(); ++i)
        {
            const Histogram hh = MakeHistogram(vw.Rows[i].Values);
            long long s = 0;
            for (long long n : hh.Counts)
                s += n;
            counts = counts && s == vw.Rows[i].Count;
            if (i > 0)
                idSum += vw.Rows[i].Count;
        }
        counts = counts && idSum == vw.Rows[0].Count;
        std::ostringstream what;
        what << "029 SC-001/SC-002: 16 ECU vehicle sample (" << vw.Rows[0].Count << " transmissions): counts and histogram totals agree";
        Check(counts, what.str());

        // the summary CSV: ID rows, Jitter_Max_us / Jitter_Min_us / Jitter_StdDev_us (2 decimals)
        std::ifstream summary((prefix + ".summary.csv").c_str());
        std::string line;
        std::getline(summary, line);
        std::vector<std::string> names;
        {
            std::istringstream hs(line);
            std::string cell;
            while (std::getline(hs, cell, ','))
                names.push_back(cell);
        }
        auto column = [&names](const char* name) {
            for (std::size_t i = 0; i < names.size(); ++i)
                if (names[i] == name)
                    return static_cast<int>(i);
            return -1;
        };
        const int cType = column("GroupType"), cKey = column("GroupKey"), cMax = column("Jitter_Max_us"),
            cMin = column("Jitter_Min_us"), cSd = column("Jitter_StdDev_us");
        const JitterAnalysis vj = AnalyzeJitter(vehicle);
        int compared = 0;
        bool same = sample && cType >= 0 && cKey >= 0 && cMax >= 0 && cMin >= 0 && cSd >= 0;
        while (same && std::getline(summary, line))
        {
            if (!line.empty() && line.back() == '\r')
                line.pop_back();
            std::vector<std::string> cells;
            std::istringstream ls(line);
            std::string cell;
            while (std::getline(ls, cell, ','))
                cells.push_back(cell);
            if (static_cast<int>(cells.size()) <= cSd || cells[static_cast<std::size_t>(cType)] != "ID" ||
                cells[static_cast<std::size_t>(cMax)].empty())
                continue;
            const unsigned id = static_cast<unsigned>(std::stoul(cells[static_cast<std::size_t>(cKey)], nullptr, 16));
            const JitterRow* row = nullptr;
            for (const JitterRow& r : vj.Rows)
                if (r.CanId == id)
                    row = &r;
            same = row != nullptr && row->Count > 0 &&
                std::fabs(std::stod(cells[static_cast<std::size_t>(cMax)]) - row->MaxUs) < 0.006 &&
                std::fabs(std::stod(cells[static_cast<std::size_t>(cMin)]) - row->MinUs) < 0.006 &&
                std::fabs(std::stod(cells[static_cast<std::size_t>(cSd)]) - row->StdDevUs) < 0.006;
            ++compared;
        }
        std::ostringstream what2;
        what2 << "029 SC-003: the jitter of all " << compared << " CAN IDs matches the summary CSV (max, min, SD to 2 decimals)";
        Check(same && compared == static_cast<int>(vj.Rows.size()) && compared > 0, what2.str());

        // Revision 1: reading in 4 MB chunks with progress and cancel (a 60 s log, about 20 MB)
        const std::string bigPrefix = Join(g_tempDir, "t029_vehicle16_60s");
        bool big = RunProjectToCsv(Join(g_sampleDir, "sample_vehicle_16ecu.canprj"), bigPrefix, 60000000LL);
        const std::wstring bigLog = Widen(bigPrefix) + L".log.csv";
        long long lines = 0;
        std::string lastLine;
        {
            std::ifstream in((bigPrefix + ".log.csv").c_str(), std::ios::binary);
            std::string l;
            while (std::getline(in, l))
            {
                if (!l.empty() && l.back() == '\r')
                    l.pop_back();
                if (!l.empty())
                {
                    ++lines;
                    lastLine = l;
                }
            }
        }
        int calls = 0;
        long long lastDone = -1, lastTotal = -2;
        std::vector<LogRecord> bigRecords;
        big = big && ReadLogRecords(bigLog, bigRecords, error, [&](long long done, long long total) {
            ++calls;
            lastDone = done;
            lastTotal = total;
            return true;
        });
        // the last line: "<Timestamp>,0x<id>,<dlc>,<tx>,<rx>,<request>,<start>,..."
        bool lastOk = big && !bigRecords.empty();
        if (lastOk)
        {
            std::vector<std::string> cells;
            std::istringstream ls(lastLine);
            std::string cell;
            while (std::getline(ls, cell, ','))
                cells.push_back(cell);
            const LogRecord& lr = bigRecords.back();
            lastOk = cells.size() > 6 && std::stoul(cells[1], nullptr, 16) == lr.CanId && std::stoi(cells[3]) == lr.TxNode &&
                std::stoll(cells[5]) == lr.TxRequestUs && std::stoll(cells[6]) == lr.TxStartUs;
        }
        std::ostringstream what3;
        what3 << "029 revision 1: a " << lastTotal / (1024 * 1024) << " MB log is read in chunks (" << calls
              << " progress calls) with the same rows and last row as line by line";
        Check(big && lastTotal > 4 * 1024 * 1024 && calls >= 2 && lastDone == lastTotal &&
            static_cast<long long>(bigRecords.size()) == lines - 1 && lastOk, what3.str());

        std::vector<LogRecord> cancelled;
        std::string cancelError;
        const bool stopped = !ReadLogRecords(bigLog, cancelled, cancelError, [](long long, long long) { return false; });
        Check(stopped && cancelError == kLogReadCancelled && cancelled.empty(),
            "029 revision 1: returning false from the progress stops reading (kLogReadCancelled, no rows)");
        Check(AnalyzeTxWait(bigRecords).Rows[0].Count == AnalyzeBusLoad(bigRecords).TransmissionCount,
            "029 revision 1: the faster CollectTransmissions keeps the transmission count of the bus load");
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
    CheckFeature019();
    CheckFeature020();
    CheckFeature022();
    CheckFeature023();
    CheckFeature024();
    CheckFeature025();
    CheckFeature026();
    CheckFeature027();
    CheckFeature028();
    CheckFeature029();

    std::cout << "\n" << g_failures << " check(s) failed.\n";
    return g_failures == 0 ? 0 : 1;
}
