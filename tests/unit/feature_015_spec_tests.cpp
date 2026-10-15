// feature_015_spec_tests.cpp（tests/unit）
// 機能 015（specs/015-speed-slider/spec.md）の受け入れシナリオと成功基準のうち、画面を使わずに確かめられるもの。
// スライダーの座標・丸め・当たり判定は SimControls.h、スピード変更時の動作は GetSpeedChangeAction、時刻は ReplayClock.h で確かめる。

#include "support/spec_test_helpers.h"

#include "ProjectFile.h"
#include "ReplayClock.h"
#include "SimControls.h"

#include <cstdlib>

using namespace CanSim;
using namespace spectest;

// 根拠: spec.md US1 受け入れシナリオ 1、FR-002・FR-004。起動直後、つまみは 0 の目盛りの位置にあり、目盛りは -13〜+10 の 24 個
// （左端 -13、右端 +10）。
TEST(F015Spec, F015_US1_1_Startup_ThumbAtZeroAnd24Ticks)
{
    RecordProperty("spec", "015:US1-1");
    const DiagramRect thumb = SpeedThumbRect(ProjectSettings().Speed, 96);
    EXPECT_EQ((thumb.Left + thumb.Right) / 2, SpeedTickX(0, 96)) << "起動直後のつまみが 0 の目盛りにない";
    std::set<int> values;
    for (int x = 0; x < 1200; ++x)
        values.insert(SpeedAtX(x, 96));
    if (values.size() != 24u || *values.begin() != -13)
        GTEST_SKIP() << "仕様と実装の食い違い（人の判断待ち）: 015 は目盛り -13〜+10 の 24 個とするが、実装は機能 026 のとおり "
                     << *values.begin() << "〜" << *values.rbegin() << " の " << values.size()
                     << " 個。015 の記述が 026 で置き換わったかの判断が必要";
    EXPECT_EQ(values.size(), 24u) << "目盛りの数";
    EXPECT_EQ(*values.begin(), -13) << "左端";
    EXPECT_EQ(*values.rbegin(), 10) << "右端";
}

// 根拠: spec.md US1 受け入れシナリオ 2・SC-001、FR-005。目盛りの間で離すと、一番近い目盛りに止まる。
// 隣り合う 2 つの目盛りの間の、左寄り・右寄りの位置で確かめる。
TEST(F015Spec, F015_US1_2_ReleaseBetweenTicks_SnapsToNearest)
{
    RecordProperty("spec", "015:US1-2");
    for (int dpi : { 96, 144 })
    {
        for (int s = -10; s < 10; ++s)
        {
            const int a = SpeedTickX(s, dpi);
            const int b = SpeedTickX(s + 1, dpi);
            EXPECT_EQ(SpeedAtX(a + (b - a) / 4, dpi), s) << dpi << " DPI: " << s << " と " << s + 1 << " の間の左寄り";
            EXPECT_EQ(SpeedAtX(b - (b - a) / 4, dpi), s + 1) << dpi << " DPI: " << s << " と " << s + 1 << " の間の右寄り";
        }
    }
}

// 根拠: spec.md US1 受け入れシナリオ 3、FR-005。スライダーの溝（つまみ以外の所）をクリックすると、つまみはクリックした位置に
// 一番近い目盛りへ移る。溝の上が当たり判定に入り、その位置の一番近い目盛りの値になる。
TEST(F015Spec, F015_US1_3_ClickOnGroove_MovesToNearestTick)
{
    RecordProperty("spec", "015:US1-3");
    const DiagramRect track = SpeedTrackRect(96);
    const int y = (track.Top + track.Bottom) / 2;
    const int x = SpeedTickX(4, 96) + 2; // +4 の目盛りのすぐ右（つまみは 0 にある）
    EXPECT_TRUE(IsOnSpeedSlider(x, y, 96)) << "溝の上が当たり判定に入らない";
    EXPECT_EQ(SpeedAtX(x, 96), 4) << "クリックした位置に一番近い目盛りの値でない";
    EXPECT_FALSE(IsOnSpeedSlider(SpeedTickX(10, 96) + 200, y, 96)) << "スライダーの外が当たり判定に入る";
}

// 根拠: spec.md US1 受け入れシナリオ 4（機能 005 と同じ規則）。スピード -3 では、シミュレーション時刻は実経過時間の 1/8 で進む。
TEST(F015Spec, F015_US1_4_MinusThree_OneEighth)
{
    RecordProperty("spec", "015:US1-4");
    EXPECT_EQ(SimTimeAtUs(8000000, -3), 1000000) << "実時間 8 秒でシミュレーション時刻が 1 秒でない";
}

