// 機能 005（シミュレーションのスピードと実時間の再生）の仕様テスト
// 根拠: specs/005-simulation-speed/spec.md、ReplayClock.h・FrameEvents.h のコメント。
// スピードの範囲は機能 026 で -10〜+10 に変わった（026 のテストで確かめる）。
// 実時間の待ち・画面の書き換え・メニューは画面側なのでテストせず、時計と強調の対象の計算を確かめる。

#include <gtest/gtest.h>

#include <climits>
#include <utility>
#include <vector>

#include "FrameEvents.h"
#include "ProjectFile.h"
#include "ReplayClock.h"
#include "support/test_helpers.h"

using namespace CanSim;
using namespace TestHelpers;

// ---------------------------------------------------------------------------
// US2-2 / FR-003: シミュレーション時刻 = 実経過時間 × 2^スピード。桁あふれは飽和する。
// 根拠: spec.md US2 シナリオ 2、FR-003、ReplayClock.h（SimTimeAtUs: saturates）。
// ---------------------------------------------------------------------------
TEST(F005Spec, F005_FR003_SimTime_RealTimeTimesPowerOfTwo)
{
    RecordProperty("spec", "005:FR-003");
    EXPECT_EQ(1000000, SimTimeAtUs(1000000, 0)) << "スピード 0 は実時間";
    EXPECT_EQ(2000000, SimTimeAtUs(1000000, 1)) << "+1 は 2 倍";
    EXPECT_EQ(8000000, SimTimeAtUs(1000000, 3)) << "+3 は 8 倍";
    EXPECT_EQ(500000, SimTimeAtUs(1000000, -1)) << "-1 は 1/2";
    EXPECT_EQ(1, SimTimeAtUs(1024, -10)) << "-10 は 1/1024";
    EXPECT_EQ(512LL * 1000000, SimTimeAtUs(1000000, 9)) << "+9 は 512 倍";
    EXPECT_EQ(LLONG_MAX, SimTimeAtUs(LLONG_MAX / 2, 3)) << "桁あふれは飽和";
}

// ---------------------------------------------------------------------------
// US4-1 / FR-009: 書き換え 1 回の間（from, to] に送信中だったフレームの送信元・受信 ECU をまとめて強調する。
// US4-2: その間にフレームが無ければ何も強調しない（バスも空き）。
// US4-3: 同じ ECU が送信と受信の両方をしたら送信として強調する（受信の集まりには入れない）。
// 根拠: spec.md US4 シナリオ 1〜3、FR-009、ReplayClock.h（ActiveFrames・Collect）。
//   A: [100, 300] ECU 1 -> {2, 3}、B: [310, 500] ECU 2 -> {1}、C: [510, 700] ECU 3 -> {1, 2}
// ---------------------------------------------------------------------------
TEST(F005Spec, F005_US4_1_FramesInRefresh_SendersReceiversAndFlows)
{
    RecordProperty("spec", "005:US4-1");
    RecordProperty("spec", "005:US4-2");
    RecordProperty("spec", "005:US4-3");
    std::vector<FrameEvent> ev(3);
    ev[0].TxStartUs = 100; ev[0].TxEndUs = 300; ev[0].TxNodeId = 1; ev[0].RxNodeIds = {2, 3};
    ev[1].TxStartUs = 310; ev[1].TxEndUs = 500; ev[1].TxNodeId = 2; ev[1].RxNodeIds = {1};
    ev[2].TxStartUs = 510; ev[2].TxEndUs = 700; ev[2].TxNodeId = 3; ev[2].RxNodeIds = {1, 2};
    ActiveFrameCursor cursor(ev);

    const ActiveFrames ab = cursor.Collect(0, 400);
    EXPECT_EQ((std::vector<int>{1, 2}), ab.Senders) << "A と B の送信元";
    EXPECT_EQ((std::vector<int>{3}), ab.Receivers) << "受信 ECU（送信もした 1・2 は送信として扱う）";
    EXPECT_TRUE(ab.BusBusy);
    EXPECT_EQ((std::vector<std::pair<int, int>>{{1, 2}, {1, 3}, {2, 1}}), ab.Flows) << "流れの組（送信もした ECU も受信先に残す）";

    const ActiveFrames c = cursor.Collect(500, 510);
    EXPECT_EQ((std::vector<int>{3}), c.Senders) << "500 で終わった B は含まない、510 で始まる C は含む";
    EXPECT_EQ((std::vector<int>{1, 2}), c.Receivers);

    const ActiveFrames none = cursor.Collect(700, 800);
    EXPECT_TRUE(none.Senders.empty()) << "フレームが無い間は強調しない";
    EXPECT_TRUE(none.Receivers.empty());
    EXPECT_FALSE(none.BusBusy) << "バスは空き";
    EXPECT_TRUE(none.Flows.empty());
}

