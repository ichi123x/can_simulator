// 機能 004（プロジェクトファイルとセッティング）の補足テスト（/coverage で、根拠のある未カバー行に足したもの）
// 根拠: specs/004-menu-project-settings/contracts/project-file-format.md、ProjectFile.h のコメント。
// メニュー・ダイアログは画面側なのでテストしない。

#include <gtest/gtest.h>

#include <set>
#include <string>
#include <vector>

#include "ProjectFile.h"
#include "support/test_helpers.h"

using namespace CanSim;
using namespace TestHelpers;

// ---------------------------------------------------------------------------
// 契約「検証」: EcuCount は 0〜16、DurationSec は 1〜600、FrameDisplay は 0 または 1 の整数。違反は読み込まず、
//   キーと理由を含むメッセージを返す。境界の値は読める。
// 根拠: contracts/project-file-format.md「検証」、FR-010。
// ---------------------------------------------------------------------------
TEST(F004Edge, F004_FR010_ProjectKeysOutOfRange_RejectedWithKeyName)
{
    RecordProperty("spec", "004:FR-010");
    const struct
    {
        const char* Key;
        const char* Value;
    } bad[] = {{"EcuCount", "17"}, {"EcuCount", "-1"}, {"EcuCount", "six"}, {"DurationSec", "0"},
               {"DurationSec", "601"}, {"FrameDisplay", "2"}};
    for (const auto& b : bad)
    {
        const ConfigLoadResult r =
            LoadFromText("f004_bad.canprj", std::string("[Project]\n") + b.Key + "=" + b.Value + "\n");
        EXPECT_FALSE(r.Success) << b.Key << "=" << b.Value << " が受け入れられた";
        EXPECT_NE(std::string::npos, r.ErrorMessage.find(b.Key)) << "メッセージにキーが無い: " << r.ErrorMessage;
    }
    const ConfigLoadResult lo = LoadFromText("f004_lo.canprj", "[Project]\nEcuCount=0\nDurationSec=1\nFrameDisplay=0\n");
    ASSERT_TRUE(lo.Success) << lo.ErrorMessage;
    EXPECT_EQ(0, lo.Project.EcuCount);
    EXPECT_EQ(1, lo.Project.DurationSec);
    const ConfigLoadResult hi = LoadFromText("f004_hi.canprj", "[Project]\nEcuCount=16\nDurationSec=600\nFrameDisplay=1\n");
    ASSERT_TRUE(hi.Success) << hi.ErrorMessage;
    EXPECT_EQ(16, hi.Project.EcuCount);
    EXPECT_EQ(600, hi.Project.DurationSec);
}

// ---------------------------------------------------------------------------
// 契約「実行時の解釈」: 保存はこの形式を書き出し、読み込み直すとノードの設定（受信フィルタ・受信処理遅延・
//   電源投入・初期化時間）と、EcuCount を超える番号の定義も保たれる。
// 根拠: contracts/project-file-format.md（[NodeN] は 001 と同じキー、EcuCount を超える定義は保存で保持）、
//        ProjectFile.h（SaveProject: settings + every node/frame definition）。
// ---------------------------------------------------------------------------
TEST(F004Edge, F004_FR008_SaveReload_NodeSettingsAndExtraDefinitionsKept)
{
    RecordProperty("spec", "004:FR-008");
    Network defs;
    defs.BitRateBps = 250000;
    Node a = MakeNode(1, {{0x100, 8, 10000, 0}});
    a.RxFilterAll = false;
    a.RxFilter = {0x080, 0x7FF};
    a.ReceiveDelayUs = 40;
    a.PowerOnTimeUs = 5000;
    a.InitDurationUs = 1000;
    Node extra = MakeNode(5, {{0x500, 2, 20000, 100}});
    defs.Nodes = {a, extra};
    ProjectSettings s;
    s.EcuCount = 3;
    s.DurationSec = 42;
    const std::wstring path = TempPath("f004_fr008.canprj");
    ASSERT_TRUE(SaveProject(path, s, defs));
    const ConfigLoadResult r = LoadConfig(path);
    DeleteFileW(path.c_str());
    ASSERT_TRUE(r.Success) << r.ErrorMessage;
    EXPECT_EQ(3, r.Project.EcuCount);
    EXPECT_EQ(42, r.Project.DurationSec);
    EXPECT_EQ(250000, r.Config.BitRateBps);
    ASSERT_EQ(2u, r.Config.Nodes.size()) << "EcuCount を超える ECU 5 の定義も保存される";
    const Node& b = r.Config.Nodes[0];
    EXPECT_FALSE(b.RxFilterAll) << "受信フィルタ";
    EXPECT_EQ((std::set<unsigned>{0x080, 0x7FF}), b.RxFilter);
    EXPECT_EQ(40, b.ReceiveDelayUs);
    EXPECT_EQ(5000, b.PowerOnTimeUs);
    EXPECT_EQ(1000, b.InitDurationUs);
    EXPECT_EQ(5, r.Config.Nodes[1].NodeId);
    EXPECT_TRUE(r.Config.Nodes[1].RxFilterAll) << "フィルタ未指定は全受信のまま";
}

// ProjectFile.h: 書けないときは false。
TEST(F004Edge, F004_FR008_SaveToMissingFolder_ReturnsFalse)
{
    RecordProperty("spec", "004:FR-008");
    EXPECT_FALSE(SaveProject(TempPath("f004_no_such_dir\\x\\p.canprj"), ProjectSettings(), Network()));
}

// ---------------------------------------------------------------------------
// 契約「実行時の解釈」/ ProjectFile.h（BuildRunNetwork）: 実行するのは ECU 1〜EcuCount。定義のある ECU はそのまま、
//   無い ECU はフレーム無しの "ECU<n>" を足し、EcuCount を超える番号の定義は実行しない。
// 根拠: contracts/project-file-format.md、ProjectFile.h（BuildRunNetwork のコメント）、FR-011。
// ---------------------------------------------------------------------------
TEST(F004Edge, F004_FR011_BuildRunNetwork_FillsMissingAndDropsExtra)
{
    RecordProperty("spec", "004:FR-011");
    Network defs;
    defs.BitRateBps = 125000;
    defs.Nodes = {MakeNode(2, {{0x200, 8, 10000, 0}}), MakeNode(5, {{0x500, 8, 10000, 0}})};
    ProjectSettings s;
    s.EcuCount = 3;
    const Network run = BuildRunNetwork(s, defs);
    EXPECT_EQ(125000, run.BitRateBps);
    ASSERT_EQ(3u, run.Nodes.size()) << "ECU 1〜3";
    EXPECT_EQ(1, run.Nodes[0].NodeId);
    EXPECT_EQ("ECU1", run.Nodes[0].Name) << "定義の無い ECU の名前";
    EXPECT_TRUE(run.Nodes[0].Frames.empty()) << "定義の無い ECU はフレーム無し";
    EXPECT_EQ(2, run.Nodes[1].NodeId);
    ASSERT_EQ(1u, run.Nodes[1].Frames.size()) << "定義のある ECU はそのまま";
    EXPECT_EQ(0x200u, run.Nodes[1].Frames[0].CanId);
    EXPECT_EQ(3, run.Nodes[2].NodeId);
    EXPECT_EQ("ECU3", run.Nodes[2].Name);
}
