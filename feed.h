#ifndef WAVE_ARBITRAGE_FEED_H
#define WAVE_ARBITRAGE_FEED_H

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <mutex>
#include <random>
#include <set>
#include <vector>

#include <glog/logging.h>
#include <google/protobuf/util/time_util.h>

#include "market_data.pb.h"
#include "util.h"

using ::google::protobuf::Timestamp;
using ::std::string;

typedef int FeedStatus;
static constexpr FeedStatus FEED_OK = 1;
static constexpr FeedStatus FEED_DAY_CHANGE = 2;
static constexpr FeedStatus FEED_DIVIDEND = 4;
static constexpr FeedStatus FEED_SPLIT = 8;
static constexpr FeedStatus FEED_END = 16;

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
  Feed(std::vector<string> symbols)
      : symbols_(symbols), prices_(symbols.size(), 0.0),
        dividends_(symbols.size(), 0.0), splits_(symbols.size(), 0.0),
        adjusts_(0) {}

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
    s += middle_indent + "adjusts: " + std::to_string(adjusts_) + ",\n";
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

  const std::vector<double> &dividends() const { return dividends_; }

  const std::vector<double> &splits() const { return splits_; }

  const Timestamp &timestamp() const { return timestamp_; }

protected:
  std::vector<string> symbols_;
  std::vector<double> prices_;
  std::vector<double> dividends_;
  std::vector<double> splits_;
  Timestamp timestamp_;
  size_t adjusts_;
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
    adjusts_++;
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

const std::vector<string>& get_available_symbols() {
  static std::vector<string> symbols;
  static std::mutex mtx;

  std::scoped_lock<std::mutex> lock(mtx);

  if (!symbols.empty()) {
    return symbols;
  }

  static const string kProcessedDir = "/home/logan/data/processed/";
  std::set<string> res;
  for (const auto &f : std::filesystem::directory_iterator(kProcessedDir)) {
    if (!f.file_size()) {
      continue;
    }
    string fname = f.path();
    size_t symbol_start = fname.find_last_of("/") + 1;
    res.insert(fname.substr(symbol_start, fname.find("_") - symbol_start));
  }
  symbols = {res.begin(), res.end()};
  return symbols;
}

std::map<string, std::vector<string>>& get_iex_files() {
  static const string kProcessedDir = "/home/logan/data/processed/";
  static std::map<string, std::vector<string>> files;
  static std::mutex mtx;

  std::scoped_lock<std::mutex> lock(mtx);
  if (!files.empty()) {
    return files;
  }

  for (const auto& symbol : get_available_symbols()) {
    files[symbol] = std::vector<string>();
  }

  for (const auto &f : std::filesystem::directory_iterator(kProcessedDir)) {
    if (!f.file_size()) {
      continue;
    }
    string fname = f.path();
    size_t symbol_start = fname.find_last_of("/") + 1;
    string symbol = fname.substr(symbol_start, fname.find("_") - symbol_start);
    files[symbol].push_back(f.path());
  }

  for (auto & item : files) {
    std::sort(item.second.begin(), item.second.end());
  }

  return files;
}

class IEXFeed : public Feed {
public:
  IEXFeed(std::vector<string> symbols) : Feed(symbols) {
    day_events_.resize(symbols.size());
    for (size_t i = 0; i < symbols.size(); i++) {
      const string symbol = symbols[i];
      iex_files_.push_back(get_iex_files()[symbol]);
      iex_files_idxs_.push_back(0);
      day_events_.push_back(market_data::Events());
      day_events_next_idxs_.push_back(0);
    }

    CHECK_NE(advance_day(), FEED_END);

    for (size_t i = 0; i < symbols.size(); i++) {
      auto trade = day_events_[i].events()[day_events_next_idxs_[i]].trade();
      if (before(timestamp_, trade.timestamp())) {
        last_timestamp_ = trade.timestamp();
        timestamp_ = trade.timestamp();
      }
      prices_[i] = trade.price() / 10000.0;
    }

    price_actions_.resize(symbols.size());
    for (size_t i = 0; i < symbols.size(); i++) {
      string filename = "/home/logan/data/dividends/" + symbols[i] + ".csv";
      std::ifstream in(filename, std::ios::in | std::ios::binary);
      if (!in.good()) {
        continue;
      }
      string csv_data{std::istreambuf_iterator<char>(in),
                      std::istreambuf_iterator<char>()};

      size_t idx = 0;
      while (idx < csv_data.size()) {
        size_t found_idx = csv_data.find("\n", idx);
        price_actions_[i].push_back(
            PriceAction(csv_data.substr(idx, found_idx - idx)));
        idx = found_idx + 1;
      }
    }
  }

  ~IEXFeed() {}

  string feed_name() const override {
    return "IEXFeed";
  }

  FeedStatus adjust_prices() override {
    adjusts_++;
    Timestamp champ;
    int champ_idx = 0;

    for (size_t i = 0; i < symbols().size(); i++) {
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

    FeedStatus fs = advance_event(champ_idx);
    if (fs == FEED_DAY_CHANGE) {
      if (advance_day() == FEED_END) {
        return FEED_END;
      }

      for (size_t i = 0; i < symbols().size(); i++) {
        for (auto price_action : price_actions_[i]) {
          if (before(last_timestamp_, price_action.timestamp) &&
              before(price_action.timestamp, timestamp_)) {
            if (price_action.is_dividend) {
              dividends_[i] = price_action.ratio;
              fs |= FEED_DIVIDEND;
            } else {
              splits_[i] = price_action.ratio;
              fs |= FEED_SPLIT;
            }
          }
        }
      }
    }

    return fs;
  }

private:
  std::vector<std::vector<string>> iex_files_;
  std::vector<int> iex_files_idxs_;
  std::vector<market_data::Events> day_events_;
  std::vector<int> day_events_next_idxs_;

  std::vector<std::vector<PriceAction>> price_actions_;

  Timestamp last_timestamp_;

  FeedStatus advance_day() {
    for (size_t i = 0; i < symbols().size(); i++) {
      dividends_[i] = 0.0;
      splits_[i] = 0.0;

      if (iex_files_idxs_[i] >= iex_files_[i].size()) {
        return FEED_END;
      }

      initialize_day(i);

      FeedStatus fs;
      while (true) {
        fs = advance_event(i);
        if (fs & FEED_END) {
          return FEED_END;
        } else if (fs & FEED_DAY_CHANGE) {
          initialize_day(i);
        } else {
          break;
        }
      }

      auto &ts =
          day_events_[i].events()[day_events_next_idxs_[i]].trade().timestamp();
      if (i == 0 || before(ts, timestamp_)) {
        timestamp_ = ts;
      }
    }

    return FEED_DAY_CHANGE;
  }

  FeedStatus advance_event(size_t symbol_index) {
    const size_t i = symbol_index;

    while (true) {
      day_events_next_idxs_[i] += 1;

      if (day_events_next_idxs_[i] >= day_events_[i].events().size()) {
        return FEED_DAY_CHANGE;
      }

      const auto event = day_events_[i].events()[day_events_next_idxs_[i]];

      if (event.has_trade()) {
        return FEED_OK;
      }
    }
  }

  void initialize_day(size_t i) {
    std::fstream input(iex_files_[i][iex_files_idxs_[i]],
                       std::ios::in | std::ios::binary);
    day_events_next_idxs_[i] = 0;
    iex_files_idxs_[i] += 1;
    day_events_[i].ParseFromIstream(&input);
  }
};

#endif // WAVE_ARBITRAGE_FEED_H
