// feature_006_edge_tests.cpp（tests）
// フレームの入力の検査（ValidateFrame）とノードの追加（EnsureNode）の単体テスト。
// カバレッジで実行されていなかった行（ID・DLC・送信周期の空欄、数字でない DLC・送信周期・初回オフセット、
// 数字の無い "0x"、番号の間へのノードの追加）を通す。
// 期待値は specs/006 の spec.md（FR-006）と data-model.md に従う。

#include "test_helpers.h"

using namespace cansimtest;

namespace
{
    FrameInput Input(const std::string& id, const std::string& dlc, const std::string& period,
        const std::string& offset)
    {
        FrameInput in;
        in.IdText = id;
        in.DlcText = dlc;
        in.PeriodMsText = period;
        in.OffsetMsText = offset;
        return in;
    }

    bool Has(const std::vector<FrameIssue>& issues, FrameField field, FrameProblem problem)
    {
        for (const FrameIssue& i : issues)
            if (i.Field == field && i.Problem == problem)
                return true;
        return false;
    }
}

// 空欄の ID・DLC・送信周期は NG（FR-006「ID が 0x000〜0x7FF、DLC が 0〜8、送信周期が 1 以上の整数」、
// data-model の FrameProblem::Empty）。空欄でよいのは初回オフセットだけ（FR-005「空欄は 0」）
TEST(Feature006Edge, Validate_EmptyFields_AreNg)
{
    Network defs;
    FrameDefinition out;

    EXPECT_TRUE(Has(ValidateFrame(defs, 1, -1, Input("", "4", "20", ""), out), FrameField::Id, FrameProblem::Empty))
        << "ID が空欄";
    EXPECT_TRUE(Has(ValidateFrame(defs, 1, -1, Input("0x200", " ", "20", ""), out), FrameField::Dlc, FrameProblem::Empty))
        << "DLC が空欄（空白だけ）";
    EXPECT_TRUE(Has(ValidateFrame(defs, 1, -1, Input("0x200", "4", "", ""), out), FrameField::Period, FrameProblem::Empty))
        << "送信周期が空欄";
}

// 数字でない DLC・送信周期・初回オフセット、数字の無い "0x" は NG（FR-006「整数」、data-model の FrameProblem::NotANumber）
TEST(Feature006Edge, Validate_NotANumber_IsNg)
{
    Network defs;
    FrameDefinition out;

    EXPECT_TRUE(Has(ValidateFrame(defs, 1, -1, Input("0x", "4", "20", ""), out), FrameField::Id, FrameProblem::NotANumber))
        << "ID が 0x だけ";
    EXPECT_TRUE(Has(ValidateFrame(defs, 1, -1, Input("0x200", "four", "20", ""), out), FrameField::Dlc, FrameProblem::NotANumber))
        << "DLC が数字でない";
    EXPECT_TRUE(Has(ValidateFrame(defs, 1, -1, Input("0x200", "4", "1.5", ""), out), FrameField::Period, FrameProblem::NotANumber))
        << "送信周期が整数でない";
    EXPECT_TRUE(Has(ValidateFrame(defs, 1, -1, Input("0x200", "4", "20", "-1"), out), FrameField::Offset, FrameProblem::NotANumber))
        << "初回オフセットが負";
}

// まだ無いノードは、ECU<n> の名前で NodeId の昇順の位置に作る（data-model の EnsureNode）
TEST(Feature006Edge, EnsureNode_InsertsInNodeIdOrder)
{
    Network defs;
    Node n1;
    n1.NodeId = 1;
    Node n3;
    n3.NodeId = 3;
    defs.Nodes = { n1, n3 };

    const Node& added = EnsureNode(defs, 2);

    EXPECT_EQ(added.Name, "ECU2");
    ASSERT_EQ(defs.Nodes.size(), 3u);
    EXPECT_EQ(defs.Nodes[0].NodeId, 1);
    EXPECT_EQ(defs.Nodes[1].NodeId, 2);
    EXPECT_EQ(defs.Nodes[2].NodeId, 3);
}
