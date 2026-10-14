// 機能 017（フレーム表示のチェックボックスとシミュレーション時間）の仕様テスト
// 根拠: specs/017-toolbar-display-time/spec.md、SimControls.h のコメント、CLAUDE.md。
// 位置は機能 019・023 で変わった（96 dpi でチェック欄 188〜202 × 16〜30、文字 208〜292 × 13〜33）。
// 描画・切り替えの反映は画面側なのでテストしない。

#include <gtest/gtest.h>

#include "ProjectFile.h"
#include "SimControls.h"
#include "FrameEditor.h"
#include "support/test_helpers.h"

using namespace CanSim;
using namespace TestHelpers;

// ---------------------------------------------------------------------------
// FR-007 / SC-004: 値は 3 けたごとに区切る。600,000,000 μs も表示できる。負は 0。
// 根拠: spec.md FR-007、SC-004、SimControls.h（FormatSimTimeUs）。
// ---------------------------------------------------------------------------
TEST(F017Spec, F017_FR007_FormatSimTime_ThousandsSeparators)
{
    RecordProperty("spec", "017:FR-007");
    RecordProperty("spec", "017:SC-004");
    EXPECT_EQ("0", FormatSimTimeUs(0)) << "起動直後";
    EXPECT_EQ("999", FormatSimTimeUs(999));
    EXPECT_EQ("1,000", FormatSimTimeUs(1000));
    EXPECT_EQ("12,345", FormatSimTimeUs(12345));
    EXPECT_EQ("1,000,000", FormatSimTimeUs(1000000));
    EXPECT_EQ("600,000,000", FormatSimTimeUs(600000000)) << "600 秒（上限）";
    EXPECT_EQ("0", FormatSimTimeUs(-5)) << "負は 0";
}

// ---------------------------------------------------------------------------
// FR-002 / FR-003: チェック欄と文字。チェック欄・文字のどちらを押しても切り替わる（当たり判定は両方を含む）。
// 根拠: spec.md FR-002、FR-003、SimControls.h（kFrameDisplay*、IsOnFrameDisplayToggle）、CLAUDE.md（023 改訂 1 の位置）。
// ---------------------------------------------------------------------------
TEST(F017Spec, F017_FR003_CheckBox_BoxAndLabelToggle)
{
    RecordProperty("spec", "017:FR-003");
    const DiagramRect box = FrameDisplayBoxRect(96);
    EXPECT_EQ(188, box.Left);
    EXPECT_EQ(16, box.Top);
    EXPECT_EQ(202, box.Right) << "14 px の正方形";
    EXPECT_EQ(30, box.Bottom);
    const DiagramRect label = FrameDisplayLabelRect(96);
    EXPECT_EQ(208, label.Left);
    EXPECT_EQ(292, label.Right);
    EXPECT_EQ(13, label.Top);
    EXPECT_EQ(33, label.Bottom);
    EXPECT_EQ(23, (box.Top + box.Bottom) / 2) << "ボタンの箱の縦の中心（y 23）にそろう";
    EXPECT_EQ(23, (label.Top + label.Bottom) / 2) << "ボタンの箱の縦の中心（y 23）にそろう";

    for (int dpi : {96, 120, 144})
    {
        const DiagramRect b = FrameDisplayBoxRect(dpi);
        const DiagramRect l = FrameDisplayLabelRect(dpi);
        EXPECT_TRUE(IsOnFrameDisplayToggle(b.Left, b.Top, dpi)) << dpi << " dpi: チェック欄";
        EXPECT_TRUE(IsOnFrameDisplayToggle((l.Left + l.Right) / 2, (l.Top + l.Bottom) / 2, dpi)) << dpi << " dpi: 文字";
    }
    EXPECT_FALSE(IsOnFrameDisplayToggle(183, 20, 96)) << "左の外（当たりは 184 から）";
    EXPECT_FALSE(IsOnFrameDisplayToggle(296, 20, 96)) << "右の外（当たりは 296 の手前まで）";
    EXPECT_FALSE(IsOnFrameDisplayToggle(250, 61, 96)) << "下の外";
}

// ---------------------------------------------------------------------------
// FR-006 / SC-004: シミュレーション時間の欄は、上段にラベル、下段に数値と単位。（はみ出さないかは文字の幅による画面側の確認）。
// 根拠: spec.md FR-006、SC-004、SimControls.h（kSimTime*）。
// ---------------------------------------------------------------------------
TEST(F017Spec, F017_FR006_SimTimeArea_CaptionAboveValueAndUnit)
{
    RecordProperty("spec", "017:FR-006");
    const DiagramRect cap = SimTimeCaptionRect(96);
    const DiagramRect val = SimTimeValueRect(96);
    const DiagramRect unit = SimTimeUnitRect(96);
    const DiagramRect area = SimTimeAreaRect(96);
    EXPECT_LE(cap.Bottom, val.Top) << "ラベルは数値の上";
    EXPECT_LE(val.Right, unit.Left) << "単位は数値の右";
    EXPECT_EQ(314, area.Left);
    EXPECT_EQ(494, area.Right);
    EXPECT_TRUE(area.Left <= cap.Left && cap.Right <= area.Right && area.Top <= cap.Top && val.Bottom <= area.Bottom)
        << "欄の中";
}

// ---------------------------------------------------------------------------
// FR-005 / SC-003: フレーム表示の設定は保存・読込みで保たれる（ON・OFF の両方）。
// 根拠: spec.md FR-005、SC-003。
// ---------------------------------------------------------------------------
TEST(F017Spec, F017_SC003_FrameDisplaySavedAndLoaded)
{
    RecordProperty("spec", "017:SC-003");
    for (bool on : {true, false})
    {
        ProjectSettings s;
        s.FrameDisplay = on;
        const std::wstring path = TempPath("f017_sc003.canprj");
        Network definitions;
        EnsureNode(definitions, 1); // ノード 0 個の構成は読込みで拒否されるので、ノードを 1 つ入れておく
        ASSERT_TRUE(SaveProject(path, s, definitions));
        const ConfigLoadResult r = LoadConfig(path);
        DeleteFileW(path.c_str());
        ASSERT_TRUE(r.Success) << r.ErrorMessage;
        EXPECT_EQ(on, r.Project.FrameDisplay) << (on ? "ON" : "OFF") << " が保たれる";
    }
}
