#include <gtest/gtest.h>

bool IsValidDlc(int dlc) {
    return 0 <= dlc && dlc <= 8;
}

TEST(Practice, DlcRange)
{
    EXPECT_TRUE(IsValidDlc(0));
    EXPECT_TRUE(IsValidDlc(8));
    EXPECT_FALSE(IsValidDlc(9));
    EXPECT_FALSE(IsValidDlc(-1));
}

// ① 表の 1 行の形：入力と、期待する答え
struct DlcCase {
    int  dlc;
    bool expected;
};


void PrintTo(const DlcCase& c, std::ostream* os) {
    *os << "dlc=" << c.dlc << " expected=" << (c.expected ? "true" : "false");
}

// ② 「表を使うテストです」という宣言（決まり文句）
class DlcTest : public ::testing::TestWithParam<DlcCase> {};

// ③ テストの中身：表の 1 行ぶんだけ書く
TEST_P(DlcTest, Range) {
    DlcCase c = GetParam();               // いまの行を取り出す
    EXPECT_EQ(c.expected, IsValidDlc(c.dlc));
}

// ④ 表そのもの
INSTANTIATE_TEST_SUITE_P(Practice, DlcTest, ::testing::Values(
    DlcCase{ 0, true  },
    DlcCase{ 4, true  },
    DlcCase{ 8, true  },
    DlcCase{ 9, false },
    DlcCase{-1, false }
));

