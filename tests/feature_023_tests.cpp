// feature_023_tests.cpp（tests）
// tools/cansim_runner/checks.cpp の CheckFeature023() から移した（機能 030）。
// 本体は、パスの取り方以外は変えていない。Check() のラベルには SC/FR の ID が残っている。

#include "test_helpers.h"

using namespace cansimtest;

TEST(Feature023, FR001to002_FR006to007)
{
    std::cout << "\n== 機能 023: ツールバーの保存・読込みボタン ==\n";
    const int dpis[] = { 96, 120, 144 };
    bool lefts = true;
    bool ordered = true;
    bool hit = true;
    bool aliases = true;
    bool placed = true;
    const int expectedLeft[kToolbarButtonCount] = { 8, 36, 92, 120, 148 };
    for (int i = 0; i < kToolbarButtonCount; ++i)
    {
        const DiagramRect box = ToolbarButtonBoxRect(i, 96);
        lefts = lefts && box.Left == expectedLeft[i] && box.Right - box.Left == 24 && box.Bottom - box.Top == 18;
    }
    for (int dpi : dpis)
    {
        const int band = ScaleToDpi(kToolbarHeight, dpi);
        for (int i = 0; i < kToolbarButtonCount; ++i)
        {
            const DiagramRect r = ToolbarButtonRect(i, dpi);
            if (i > 0)
                ordered = ordered && ToolbarButtonRect(i - 1, dpi).Right < r.Left;
            hit = hit && FindToolbarButtonAt(r.Left, r.Top, dpi) == i &&
                FindToolbarButtonAt(r.Right - 1, r.Bottom - 1, dpi) == i &&
                FindToolbarButtonAt(r.Right, r.Top, dpi) != i && FindToolbarButtonAt(r.Left, r.Bottom, dpi) == -1 &&
                FindToolbarButtonAt(r.Left - 1, r.Top, dpi) != i && r.Bottom <= band;
        }
        // 読込みと開始の間にボタン 1 つ分の空き
        const DiagramRect load = ToolbarButtonRect(static_cast<int>(ToolbarButton::Load), dpi);
        const DiagramRect start = ToolbarButtonRect(static_cast<int>(ToolbarButton::Start), dpi);
        ordered = ordered && start.Left - load.Right >= ScaleToDpi(kSimButtonPitch, dpi);
        hit = hit && FindToolbarButtonAt(ScaleToDpi(76, dpi), (load.Top + load.Bottom) / 2, dpi) == -1 &&
            FindToolbarButtonAt(ScaleToDpi(10, dpi), band + 5, dpi) == -1;

        for (int i = 0; i < kSimButtonCount; ++i)
        {
            const DiagramRect a = SimButtonRect(i, dpi);
            const DiagramRect b = ToolbarButtonRect(i + kFileButtonCount, dpi);
            aliases = aliases && a.Left == b.Left && a.Right == b.Right && a.Top == b.Top && a.Bottom == b.Bottom &&
                FindSimButtonAt((a.Left + a.Right) / 2, (a.Top + a.Bottom) / 2, dpi) == i;
        }
        for (int i = 0; i < kFileButtonCount; ++i)
        {
            const DiagramRect r = ToolbarButtonRect(i, dpi);
            aliases = aliases && FindSimButtonAt((r.Left + r.Right) / 2, (r.Top + r.Bottom) / 2, dpi) == -1;
        }

        placed = placed && SimButtonRect(2, dpi).Right < ScaleToDpi(kFrameDisplayHitLeft, dpi) &&
            SpeedLabelRect(kMaxSpeed, dpi).Right <= ScaleToDpi(kDiagramWidth, dpi) &&
            SpeedLabelRect(kMaxSpeed, dpi).Bottom <= band;
    }
    Check(lefts, "023 FR-001/FR-002: 保存・読込み・開始・中断・終了の箱は x 8, 36, 92, 120, 148（24 x 18、96 DPI）");
    Check(ordered, "023 FR-001: ボタンは重ならずに左から右に並び、読込みと開始の間にボタン 1 つ分の空きがある（DPI 96/120/144）");
    Check(hit, "023 FR-007: FindToolbarButtonAt は [Left, Right) x [Top, Bottom) と一致し、空きの位置と帯より下では -1 になる");
    Check(aliases, "023: SimButtonRect(i) == ToolbarButtonRect(i + 2) で、FindSimButtonAt は保存・読込みの上で -1 になる");
    Check(placed, "023: チェックボックスは終了ボタンの右にあり、+10 の数字もツールバーの帯とウィンドウ幅に収まる");

    bool states = true;
    const RunPhase phases[] = { RunPhase::Idle, RunPhase::Running, RunPhase::Paused };
    for (RunPhase phase : phases)
        for (int editing = 0; editing < 2; ++editing)
        {
            const SimCommandStates s = GetSimCommandStates(phase, editing != 0);
            const bool file = phase == RunPhase::Idle;
            states = states &&
                IsToolbarButtonEnabled(static_cast<int>(ToolbarButton::Save), phase, editing != 0) == file &&
                IsToolbarButtonEnabled(static_cast<int>(ToolbarButton::Load), phase, editing != 0) == file &&
                IsToolbarButtonEnabled(static_cast<int>(ToolbarButton::Start), phase, editing != 0) == s.Start &&
                IsToolbarButtonEnabled(static_cast<int>(ToolbarButton::Pause), phase, editing != 0) == s.Pause &&
                IsToolbarButtonEnabled(static_cast<int>(ToolbarButton::Stop), phase, editing != 0) == s.Stop;
        }
    Check(states, "023 FR-006: 保存・読込みは待機中だけ有効（編集中も有効）。開始・中断・終了は GetSimCommandStates のとおり");

    // 改訂 1 A: チェックボックスとその文字は、ボタンの箱と縦の中心をそろえる。
    bool aligned = true;
    for (int dpi : dpis)
    {
        const DiagramRect stop = ToolbarButtonBoxRect(static_cast<int>(ToolbarButton::Stop), dpi);
        const DiagramRect box = FrameDisplayBoxRect(dpi);
        const DiagramRect label = FrameDisplayLabelRect(dpi);
        const int stopMid = stop.Top + stop.Bottom;   // 中心を 2 倍にして半ピクセルを保つ
        const int boxMid = box.Top + box.Bottom;
        const int labelMid = label.Top + label.Bottom;
        aligned = aligned && std::abs(boxMid - stopMid) <= 2 && std::abs(labelMid - stopMid) <= 2 &&
            box.Top >= ScaleToDpi(kFrameDisplayHitTop, dpi) && label.Top >= ScaleToDpi(kFrameDisplayHitTop, dpi) &&
            box.Bottom <= ScaleToDpi(kToolbarHeight, dpi);
    }
    Check(aligned, "023 改訂 1 A: チェックボックスと文字は終了ボタンの箱の中心に 1 px 以内で揃い、当たり判定の範囲の中にある（DPI 96/120/144）");
}
