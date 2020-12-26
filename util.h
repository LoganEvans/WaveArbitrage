#ifndef WAVE_ARBITRAGE_UTIL_H
#define WAVE_ARBITRAGE_UTIL_H

#include <algorithm>
#include <deque>
#include <mutex>
#include <google/protobuf/util/time_util.h>
#include "external/dynamic_histogram/cpp/DynamicHistogram.h"


using ::google::protobuf::Duration;
using ::google::protobuf::Timestamp;
using DynamicHistogram =
    dhist::DynamicHistogram</*kUseDecay=*/false, /*kThreadsafe=*/true>;

class WelfordRunningStatistics {
public:
  WelfordRunningStatistics()
      : count_(0), mean_(0.0), M2_(0.0) {}

  void update(double new_value) {
    std::scoped_lock<std::mutex> lock(mu_);

    count_++;
    double delta = new_value - mean_;
    mean_ += delta / count_;
    double delta2 = new_value - mean_;
    M2_ += delta * delta2;
  }

  int64_t count() const { return count_; }

  double mean() const { return mean_; }

  double variance() const {
    return M2_ / std::max(static_cast<int64_t>(1), count_);
  }

  double sample_variance() const {
    return M2_ / std::max(static_cast<int64_t>(1), count_ - 1);
  };

private:
  std::mutex mu_;
  int64_t count_;
  double mean_;
  double M2_;
};

void get_duration(const Timestamp &start, const Timestamp &end,
                  Duration *duration) {
  duration->set_seconds(end.seconds() - start.seconds());
  duration->set_nanos(end.nanos() - start.nanos());

  if (duration->seconds() < 0 && duration->nanos() > 0) {
    duration->set_seconds(duration->seconds() + 1);
    duration->set_nanos(duration->nanos() - 1000000000);
  } else if (duration->nanos() < 0 && duration->seconds() > 0) {
    duration->set_seconds(duration->seconds() - 1);
    duration->set_nanos(duration->nanos() + 1000000000);
  }
}

bool less_than(const Duration& a, const Duration& b) {
  return a.seconds() < b.seconds() ||
         (a.seconds() == b.seconds() && a.nanos() < b.nanos());
}

bool before(const Timestamp& a, const Timestamp& b) {
  return a.seconds() < b.seconds() ||
         (a.seconds() == b.seconds() && a.nanos() < b.nanos());
}

class StreamIntervalStatistics {
public:
  StreamIntervalStatistics(const Duration &duration)
      : interval_duration_(duration), dhist_(/*max_num_buckets=*/61) {}

  void update(double val, std::shared_ptr<Timestamp> timestamp) {
    Duration duration;

    while (true) {
      if (timestamps_.empty()) {
        break;
      }

      get_duration(*timestamps_.front(), *timestamp, &duration);
      if (less_than(duration, interval_duration_)) {
        break;
      }

      timestamps_.pop_front();
      const double old_val = vals_.front();
      vals_.pop_front();
      if (old_val == 0.0) {
        continue;
      }
      const double stat = val / old_val;
      stats_.update(stat);
      dhist_.addValue(stat);
    }

    vals_.push_back(val);
    timestamps_.push_back(std::move(timestamp));
  }

  const WelfordRunningStatistics &stats() const { return stats_; }

  DynamicHistogram* hist() { return &dhist_; }

private:
  const Duration interval_duration_;
  WelfordRunningStatistics stats_;
  DynamicHistogram dhist_;
  std::deque<double> vals_;
  std::deque<std::shared_ptr<Timestamp>> timestamps_;
};

#endif // WAVE_ARBITRAGE_UTIL_H
