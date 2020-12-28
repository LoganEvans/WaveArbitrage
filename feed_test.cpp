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

  FeedStatus fs = FEED_OK;
  while (fs == FEED_OK) {
    fs = feed.adjust_prices();
  }

  EXPECT_EQ(fs, FEED_DAY_CHANGE);
  EXPECT_EQ(feed.adjust_prices(), FEED_OK);
}

TEST(FeedTest, dividends) {
  IEXFeed feed(/*symbols=*/{"F"});

  FeedStatus fs = FEED_OK;
  while (!(fs & FEED_END) && !(fs & FEED_DIVIDEND)) {
    EXPECT_EQ(feed.dividends()[0], 0.0);
    fs = feed.adjust_prices();
  }

  EXPECT_TRUE(fs & FEED_DIVIDEND);
  EXPECT_NEAR(feed.dividends()[0], 0.2, 1e-5);
}

TEST(FeedTest, splits) {
  IEXFeed feed(/*symbols=*/{"GE"});

  FeedStatus fs = FEED_OK;
  while (!(fs & FEED_END) && !(fs & FEED_SPLIT)) {
    EXPECT_EQ(feed.splits()[0], 0.0);
    fs = feed.adjust_prices();
  }

  EXPECT_TRUE(fs & FEED_SPLIT);
  EXPECT_NEAR(feed.splits()[0], 0.9615384615384616, 1e-5);
}

int main(int argc, char **argv) {
  google::InitGoogleLogging(argv[0]);
  ::testing::InitGoogleTest(&argc, argv);
  google::InstallFailureSignalHandler();
  return RUN_ALL_TESTS();
}
