// feature_014_tests.cpp（tests）
// tools/cansim_runner/checks.cpp の CheckFeature014() から移した（機能 030）。
// 本体は、パスの取り方以外は変えていない。Check() のラベルには SC/FR の ID が残っている。

#include "test_helpers.h"

using namespace cansimtest;

TEST(Feature014, SC003_FR002to004_FR006_FR009)
{
    std::cout << "\n== 機能 014: シミュレーションの操作とツールバー ==\n";

    // FR-004: すべての状態での 開始 / 中断 / 終了。フレーム編集ウィンドウが
    // 開いているときと開いていないときの両方。
    struct Row { RunPhase Phase; bool Editing; bool Start; bool Pause; bool Stop; };
    const Row rows[] = {
        { RunPhase::Idle, false, true, false, false },
        { RunPhase::Idle, true, false, false, false },
        { RunPhase::Running, false, false, true, true },
        { RunPhase::Running, true, false, true, true },
        { RunPhase::Paused, false, true, false, true },
        { RunPhase::Paused, true, true, false, true },
    };
    bool table = true;
    for (const Row& r : rows)
    {
        const SimCommandStates s = GetSimCommandStates(r.Phase, r.Editing);
        table = table && s.Start == r.Start && s.Pause == r.Pause && s.Stop == r.Stop;
    }
    Check(table, "014 FR-004: 開始 / 中断 / 終了の有効・無効が、待機・実行中・中断中の表と一致する");

    // DPI 96/120/144 でのツールバーの配置
    const int dpis[] = { 96, 120, 144 };
    bool inBand = true;
    bool ordered = true;
    bool partsInside = true;
    bool hit = true;
    for (int dpi : dpis)
    {
        const int band = ScaleToDpi(kToolbarHeight, dpi);
        for (int i = 0; i < kSimButtonCount; ++i)
        {
            const DiagramRect r = SimButtonRect(i, dpi);
            const DiagramRect box = SimButtonBoxRect(i, dpi);
            const DiagramRect label = SimButtonLabelRect(i, dpi);
            inBand = inBand && r.Left >= 0 && r.Top >= 0 && r.Bottom <= band && r.Left < r.Right && r.Top < r.Bottom;
            if (i > 0)
                ordered = ordered && SimButtonRect(i - 1, dpi).Right < r.Left;
            partsInside = partsInside &&
                box.Left >= r.Left && box.Right <= r.Right && box.Top >= r.Top && box.Bottom <= label.Top &&
                label.Left >= r.Left && label.Right <= r.Right && label.Bottom <= r.Bottom;
            hit = hit && FindSimButtonAt(r.Left, r.Top, dpi) == i &&
                FindSimButtonAt(r.Right - 1, r.Bottom - 1, dpi) == i &&
                FindSimButtonAt(r.Right, r.Top, dpi) != i && FindSimButtonAt(r.Left, r.Bottom, dpi) == -1 &&
                FindSimButtonAt(r.Left - 1, r.Top, dpi) != i;
            if (i > 0)
                hit = hit && FindSimButtonAt(SimButtonRect(i - 1, dpi).Right, r.Top, dpi) == -1;
        }
        hit = hit && FindSimButtonAt(0, 0, dpi) == -1 && FindSimButtonAt(10, band + 5, dpi) == -1;
    }
    Check(inBand, "014 FR-002/FR-009: 3 つのボタンはツールバーの帯の中にあり、下げた構成図の上にある（DPI 96/120/144）");
    Check(ordered, "014 FR-002: 開始・中断・終了は重ならずに左から右に並ぶ");
    Check(partsInside, "014 FR-002: 各ボタンの箱と名前は当たり判定の範囲の中にあり、名前は箱の下にある");
    Check(hit, "014 FR-003: FindSimButtonAt はボタンの範囲 [Left, Right) x [Top, Bottom) と一致する");

    // FR-006: 再開後の時刻
    bool same = true;
    const long long elapsed[] = { 0, 1, 999, 1000000, 37000001 };
    const int speeds[] = { kMinSpeed, 0, 5 };
    for (long long e : elapsed)
        for (int s : speeds)
            same = same && ResumedSimTimeUs(0, e, s) == SimTimeAtUs(e, s);
    Check(same, "014 FR-006: 基準が 0 なら再開後の時刻は 005 の時計と等しい");

    bool split = true;
    const long long pairs[][2] = { { 0, 5 }, { 3000000, 7000000 }, { 1, 1 }, { 123456, 654321 }, { 9999999, 1 } };
    for (const auto& p : pairs)
        split = split && ResumedSimTimeUs(ResumedSimTimeUs(0, p[0], 0), p[1], 0) == SimTimeAtUs(p[0] + p[1], 0);
    Check(split, "014 SC-003: 中断して再開（スピード 0）しても、中断しない場合と同じ時刻になる");
    Check(ResumedSimTimeUs(LLONG_MAX - 1, 1000000, 0) == LLONG_MAX && ResumedSimTimeUs(-5, 10, 0) == 10,
        "014: 再開後の時刻は飽和し、負の基準は 0 として扱う");
}
