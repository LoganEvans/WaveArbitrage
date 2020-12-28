#include <stdio.h>

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "feed.h"
#include "util.h"
#include "market_data.pb.h"
#include "strategy.h"

void job(std::unique_ptr<Feed> feed, double cash, double rebalance_threshold,
         StreamIntervalStatistics *bh_stats,
         StreamIntervalStatistics *wave_stats) {
  BuyAndHold bh(cash, feed->symbols(), feed->prices());
  WaveArbitrage wave(cash, feed->symbols(), feed->prices(),
                     rebalance_threshold);

  while (true) {
    FeedStatus fs = feed->adjust_prices();
    //printf("%lf,%lf\n", feed->prices()[0], feed->prices()[1]);
    if (fs & FEED_DIVIDEND) {
      for (size_t i = 0; i < feed->dividends().size(); i++) {
        if (feed->dividends()[i] != 0.0) {
          printf("paying dividend: %lf on %s\n", feed->dividends()[i],
                 feed->symbols()[i].c_str());
          bh.pay_dividend(feed->symbols()[i], feed->dividends()[i]);
          wave.pay_dividend(feed->symbols()[i], feed->dividends()[i]);
        }
      }
    }
    if (fs & FEED_SPLIT) {
      for (size_t i = 0; i < feed->splits().size(); i++) {
        if (feed->splits()[i] != 0.0) {
          printf("split: %lf on %s\n", feed->splits()[i],
                 feed->symbols()[i].c_str());
          printf("old shares: %lf\n", bh.portfolio().shares(i));
          bh.stock_split(feed->symbols()[i], 1.0 / feed->splits()[i]);
          printf("new shares: %lf\n", bh.portfolio().shares(i));
          wave.stock_split(feed->symbols()[i], 1.0 / feed->splits()[i]);
        }
      }
    }
    if (fs & FEED_END) {
      break;
    }
    bh.price_event(feed->prices());
    wave.price_event(feed->prices());
    auto timestamp = std::make_shared<Timestamp>(feed->timestamp());
    bh_stats->update(bh.portfolio().value(feed->prices()), timestamp);
    wave_stats->update(wave.portfolio().value(feed->prices()), timestamp);
  }

  printf("%s\n", feed->to_string().c_str());
  printf("%s\n", bh.to_string(feed->prices()).c_str());
  printf("%s\n", wave.to_string(feed->prices()).c_str());
  printf("%lf\n", bh.portfolio().value(feed->prices()) -
                      wave.portfolio().value(feed->prices()));
  printf("\n");
}

int main() {
  GOOGLE_PROTOBUF_VERIFY_VERSION;

  static constexpr double cash = 100000.0;
  static constexpr double rebalance_threshold = 1.0;
  Duration dur;
  dur.set_seconds(365 * 24 * 60 * 60);
  StreamIntervalStatistics bh_stats(dur);
  StreamIntervalStatistics wave_stats(dur);

  std::unique_ptr<Feed> feed;
  if (false) {
    static constexpr double dt = 1.0 / 252;
    static constexpr double sigma = 1.0 / 252;
    feed = std::make_unique<RandomFeed>(RandomFeed(
        /*symbols=*/{"FOO", "BAR"}, /*prices=*/{10.0, 10.0},
        /*gbm_dt=*/dt, /*gbm_sigma=*/sigma,
        /*lifespan=*/5 * 365 * 24 * 60 * 60));
  } else {
    //'ABT', 'ABBV', 'ACN', 'ACE', 'ADBE', 'ADT', 'AAP', 'AES', 'AET', 'AFL',
    feed = std::make_unique<IEXFeed>(IEXFeed(
        ///*symbols=*/{"AAP", "AES"}));
        /*symbols=*/{"F", "ZION"}));
        ///*symbols=*/{"AMZN", "WMT"}));
        ///*symbols=*/{"GOOG", "FB"}));
    printf("%s\n", feed->to_string().c_str());
  }

  job(/*feed=*/std::move(feed), /*cash=*/cash,
      /*rebalance_threshold=*/rebalance_threshold,
      /*bh_stats=*/&bh_stats, /*wave_stats=*/&wave_stats);

  printf("%s\n", bh_stats.hist()
                     ->json(/*title=*/"",
                            /*label=*/"bh -- mean: " +
                                std::to_string(bh_stats.stats().mean()))
                     .c_str());
  printf("%s\n", wave_stats.hist()
                     ->json(/*title=*/"",
                            /*label=*/"wave -- mean: " +
                                std::to_string(wave_stats.stats().mean()))
                     .c_str());

  return 0;
}
