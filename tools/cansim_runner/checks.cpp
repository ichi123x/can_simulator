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
#include "FrameEditor.h"

#include <climits>

#include <algorithm>
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
                a[i].RetryCount != b[i].RetryCount || a[i].OverwriteCount != b[i].OverwriteCount)
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
                    firstOverwrite = row.OverwriteCount;
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
                    std::string now = ReadTextFile(prefix + kind);
                    std::string before = ReadTextFile(Join(g_baselineDir, std::string(name) + kind));
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

    std::cout << "\n" << g_failures << " check(s) failed.\n";
    return g_failures == 0 ? 0 : 1;
}
