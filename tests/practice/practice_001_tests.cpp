// 練習 001: feature_001_spec_tests.cpp の流れで、仕様テストを一から書く練習
// 進め方は tests/practice/README.md を参照。解答例は practice_001_answers.cpp（書き終えてから見る）。
//
// 各レッスンの手順:
//   1. コメントの「根拠」を spec.md で読む
//   2. GTEST_SKIP() の行を消す
//   3. TODO の所にコードを書く
//   4. ビルドして ctest -L practice で実行し、PASSED になるのを確かめる
//   5. わざと期待値を 1 つ変えて FAILED になること（テストが本当に確かめていること）も見る
//
// 根拠: specs/001-can-simulation-core/spec.md
// 起動直後は 11 ビット時間（500 kbps で 22 us）のバス同期検出を経て通常動作に入るので、最初の送信要求は 22 us。

#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <vector>

#include "CanTypes.h"
#include "ConfigLoader.h"
#include "FrameTiming.h"
#include "support/test_helpers.h" // LoadFromText / MakeNode / RunSim / UniqueTransmissions

using namespace CanSim;
using namespace TestHelpers;

namespace
{
    const long long kNormalEntryUs = 22;
}

// ---------------------------------------------------------------------------
// レッスン 1: 最小のテスト（関数を 1 回呼んで、戻り値を EXPECT_EQ で比べる）
// 根拠: SC-003、FR-010。
// 対象: FrameTiming.h の ComputeFrameTiming(canId, dlc, data, bitRateBps)
// 期待値: CAN ID 0x7FF・DLC 0・データなし・500 kbps のとき
//   総ビット数 47、スタッフビット 3、1 ビット時間 2000 ns、
//   伝送時間 = 47 × 2000 ns、インターミッション = 3 ビット時間
// 覚えること:
//   - TEST(スイート名, テスト名)。名前は英数字と _ だけ（日本語にすると ctest で 0 件実行になる）
//   - EXPECT_EQ(期待値, 実際の値) << "失敗したときに出す日本語のメッセージ";
//   - RecordProperty("spec", "001:SC-003"); で根拠の ID を記録する
// ---------------------------------------------------------------------------
TEST(P001Practice, P001_L1_Id7FFDlc0At500k_BitsAndDurationMatch)
{
    RecordProperty("spec", "001:SC-003");
    const FrameTimingResult r = ComputeFrameTiming(0x7FF, 0, {}, 500000);
    EXPECT_EQ(47, r.TotalBits) << "総ビット数";
    EXPECT_EQ(3, r.StuffBitCount) << "スタッフビット数";
    EXPECT_EQ(2000, r.BitTimeNs) << "1 ビット時間";
    EXPECT_EQ(47 * 2000, r.TxDurationNs) << "伝送時間 = 総ビット数 × 1 ビット時間";
    EXPECT_EQ(3 * 2000, r.IntermissionNs) << "インターミッションは 3 ビット時間";

}

// ---------------------------------------------------------------------------
// レッスン 2: ASSERT と EXPECT の使い分け（設定ファイルの読み込み）
// 根拠: FR-002（通信速度は 125k / 250k / 500k / 1M から選ぶ）
// 対象: test_helpers.h の LoadFromText(一時ファイル名, 中身) → ConfigLoadResult
//       （Success / Config / ErrorMessage を持つ。ConfigLoader.h 参照）
// 覚えること:
//   - ASSERT_* は失敗したらそのテストをそこで終える。後の確認が意味を失うとき（読み込み失敗・要素数の不足）に使う
//   - EXPECT_* は失敗しても続ける。独立した確認を並べるときに使う
//   - 設定ファイルの書き方は specs/001-can-simulation-core/contracts/config-input-format.md と
//     feature_001_spec_tests.cpp の MakeConfigText を参考にする
// ---------------------------------------------------------------------------

// 2-a: BitRateBps=250000、ノード 2 つ（ノード 1 が CAN ID 0x100 を送信）の構成は読み込める
TEST(P001Practice, P001_L2_BitRate250k_LoadedAsWritten)
{
    RecordProperty("spec", "001:FR-002");
    const std::string text =
        "[Project]\nEcuCount=2\n\n[Network]\nBitRateBps=250000\n\n"
        "[Node1]\nNodeId=1\n\n[Node1.Frame1]\nCanId=0x100\nDlc=8\nPeriodUs=10000\n\n"
        "[Node2]\nNodeId=2\n";
    const ConfigLoadResult result = LoadFromText("p001_l2_250k.canprj", text);
    // 読めなかったら以降の確認は意味が無い（Config が空）ので ASSERT で止める
    ASSERT_TRUE(result.Success) << "正しい構成が拒否された: " << result.ErrorMessage;
    EXPECT_EQ(250000, result.Config.BitRateBps) << "通信速度";
    ASSERT_EQ(2u, result.Config.Nodes.size()) << "ノード数";
    ASSERT_EQ(1u, result.Config.Nodes[0].Frames.size()) << "ノード 1 のフレーム数";
    EXPECT_EQ(0x100u, result.Config.Nodes[0].Frames[0].CanId) << "ノード 1 のフレームの CAN ID";
}

