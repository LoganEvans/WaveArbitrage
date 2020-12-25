#include <glog/logging.h>
#include <google/protobuf/util/time_util.h>
#include <memory>
#include <random>

#include "gtest/gtest.h"

#include "util.h"

using ::google::protobuf::Duration;
using ::google::protobuf::Timestamp;

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

TEST(UtilTest, StreamInterval) {
  std::default_random_engine generator;
  std::normal_distribution<double> norm_dist(10.0, 1.0);

  Timestamp timestamp;
  Duration dur;
  dur.set_seconds(36000);
  dur.set_nanos(500000);

  StreamIntervalStatistics si_stats(dur);

  for (int i = 0; i < 100000; i++) {
    double val = norm_dist(generator);
    timestamp.set_seconds(timestamp.seconds() + 360 +
                          10 * norm_dist(generator));
    si_stats.update(val, std::make_shared<Timestamp>(timestamp));
  }

  EXPECT_NEAR(si_stats.hist()->getQuantileEstimate(0.5), 1.0, 0.1);
}

int main(int argc, char **argv) {
  google::InitGoogleLogging(argv[0]);
  ::testing::InitGoogleTest(&argc, argv);
  google::InstallFailureSignalHandler();
  return RUN_ALL_TESTS();
}
