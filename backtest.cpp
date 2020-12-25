#include <stdio.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "feed.h"
#include "util.h"
#include "market_data.pb.h"
#include "strategy.h"

std::vector<std::string> get_iex(std::string symbol) {
  static const std::string kProcessedDir = "/home/logan/data/processed/";
  std::string comparison = kProcessedDir + symbol;
  std::vector<std::string> res;
  for (const auto &f : std::filesystem::directory_iterator(kProcessedDir)) {
    if (0 == f.path().string().compare(0, comparison.size(), comparison)) {
      res.push_back(f.path());
    }
  }

  std::sort(res.begin(), res.end());

  return res;
}

void job(std::unique_ptr<Feed> feed, double cash, double rebalance_threshold,
         StreamIntervalStatistics *bh_stats,
         StreamIntervalStatistics *wave_stats) {
  BuyAndHold bh(cash, feed->symbols(), feed->prices());
  WaveArbitrage wave(cash, feed->symbols(), feed->prices(),
                     rebalance_threshold);

  while (feed->adjust_prices()) {
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
  // market_data::Events events;
  // for (auto f : get_iex("PPL")) {
  //  std::fstream input(f, std::ios::in | std::ios::binary);
  //  events.ParseFromIstream(&input);
  //  for (auto event : events.events()) {
  //    if (event.has_trade()) {
  //      printf("%lf\n", event.trade().price() / 10000.0);
  //    } else {
  //      printf("here: %s\n", event.DebugString().c_str());
  //    }
  //  }
  //}

  static constexpr double cash = 100000.0;
  static constexpr double dt = 1.0 / 252;
  static constexpr double sigma = 1.0 / 252;
  static constexpr double rebalance_threshold = 1.0;
  Duration dur;
  dur.set_seconds(365 * 24 * 60 * 60);
  StreamIntervalStatistics bh_stats(dur);
  StreamIntervalStatistics wave_stats(dur);
  std::unique_ptr<Feed> feed = std::make_unique<RandomFeed>(RandomFeed(
      /*symbols=*/{"FOO", "BAR"}, /*prices=*/{10.0, 10.0},
      /*gbm_dt=*/dt, /*gbm_sigma=*/sigma,
      /*lifespan=*/5 * 365 * 24 * 60 * 60));
  job(/*feed=*/std::move(feed), /*cash=*/cash,
      /*rebalance_threshold=*/rebalance_threshold,
      /*bh_stats=*/&bh_stats, /*wave_stats=*/&wave_stats);

  printf("%s\n", bh_stats.hist()
                     ->json(/*title=*/"bh",
                            /*label=*/"mean: " +
                                std::to_string(bh_stats.stats().mean()))
                     .c_str());
  printf("%s\n",
         wave_stats.hist()
             ->json(
                 /*title=*/"wave",
                 /*label=*/"mean: " + std::to_string(wave_stats.stats().mean()))
             .c_str());

  return 0;
}
