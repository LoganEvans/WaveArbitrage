#include <glog/logging.h>
#include <google/protobuf/timestamp.pb.h>

#include "gtest/gtest.h"
#include "feed.h"

using ::google::protobuf::Timestamp;
using ::std::string;

TEST(FeedTest, Random) {
  RandomFeed feed(/*symbols=*/{"FOO", "BAR"}, {10.0, 10.0}, 1.0 / 252, 1.0 / 252,
                  /*lifespan=*/10);
  EXPECT_EQ(feed.prices()[0], 10.0);

  Timestamp before = feed.timestamp();

  // The normal generator *could* produce a 0.0, but the odds of it doing so 10
  // times in a row are vanishingly small.
  int count = 0;
  for (int i = 0; i < 10; i++) {
    feed.adjust_prices();
    if (feed.prices()[0] != 1.0) {
      count++;
    }
  }
  EXPECT_NE(count, 0);
  EXPECT_NE(feed.timestamp().seconds(), before.seconds());
}

TEST(FeedTest, IEX) {
  IEXFeed feed(/*symbols=*/{"GOOG", "FB"});
  feed.adjust_prices();

  for (int i = 0; i < 10000; i++) {
    EXPECT_EQ(feed.adjust_prices(), Feed::FEED_OK);
  }
}

int main(int argc, char **argv) {
  google::InitGoogleLogging(argv[0]);
  ::testing::InitGoogleTest(&argc, argv);
  google::InstallFailureSignalHandler();
  return RUN_ALL_TESTS();
}
