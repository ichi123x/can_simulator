// feature_008_tests.cpp（tests）
// tools/cansim_runner/checks.cpp の CheckFeature008() から移した（機能 030）。
// 本体は、パスの取り方以外は変えていない。Check() のラベルには SC/FR の ID が残っている。

#include "test_helpers.h"

using namespace cansimtest;

TEST(Feature008, SC001to006_FR004_FR006_FR009to010)
{
    std::cout << "\n== 機能 008: フレームバッファ ==\n";
    const std::string path = Join(SampleDir(), "sample_buffer_full.canprj");
    ConfigLoadResult cfg = LoadConfig(Widen(path));
    Check(cfg.Success, "sample_buffer_full.canprj を読み込める");
    if (!cfg.Success)
        return;

    const Network net = BuildRunNetwork(cfg.Project, cfg.Config);
    SimulationEngine engine(net, 10 * 1000000LL);
    Logger logger;
    engine.Run(logger);
    const std::vector<LogRow>& rows = logger.Rows();
    const Node* ecu1 = FindNode(engine.GetNetwork(), 1);
    const Node* ecu2 = FindNode(engine.GetNetwork(), 2);

    // --- 送信側（US1）---
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
    std::cout << "  送信側の OVERWRITE 行数=" << txDropped.size()
              << " ECU1 の TxDiscardCount=" << (ecu1 ? ecu1->TxDiscardCount : -1) << "\n";
    Check(txDropped.size() == 400 && idsOk,
        "008 SC-001: 100 ms ごとに ECU1 の ID が大きい 4 つ（0x110..0x113）が捨てられる -> 400 行");
    Check(blanksOk && timesOk, "008 FR-004: 捨てられた要求は、要求時刻の行で Rx_Node・送信・受信の時刻が空欄になる");
    Check(ecu1 && ecu1->TxDiscardCount == 400, "008 FR-004: ECU1 の送信側の破棄は 400 件");

    // ECU1 の送信バッファを持つフレーム（要求 .. EOF）は最大 16 本で、
    // 要求が捨てられるときは 16 本すべてが使用中
    std::map<std::pair<long long, unsigned>, std::pair<long long, long long>> tx1; // (開始, ID) -> (要求, 終了)
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
    Check(fullAtDrop, "008 SC-002: 捨てられた送信要求のとき、ECU1 のバッファを持つフレームはちょうど 16 本だった");

    // --- 受信側（US2）---
    bool onlyEcu2 = true, rxBlank = true;
    for (const LogRow* r : rxDropped)
    {
        onlyEcu2 = onlyEcu2 && r->RxNodeId == 2;
        rxBlank = rxBlank && r->RxProcessUs < 0 && r->RxEndUs >= 0 && r->TimestampUs == r->RxEndUs;
    }
    std::cout << "  受信側の OVERWRITE 行数=" << rxDropped.size()
              << " ECU2 の RxDiscardCount=" << (ecu2 ? ecu2->RxDiscardCount : -1) << "\n";
    Check(!rxDropped.empty() && onlyEcu2 && rxBlank,
        "008 FR-006: ECU2（受信処理遅延 2 ms）はフレームを捨てる。その行は Status が OVERWRITE で Rx_Process が無い");
    Check(ecu2 && ecu2->RxDiscardCount == static_cast<int>(rxDropped.size()),
        "008 SC-003: ECU2 の受信側の破棄数は OVERWRITE の行数に等しい");

    // 受信 FIFO の使用状況: ノードが受け付けたフレームは [Rx_End, Rx_Process) の間、場所を占める
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
                    ++inUse; // このフレームが届いたときにまだ待っているフレーム
            if (r.RxProcessUs >= 0)
                rxAtMost8 = rxAtMost8 && inUse < kRxBufferCount;
            else
                fullAtRxDrop = fullAtRxDrop && inUse == kRxBufferCount;
        }
    }
    Check(rxAtMost8 && fullAtRxDrop, "008 SC-002: 受信 FIFO は 8 本を超えない。破棄は 8 本が詰まっているときだけ起きる");

    // 再現性（SC-006）
    {
        SimulationEngine again(net, 10 * 1000000LL);
        Logger logger2;
        again.Run(logger2);
        Check(RowsEqual(rows, logger2.Rows()), "008 SC-006: 2 回の実行は OVERWRITE 行を含めて同じログになる");
    }

    // 集計: ノードごとの破棄数。OVERWRITE は統計から除く（FR-009, FR-010）
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
        Check(found1 && found2, "008 FR-009: 集計に ECU1 の TxDiscard_Count 400 と ECU2 の RxDiscard_Count が出る");
        Check(idCountOk, "008 FR-010: OVERWRITE の行は統計に数えない（0x113 は一度も送られない）");
    }

    // 受信処理遅延 0 -> 受信側の破棄なし（FR-095 / SC-004）
    {
        const char* samples[] = { "sample_appendix_a", "sample_collision", "sample_startup",
                                  "sample_startup_latejoin", "sample_16ecu" };
        bool none = true;
        for (const char* name : samples)
        {
            std::vector<LogRow> r;
            RunProjectToCsv(Join(SampleDir(), std::string(name) + ".canprj"), Join(TempDir(), std::string("b8_") + name),
                10 * 1000000LL, &r);
            for (const LogRow& row : r)
                none = none && row.Status != kStatusOverwrite;
        }
        Check(none, "008 SC-004: 既存のサンプル（受信処理遅延 0、待ち 16 本以下）に OVERWRITE の行は無い");
    }

    // SC-005: sample_16ecu を基準と比べる（ほかの 4 つのサンプルは 004 SC-009 で比べる）
    if (!BaselineDir().empty())
    {
        const std::string prefix = Join(TempDir(), "b8_sample_16ecu");
        bool same = true;
        const char* kinds[] = { ".log.csv", ".summary.csv", ".startup.csv" };
        for (const char* kind : kinds)
        {
            const std::string before = ReadTextFile(Join(BaselineDir(), std::string("sample_16ecu") + kind));
            const std::string now = NormalizeToBaseline(ReadTextFile(prefix + kind), before);
            same = same && !before.empty() && now == before;
        }
        Check(same, "008 SC-005: sample_16ecu は、名前を変えた列・足した列を除いて基準と一致する");
    }
    if (BaselineDir().empty())
        std::cout << "[  注記   ] 基準 CSV との比較は省略しました（CANSIM_BASELINE_DIR を設定してください。tools/cansim_runner/README.md 参照）\n";
}
