#ifndef WAVE_ARBITRAGE_UTIL_H
#define WAVE_ARBITRAGE_UTIL_H

#include <algorithm>

class WelfordRunningStatistics {
public:
  WelfordRunningStatistics() : count_(0), mean_(0.0), M2_(0.0) {}

  void update(double new_value) {
    count_++;
    double delta = new_value - mean_;
    mean_ += delta / count_;
    double delta2 = new_value - mean_;
    M2_ += delta * delta2;
  }

  int64_t count() { return count_; }

  double mean() { return mean_; }

  double variance() { return M2_ / std::max(static_cast<int64_t>(1), count_); }

  double sample_variance() {
    return M2_ / std::max(static_cast<int64_t>(1), count_ - 1);
  };

private:
  int64_t count_;
  double mean_;
  double M2_;
};

class StreamIntervalStatistics {
public:
  StreamIntervalStatistics(size_t interval)
      : vals_(interval, 0.0), next_index_(0), wrapped_(false) {}

  void update(double val) {
    if (next_index_ >= vals_.size()) {
      next_index_ = 0;
      wrapped_ = true;
    }

    if (wrapped_) {
      stats_.update(val / vals_[next_index_]);
    }

    vals_[next_index_] = val;
    next_index_ += 1;
  }

  const WelfordRunningStatistics &stats() { return stats_; }

private:
  WelfordRunningStatistics stats_;
  std::vector<double> vals_;
  size_t next_index_;
  bool wrapped_;
};

#endif // WAVE_ARBITRAGE_UTIL_H
