#ifndef WAVE_ARBITRAGE_FEED_H
#define WAVE_ARBITRAGE_FEED_H

#include <chrono>
#include <random>

#include <glog/logging.h>

class Feed {
public:
  Feed(std::vector<double> prices, double gbm_dt, double gbm_sigma)
      : prices_(prices), gbm_sqrt_dt_(sqrt(gbm_dt)),
        gbm_sigma_(gbm_sigma), norm_dist_(0.0, 1.0) {
    generator_.seed(
        std::chrono::high_resolution_clock().now().time_since_epoch().count());
  }

  void adjust_prices() {
    for (size_t i = 0; i < prices_.size(); i++) {
      prices_[i] = prices_[i] + prices_[i] * (gbm_sigma_ * gbm_sqrt_dt_ *
                                              norm_dist_(generator_));
    }
  }

  const std::vector<double> &prices() const { return prices_; }

private:
  std::vector<double> prices_;
  const double gbm_sqrt_dt_;
  const double gbm_sigma_;
  std::normal_distribution<double> norm_dist_;
  std::default_random_engine generator_;
};

#endif // WAVE_ARBITRAGE_FEED_H