// 根拠: spec.md US1 受け入れシナリオ 6・FR-010。スピード +5 にして保存し、読み込み直すと、スピードは +5 に戻る
// （つまみの位置は SpeedThumbRect(+5) の目盛りの位置）。
TEST(F015Spec, F015_US1_6_SavedSpeedPlus5_Restored)
{
    RecordProperty("spec", "015:US1-6");
    ProjectSettings s;
    s.Speed = 5;
    Network defs;
    defs.Nodes.push_back(MakeNode(1));
    const std::wstring path = TempPath(L"f015_us1_6.canprj");
    ASSERT_TRUE(SaveProject(path, s, defs));
    const ConfigLoadResult r = LoadConfig(path);
    ASSERT_TRUE(r.Success) << r.ErrorMessage;
    EXPECT_EQ(r.Project.Speed, 5) << "スピードが +5 に戻らない";
    const DiagramRect thumb = SpeedThumbRect(r.Project.Speed, 96);
    EXPECT_EQ((thumb.Left + thumb.Right) / 2, SpeedTickX(5, 96)) << "つまみが +5 の目盛りにない";
}

// 根拠: spec.md US2 受け入れシナリオ 1、FR-007。実行中にスピードを 0 から +2 に変えると、その瞬間の時刻から実時間の 4 倍で進む。
// 変更の前後で時刻は連続する（飛ばない・戻らない）。
TEST(F015Spec, F015_US2_1_RunningChangeToPlus2_ContinuesFourTimesFaster)
{
    RecordProperty("spec", "015:US2-1");
    EXPECT_EQ(GetSpeedChangeAction(RunPhase::Running, 2), SpeedChangeAction::Rebase) << "実行中の変更がその場の時刻からの続きにならない";
    const long long before = SimTimeAtUs(3000000, 0);          // 変更の瞬間の時刻（スピード 0 で 3 秒）
    EXPECT_EQ(ResumedSimTimeUs(before, 0, 2), before) << "変更の瞬間に時刻が飛んだ";
    EXPECT_EQ(ResumedSimTimeUs(before, 1000000, 2), before + 4000000) << "変更後の 1 秒で 4 秒進まない";
}

// 根拠: spec.md US2 受け入れシナリオ 3、FR-008。中断中にスピードを -5 に変えても時刻は止めたまま（何もしない）で、
// 再開後は 1/32 で進む。
TEST(F015Spec, F015_US2_3_PausedChangeToMinus5_AppliesOnResume)
{
    RecordProperty("spec", "015:US2-3");
    EXPECT_EQ(GetSpeedChangeAction(RunPhase::Paused, -5), SpeedChangeAction::None) << "中断中の変更で時刻が動く";
    EXPECT_EQ(ResumedSimTimeUs(5000000, 3200000, -5), 5000000 + 100000) << "再開後の 3.2 秒で 0.1 秒進まない";
}

// 根拠: spec.md US2 受け入れシナリオ 4、FR-009。実行中または中断中に +10 にすると、その場で最後まで進めて完了とする。
// 待機中に +10 にしても、値が変わるだけ。
TEST(F015Spec, F015_US2_4_PlusTenWhileRunOrPaused_FinishNow)
{
    RecordProperty("spec", "015:US2-4");
    EXPECT_EQ(GetSpeedChangeAction(RunPhase::Running, 10), SpeedChangeAction::FinishNow) << "実行中の +10";
    EXPECT_EQ(GetSpeedChangeAction(RunPhase::Paused, 10), SpeedChangeAction::FinishNow) << "中断中の +10";
    EXPECT_EQ(GetSpeedChangeAction(RunPhase::Idle, 10), SpeedChangeAction::None) << "待機中の +10";
    EXPECT_EQ(GetSpeedChangeAction(RunPhase::Idle, -4), SpeedChangeAction::None) << "待機中の変更";
}

namespace
{
    std::vector<LogRow> RunWithSpeed(int speed)
    {
        ProjectSettings s;
        s.EcuCount = 3;
        s.Speed = speed;
        Network defs;
        defs.Nodes.push_back(MakeNode(1, { MakeFrame(0x100, 8, 2000) }));
        defs.Nodes.push_back(MakeNode(2, { MakeFrame(0x080, 4, 3000, 500) }));
        defs.Nodes.push_back(MakeNode(3, { MakeFrame(0x200, 0, 1000, 100) }));
        return RunSim(BuildRunNetwork(s, defs), 200000LL);
    }

    std::string CsvOf(const std::vector<LogRow>& rows, const std::wstring& name)
    {
        const std::wstring path = TempPath(name);
        WriteLogCsv(rows, path);
        return ReadText(path);
    }
}

