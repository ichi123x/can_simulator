// 機能 016（受信 ECU の青い強調）の仕様テスト
// 根拠: specs/016-receiver-blue-highlight/spec.md、plan.md（強調の対象は CanSim の ActiveFrameCursor が決め、
//        ダイアログは描き方だけ）、ReplayClock.h。色・太さは画面側なのでテストせず、強調の対象を確かめる。

#include <gtest/gtest.h>

#include <vector>

#include "ReplayClock.h"

using namespace CanSim;

// ---------------------------------------------------------------------------
// FR-001 / FR-003: 受信の強調は、その ECU がフレームを受信している間だけ。受信しない ECU は対象外。
// SC-002: バスが空いている書き換えでは、受信の強調（青）が 1 つも無い。
// 根拠: spec.md FR-001、FR-003、SC-002、ReplayClock.h（Receivers）。
//   A: [100, 300] ECU 1 -> {2, 4}（ECU 3 は受信しない）
// ---------------------------------------------------------------------------
TEST(F016Spec, F016_SC002_ReceiversOnlyWhileReceiving)
{
    RecordProperty("spec", "016:FR-001");
    RecordProperty("spec", "016:FR-003");
    RecordProperty("spec", "016:SC-002");
    std::vector<FrameEvent> ev(1);
    ev[0].TxStartUs = 100;
    ev[0].TxEndUs = 300;
    ev[0].TxNodeId = 1;
    ev[0].RxNodeIds = {2, 4};
    ActiveFrameCursor cursor(ev);

    const ActiveFrames before = cursor.Collect(0, 50);
    EXPECT_TRUE(before.Receivers.empty()) << "受信の前は青くしない";
    const ActiveFrames during = cursor.Collect(50, 200);
    EXPECT_EQ((std::vector<int>{2, 4}), during.Receivers) << "受信している ECU 2・4 だけ（ECU 3 は含まない）";
    EXPECT_EQ((std::vector<int>{1}), during.Senders) << "送信 ECU は従来どおり";
    const ActiveFrames after = cursor.Collect(300, 400);
    EXPECT_TRUE(after.Receivers.empty()) << "バスが空いている書き換えでは青い ECU が無い";
    EXPECT_FALSE(after.BusBusy);
}
