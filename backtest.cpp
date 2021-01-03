#include <stdio.h>

#include <algorithm>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <tuple>
#include <vector>

#include "feed.h"
#include "market_data.pb.h"
#include "strategy.h"
#include "util.h"

using DynamicHistogram =
    dhist::DynamicHistogram</*kUseDecay=*/false, /*kThreadsafe=*/true>;

void job(std::unique_ptr<Feed> feed, double cash, double rebalance_threshold,
         WelfordRunningStatistics *bh_stats,
         WelfordRunningStatistics *wave_stats, DynamicHistogram *bh_hist,
         DynamicHistogram *wave_hist) {
  BuyAndHold bh(cash, feed->symbols(), feed->prices());
  WaveArbitrage wave(cash, feed->symbols(), feed->prices(),
                     rebalance_threshold);

  Duration dur;
  dur.set_seconds(365 * 24 * 60 * 60);
  Duration cooldown;
  cooldown.set_seconds(60);

  StreamIntervalStatistics bh_si_stats(dur, cooldown, bh_stats, bh_hist);
  StreamIntervalStatistics wave_si_stats(dur, cooldown, wave_stats, wave_hist);

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
    bh_si_stats.update(bh.portfolio().value(feed->prices()), feed->timestamp());
    wave_si_stats.update(wave.portfolio().value(feed->prices()),
                         feed->timestamp());
  }
}

int main(int argc, char **argv) {
  GOOGLE_PROTOBUF_VERIFY_VERSION;
  google::InitGoogleLogging(argv[0]);

  static constexpr double cash = 100000.0;
  static constexpr double rebalance_threshold = 1.0;

  WelfordRunningStatistics bh_stats;
  WelfordRunningStatistics wave_stats;
  static constexpr size_t kMaxNumBuckets = 200;
  DynamicHistogram bh_hist(kMaxNumBuckets);
  DynamicHistogram wave_hist(kMaxNumBuckets);

  if (false) {
    static constexpr double dt = 1.0 / 252;
    static constexpr double sigma = 1.0 / 252;
    std::unique_ptr<Feed> feed = std::make_unique<RandomFeed>(RandomFeed(
        /*symbols=*/{"FOO", "BAR"}, /*prices=*/{10.0, 10.0},
        /*gbm_dt=*/dt, /*gbm_sigma=*/sigma,
        /*lifespan=*/5 * 365 * 24 * 60 * 60));
    job(/*feed=*/std::move(feed), /*cash=*/cash,
        /*rebalance_threshold=*/rebalance_threshold,
        /*bh_stats=*/&bh_stats, /*wave_stats=*/&wave_stats,
        /*bh_hist=*/&bh_hist, /*wave_hist=*/&wave_hist);
  } else if (false) {
    std::unique_ptr<Feed> feed = std::make_unique<IEXFeed>(IEXFeed(
        /*symbols=*/{"AIV", "XRX"}));
    ///*symbols=*/{"F", "ZION"}));
    ///*symbols=*/{"AMZN", "WMT"}));
    ///*symbols=*/{"GOOG", "FB"}));
    printf("%s\n", feed->to_string().c_str());
    job(/*feed=*/std::move(feed), /*cash=*/cash,
        /*rebalance_threshold=*/rebalance_threshold,
        /*bh_stats=*/&bh_stats, /*wave_stats=*/&wave_stats,
        /*bh_hist=*/&bh_hist, /*wave_hist=*/&wave_hist);
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
    for (const auto &symbol : get_available_symbols()) {
      if (split_set.find(symbol) == split_set.end() &&
          blacklist_set.find(symbol) == blacklist_set.end()) {
        symbols.push_back(symbol);
      }
    }

    std::uniform_int_distribution<int> uniform_dist(0, symbols.size() - 1);
    std::default_random_engine generator;

    const auto num_cpus = std::thread::hardware_concurrency();
    std::thread threads[num_cpus];
    std::atomic<int> jobs_completed = 0;

    std::mutex mu;
    size_t first_stock_idx = 0;
    size_t second_stock_idx = 0;
    bool is_running = true;
    auto get_next = [&]() -> std::tuple<size_t, size_t, bool> {
      std::scoped_lock<std::mutex> lock(mu);

      if (second_stock_idx + 1 >= symbols.size()) {
        first_stock_idx += 1;
        second_stock_idx = first_stock_idx + 1;

        if (second_stock_idx >= symbols.size()) {
          is_running = false;
        }
      } else {
        second_stock_idx += 1;
      }
      return std::make_tuple(first_stock_idx, second_stock_idx, is_running);
    };

    for (int tx = 0; tx < num_cpus; tx++) {
      threads[tx] = std::thread([&]() {
        while (true) {
          auto idxs = get_next();
          if (!std::get<2>(idxs)) {
            return;
          }
          size_t i = std::get<0>(idxs);
          size_t j = std::get<1>(idxs);

          std::unique_ptr<Feed> feed = std::make_unique<IEXFeed>(IEXFeed(
              /*symbols=*/{symbols[i], symbols[j]}));
          job(/*feed=*/std::move(feed), /*cash=*/cash,
              /*rebalance_threshold=*/rebalance_threshold,
              /*bh_stats=*/&bh_stats, /*wave_stats=*/&wave_stats,
              /*bh_hist=*/&bh_hist, /*wave_hist=*/&wave_hist);

          int completed =
              jobs_completed.fetch_add(1, std::memory_order_acq_rel);

          if (completed % 100 == 0) {
            printf("%s\n",
                   bh_hist
                       .json(/*title=*/"",
                             /*label=*/"bh -- mean: " +
                                 std::to_string(bh_stats.mean()) + " var: " +
                                 std::to_string(bh_stats.sample_variance()))
                       .c_str());
            printf("%s\n",
                   wave_hist
                       .json(
                           /*title=*/"",
                           /*label=*/"wave -- mean: " +
                               std::to_string(wave_stats.mean()) + " var: " +
                               std::to_string(wave_stats.sample_variance()))
                       .c_str());
            printf("completed: %d, symbols: %s, %s\n", completed,
                   symbols[i].c_str(), symbols[j].c_str());
            printf("bh: %lf, wave: %lf, delta: %lf\n\n",
                   bh_stats.mean(), wave_stats.mean(),
                   bh_stats.mean() - wave_stats.mean());
          }
        }
      });
    }

    for (int tx = 0; tx < num_cpus; tx++) {
      threads[tx].join();
    }
  }

  printf("%s\n",
         bh_hist
             .json(/*title=*/"",
                   /*label=*/"bh -- mean: " + std::to_string(bh_stats.mean()) +
                       " var: " + std::to_string(bh_stats.sample_variance()))
             .c_str());
  printf(
      "%s\n",
      wave_hist
          .json(
              /*title=*/"",
              /*label=*/"wave -- mean: " + std::to_string(wave_stats.mean()) +
                  " var: " + std::to_string(wave_stats.sample_variance()))
          .c_str());

  return 0;
}
