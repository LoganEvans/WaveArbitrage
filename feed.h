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

struct PriceAction {
  PriceAction(Timestamp timestamp, double ratio, bool is_dividend)
      : timestamp(timestamp), ratio(ratio), is_dividend(is_dividend) {}

  PriceAction(const PriceAction& pa)
      : timestamp(pa.timestamp), ratio(pa.ratio), is_dividend(pa.is_dividend) {}

  PriceAction(string csv_line) {
    size_t year_end = csv_line.find("-");
    size_t month_end = csv_line.find("-", year_end + 1);
    size_t day_end = csv_line.find(",", month_end + 1);
    int year = std::stoi(csv_line.substr(0, year_end));
    int month = std::stoi(csv_line.substr(year_end + 1, month_end - year_end));
    int day = std::stoi(csv_line.substr(month_end + 1, day_end - month_end));

    std::tm ts{};
    ts.tm_year = year - 1900;
    ts.tm_mon = month - 1;
    ts.tm_mday = day;
    timestamp.set_seconds(timegm(&ts));

    size_t action_end = csv_line.find(",", day_end + 1);
    if (csv_line.substr(day_end + 1, action_end - day_end - 1) == "DIVIDEND") {
      is_dividend = true;
    }
    ratio = std::stof(csv_line.substr(action_end + 1));
  }

  string to_string() const {
    string s = "PriceAction{action: ";
    if (is_dividend) {
      s += "DIVIDEND";
    } else {
      s += "SPLIT";
    }
    s += ", sec: " + std::to_string(timestamp.seconds()) +
         ", ratio: " + std::to_string(ratio) + "}";
    return s;
  }

  Timestamp timestamp;
  double ratio = 0.0;
  bool is_dividend = 0;

  bool operator<(const PriceAction &other) const {
    return before(timestamp, other.timestamp);
  }
};

class Feed {
public:
  enum FeedStatus {
    FEED_OK = 0,
    FEED_DAY_CHANGE = 1,
    FEED_DIVIDEND = 2,
    FEED_SPLIT = 4,
    FEED_END = 8,
  };

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

  virtual FeedStatus adjust_prices() = 0;

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

  FeedStatus adjust_prices() override {
    if (num_adjusts_++ >= lifespan_) {
      return FEED_END;
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
    return FEED_OK;
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

std::vector<string> get_iex(string symbol) {
  static const string kProcessedDir = "/home/logan/data/processed/";
  string comparison = kProcessedDir + symbol + "_";
  std::vector<string> res;
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
    dividends_.resize(symbols.size());
    splits_.resize(symbols.size());

    size_t i = 0;
    for (auto symbol : symbols) {
      iex_files_.push_back(get_iex(symbol));
      iex_files_idxs_.push_back(0);
      day_events_.push_back(market_data::Events());
      day_events_next_idxs_.push_back(0);

      CHECK_NE(advance(i), FEED_END);
      auto trade = day_events_[i].events()[day_events_next_idxs_[i]].trade();
      if (before(timestamp_, trade.timestamp())) {
        last_timestamp_ = trade.timestamp();
        timestamp_ = trade.timestamp();
      }
      prices_[i] = trade.price() / 10000.0;

      string filename = "/home/logan/data/dividends/" + symbol + ".csv";
      std::ifstream in(filename, std::ios::in | std::ios::binary);
      if (!in.good()) {
        continue;
      }
      string csv_data{std::istreambuf_iterator<char>(in),
                           std::istreambuf_iterator<char>()};

      setbuf(stdout, 0);
      size_t idx = 0;
      while (idx < csv_data.size()) {
        size_t found_idx = csv_data.find("\n", idx);
        auto pa = PriceAction(csv_data.substr(idx, found_idx - idx));

        if (pa.is_dividend) {
          dividends_[i].push_back(pa);
        } else {
          splits_[i].push_back(pa);
        }

        idx = found_idx + 1;
      }

      std::sort(dividends_[i].begin(), dividends_[i].end());
      std::sort(splits_[i].begin(), splits_[i].end());

      i += 1;
    }
  }

  ~IEXFeed() {}

  string feed_name() const override {
    return "IEXFeed";
  }

  FeedStatus adjust_prices() override {
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

    auto trade = day_events_[champ_idx]
                     .events()[day_events_next_idxs_[champ_idx]]
                     .trade();
    prices_[champ_idx] = trade.price() / 10000.0;
    last_timestamp_ = timestamp_;
    timestamp_ = trade.timestamp();

    FeedStatus fs = advance(champ_idx);

    if (fs & FEED_SPLIT) {
      // TODO(lpe): Detect if one of the split_[i] price actions is in between
      // last_timestamp_ and timestamp_. If so, prepare a splits_ vector so that
      // a splits() function can return the ratios. Then, pipe that through to
      // the backtester to trigger the split.
    }

    if (fs & FEED_DIVIDEND) {
      
    }

    return fs;
  }

private:
  std::vector<std::vector<string>> iex_files_;
  std::vector<int> iex_files_idxs_;
  std::vector<market_data::Events> day_events_;
  std::vector<int> day_events_next_idxs_;

  std::vector<std::vector<PriceAction>> dividends_;
  std::vector<std::vector<PriceAction>> splits_;

  Timestamp last_timestamp_;

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
        return true;
      }
    }
  }

  FeedStatus advance(size_t symbol_index) {
    const size_t i = symbol_index;

    while (true) {
      if (advance_event(i)) {
        return FEED_OK;
      }

      if (!advance_file(i)) {
        return FEED_END;
      }
    }
  }
};

#endif // WAVE_ARBITRAGE_FEED_H
