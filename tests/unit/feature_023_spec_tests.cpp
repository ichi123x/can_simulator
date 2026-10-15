// feature_023_spec_tests.cpp（tests/unit）
// 機能 023（specs/023-toolbar-file-buttons/spec.md）の受け入れシナリオと成功基準のうち、画面を使わずに確かめられるもの。
// ボタンの並び・大きさ・当たり判定・有効／無効は SimControls.h、保存は ProjectFile.h で確かめる。
// ファイルを選ぶ画面・保存先を尋ねる画面・エラーメッセージは画面の確認。

#include "support/spec_test_helpers.h"

#include "BusDiagramLayout.h"
#include "ProjectFile.h"
#include "SimControls.h"

using namespace CanSim;
using namespace spectest;

namespace
{
    const int kSave = static_cast<int>(ToolbarButton::Save);
    const int kLoad = static_cast<int>(ToolbarButton::Load);
    const int kStart = static_cast<int>(ToolbarButton::Start);
}

// 根拠: spec.md US1 受け入れシナリオ 4、FR-005。保存先に書き込めないときは保存に失敗する（失敗を返すので、画面はエラーを出して
// 現在のプロジェクトファイルを変えない）。存在しないフォルダーへの保存で確かめる。
TEST(F023Spec, F023_US1_4_UnwritableDestination_SaveFails)
{
    RecordProperty("spec", "023:US1-4");
    Network defs;
    defs.Nodes.push_back(MakeNode(1));
    EXPECT_FALSE(SaveProject(L"Z:\\no_such_folder_f023\\x\\a.canprj", ProjectSettings(), defs)) << "書き込めない保存先で成功した";
}

// 根拠: spec.md US3 受け入れシナリオ 1・2・SC-003、FR-006。実行中・中断中は「読込み」「保存」が無効（グレー）で、押しても何もしない。
TEST(F023Spec, F023_US3_1_RunningOrPaused_FileButtonsDisabled)
{
    RecordProperty("spec", "023:US3-1");
    for (RunPhase phase : { RunPhase::Running, RunPhase::Paused })
    {
        EXPECT_FALSE(IsToolbarButtonEnabled(kSave, phase, false)) << "実行中・中断中に「保存」が有効";
        EXPECT_FALSE(IsToolbarButtonEnabled(kLoad, phase, false)) << "実行中・中断中に「読込み」が有効";
    }
}

// 根拠: spec.md US3 受け入れシナリオ 3、FR-006。待機に戻ると「読込み」「保存」は押せる。待機中は編集ウィンドウが開いていても有効。
TEST(F023Spec, F023_US3_3_Idle_FileButtonsEnabledEvenWhileEditing)
{
    RecordProperty("spec", "023:US3-3");
    for (bool editing : { false, true })
    {
        EXPECT_TRUE(IsToolbarButtonEnabled(kSave, RunPhase::Idle, editing)) << "待機中に「保存」が無効（編集中 " << editing << "）";
        EXPECT_TRUE(IsToolbarButtonEnabled(kLoad, RunPhase::Idle, editing)) << "待機中に「読込み」が無効（編集中 " << editing << "）";
    }
}

// 根拠: spec.md SC-001、FR-004。プロジェクトファイルが無い状態から保存先を指定して保存し、読み込み直した内容が保存前と一致する。
TEST(F023Spec, F023_SC001_SaveFromScratch_ReloadIdentical)
{
    RecordProperty("spec", "023:SC-001");
    ProjectSettings s;
    s.EcuCount = 4;
    s.DurationSec = 33;
    s.Speed = -2;
    s.FrameDisplay = false;
    Network defs;
    defs.BitRateBps = 125000;
    Node n = MakeNode(2, { MakeFrame(0x2A0, 5, 40000, 1000) });
    n.Label = "ブレーキ";
    defs.Nodes.push_back(n);
    const std::wstring path = TempPath(L"f023_sc001.canprj");
    ASSERT_TRUE(SaveProject(path, s, defs)) << "保存できない";
    const ConfigLoadResult r = LoadConfig(path);
    ASSERT_TRUE(r.Success) << r.ErrorMessage;
    EXPECT_EQ(r.Project.EcuCount, 4);
    EXPECT_EQ(r.Project.DurationSec, 33);
    EXPECT_EQ(r.Project.Speed, -2);
    EXPECT_FALSE(r.Project.FrameDisplay);
    EXPECT_EQ(r.Config.BitRateBps, 125000);
    ASSERT_EQ(r.Config.Nodes.size(), 1u);
    EXPECT_EQ(r.Config.Nodes[0].Label, "ブレーキ");
    ASSERT_EQ(r.Config.Nodes[0].Frames.size(), 1u);
    EXPECT_EQ(r.Config.Nodes[0].Frames[0].CanId, 0x2A0u);
    EXPECT_EQ(r.Config.Nodes[0].Frames[0].InitialOffsetUs, 1000);
}