// ---------------------------------------------------------------------------
// FrameEvents.h: フレームログ（1 フレーム × 受信ノード）を 1 フレーム 1 件にまとめ、送信開始の順に並べる。
// 機能 008 Edge Cases: 送信側で捨てられた要求は表示しない。受信側で捨てられたフレームは、バスでは送受信されて
//   いるので受信 ECU の強調は従来どおり。
// 根拠: FrameEvents.h、specs/008-frame-buffers/spec.md Edge Cases（フレーム表示）。
// ---------------------------------------------------------------------------
TEST(F005Spec, F005_FR009_BuildFrameEvents_GroupedSortedOverwriteHandled)
{
    RecordProperty("spec", "005:FR-009");
    auto row = [](int tx, int rx, long long start, long long end, const char* status) {
        LogRow r;
        r.TxNodeId = tx;
        r.RxNodeId = rx;
        r.TxStartUs = start;
        r.TxEndUs = end;
        r.Status = status;
        return r;
    };
    const std::vector<LogRow> rows = {
        row(2, 1, 400, 600, "SUCCESS"),
        row(1, 2, 100, 300, "SUCCESS"),
        row(1, 3, 100, 300, "OVERWRITE"), // 受信側の破棄: 受信 ECU 3 は強調する
        row(3, -1, -1, -1, "OVERWRITE"),  // 送信側の破棄: 表示しない
    };
    const std::vector<FrameEvent> ev = BuildFrameEvents(rows);
    ASSERT_EQ(2u, ev.size()) << "送信 2 回（送信側の破棄は含まない）";
    EXPECT_EQ(100, ev[0].TxStartUs) << "送信開始の順";
    EXPECT_EQ(1, ev[0].TxNodeId);
    EXPECT_EQ((std::vector<int>{2, 3}), ev[0].RxNodeIds) << "受信側で捨てられた ECU 3 も受信 ECU";
    EXPECT_EQ(400, ev[1].TxStartUs);
    EXPECT_EQ((std::vector<int>{1}), ev[1].RxNodeIds);
}

// ---------------------------------------------------------------------------
// US2-5 / FR-002: スピードは保存・読込みの対象。スピードの無い既存の .canprj は 0。
// 根拠: spec.md US2 シナリオ 5、FR-002、Edge Cases（スピードの項目を持たない既存の .canprj）。
// ---------------------------------------------------------------------------
TEST(F005Spec, F005_US2_5_SpeedSavedAndLoaded_MissingIsZero)
{
    RecordProperty("spec", "005:US2-5");
    RecordProperty("spec", "005:FR-002");
    for (int speed : {-10, -3, 0, 4, 10})
    {
        ProjectSettings s;
        s.Speed = speed;
        const std::wstring path = TempPath("f005_speed.canprj");
        ASSERT_TRUE(SaveProject(path, s, Network()));
        const ConfigLoadResult r = LoadConfig(path);
        DeleteFileW(path.c_str());
        ASSERT_TRUE(r.Success) << r.ErrorMessage;
        EXPECT_EQ(speed, r.Project.Speed) << "保存したスピードに戻る";
    }
    const ConfigLoadResult old = LoadFromText("f005_nospeed.canprj", "[Project]\nEcuCount=2\nDurationSec=10\n");
    ASSERT_TRUE(old.Success) << old.ErrorMessage;
    EXPECT_EQ(0, old.Project.Speed) << "スピードの無いファイルは 0";
}