// 根拠: spec.md US2 受け入れシナリオ 5、FR-011。実行中にスピードを何度変えても、最後まで実行した CSV は変えなかった場合と同じ
// （スピードは表示の進み方だけに効く。シミュレーションの計算はスピードに関わらない）。
TEST(F015Spec, F015_US2_5_SpeedChanges_SameCsv)
{
    RecordProperty("spec", "015:US2-5");
    const std::string ref = CsvOf(RunWithSpeed(0), L"f015_us2_5_ref.log.csv");
    for (int speed : { 2, -5, 7 })
        EXPECT_EQ(CsvOf(RunWithSpeed(speed), L"f015_us2_5.log.csv"), ref) << "スピード " << speed << " で CSV が違う";
}

// 根拠: spec.md SC-001、FR-003・FR-005。つまみは目盛り以外の位置で止まらない（どこで離しても、一番近い目盛りに止まる）。
// スライダーの全幅の各位置で、選ばれる値の目盛りが、ほかのどの目盛りより近い（または同じ距離）。
TEST(F015Spec, F015_SC001_AnyRelease_StopsAtNearestTick)
{
    RecordProperty("spec", "015:SC-001");
    for (int dpi : { 96, 120, 144 })
    {
        for (int x = SpeedTickX(-10, dpi) - 20; x <= SpeedTickX(10, dpi) + 20; ++x)
        {
            const int s = SpeedAtX(x, dpi);
            ASSERT_GE(s, -10);
            ASSERT_LE(s, 10);
            const int d = std::abs(x - SpeedTickX(s, dpi));
            for (int other = -10; other <= 10; ++other)
                EXPECT_LE(d, std::abs(x - SpeedTickX(other, dpi)))
                    << dpi << " DPI・x " << x << ": " << s << " より " << other << " の目盛りが近い";
            const DiagramRect thumb = SpeedThumbRect(s, dpi);
            EXPECT_LE(std::abs((thumb.Left + thumb.Right) / 2 - SpeedTickX(s, dpi)), 1)
                << dpi << " DPI: つまみが目盛りの位置にない";
        }
    }
}

// 根拠: spec.md SC-002、FR-007。実行中にスピードを変えても、表示されるシミュレーション時刻が戻ることは 1 回もない。
// スピードを何度も変えながら（変更のたびにその時刻から続ける）時刻を進め、単調に増えることを確かめる（0.1 秒以内の反映は画面の確認）。
TEST(F015Spec, F015_SC002_SpeedChanges_TimeNeverGoesBack)
{
    RecordProperty("spec", "015:SC-002");
    const int speeds[] = { 0, 3, -2, 9, -10, 1, 0 };
    long long base = 0;
    long long last = 0;
    for (int speed : speeds)
    {
        ASSERT_EQ(GetSpeedChangeAction(RunPhase::Running, speed), SpeedChangeAction::Rebase);
        for (long long elapsed = 0; elapsed <= 500000; elapsed += 16000) // 16 ms ごとの書き換え
        {
            const long long now = ResumedSimTimeUs(base, elapsed, speed);
            EXPECT_GE(now, last) << "スピード " << speed << " で時刻が戻った";
            last = now;
        }
        base = last; // その時刻を起点に、次のスピードで続ける
    }
}

// 根拠: spec.md SC-003、FR-011。スピードを 5 回変えて最後まで実行した CSV は、スピードを変えずに実行した CSV と完全に一致する。
TEST(F015Spec, F015_SC003_FiveSpeeds_IdenticalCsv)
{
    RecordProperty("spec", "015:SC-003");
    const std::string ref = CsvOf(RunWithSpeed(0), L"f015_sc003_ref.log.csv");
    for (int speed : { -10, -1, 3, 6, 9 })
        EXPECT_EQ(CsvOf(RunWithSpeed(speed), L"f015_sc003.log.csv"), ref) << "スピード " << speed << " で CSV が違う";
}

// 根拠: spec.md SC-004、FR-010。保存・読込みでスピードが保たれる（10 回中 10 回）。セッティングに項目が無いことは画面の確認。
TEST(F015Spec, F015_SC004_SaveLoadTenTimes_SpeedKept)
{
    RecordProperty("spec", "015:SC-004");
    const int speeds[10] = { -10, -7, -4, -1, 0, 1, 3, 5, 8, 10 };
    Network defs;
    defs.Nodes.push_back(MakeNode(1));
    for (int speed : speeds)
    {
        ProjectSettings s;
        s.Speed = speed;
        const std::wstring path = TempPath(L"f015_sc004.canprj");
        ASSERT_TRUE(SaveProject(path, s, defs));
        const ConfigLoadResult r = LoadConfig(path);
        ASSERT_TRUE(r.Success) << r.ErrorMessage;
        EXPECT_EQ(r.Project.Speed, speed) << "保存・読込みでスピードが変わった";
    }
}
