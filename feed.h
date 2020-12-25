#ifndef WAVE_ARBITRAGE_FEED_H
#define WAVE_ARBITRAGE_FEED_H

#include <chrono>
#include <random>

#include <glog/logging.h>
#include <google/protobuf/util/time_util.h>

#include "market_data.pb.h"

using ::google::protobuf::Timestamp;
using ::std::string;

class Feed {
public:
  Feed(std::vector<string> symbols, std::vector<double> prices)
      : symbols_(symbols), prices_(prices) {}

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
    s += middle_indent + "time: " + std::to_string(timestamp_.seconds()) +
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
      : Feed(symbols, prices), gbm_sqrt_dt_(sqrt(gbm_dt)),
        gbm_sigma_(gbm_sigma), lifespan_(lifespan), uniform_dist_(-1, 1),
        norm_dist_(0.0, 1.0) {
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
      }
    } else {
      for (size_t i = 0; i < prices_.size(); i++) {
        prices_[i] = prices_[i] + uniform_dist_(generator_) / 100.0;
        if (prices_[i] <= 5.0) {
          prices_[i] = 5.01;
        }
      }
    }

    timestamp_.set_seconds(timestamp_.seconds() + 3600 +
                           100 * norm_dist_(generator_));
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

#endif // WAVE_ARBITRAGE_FEED_H
