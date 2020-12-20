#include <glog/logging.h>
#include <random>

#include "gtest/gtest.h"

#include "util.h"

TEST(UtilTest, WelfordMean) {
  std::default_random_engine generator;
  std::normal_distribution<double> norm_dist(0.0, 1.0);

  double sum = 0.0;
  WelfordRunningStatistics stats;
  for (int i = 0; i < 100; i++) {
    double val = norm_dist(generator);
    sum += val;
    stats.update(val);
  }

  EXPECT_NEAR(sum / 100, stats.mean(), 1e-9);
}

int main(int argc, char **argv) {
  google::InitGoogleLogging(argv[0]);
  ::testing::InitGoogleTest(&argc, argv);
  google::InstallFailureSignalHandler();
  return RUN_ALL_TESTS();
}
