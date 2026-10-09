// feature_002_tests.cpp（tests）
// tools/cansim_runner/checks.cpp の CheckFeature002() から移した（機能 030）。
// 本体は、パスの取り方以外は変えていない。Check() のラベルには SC/FR の ID が残っている。

#include "test_helpers.h"

using namespace cansimtest;

TEST(Feature002, SC001to005_FR007to010_FR013)
{
    std::cout << "\n== 機能 002: ノードの起動制御 ==\n";
    const std::string startupCfg = Join(SampleDir(), "sample_startup.canprj");

    // SC-001 / SC-003: 電源投入 + 初期化 + 11 ビット時間（500 kbps で 22 us）
    RunResult run = RunConfigFile(startupCfg, 1000000LL);
    Check(run.Loaded, "sample_startup.canprj を読み込める");
    if (!run.Loaded)
    {
        std::cout << "  エラー: " << run.Error << "\n";
        return;
    }
    bool allMatch = true;
    for (const Node& n : run.EndState.Nodes)
    {
        const long long expected = n.PowerOnTimeUs + n.InitDurationUs + 22;
        if (n.NormalEntryUs != expected)
        {
            allMatch = false;
            std::cout << "  ノード " << n.NodeId << " Normal_Entry=" << n.NormalEntryUs
                      << " 期待値 " << expected << "\n";
        }
    }
    Check(allMatch, "SC-001 / SC-003: すべての Normal_Entry_us が 電源投入 + 初期化 + 22 us に等しい");

    // SC-002: 送信ノードが Normal になる前には何も送信されない
    bool beforeNormal = false;
    for (const LogRow& row : run.Rows)
    {
        const Node* tx = FindNode(run.EndState, row.TxNodeId);
        if (tx && (tx->NormalEntryUs < 0 || row.TxStartUs < tx->NormalEntryUs))
            beforeNormal = true;
    }
    Check(!beforeNormal, "SC-002: ノードが Normal になる前にフレームが始まらない");

    // FR-010: 起動中に発生した送信要求は元の要求時刻を保つ
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
        std::cout << "  ノード 6 の最初のフレーム: Tx_Request=" << firstRequest
                  << " Tx_Start=" << firstStart << "\n";
        // 機能 006 FR-015（ドメイン仕様 FR-020a）: 送信要求は Normal に入った
        // 時点から数えるので、ノード 6（電源投入 50 ms）は 0 で発生した要求を
        // 保留せず、Normal に入った時点で最初の要求を出す。
        Check(late != nullptr && firstRequest == late->NormalEntryUs && firstStart == late->NormalEntryUs,
            "006 FR-015（旧 002 FR-010）: 遅れて起動したノードの最初の要求は Normal に入った時点で発生する");
    }

    // SC-005: 起動記録を含む再現性
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
        Check(same, "SC-005: 同じ条件の 2 回の実行は同じログと起動記録になる");
    }

    // FR-013: 実行の終了後に電源が入るノードは Normal に達せず、何も送らない
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
        const std::string path = Join(TempDir(), "late_node.cfg");
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
            "FR-013: 実行の終了後に電源が入るノードは Normal に達しない");
        Check(txRows == 0 && rxRows == 0,
            "FR-008 / FR-009: Normal に達しないノードは送信も受信もしない");
    }

    // SC-004 / FR-007: すでに使用中のバスへの参加
    {
        const std::string path = Join(SampleDir(), "sample_startup_latejoin.canprj");
        RunResult lj = RunConfigFile(path, 1000000LL);
        Check(lj.Loaded, "sample_startup_latejoin.canprj を読み込める");
        if (lj.Loaded)
        {
            const Node* a = FindNode(lj.EndState, 1);
            const Node* b = FindNode(lj.EndState, 2);
            const Node* c = FindNode(lj.EndState, 3);
            std::cout << "  Normal_Entry: ノード 1=" << (a ? a->NormalEntryUs : -1)
                      << " ノード 2=" << (b ? b->NormalEntryUs : -1)
                      << " ノード 3=" << (c ? c->NormalEntryUs : -1) << "\n";
            Check(a && a->NormalEntryUs == 22, "ノード 1 は空いているバスで 22 us に同期する");
            Check(b && b->NormalEntryUs == 280,
                "SC-004: フレームの途中で参加するノードはインターミッションの終わり（280 us）に同期する");
            // 機能 006: ノード 1 の 2 回目の要求は 300 us から 322 us
            // （Normal 22 + 周期 300）に移り、ノード 3 の電源投入は 305 us になった。
            // そのため待ち時間の窓（305 + 22 = 327）がフレーム 322 -> 574 で
            // 切られ、574 - 16 + 22 = 580 us に同期する。
            Check(c && c->NormalEntryUs == 580,
                "FR-007: 新しいフレームに中断されたノードはやり直して 580 us に同期する");
        }
    }

    // 機能 006 FR-015: ノードが Normal になる前には送信要求が発生しないので、
    // 起動待ちの間に何も溜まらない（002 で起動中に 5 回上書きされたケースは
    // 起きなくなった）。
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
        const std::string path = Join(TempDir(), "overwrite.cfg");
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
        std::cout << "  ノード 6 の最初のフレームの Overwrite_Count=" << firstOverwrite << "\n";
        Check(firstOverwrite == 0,
            "006 FR-015: 起動待ちのノードにはまだ要求が無いので Overwrite_Count は 0");
    }
}
