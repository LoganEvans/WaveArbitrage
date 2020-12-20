#include <stdio.h>
#include <time.h>

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <mutex>
#include <random>
#include <thread>
#include <vector>

#define USE_INTERVAL_STATISTICS true

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

class Flipper {
public:
  Flipper(int num_stocks, double threshold, double gbm_mu, double gbm_dt,
          double gbm_sigma)
      : num_stocks_(num_stocks), threshold_(threshold), gbm_mu_(gbm_mu),
        gbm_dt_(gbm_dt), gbm_sqrt_dt_(sqrt(gbm_dt)), gbm_sigma_(gbm_sigma),
        positions_(num_stocks, 1.0), prices_(num_stocks, 1.0),
        norm_dist_(0.0, 1.0) {
    generator_.seed(
        std::chrono::high_resolution_clock().now().time_since_epoch().count());
  }

  virtual ~Flipper() {}

  double total_shares() {
    double total = 0.0;
    for (size_t i = 0; i < positions_.size(); i++) {
      total += positions_[i] * prices_[i];
    }
    return total;
  }

  double g() {
    double total = total_shares();
    double prod = 1.0;
    for (auto price : prices_) {
      prod *= total / price;
    }

    return pow(prod, 1.0 / num_stocks_);
  }

  double value() {
    double value = 0.0;
    for (size_t i = 0; i < positions_.size(); i++) {
      value += positions_[i] * prices_[i];
    }
    return value;
  }

  void simulate(int kFlips) {
    for (int i = 0; i < kFlips; i++) {
      adjust_prices();
      rebalance();
    }
  }

  virtual int num_rebalances() { return 0; }

protected:
  int num_stocks_;
  double threshold_;
  // gbm for Geometric Brownian Motion. See
  // https://en.wikipedia.org/wiki/Geometric_Brownian_motion
  double gbm_mu_;
  double gbm_dt_;
  double gbm_sqrt_dt_;
  double gbm_sigma_;
  std::vector<double> positions_;
  std::vector<double> prices_;
  std::default_random_engine generator_;
  std::normal_distribution<double> norm_dist_;

  virtual void rebalance() {}

  void adjust_prices() {
    if (false) {
      for (size_t i = 0; i < prices_.size(); i++) {
        prices_[i] = prices_[i] + prices_[i] * (gbm_sigma_ * gbm_sqrt_dt_ *
                                                norm_dist_(generator_));
      }
    } else {
      for (size_t i = 0; i < prices_.size(); i++) {
        prices_[i] =
            prices_[i] * exp((gbm_mu_ - gbm_sigma_ * gbm_sigma_ / 2) * gbm_dt_ +
                             gbm_sigma_ * norm_dist_(generator_));
      }
    }
  }
};

class BuyAndHold : public Flipper {
public:
  BuyAndHold(int num_stocks, double threshold, double gbm_mu,
             double gbm_dt, double gbm_sigma)
      : Flipper(num_stocks, threshold, /*gbm_mu=*/gbm_mu,
                /*gbm_dt=*/gbm_dt, /*gbm_sigma=*/gbm_sigma) {}
  ~BuyAndHold() {}

protected:
  void rebalance() override {}
};

class WaveArbitrage : public Flipper {
public:
  WaveArbitrage(int num_stocks, double threshold, double gbm_mu,
                double gbm_dt, double gbm_sigma)
      : Flipper(num_stocks, threshold, /*gbm_mu=*/gbm_mu,
                /*gbm_dt=*/gbm_dt, /*gbm_sigma=*/gbm_sigma) {}
  ~WaveArbitrage() {}

  int num_rebalances() override { return rebalances_; }

protected:
  int rebalances_ = 0;

  void rebalance() override {
    double dollars_per_stock = total_shares() / num_stocks_;
    double threshold = dollars_per_stock * threshold_;

    bool skip_rebalance = true;
    for (int i = 0; i < num_stocks_; i++) {
      if (positions_[i] * prices_[i] > threshold) {
        skip_rebalance = false;
        break;
      }
    }

    if (skip_rebalance) {
      return;
    }

    rebalances_++;

    for (int i = 0; i < num_stocks_; i++) {
      positions_[i] = dollars_per_stock / prices_[i];
    }
  }
};