// 根拠: spec.md SC-003、FR-006。実行中・中断中は、5 つのボタンのうち「読込み」「保存」がどの位置を押しても動かない
// （有効なのは機能 014 の表どおりの開始・中断・終了だけ）。
TEST(F023Spec, F023_SC003_RunningOrPaused_NoFileButtonEverEnabled)
{
    RecordProperty("spec", "023:SC-003");
    for (RunPhase phase : { RunPhase::Running, RunPhase::Paused })
        for (bool editing : { false, true })
            for (int i = 0; i < kFileButtonCount; ++i)
                EXPECT_FALSE(IsToolbarButtonEnabled(i, phase, editing)) << "ボタン " << i << " が有効";
}

// 根拠: spec.md SC-004、FR-001・FR-002。ツールバーの部品がすべて重ならずに帯とウィンドウの幅に収まる（DPI 96/120/144）。
// 並びは左から 保存 → 読込み →（ボタン 1 つ分の空き）→ 開始 → 中断 → 終了 で、5 つのボタンは同じ大きさ。空きを押しても何も起きない。
TEST(F023Spec, F023_SC004_ButtonsInOrderWithGap_AllFit)
{
    RecordProperty("spec", "023:SC-004");
    for (int dpi : { 96, 120, 144 })
    {
        const int band = ScaleToDpi(kToolbarHeight, dpi);
        const DiagramRect save = ToolbarButtonBoxRect(kSave, dpi);
        const DiagramRect load = ToolbarButtonBoxRect(kLoad, dpi);
        const DiagramRect start = ToolbarButtonBoxRect(kStart, dpi);
        const int pitch = load.Left - save.Left;
        EXPECT_GT(start.Left - load.Left, pitch) << dpi << " DPI: 読込みと開始の間に空きが無い";
        for (int i = 0; i < kToolbarButtonCount; ++i)
        {
            const DiagramRect b = ToolbarButtonBoxRect(i, dpi);
            EXPECT_EQ(b.Right - b.Left, save.Right - save.Left) << dpi << " DPI: ボタン " << i << " の幅が違う";
            EXPECT_EQ(b.Bottom - b.Top, save.Bottom - save.Top) << dpi << " DPI: ボタン " << i << " の高さが違う";
            EXPECT_LE(ToolbarButtonRect(i, dpi).Bottom, band) << dpi << " DPI: ボタン " << i << " が帯の下にはみ出す";
            if (i > 0)
                EXPECT_LE(ToolbarButtonRect(i - 1, dpi).Right, ToolbarButtonRect(i, dpi).Left)
                    << dpi << " DPI: ボタン " << i - 1 << " と " << i << " が順に並んでいない";
        }
        const int gapX = (ToolbarButtonRect(kLoad, dpi).Right + ToolbarButtonRect(kStart, dpi).Left) / 2;
        EXPECT_EQ(FindToolbarButtonAt(gapX, (load.Top + load.Bottom) / 2, dpi), -1) << dpi << " DPI: 空きでボタンが反応する";
        EXPECT_LE(ToolbarButtonRect(4, dpi).Right, FrameDisplayBoxRect(dpi).Left) << dpi << " DPI: 終了とチェックボックスが重なる";
        EXPECT_LE(SpeedLabelRect(10, dpi).Right, ScaleToDpi(kDiagramWidth, dpi)) << dpi << " DPI: 右端がウィンドウの幅を超える";
    }
}
