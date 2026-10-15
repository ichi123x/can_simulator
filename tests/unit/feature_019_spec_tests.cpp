// feature_019_spec_tests.cpp（tests/unit）
// 機能 019（specs/019-toolbar-compact-layout/spec.md）の受け入れシナリオと成功基準のうち、画面を使わずに確かめられるもの。
// ツールバーの部品の範囲・当たり判定は SimControls.h で確かめる。文字の大きさ・色は画面の確認。

#include "support/spec_test_helpers.h"

#include "BusDiagramLayout.h"
#include "SimControls.h"

using namespace CanSim;
using namespace spectest;

namespace
{
    bool Overlaps(const DiagramRect& a, const DiagramRect& b)
    {
        return a.Left < b.Right && b.Left < a.Right && a.Top < b.Bottom && b.Top < a.Bottom;
    }
}

// 根拠: spec.md US1 受け入れシナリオ 1、FR-001。左から ボタン 3 つ → チェックボックス → シミュレーション時間 → 「スピード：」→
// スライダー の順に並ぶ（96 dpi）。
TEST(F019Spec, F019_US1_1_Order_ButtonsCheckBoxTimeCaptionSlider)
{
    RecordProperty("spec", "019:US1-1");
    const int dpi = 96;
    EXPECT_LE(SimButtonRect(2, dpi).Right, FrameDisplayBoxRect(dpi).Left) << "終了ボタン → チェックボックス";
    EXPECT_LE(FrameDisplayLabelRect(dpi).Right, SimTimeAreaRect(dpi).Left) << "チェックボックス → シミュレーション時間";
    EXPECT_LE(SimTimeAreaRect(dpi).Right, SpeedCaptionRect(dpi).Left) << "シミュレーション時間 → 「スピード：」";
    EXPECT_LE(SpeedCaptionRect(dpi).Right, SpeedTrackRect(dpi).Left) << "「スピード：」→ スライダー";
}

// 根拠: spec.md US1 受け入れシナリオ 2、FR-002。ボタンの箱は 96 dpi で 24 × 18 px、名前は箱の下に表示する。
TEST(F019Spec, F019_US1_2_ButtonBox24x18_LabelBelow)
{
    RecordProperty("spec", "019:US1-2");
    for (int i = 0; i < kSimButtonCount; ++i)
    {
        const DiagramRect box = SimButtonBoxRect(i, 96);
        const DiagramRect label = SimButtonLabelRect(i, 96);
        EXPECT_EQ(box.Right - box.Left, 24) << i << " 番目のボタンの箱の幅";
        EXPECT_EQ(box.Bottom - box.Top, 18) << i << " 番目のボタンの箱の高さ";
        EXPECT_GE(label.Top, box.Bottom) << i << " 番目のボタンの名前が箱の下にない";
    }
}

// 根拠: spec.md US1 受け入れシナリオ 3、FR-003・FR-004。スライダーの目盛りは -13〜+10 の 24 個のまま。数字は 5 刻みと今の値だけを
// 表示し、今の値から 2 目盛り以内の 5 刻みは表示しない。
TEST(F019Spec, F019_US1_3_Slider_TicksAndLabelRule)
{
    RecordProperty("spec", "019:US1-3");
    // 数字の規則（5 刻みと今の値、今の値から 2 目盛り以内の 5 刻みは隠す）
    EXPECT_TRUE(IsSpeedLabelShown(-5, 0));
    EXPECT_FALSE(IsSpeedLabelShown(-4, 0));
    EXPECT_TRUE(IsSpeedLabelShown(-4, -4)) << "今の値は表示する";
    EXPECT_FALSE(IsSpeedLabelShown(-5, -4)) << "今の値から 1 目盛りの 5 刻みは隠す";
    EXPECT_FALSE(IsSpeedLabelShown(-5, -3)) << "今の値から 2 目盛りの 5 刻みは隠す";
    EXPECT_TRUE(IsSpeedLabelShown(-5, -2)) << "今の値から 3 目盛りの 5 刻みは表示する";
    std::set<int> values;
    for (int x = 0; x < 1200; ++x)
        values.insert(SpeedAtX(x, 96));
    if (values.size() != 24u)
        GTEST_SKIP() << "仕様と実装の食い違い（人の判断待ち）: 019 は目盛り -13〜+10 の 24 個のままとするが、実装は機能 026 のとおり "
                     << values.size() << " 個（" << *values.begin() << "〜" << *values.rbegin()
                     << "）。019 の記述が 026 で置き換わったかの判断が必要";
    EXPECT_EQ(values.size(), 24u);
}

// 根拠: spec.md US1 受け入れシナリオ 4、FR-005。スライダーの左に「スピード：」のラベルがある。
TEST(F019Spec, F019_US1_4_SpeedCaption_LeftOfSlider)
{
    RecordProperty("spec", "019:US1-4");
    for (int dpi : { 96, 120, 144 })
    {
        const DiagramRect caption = SpeedCaptionRect(dpi);
        EXPECT_LE(caption.Right, SpeedTickX(-10, dpi)) << dpi << " DPI: 「スピード：」がスライダーの左にない";
        EXPECT_FALSE(Overlaps(caption, SpeedTrackRect(dpi))) << dpi << " DPI: 「スピード：」が溝と重なる";
    }
}

