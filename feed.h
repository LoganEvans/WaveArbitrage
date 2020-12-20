#ifndef WAVE_ARBITRAGE_FEED_H
#define WAVE_ARBITRAGE_FEED_H

#include <chrono>
#include <random>

#include <glog/logging.h>

#include "market_data.pb.h"

using ::google::protobuf::Timestamp;

class Feed {
public:
  Feed(std::vector<double> prices) : prices_(prices) {}

  virtual void adjust_prices() = 0;

  const std::vector<double> &prices() const { return prices_; }

  const Timestamp& timestamp() const { return timestamp_; }

protected:
  std::vector<double> prices_;
  Timestamp timestamp_;
};

class RandomFeed : public Feed {
public:
  RandomFeed(std::vector<double> prices, double gbm_dt, double gbm_sigma)
      : Feed(prices), gbm_sqrt_dt_(sqrt(gbm_dt)), gbm_sigma_(gbm_sigma),
        norm_dist_(0.0, 1.0) {
    generator_.seed(
        std::chrono::high_resolution_clock().now().time_since_epoch().count());
  }

  void adjust_prices() {
    for (size_t i = 0; i < prices_.size(); i++) {
      prices_[i] = prices_[i] + prices_[i] * (gbm_sigma_ * gbm_sqrt_dt_ *
                                              norm_dist_(generator_));
    }

    timestamp_.set_seconds(timestamp_.seconds() + 3600 +
                           100 * norm_dist_(generator_));
  }

private:
  const double gbm_sqrt_dt_;
  const double gbm_sigma_;
  std::normal_distribution<double> norm_dist_;
  std::default_random_engine generator_;
};

#endif // WAVE_ARBITRAGE_FEED_H
