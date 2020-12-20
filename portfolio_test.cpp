#include <glog/logging.h>

#include "gtest/gtest.h"
#include "portfolio.h"

TEST(PortfolioTest, index) {
  Portfolio folio(100.0, {"FOO", "BAR", "BAZ"});
  EXPECT_EQ(folio.index("FOO"), 0);
  EXPECT_EQ(folio.index("BAR"), 1);
  EXPECT_EQ(folio.index("BAZ"), 2);
}

TEST(PortfolioTest, shares) {
  Portfolio folio(100.0, {"FOO", "BAR"});
  EXPECT_EQ(folio.shares(folio.index("BAR")), 0.0);
}

TEST(PortfolioTest, buy) {
  Portfolio folio(100.0, {"FOO", "BAR"});
  EXPECT_EQ(folio.shares(folio.index("BAR")), 0.0);

  folio.buy(/*symbol_index=*/1, /*quantity=*/1, /*price=*/50.0);
  EXPECT_EQ(folio.shares(folio.index("BAR")), 1.0);

  folio.buy(/*symbol_index=*/1, /*quantity=*/1, /*price=*/50.0);
  EXPECT_EQ(folio.shares(folio.index("BAR")), 2.0);
}

TEST(PortfolioTest, sell) {
  Portfolio folio(100.0, {"FOO", "BAR"});

  folio.buy(/*symbol_index=*/0, /*quantity=*/1, /*price=*/50.0);
  EXPECT_EQ(folio.shares(folio.index("FOO")), 1.0);
  folio.buy(/*symbol_index=*/1, /*quantity=*/1, /*price=*/50.0);
  EXPECT_EQ(folio.shares(folio.index("BAR")), 1.0);

  folio.sell(/*symbol_index=*/0, /*quantity=*/1, /*price=*/20.0);
  EXPECT_EQ(folio.shares(folio.index("FOO")), 0.0);

  folio.sell(/*symbol_index=*/1, /*quantity=*/1, /*price=*/10.0);
  EXPECT_EQ(folio.shares(folio.index("BAR")), 0.0);

  EXPECT_EQ(folio.cash(), 30.0);
}

TEST(PortfolioTest, value) {
  Portfolio folio(110.0, {"FOO", "BAR"});

  folio.buy(/*symbol_index=*/0, /*quantity=*/1, /*price=*/50.0);
  folio.buy(/*symbol_index=*/1, /*quantity=*/1, /*price=*/50.0);

  // Should be $10 cash, 1 share each.
  EXPECT_EQ(folio.value({200.0, 3000.0}), 3210.0);
}

int main(int argc, char **argv) {
  google::InitGoogleLogging(argv[0]);
  ::testing::InitGoogleTest(&argc, argv);
  google::InstallFailureSignalHandler();
  return RUN_ALL_TESTS();
}
