// feature_022_tests.cpp（tests）
// tools/cansim_runner/checks.cpp の CheckFeature022() から移した（機能 030）。
// 本体は、パスの取り方以外は変えていない。Check() のラベルには SC/FR の ID が残っている。

#include "test_helpers.h"

using namespace cansimtest;

TEST(Feature022, FR003to004_FR006)
{
    std::cout << "\n== 機能 022: ECU ラベル名 ==\n";
    // UTF-8 のテスト文字列はバイト列で書く
    const std::string engine = "\xE3\x82\xA8\xE3\x83\xB3\xE3\x82\xB8\xE3\x83\xB3";           // 4 文字
    const std::string ten = "0123456789";
    const std::string eleven = "0123456789A";
    std::string longJp;                                                                       // 14 文字
    for (int i = 0; i < 14; ++i)
        longJp += "\xE3\x81\x82";
    std::string firstTenJp;
    for (int i = 0; i < 10; ++i)
        firstTenJp += "\xE3\x81\x82";
    Check(CountUtf8Chars(engine) == 4 && CountUtf8Chars("ABC") == 3 && CountUtf8Chars("") == 0 && CountUtf8Chars(longJp) == 14,
        "022: CountUtf8Chars は全角・半角とも 1 文字と数える");
    Check(EcuLabelForDiagram(engine) == engine && EcuLabelForDiagram(ten) == ten &&
        EcuLabelForDiagram(eleven) == ten + "..." && EcuLabelForDiagram(longJp) == firstTenJp + "..." &&
        EcuLabelForDiagram("") == "",
        "022 FR-003: 10 文字まではそのまま、それより長いラベルは先頭 10 文字と ... にする");
    Check(TrimEcuLabel("  " + engine + "\t") == engine && TrimEcuLabel("\xE3\x80\x80" + engine + "\xE3\x80\x80") == engine &&
        TrimEcuLabel(" \xE3\x80\x80 ") == "" && TrimEcuLabel("a b") == "a b",
        "022: TrimEcuLabel は前後の空白・タブ・全角空白だけを取り除く");
    std::string thirtyThree;
    for (int i = 0; i < 33; ++i)
        thirtyThree += "\xE3\x81\x82";
    Check(CountUtf8Chars(LimitEcuLabel(thirtyThree)) == 32 && LimitEcuLabel(engine) == engine,
        "022: LimitEcuLabel は先頭 32 文字を残す");

    // Label= の往復。空のときは書かない。無ければ空
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
        const std::string path = Join(TempDir(), "label_roundtrip.canprj");
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
            "022 FR-006: Label= は設定されているときだけ保存され、読み戻せる。Name は変わらない");
        ConfigLoadResult old = LoadText("label_missing.canprj", "[Node1]\r\nNodeId=1\r\nName=X\r\n");
        Check(old.Success && old.Config.Nodes.size() == 1 && old.Config.Nodes[0].Label.empty(),
            "022: Label= の無いファイルのラベルは空になる");
    }

    // 縦に高くなった箱が枠の内側に収まり、バスに触れない
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
    Check(boxes, "022 FR-004: ECU の箱は高さ 48（上段 20..68、下段 152..200）で、構成図の枠の内側にある");
}
