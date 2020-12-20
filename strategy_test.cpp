#include "gtest/gtest.h"
#include "strategy.h"

TEST(StrategyTest, BuyAndHold) {
  std::vector<double> prices = {10.0, 5.0};
  BuyAndHold bh(1000, {"FOO", "BAR"}, prices);
  EXPECT_EQ(bh.portfolio().value(prices), 1000);
  EXPECT_EQ(bh.portfolio().shares(0), 50);
  EXPECT_EQ(bh.portfolio().shares(1), 100);
  EXPECT_EQ(bh.portfolio().shares(0) * prices[0],
            bh.portfolio().shares(1) * prices[1]);
}

TEST(StrategyTest, WaveArbitrage) {
  WaveArbitrage wave(1000, {"FOO", "BAR"}, {10.0, 5.0}, 1.0);
  EXPECT_FALSE(wave.price_event({5.0, 10.0})) << wave.to_string();
}

int main(int argc, char **argv) {
  google::InitGoogleLogging(argv[0]);
  ::testing::InitGoogleTest(&argc, argv);
  google::InstallFailureSignalHandler();
  return RUN_ALL_TESTS();
}
