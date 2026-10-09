// feature_018_edge_tests.cpp（tests）
// 流れのエフェクト（FlowEffect）の単体テスト。
// カバレッジで実行されていなかった行（進み f の範囲外、レイアウトに無い ECU、送信 = 受信の組）を通す。
// 期待値は FlowEffect.h のコメントと specs/018 の data-model.md に従う。
// FlowEffect.cpp の残り 2 行は、実際には来ない入力への守りなのでテストしない（2026-10-09 判断）。
//   84 行目 elapsedUs = 0;  時刻は実行中に足していくだけで負にならない
//   96 行目 continue;       別々の ECU は接続線の位置が違い、長さ 0 の道筋はできない

#include "test_helpers.h"

using namespace cansimtest;

namespace
{
    bool SamePoint(const FlowPoint& a, const FlowPoint& b)
    {
        return std::fabs(a.X - b.X) < 1e-9 && std::fabs(a.Y - b.Y) < 1e-9;
    }
}

// f が 0 より小さいときは 0 として扱う（FlowEffect.h「clamped to 0..1」、data-model「進み f（0〜1）」）
TEST(Feature018Edge, FlowPointAt_NegativeF_IsClampedToStart)
{
    const BusDiagramLayout six = ComputeBusDiagramLayout(6);
    const EcuPlacement& s = six.Ecus[0];
    const EcuPlacement& r = six.Ecus[5];

    EXPECT_TRUE(SamePoint(FlowPointAt(s, r, -0.5), FlowPointAt(s, r, 0.0)))
        << "f < 0 のとき、道筋の始点（送信側の箱の縁）にならない";
}

// f が 1 より大きいときは 1 として扱う（同上）
TEST(Feature018Edge, FlowPointAt_FOverOne_IsClampedToEnd)
{
    const BusDiagramLayout six = ComputeBusDiagramLayout(6);
    const EcuPlacement& s = six.Ecus[0];
    const EcuPlacement& r = six.Ecus[5];

    EXPECT_TRUE(SamePoint(FlowPointAt(s, r, 1.5), FlowPointAt(s, r, 1.0)))
        << "f > 1 のとき、道筋の終点（受信側の箱の縁）にならない";
}

// レイアウトに無い ECU 番号の組は無視し、ほかの組には影響しない（data-model「レイアウトに無い ECU 番号は無視する」）
TEST(Feature018Edge, ComputeFlowEffect_UnknownEcu_IsIgnored)
{
    const BusDiagramLayout six = ComputeBusDiagramLayout(6);
    const std::vector<std::pair<int, int>> valid = { { 1, 6 } };
    const std::vector<std::pair<int, int>> withUnknown = { { 1, 6 }, { 1, 7 }, { 9, 2 } };

    const FlowEffect expected = ComputeFlowEffect(six, valid, 12345);
    const FlowEffect actual = ComputeFlowEffect(six, withUnknown, 12345);

    EXPECT_EQ(actual.Particles.size(), expected.Particles.size()) << "無い ECU への組で粒が増減した";
    EXPECT_EQ(actual.Glows.size(), expected.Glows.size()) << "無い ECU が光の対象になった";
}

// 送信 = 受信の組は道筋が無いので無視する（data-model の Flows は「送信 ≠ 受信」。それ以外が来たときの守り）
TEST(Feature018Edge, ComputeFlowEffect_SelfPair_IsIgnored)
{
    const BusDiagramLayout six = ComputeBusDiagramLayout(6);
    const std::vector<std::pair<int, int>> self = { { 3, 3 } };

    const FlowEffect f = ComputeFlowEffect(six, self, 0);

    EXPECT_TRUE(f.Particles.empty()) << "送信 = 受信の組で粒が出た";
    EXPECT_TRUE(f.Glows.empty()) << "送信 = 受信の組で光が出た";
}
