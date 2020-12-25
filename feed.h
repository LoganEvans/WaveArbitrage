#ifndef WAVE_ARBITRAGE_FEED_H
#define WAVE_ARBITRAGE_FEED_H

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>

#include <glog/logging.h>
#include <google/protobuf/util/time_util.h>

#include "market_data.pb.h"
#include "util.h"

using ::google::protobuf::Timestamp;
using ::std::string;

class Feed {
public:
  Feed(std::vector<string> symbols)
      : symbols_(symbols), prices_(symbols.size(), 0.0) {}

  virtual ~Feed() {}

  string to_string(int indent = 0) const {
    string top_indent = "";
    for (int i = 0; i < indent; i++) {
      top_indent += " ";
    }

    string middle_indent = "";
    for (int i = 0; i < indent + 2; i++) {
      middle_indent += " ";
    }

    string s = top_indent + feed_name() + " {\n";
    s += middle_indent + "time: " + std::to_string(timestamp().seconds()) +
         ",\n" + middle_indent + "prices: {";
    for (size_t i = 0; i < prices_.size(); i++) {
      s += symbols_[i] + ": " + std::to_string(prices_[i]) + ", ";
    }
    s += "}\n" + top_indent + "}";
    return s;
  }

  virtual string feed_name() const = 0;

  virtual bool adjust_prices() = 0;

  const std::vector<string> &symbols() const { return symbols_; }

  const std::vector<double> &prices() const { return prices_; }

  const Timestamp &timestamp() const { return timestamp_; }

protected:
  std::vector<string> symbols_;
  std::vector<double> prices_;
  Timestamp timestamp_;
};

class RandomFeed : public Feed {
public:
  RandomFeed(std::vector<string> symbols, std::vector<double> prices,
             double gbm_dt, double gbm_sigma, int lifespan)
      : Feed(symbols), gbm_sqrt_dt_(sqrt(gbm_dt)), gbm_sigma_(gbm_sigma),
        lifespan_(lifespan), uniform_dist_(-1, 1), norm_dist_(0.0, 1.0) {
    prices_ = prices;
    generator_.seed(
        std::chrono::high_resolution_clock().now().time_since_epoch().count());
  }

  ~RandomFeed() {}

  string feed_name() const override {
    return "RandomFeed";
  }

  bool adjust_prices() override {
    if (num_adjusts_++ >= lifespan_) {
      return false;
    }

    if (true) {
      for (size_t i = 0; i < prices_.size(); i++) {
        prices_[i] = prices_[i] + prices_[i] * (gbm_sigma_ * gbm_sqrt_dt_ *
                                                norm_dist_(generator_));
        if (prices_[i] <= 5.0) {
          prices_[i] = 5.01;
        }
      }
    } else {
      for (size_t i = 0; i < prices_.size(); i++) {
        prices_[i] = prices_[i] + uniform_dist_(generator_) / 100.0;
        if (prices_[i] <= 5.0) {
          prices_[i] = 5.01;
        }
      }
    }

    timestamp_.set_seconds(timestamp_.seconds() + 100);
    return true;
  }

private:
  const double gbm_sqrt_dt_;
  const double gbm_sigma_;
  const int lifespan_;
  int num_adjusts_ = 0;
  std::uniform_int_distribution<int> uniform_dist_;
  std::normal_distribution<double> norm_dist_;
  std::default_random_engine generator_;
};

std::vector<std::string> get_iex(std::string symbol) {
  static const std::string kProcessedDir = "/home/logan/data/processed/";
  std::string comparison = kProcessedDir + symbol + "_";
  std::vector<std::string> res;
  for (const auto &f : std::filesystem::directory_iterator(kProcessedDir)) {
    if (0 == f.path().string().compare(0, comparison.size(), comparison)) {
      res.push_back(f.path());
    }
  }

  std::sort(res.begin(), res.end());

  return res;
}

class IEXFeed : public Feed {
public:
  IEXFeed(std::vector<string> symbols) : Feed(symbols) {
    size_t i = 0;
    for (auto symbol : symbols) {
      iex_files_.push_back(get_iex(symbol));
      iex_files_idxs_.push_back(0);
      day_events_.push_back(market_data::Events());
      day_events_next_idxs_.push_back(0);

      CHECK(advance(i));
      prices_[i] =
          day_events_[i].events()[day_events_next_idxs_[i]].trade().price() /
          10000.0;
      i += 1;
    }
  }

  ~IEXFeed() {}

  string feed_name() const override {
    return "IEXFeed";
  }

  bool adjust_prices() override {
    Timestamp champ;
    int champ_idx = 0;

    for (size_t i = 0; i < symbols_.size(); i++) {
      auto event = day_events_[i].events()[day_events_next_idxs_[i]].trade();

      Timestamp chump = event.timestamp();
      if (champ.seconds() == 0 || before(chump, champ)) {
        champ = chump;
        champ_idx = i;
      }
    }

    return advance(champ_idx);
  }

private:
  std::vector<std::vector<string>> iex_files_;
  std::vector<int> iex_files_idxs_;
  std::vector<market_data::Events> day_events_;
  std::vector<int> day_events_next_idxs_;

  bool advance_file(size_t symbol_index) {
    const size_t i = symbol_index;

    if (iex_files_idxs_[i] >= iex_files_[i].size()) {
      return false;
    }

    std::fstream input(iex_files_[i][iex_files_idxs_[i]],
                       std::ios::in | std::ios::binary);
    day_events_next_idxs_[i] = 0;
    iex_files_idxs_[i] += 1;
    day_events_[i].ParseFromIstream(&input);

    return true;
  }

  bool advance_event(size_t symbol_index) {
    const size_t i = symbol_index;

    while (true) {
      day_events_next_idxs_[i] += 1;

      if (day_events_next_idxs_[i] >= day_events_[i].events().size()) {
        return false;
      }

      const auto event = day_events_[i].events()[day_events_next_idxs_[i]];

      if (event.has_trade()) {
        break;
      }
    }

    return true;
  }

  bool advance(size_t symbol_index) {
    const size_t i = symbol_index;
    while (true) {
      if (advance_event(i)) {
        break;
      }

      if (!advance_file(i)) {
        return false;
      }
    }

    auto trade = day_events_[i].events()[day_events_next_idxs_[i]].trade();
    prices_[i] = trade.price() / 10000.0;
    timestamp_ = trade.timestamp();

    return true;
  }
};

#endif // WAVE_ARBITRAGE_FEED_H
