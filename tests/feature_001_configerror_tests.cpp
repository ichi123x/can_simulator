// feature_001_configerror_tests.cpp（tests）
// 設定ファイル読み込み（LoadConfig）のエラー処理の単体テスト。
// カバレッジで実行されていなかった行（不正な設定ファイルを読んだときの処理）を通す。
// 期待するメッセージは ConfigLoader.cpp が返す文言そのもの。

#include "test_helpers.h"

using namespace cansimtest;

namespace
{
    // 正しいノード 1 つ（この後ろにキーを書き足すと、同じキーは後の値で上書きされる）
    const std::string kNode = "[Node1]\r\nNodeId=1\r\n";
    // 正しいフレームを 1 つ持つノード
    const std::string kFrame = kNode + "[Node1.Frame1]\r\nCanId=100\r\nDlc=8\r\nPeriodUs=10000\r\n";

    struct BadCase
    {
        const char* File;      // 一時ファイルの名前（失敗したときにどの行か分かるように）
        std::string Text;      // 設定ファイルの中身
        const char* Expected;  // 期待するエラーメッセージ
    };
}

// 存在しないファイルを読んだら、開けないというエラーになる
TEST(Feature001ConfigErrors, F001_MissingFile_ReportsError)
{
    ConfigLoadResult r = LoadConfig(Widen(Join(TempDir(), "no_such_file.canprj")));
    EXPECT_FALSE(r.Success) << "存在しないファイルなのに読み込めたことになっている";
    EXPECT_EQ(r.ErrorMessage, "Failed to open configuration file.");
}

// 不正な設定ファイルを読んだら、失敗して、原因を示すメッセージを返す
TEST(Feature001ConfigErrors, F001_BadConfig_ReportsError)
{
    const std::vector<BadCase> cases = {
        // 書式
        { "e_noeq.canprj", "[Network]\r\nBitRateBps\r\n",
          "Line 2: expected 'key=value' or a '[Section]' header." },
        { "e_section.canprj", "[Foo]\r\nA=1\r\n",
          "Line 2: '[Foo]' is not a recognized section (expected [Project], [Network], [NodeN] or [NodeN.FrameM])." },
        { "e_node_nonum.canprj", "[Node]\r\nA=1\r\n",
          "Line 2: '[Node]' is not a recognized section (expected [Project], [Network], [NodeN] or [NodeN.FrameM])." },
        { "e_node_alpha.canprj", "[NodeX]\r\nA=1\r\n",
          "Line 2: '[NodeX]' is not a recognized section (expected [Project], [Network], [NodeN] or [NodeN.FrameM])." },
        { "e_frame_word.canprj", "[Node1.Foo1]\r\nA=1\r\n",
          "Line 2: '[Node1.Foo1]' is not a recognized section (expected [Project], [Network], [NodeN] or [NodeN.FrameM])." },
        { "e_frame_nonum.canprj", "[Node1.Frame]\r\nA=1\r\n",
          "Line 2: '[Node1.Frame]' is not a recognized section (expected [Project], [Network], [NodeN] or [NodeN.FrameM])." },
        // 通信速度
        { "e_br_empty.canprj", "[Network]\r\nBitRateBps=\r\n",
          "[Network] BitRateBps: '' is not a valid number." },
        { "e_br_tail.canprj", "[Network]\r\nBitRateBps=500000x\r\n",
          "[Network] BitRateBps: '500000x' is not a valid number." },
        { "e_br_alpha.canprj", "[Network]\r\nBitRateBps=abc\r\n",
          "[Network] BitRateBps: 'abc' is not a valid number." },
        { "e_br_value.canprj", "[Network]\r\nBitRateBps=100000\r\n",
          "[Network] BitRateBps must be one of 125000/250000/500000/1000000." },
        // ノード
        { "e_nodeid_none.canprj", "[Node1]\r\nName=A\r\n",
          "[Node1] is missing required key 'NodeId'." },
        { "e_nodeid_17.canprj", "[Node1]\r\nNodeId=17\r\n",
          "[Node1] NodeId must be an integer between 1 and 16." },
        { "e_nodeid_dup.canprj", "[Node1]\r\nNodeId=1\r\n[Node2]\r\nNodeId=1\r\n",
          "Duplicate NodeId 1." },
        { "e_delay.canprj", kNode + "ReceiveDelayUs=abc\r\n",
          "[Node1] ReceiveDelayUs must be a non-negative integer." },
        { "e_poweron.canprj", kNode + "PowerOnTimeUs=abc\r\n",
          "[Node1] PowerOnTimeUs must be a non-negative integer." },
        { "e_init.canprj", kNode + "InitDurationUs=abc\r\n",
          "[Node1] InitDurationUs must be a non-negative integer." },
        { "e_rxfilter.canprj", kNode + "RxFilter=800\r\n",
          "[Node1] RxFilter entry '800' is not a valid 11-bit CAN ID." },
        // フレーム
        { "e_frame_keys.canprj", kNode + "[Node1.Frame1]\r\nCanId=100\r\n",
          "[Node1.Frame1] requires CanId, Dlc and PeriodUs." },
        { "e_canid.canprj", kFrame + "CanId=800\r\n",
          "[Node1.Frame1] CanId '800' is not a valid 11-bit CAN ID." },
        { "e_offset.canprj", kFrame + "InitialOffsetUs=abc\r\n",
          "[Node1.Frame1] InitialOffsetUs must be a non-negative integer." },
        // ここに、ご自分で 2 つ書き足す（Dlc=9 と PeriodUs=0）
        { "e_dlc.canprj", kFrame + "Dlc=9\r\n",
          "[Node1.Frame1] Dlc must be an integer between 0 and 8." },
        { "e_period.canprj", kFrame + "PeriodUs=0\r\n",
          "[Node1.Frame1] PeriodUs must be a positive integer." },
    };

    for (const BadCase& c : cases)
    {
        SCOPED_TRACE(c.File); // 失敗したとき、どの設定ファイルだったかを表示する
        ConfigLoadResult r = LoadText(c.File, c.Text);
        EXPECT_FALSE(r.Success) << "不正な設定なのに読み込めたことになっている";
        EXPECT_EQ(r.ErrorMessage, c.Expected);
    }
}