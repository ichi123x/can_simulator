// feature_006_tests.cpp（tests）
// tools/cansim_runner/checks.cpp の CheckFeature006() から移した（機能 030）。
// 本体は、パスの取り方以外は変えていない。Check() のラベルには SC/FR の ID が残っている。

#include "test_helpers.h"

using namespace cansimtest;

TEST(Feature006, SC003_SC005_SC007_FR006_FR009_FR012to015)
{
    std::cout << "\n== 機能 006: データフレーム編集 ==\n";

    ConfigLoadResult a = LoadConfig(Widen(Join(SampleDir(), "sample_appendix_a.canprj")));
    Check(a.Success, "sample_appendix_a.canprj を読み込める");
    if (!a.Success)
        return;

    // 読込み: データは常に Dlc 個の 0xFF、名前は読む（FR-014, FR-012）
    {
        bool allFF = true;
        for (const Node& n : a.Config.Nodes)
            for (const FrameDefinition& f : n.Frames)
                allFF = allFF && f.Data == MakeFrameData(f.Dlc);
        const Node* ecu1 = FindNode(a.Config, 1);
        Check(allFF, "006 FR-014: すべてのフレームのデータは Dlc バイトの 0xFF");
        Check(ecu1 && ecu1->Frames.size() == 2 && !ecu1->Frames[0].Name.empty(),
            "006 FR-012: フレーム名はプロジェクトから読まれる");

        ConfigLoadResult old = LoadText("f_olddata.canprj",
            "[Node1]\r\nNodeId=1\r\n[Node1.Frame1]\r\nCanId=0x10\r\nDlc=2\r\nData=00,01\r\nPeriodUs=1000\r\n");
        Check(old.Success && old.Config.Nodes[0].Frames[0].Data == MakeFrameData(2),
            "006 FR-014: 古い Data=00,01 の行は無視され、データは 0xFF 0xFF になる");
    }

    // 読込み: 拒否（FR-013、オフセット < 周期、名前の長さ）
    {
        std::ostringstream big;
        big << "[Node1]\r\nNodeId=1\r\n";
        for (int i = 1; i <= 101; ++i)
            big << "[Node1.Frame" << i << "]\r\nCanId=0x" << std::hex << (0x100 + i) << std::dec
                << "\r\nDlc=0\r\nPeriodUs=1000\r\n";
        ConfigLoadResult r101 = LoadText("f_101.canprj", big.str());
        Check(!r101.Success, "006 FR-013: 101 フレームのノードは拒否される");

        ConfigLoadResult off = LoadText("f_offset.canprj",
            "[Node1]\r\nNodeId=1\r\n[Node1.Frame1]\r\nCanId=0x10\r\nDlc=0\r\nPeriodUs=1000\r\nInitialOffsetUs=1000\r\n");
        Check(!off.Success, "006: InitialOffsetUs が PeriodUs と等しいと拒否される");

        ConfigLoadResult longName = LoadText("f_name.canprj",
            "[Node1]\r\nNodeId=1\r\n[Node1.Frame1]\r\nName=abcdefghijabcdefghijabcdefghijabc\r\nCanId=0x10\r\nDlc=0\r\nPeriodUs=1000\r\n");
        Check(!longName.Success, "006: 33 文字のフレーム名は拒否される");
    }

    // ValidateFrame（FR-006）
    {
        FrameDefinition out;
        std::vector<FrameIssue> ok = ValidateFrame(a.Config, 1, -1, MakeInput("0x200", "4", "20", ""), out);
        Check(ok.empty() && out.CanId == 0x200 && out.Dlc == 4 && out.PeriodUs == 20000 &&
                  out.InitialOffsetUs == 0 && out.Data == MakeFrameData(4),
            "006 FR-006: 正しい入力は通る（オフセット空 = 0、ms -> us、データ 0xFF）");
        Check(ValidateFrame(a.Config, 1, -1, MakeInput("200", "4", "20", "5"), out).empty() && out.InitialOffsetUs == 5000,
            "006 FR-006: ID は 0x なしで入力してもよい");

        Check(HasIssue(ValidateFrame(a.Config, 1, -1, MakeInput("0x800", "4", "20", ""), out), FrameField::Id, FrameProblem::OutOfRange),
            "006 FR-006: ID 0x800 は範囲外");
        Check(HasIssue(ValidateFrame(a.Config, 1, -1, MakeInput("xyz", "4", "20", ""), out), FrameField::Id, FrameProblem::NotANumber),
            "006 FR-006: ID 'xyz' は数値ではない");
        Check(HasIssue(ValidateFrame(a.Config, 1, -1, MakeInput("0x200", "9", "20", ""), out), FrameField::Dlc, FrameProblem::OutOfRange),
            "006 FR-006: DLC 9 は範囲外");
        Check(HasIssue(ValidateFrame(a.Config, 1, -1, MakeInput("0x200", "4", "0", ""), out), FrameField::Period, FrameProblem::OutOfRange),
            "006 FR-006: 周期 0 ms は範囲外");
        Check(HasIssue(ValidateFrame(a.Config, 1, -1, MakeInput("0x200", "4", "20", "20"), out), FrameField::Offset, FrameProblem::OffsetNotBelowPeriod),
            "006 FR-006: 周期と等しいオフセットは拒否される");

        FrameInput longName = MakeInput("0x200", "4", "20", "");
        longName.NameChars = 33;
        Check(HasIssue(ValidateFrame(a.Config, 1, -1, longName, out), FrameField::Name, FrameProblem::TooLong),
            "006 FR-006: 33 文字の名前は拒否される");

        // 0x080 は ECU3 のもの
        std::vector<FrameIssue> dup = ValidateFrame(a.Config, 1, -1, MakeInput("0x080", "4", "20", ""), out);
        Check(HasIssue(dup, FrameField::Id, FrameProblem::Duplicate) && dup[0].DuplicateNodeId == 3,
            "006 FR-006: 別のノードが使っている ID は重複になる（そのノードを報告する）");
        // ECU1 の最初のフレームを編集しても、自分自身の ID とは重複しない
        Check(ValidateFrame(a.Config, 1, 0, MakeInput("0x100", "8", "10", ""), out).empty(),
            "006 FR-006: フレームの編集は自分自身と衝突しない");

        // ECU 数を超える ECU も自分の ID を持ち続ける
        Network defs = a.Config;
        AddFrame(defs, 16, out);  // 0x100 は ECU1 のもの。代わりに新しいフレームを使う
        defs.Nodes.back().Frames.back().CanId = 0x7F0;
        Check(HasIssue(ValidateFrame(defs, 1, -1, MakeInput("0x7F0", "0", "10", ""), out), FrameField::Id, FrameProblem::Duplicate),
            "006: ECU 数の外にある ECU の ID も重複を調べる");
    }

    // AddFrame / ReplaceFrame / DeleteFrame / EnsureNode（FR-007..FR-009）
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
            "006: 定義のない ECU に追加すると、NodeId 順にその ECU が作られる");

        FrameDefinition g = f;
        g.CanId = 0x701;
        AddFrame(defs, 7, g);
        FrameDefinition h = f;
        h.PeriodUs = 70000;
        ReplaceFrame(defs, 7, 0, h);
        DeleteFrame(defs, 7, 0);
        n7 = FindNode(defs, 7);
        Check(n7 && n7->Frames.size() == 1 && n7->Frames[0].CanId == 0x701,
            "006 FR-009: No.1 を削除すると、残ったフレームが No.1 になる");

        Network full;
        for (int i = 0; i < kMaxFramesPerNode; ++i)
        {
            FrameDefinition x;
            x.CanId = static_cast<unsigned>(i);
            x.PeriodUs = 1000;
            AddFrame(full, 1, x);
        }
        Check(HasIssue(ValidateFrame(full, 1, -1, MakeInput("0x700", "0", "10", ""), f), FrameField::Name, FrameProblem::ListFull),
            "006 FR-013: 101 件目のフレームは追加できない");
    }

    // 保存 -> 再読込みで名前と順序が保たれ、Data= は書かれない（SC-003）
    {
        Network defs = a.Config;
        FrameDefinition f;
        ValidateFrame(defs, 2, -1, MakeInput("0x210", "3", "25", "7", "added"), f);
        AddFrame(defs, 2, f);
        ProjectSettings s = a.Project;
        const std::string path = Join(TempDir(), "f_roundtrip.canprj");
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
            "006 SC-003: 保存して読み直すとすべての一覧（名前・順序）が保たれ、Data= の行は書かれない");
    }

    // 最初の要求 = Normal に入った時刻 + オフセット（FR-015, SC-005, SC-007）
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
        std::cout << "  Normal_Entry=" << normal << " 最初の 0x100=" << (req100.empty() ? -1 : req100[0])
                  << " 最初の 0x101=" << (req101.empty() ? -1 : req101[0]) << "\n";
        Check(late.Success && normal == 50022 && !req100.empty() && req100[0] == normal,
            "006 SC-007: オフセット 0 -> 最初の要求は Normal に入った時刻（50 ms + 22 us）");
        Check(!req101.empty() && req101[0] == normal + 5000 && req101.size() > 1 && req101[1] == normal + 25000,
            "006 FR-015: オフセット 5 ms -> Normal + 5 ms、以後は周期ごと（+20 ms）");
        // (200 ms - 50.022 ms) / 20 ms -> それぞれ 8 回の要求
        Check(req100.size() == 8 && req101.size() == 8, "006 SC-005: 要求の回数は Normal に入った後の周期に従う");
    }
}
