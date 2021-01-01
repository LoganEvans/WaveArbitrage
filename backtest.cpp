#include <stdio.h>

#include <algorithm>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "feed.h"
#include "market_data.pb.h"
#include "strategy.h"
#include "util.h"

void job(std::unique_ptr<Feed> feed, double cash, double rebalance_threshold,
         StreamIntervalStatistics *bh_stats,
         StreamIntervalStatistics *wave_stats) {
  BuyAndHold bh(cash, feed->symbols(), feed->prices());
  WaveArbitrage wave(cash, feed->symbols(), feed->prices(),
                     rebalance_threshold);

  while (true) {
    FeedStatus fs = feed->adjust_prices();

    if (fs & FEED_DIVIDEND) {
      for (size_t i = 0; i < feed->dividends().size(); i++) {
        if (feed->dividends()[i] != 0.0) {
          bh.pay_dividend(feed->symbols()[i], feed->dividends()[i]);
          wave.pay_dividend(feed->symbols()[i], feed->dividends()[i]);
        }
      }
    }

    if (fs & FEED_SPLIT) {
      for (size_t i = 0; i < feed->splits().size(); i++) {
        if (feed->splits()[i] != 0.0) {
          bh.stock_split(feed->symbols()[i], 1.0 / feed->splits()[i]);
          wave.stock_split(feed->symbols()[i], 1.0 / feed->splits()[i]);
        }
      }
    }

    if (fs & FEED_END) {
      break;
    }

    bool price_threshold = true;
    for (auto price : feed->prices()) {
      if (price < 5.0) {
        price_threshold = false;
      }
    }
    if (!price_threshold) {
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

int main(int argc, char **argv) {
  GOOGLE_PROTOBUF_VERIFY_VERSION;
  google::InitGoogleLogging(argv[0]);

  static constexpr double cash = 100000.0;
  static constexpr double rebalance_threshold = 1.0;
  Duration dur;
  dur.set_seconds(365 * 24 * 60 * 60);
  StreamIntervalStatistics bh_stats(dur);
  StreamIntervalStatistics wave_stats(dur);

  if (false) {
    static constexpr double dt = 1.0 / 252;
    static constexpr double sigma = 1.0 / 252;
    std::unique_ptr<Feed> feed = std::make_unique<RandomFeed>(RandomFeed(
        /*symbols=*/{"FOO", "BAR"}, /*prices=*/{10.0, 10.0},
        /*gbm_dt=*/dt, /*gbm_sigma=*/sigma,
        /*lifespan=*/5 * 365 * 24 * 60 * 60));
    job(/*feed=*/std::move(feed), /*cash=*/cash,
        /*rebalance_threshold=*/rebalance_threshold,
        /*bh_stats=*/&bh_stats, /*wave_stats=*/&wave_stats);
  } else if (false) {
    std::unique_ptr<Feed> feed = std::make_unique<IEXFeed>(IEXFeed(
        /*symbols=*/{"AIV", "XRX"}));
    ///*symbols=*/{"F", "ZION"}));
    ///*symbols=*/{"AMZN", "WMT"}));
    ///*symbols=*/{"GOOG", "FB"}));
    printf("%s\n", feed->to_string().c_str());
    job(/*feed=*/std::move(feed), /*cash=*/cash,
        /*rebalance_threshold=*/rebalance_threshold,
        /*bh_stats=*/&bh_stats, /*wave_stats=*/&wave_stats);
  } else {
    std::set<std::string> split_set = {
        "AFL", "AIV",  "BF-B", "BLL", "CHK",  "CMCSA", "CNX", "CTXS",
        "DD",  "DOV",  "EQT",  "EW",  "FAST", "FMC",   "FTI", "GE",
        "HON", "HSIC", "ISRG", "LEN", "MET",  "MKC",   "NEE", "PFE",
        "PNR", "TGNA", "VAR",  "VFC", "VNO",  "XRX"};
    std::set<std::string> blacklist_set = {
        "FTR",
    };
    std::vector<string> symbols;
    for (auto symbol : get_available_symbols()) {
      if (split_set.find(symbol) == split_set.end() &&
          blacklist_set.find(symbol) == blacklist_set.end()) {
        symbols.push_back(symbol);
      }
    }

    std::uniform_int_distribution<int> uniform_dist(0, symbols.size() - 1);
    std::default_random_engine generator;
    for (int trial = 0; trial < 500; trial++) {
      int i = uniform_dist(generator);
      int j;
      do {
        j = uniform_dist(generator);
      } while (j == i);
      std::unique_ptr<Feed> feed = std::make_unique<IEXFeed>(IEXFeed(
          /*symbols=*/{symbols[i], symbols[j]}));
      job(/*feed=*/std::move(feed), /*cash=*/cash,
          /*rebalance_threshold=*/rebalance_threshold,
          /*bh_stats=*/&bh_stats, /*wave_stats=*/&wave_stats);
      printf("%s\n",
             bh_stats.hist()
                 ->json(/*title=*/"",
                        /*label=*/"bh -- mean: " +
                            std::to_string(bh_stats.stats().mean()) + " var: " +
                            std::to_string(bh_stats.stats().sample_variance()))
                 .c_str());
      printf(
          "%d, %d, %s\n", i, j,
          wave_stats.hist()
              ->json(/*title=*/"",
                     /*label=*/"wave -- mean: " +
                         std::to_string(wave_stats.stats().mean()) + " var: " +
                         std::to_string(wave_stats.stats().sample_variance()))
              .c_str());
      bh_stats.reset_interval();
      wave_stats.reset_interval();
    }
  }

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