void run_experiment(int num_stocks, int flips, int num_trials, double gbm_mu,
                    double gbm_dt, double gbm_sigma) {
  const auto num_cpus = std::thread::hardware_concurrency();

  std::mutex mu;
  mu.lock();

  int trials_remaining = num_trials;
  // Use threshold = 1.0 to rebalance after every price adjustment.
  double threshold = 1.05;
  WelfordRunningStatistics bh_g_stats;
  WelfordRunningStatistics bh_val_stats;
  WelfordRunningStatistics wave_g_stats;
  WelfordRunningStatistics wave_val_stats;
  int64_t total_wave_rebalances = 0;

  mu.unlock();

  std::thread threads[num_cpus];
  for (size_t i = 0; i < num_cpus; i++) {
    threads[i] = std::thread(
        [&](int i) {
          while (true) {
            mu.lock();
            if (trials_remaining <= 0) {
              mu.unlock();
              break;
            }

            trials_remaining -= 1;

            if (trials_remaining % 100 == 0) {
              printf("trials: %d/%d\r", num_trials - trials_remaining,
                     num_trials);
              fflush(stdout);
            }

            mu.unlock();

            BuyAndHold bh(/*num_stocks=*/num_stocks, /*threshold=*/threshold,
                          /*gbm_mu=*/gbm_mu, /*gbm_dt=*/gbm_dt,
                          /*gbm_sigma=*/gbm_sigma);
            bh.simulate(flips);
            WaveArbitrage wave(/*num_stocks=*/num_stocks,
                               /*threshold=*/threshold, /*gbm_mu=*/gbm_mu,
                               /*gbm_dt=*/gbm_dt, /*gbm_sigma=*/gbm_sigma);
            wave.simulate(flips);

            mu.lock();

            bh_g_stats.update(bh.g());
            bh_val_stats.update(bh.value());
            wave_g_stats.update(wave.g());
            wave_val_stats.update(wave.value());
            total_wave_rebalances += wave.num_rebalances();

            //fprintf(stderr, "%lf,%lf,%lf,%lf\n", bh.g(), bh.value(), wave.g(),
            //        wave.value());

            mu.unlock();
          }
        },
        i);
  }

  for (size_t i = 0; i < num_cpus; i++) {
    threads[i].join();
  }

  printf("\n{\n"
         "  \"stocks\": %d,\n"
         "  \"flips_per_trial\": %d,\n"
         "  \"trials\": %d,\n"
         "  \"bh_g\": %.10lf,\n"
         "  \"bh_g_stddev\": %.10lf,\n"
         "  \"bh_val\": %.10lf,\n"
         "  \"bh_stddev\": %.10lf,\n"
         "  \"wave_g\": %.10lf,\n"
         "  \"wave_g_stddev\": %.10lf,\n"
         "  \"wave_val\": %.10lf,\n"
         "  \"wave_stddev\": %.10lf,\n"
         "  \"wave_num_rebalances\": %ld,\n"
         "}\n",
         num_stocks, flips, num_trials, bh_g_stats.mean(),
         bh_g_stats.sample_variance(), bh_val_stats.mean(),
         bh_val_stats.sample_variance(), wave_g_stats.mean(),
         wave_g_stats.sample_variance(), wave_val_stats.mean(),
         wave_val_stats.sample_variance(), total_wave_rebalances);
}

int main() {
  srand(time(NULL));
  setbuf(stdout, NULL);

  static constexpr double sigma = 1.0 / 252;
  static constexpr double dt = 1.0 / 252;

  run_experiment(/*num_stocks=*/2, /*flips=*/100000, /*num_trials=*/100000,
                 /*gbm_mu=*/0.0, /*gbm_dt=*/dt, /*gbm_sigma=*/sigma);
}
