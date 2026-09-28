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
        const std::string cfgPath = Join(g_sampleDir, "sample_appendix_a.cfg");
        const long long tenSeconds = 10 * 1000000LL;

        RunResult run1 = RunConfigFile(cfgPath, tenSeconds);
        Check(run1.Loaded, "sample_appendix_a.cfg loads");
        if (!run1.Loaded)
        {
            std::cout << "  error: " << run1.Error << "\n";
            return;
        }
        Check(run1.Config.Nodes.size() == 6, "sample_appendix_a.cfg has 6 nodes");
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
        const std::string startupCfg = Join(g_sampleDir, "sample_startup.cfg");

        // SC-001 / SC-003: power-on + init + 11 bit times (22 us at 500 kbps)
        RunResult run = RunConfigFile(startupCfg, 1000000LL);
        Check(run.Loaded, "sample_startup.cfg loads");
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
            Check(late != nullptr && firstRequest == 0 && firstStart == late->NormalEntryUs,
                "FR-010: a request queued during startup keeps Tx_Request and is sent at Normal entry");
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
            const std::string path = Join(g_sampleDir, "sample_startup_latejoin.cfg");
            RunResult lj = RunConfigFile(path, 1000000LL);
            Check(lj.Loaded, "sample_startup_latejoin.cfg loads");
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
                Check(c && c->NormalEntryUs == 558,
                    "FR-007: a node interrupted by a new frame restarts and syncs at 558 us");
            }
        }

        // FR-011 still applies while a node waits for startup: repeated periods
        // overwrite the queued frame instead of piling up.
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
            Check(firstOverwrite == 5,
                "FR-011: 5 periods elapse while node 6 waits for startup, so Overwrite_Count is 5");
        }
    }
}

int main(int argc, char** argv)
{
    if (argc < 3)
    {
        std::cout << "usage: checks.exe <SampleConfigs dir> <temp output dir>\n";
        return 2;
    }
    g_sampleDir = argv[1];
    g_tempDir = argv[2];

    CheckFeature001();
    CheckFeature002();

    std::cout << "\n" << g_failures << " check(s) failed.\n";
    return g_failures == 0 ? 0 : 1;
}
