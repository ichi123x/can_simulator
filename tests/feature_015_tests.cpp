// feature_015_tests.cpp（tests）
// tools/cansim_runner/checks.cpp の CheckFeature015() から移した（機能 030）。
// 本体は、パスの取り方以外は変えていない。Check() のラベルには SC/FR の ID が残っている。

#include "test_helpers.h"

using namespace cansimtest;

TEST(Feature015, SC001_FR002to005_FR007_FR009_FR012)
{
    std::cout << "\n== 機能 015: スピードスライダー ==\n";
    const int dpis[] = { 96, 120, 144 };
    bool even = true, placed = true, labels = true, thumbs = true, nearest = true, hit = true;
    for (int dpi : dpis)
    {
        const int pitch = ScaleToDpi(kSpeedTickPitch, dpi);
        for (int s = kMinSpeed + 1; s <= kMaxSpeed; ++s)
        {
            const int d = SpeedTickX(s, dpi) - SpeedTickX(s - 1, dpi);
            even = even && d >= pitch - 1 && d <= pitch + 1;
            // 019: 今の値で表示される数字だけが重ならなければよい
            for (int cur = kMinSpeed; cur <= kMaxSpeed; ++cur)
                for (int prev = kMinSpeed; prev < s; ++prev)
                    if (IsSpeedLabelShown(prev, cur) && IsSpeedLabelShown(s, cur))
                        labels = labels && SpeedLabelRect(prev, dpi).Right <= SpeedLabelRect(s, dpi).Left;
        }
        const int band = ScaleToDpi(kToolbarHeight, dpi);
        const int hitLeft = SpeedTickX(kMinSpeed, dpi) - ScaleToDpi(kSpeedTickPitch / 2, dpi);
        placed = placed && hitLeft > SimButtonRect(2, dpi).Right &&
            SpeedLabelRect(kMaxSpeed, dpi).Right <= ScaleToDpi(kDiagramWidth, dpi) &&
            SpeedLabelRect(kMaxSpeed, dpi).Bottom <= band && ScaleToDpi(kSpeedHitBottom, dpi) <= band &&
            SpeedThumbRect(kMaxSpeed, dpi).Bottom <= band;
        for (int s = kMinSpeed; s <= kMaxSpeed; ++s)
        {
            const DiagramRect th = SpeedThumbRect(s, dpi);
            const int x = SpeedTickX(s, dpi);
            thumbs = thumbs && th.Left <= x && x < th.Right;
            nearest = nearest && SpeedAtX(x, dpi) == s;
            if (s < kMaxSpeed)
            {
                const int next = SpeedTickX(s + 1, dpi);
                nearest = nearest && SpeedAtX(x + (next - x) / 2 - 1, dpi) == s &&
                    SpeedAtX(x + (next - x) / 2 + 2, dpi) == s + 1;
            }
            const DiagramRect lb = SpeedLabelRect(s, dpi);
            hit = hit && IsOnSpeedSlider(x, (th.Top + th.Bottom) / 2, dpi) &&
                IsOnSpeedSlider(x, (lb.Top + lb.Bottom) / 2, dpi);
        }
        nearest = nearest && SpeedAtX(0, dpi) == kMinSpeed && SpeedAtX(ScaleToDpi(2000, dpi), dpi) == kMaxSpeed;
        const DiagramRect stop = SimButtonRect(2, dpi);
        hit = hit && !IsOnSpeedSlider((stop.Left + stop.Right) / 2, (stop.Top + stop.Bottom) / 2, dpi) &&
            !IsOnSpeedSlider(SpeedTickX(0, dpi), band + 1, dpi) &&
            !IsOnSpeedSlider(ScaleToDpi(kSpeedSliderLeft - kSpeedTickPitch / 2, dpi) - 1, ScaleToDpi(20, dpi), dpi) &&
            IsOnSpeedSlider(ScaleToDpi(kSpeedSliderLeft - kSpeedTickPitch / 2, dpi), ScaleToDpi(20, dpi), dpi);
    }
    Check(even, "015 FR-002 / 026: -10 から +10 まで 21 個の目盛りが等間隔に並ぶ（DPI 96/120/144）");
    Check(placed, "015 FR-002/FR-012: スライダーは終了ボタンの右にあり、ツールバーの帯とウィンドウ幅の中に収まる");
    Check(labels, "015 FR-004 / 019 FR-004: 表示される目盛りの数字は、どの現在値でも重ならない");
    Check(thumbs, "015 FR-003: つまみはスピードの目盛りの上に描かれる");
    Check(nearest, "015 FR-005/SC-001: SpeedAtX は一番近い目盛りに丸め、範囲の外では両端の値になる");
    Check(hit, "015 FR-005: IsOnSpeedSlider はつまみ・溝・数字を含み、ボタンや帯より下は含まない");

    struct Row { RunPhase Phase; int Speed; SpeedChangeAction Action; };
    const Row rows[] = {
        { RunPhase::Idle, kMinSpeed, SpeedChangeAction::None }, { RunPhase::Idle, 0, SpeedChangeAction::None },
        { RunPhase::Idle, 9, SpeedChangeAction::None }, { RunPhase::Idle, 10, SpeedChangeAction::None },
        { RunPhase::Running, kMinSpeed, SpeedChangeAction::Rebase }, { RunPhase::Running, 0, SpeedChangeAction::Rebase },
        { RunPhase::Running, 9, SpeedChangeAction::Rebase }, { RunPhase::Running, 10, SpeedChangeAction::FinishNow },
        { RunPhase::Paused, kMinSpeed, SpeedChangeAction::None }, { RunPhase::Paused, 0, SpeedChangeAction::None },
        { RunPhase::Paused, 9, SpeedChangeAction::None }, { RunPhase::Paused, 10, SpeedChangeAction::FinishNow },
    };
    bool actions = true;
    for (const Row& r : rows)
        actions = actions && GetSpeedChangeAction(r.Phase, r.Speed) == r.Action;
    Check(actions, "015 FR-007..FR-009: スピードの変更は、実行中は基準を取り直し、中断中は再開まで待ち、+10 はその場で完了する");
}