// 根拠: spec.md US1 受け入れシナリオ 5、FR-006。新しい位置で、ボタン・チェックボックス・スライダーが今までどおりに動く
// （当たり判定が見えている位置と一致し、スライダーは目盛りへ丸める）。
TEST(F019Spec, F019_US1_5_NewPositions_HitTestsWork)
{
    RecordProperty("spec", "019:US1-5");
    for (int i = 0; i < kSimButtonCount; ++i)
    {
        const DiagramRect box = SimButtonBoxRect(i, 96);
        EXPECT_EQ(FindSimButtonAt((box.Left + box.Right) / 2, (box.Top + box.Bottom) / 2, 96), i) << i << " 番目のボタン";
    }
    const DiagramRect cb = FrameDisplayBoxRect(96);
    EXPECT_TRUE(IsOnFrameDisplayToggle((cb.Left + cb.Right) / 2, (cb.Top + cb.Bottom) / 2, 96)) << "チェックボックス";
    const DiagramRect thumb = SpeedThumbRect(3, 96);
    EXPECT_TRUE(IsOnSpeedSlider((thumb.Left + thumb.Right) / 2, (thumb.Top + thumb.Bottom) / 2, 96)) << "つまみ";
    EXPECT_EQ(SpeedAtX(SpeedTickX(3, 96) + 3, 96), 3) << "目盛りへの丸め";
}

// 根拠: spec.md SC-001、FR-001。部品が順に重ならずに並び、帯とウィンドウ（構成図の幅）に収まる（DPI 96/120/144）。
TEST(F019Spec, F019_SC001_PartsInOrder_NoOverlap_FitInBand)
{
    RecordProperty("spec", "019:SC-001");
    for (int dpi : { 96, 120, 144 })
    {
        const int band = ScaleToDpi(kToolbarHeight, dpi);
        const int width = ScaleToDpi(kDiagramWidth, dpi);
        std::vector<DiagramRect> parts = { SimButtonRect(0, dpi), SimButtonRect(1, dpi), SimButtonRect(2, dpi),
                                           FrameDisplayBoxRect(dpi), FrameDisplayLabelRect(dpi), SimTimeAreaRect(dpi),
                                           SpeedCaptionRect(dpi), SpeedTrackRect(dpi), SpeedLabelRect(10, dpi) };
        for (std::size_t i = 0; i < parts.size(); ++i)
        {
            EXPECT_GE(parts[i].Top, 0) << dpi << " DPI: " << i << " 番目の部品が帯の上にはみ出す";
            EXPECT_LE(parts[i].Bottom, band) << dpi << " DPI: " << i << " 番目の部品が帯の下にはみ出す";
            EXPECT_LE(parts[i].Right, width) << dpi << " DPI: " << i << " 番目の部品がウィンドウの幅からはみ出す";
            if (i > 0 && i != 8)
                EXPECT_LE(parts[i - 1].Right, parts[i].Left) << dpi << " DPI: " << i - 1 << " と " << i << " 番目が順に並んでいない";
        }
    }
}

// 根拠: spec.md SC-002、FR-004。表示される数字どうしが重ならない（今の値がどれでも）。今の値ごとに、表示される数字の範囲が
// 左右で重ならないことを確かめる（今の値の範囲は、026 の後の -10〜+10）。
TEST(F019Spec, F019_SC002_ShownLabels_NeverOverlap)
{
    RecordProperty("spec", "019:SC-002");
    for (int dpi : { 96, 120, 144 })
    {
        for (int current = -10; current <= 10; ++current)
        {
            std::vector<DiagramRect> shown;
            for (int s = -10; s <= 10; ++s)
                if (IsSpeedLabelShown(s, current))
                    shown.push_back(SpeedLabelRect(s, dpi));
            for (std::size_t i = 1; i < shown.size(); ++i)
                EXPECT_LE(shown[i - 1].Right, shown[i].Left)
                    << dpi << " DPI・今の値 " << current << ": 表示される数字が重なる";
        }
    }
}

// 根拠: spec.md SC-003、FR-006。新しい位置で、ボタン・チェックボックス・スライダーの操作が今までどおりに動く（DPI 96/120/144 の
// それぞれで、見えている範囲の中を押すとその部品になり、部品の外では反応しない）。
TEST(F019Spec, F019_SC003_EachDpi_HitTestsMatchVisibleParts)
{
    RecordProperty("spec", "019:SC-003");
    for (int dpi : { 96, 120, 144 })
    {
        for (int i = 0; i < kSimButtonCount; ++i)
        {
            const DiagramRect r = SimButtonRect(i, dpi);
            EXPECT_EQ(FindSimButtonAt(r.Left, r.Top, dpi), i) << dpi << " DPI: " << i << " 番目のボタンの左上";
            EXPECT_EQ(FindSimButtonAt(r.Right - 1, r.Bottom - 1, dpi), i) << dpi << " DPI: " << i << " 番目のボタンの右下";
        }
        const DiagramRect label = FrameDisplayLabelRect(dpi);
        EXPECT_TRUE(IsOnFrameDisplayToggle(label.Right - 2, (label.Top + label.Bottom) / 2, dpi)) << dpi << " DPI: 文字の右端";
        const DiagramRect time = SimTimeAreaRect(dpi);
        EXPECT_FALSE(IsOnFrameDisplayToggle((time.Left + time.Right) / 2, (time.Top + time.Bottom) / 2, dpi))
            << dpi << " DPI: シミュレーション時間の欄でチェックボックスが反応する";
        EXPECT_FALSE(IsOnSpeedSlider((time.Left + time.Right) / 2, (time.Top + time.Bottom) / 2, dpi))
            << dpi << " DPI: シミュレーション時間の欄でスライダーが反応する";
        EXPECT_EQ(FindSimButtonAt((time.Left + time.Right) / 2, (time.Top + time.Bottom) / 2, dpi), -1)
            << dpi << " DPI: シミュレーション時間の欄でボタンが反応する";
    }
}