// 2-b: BitRateBps=300000 は選べない値なので拒否され、理由が示される
TEST(P001Practice, P001_L2_BitRate300k_Rejected)
{
    GTEST_SKIP() << "練習: この行を消して、下の TODO を書く";
    // TODO: 300000 の構成を読み、EXPECT_FALSE(result.Success) と EXPECT_FALSE(result.ErrorMessage.empty())
}

// ---------------------------------------------------------------------------
// レッスン 3: シミュレーションを動かす（準備 → 実行 → 確認）
// 根拠: FR-003、機能 006（最初の送信要求 = 通常動作に入った時刻 22 us + 初回オフセット、以後は周期ごと）
// 構成: 500 kbps。ノード 1 が CAN ID 0x100・DLC 8・周期 10000 us・オフセット 0 を送信、ノード 2 は受信だけ
// 期待値: 100 ms（100000 us）実行すると送信は 10 回、k 回目の送信要求時刻は 22 + 10000 × k us
// 使う道具（test_helpers.h）:
//   MakeNode(ノード番号, {{CAN ID, DLC, 周期, オフセット}, ...})
//   RunSim(network, 実行時間 us) → RunResult（.Rows がログの行）
//   UniqueTransmissions(rows) → 受信ノードの数だけ重複する行を送信 1 回ずつにまとめる
// ---------------------------------------------------------------------------
TEST(P001Practice, P001_L3_OneSenderPeriod10ms_TenTransmissionsAtPeriod)
{
    GTEST_SKIP() << "練習: この行を消して、下の TODO を書く";
    // TODO（準備）: CanSim::Network net を作り、BitRateBps とノード 2 つを入れる
    // TODO（実行）: RunSim → UniqueTransmissions
    // TODO（確認）: ASSERT_EQ で回数 10、for で各回の TxRequestUs を EXPECT_EQ（メッセージに何回目かを入れる）
}

// ---------------------------------------------------------------------------
// レッスン 4: ログの全行に同じ性質が成り立つことを確かめる（ループ + 条件 + 件数の確認）
// 根拠: FR-005（受信フィルタ。未指定は全 ID 受信）、FR-016（受信処理時刻 = 受信完了 + 受信処理遅延）
// 構成: 500 kbps、100 ms 実行
//   ノード 1: 0x100 と 0x200 を送信（どちらも DLC 8・周期 10000 us・オフセット 0）
//   ノード 2: 全 ID を受信、受信処理遅延 Node::ReceiveDelayUs = 500
//   ノード 3: 0x200 だけ受信（Node::RxFilterAll = false、Node::RxFilter = {0x200}）
// 期待値:
//   - RxNodeId == 2 の行はすべて RxProcessUs == RxEndUs + 500
//   - RxNodeId == 3 の行はすべて CanId == 0x200、RxProcessUs == RxEndUs
//   - ノード 2 の行は 20 行、ノード 3 の行は 10 行
// 覚えること:
//   - ループの中の EXPECT だけだと、行が 0 件のとき何も確かめずに PASSED になる。
//     件数も数えて最後に確かめる（feature_001_spec_tests.cpp の SC-002 の checkedPairs と同じ考え方）
// ---------------------------------------------------------------------------
TEST(P001Practice, P001_L4_RxFilterAndReceiveDelay_AppliedToEveryRow)
{
    GTEST_SKIP() << "練習: この行を消して、下の TODO を書く";
    // TODO: MakeNode で作ったノードを変数に受けてから ReceiveDelayUs・RxFilterAll・RxFilter を書き換え、net に入れる
    // TODO: RunSim して、run.Rows を for で回し、ノード 2・3 の行ごとに EXPECT、件数を数える
    // TODO: ループの後で件数を EXPECT_EQ
}

// ---------------------------------------------------------------------------
// レッスン 5: パラメータ化テスト（同じ確認を表の行ごとにくり返す）— ここは一から書く
// 根拠: SC-003、FR-002
// 内容: 0x7FF・DLC 0 の総ビット数は通信速度に関係なく 47。1 ビット時間 = 10^9 / 通信速度 ns
//   125000 → 8000、250000 → 4000、500000 → 2000、1000000 → 1000
// 書くもの（feature_001_spec_tests.cpp の SC-003・SC-004 を見本にする）:
//   1. 1 行分のデータの struct（通信速度と期待する 1 ビット時間）
//   2. class XXX : public ::testing::TestWithParam<その struct> {};
//   3. TEST_P(XXX, P001_L5_...) { const auto& c = GetParam(); ... }
//   4. INSTANTIATE_TEST_SUITE_P(名前, XXX, ::testing::Values(...), 名前を付ける関数);
// 注意: TEST_P を書いて INSTANTIATE_TEST_SUITE_P を書き忘れると、GoogleTest がテストの失敗として知らせる
//       （GoogleTestVerification.UninstantiatedParameterizedTestSuite）。必ず対で書く
// クラス名は解答例と重ならないように P001PracticeBitRate などにする
// ---------------------------------------------------------------------------

// TODO: ここにレッスン 5 を書く
