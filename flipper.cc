#include <stdio.h>
#include <time.h>

#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <mutex>
#include <random>
#include <thread>
#include <vector>

class Flipper {
public:
  Flipper(int num_stocks, double delta, double threshold)
      : num_stocks_(num_stocks), delta_(delta), threshold_(threshold),
        positions_(num_stocks, 1.0), prices_(num_stocks, 1.0),
        norm_dist_(0.0, 1.0) {
    unsigned int v = RAND_MAX; // count the number of bits set in v
    unsigned int c;            // c g_accumulates the total bits set in v
    for (c = 0; v; v >>= 1) {
      c += v & 1;
    }
    num_rand_bits_ = c;
    next_rand_index_ = c;
    generator_.seed(
        std::chrono::high_resolution_clock().now().time_since_epoch().count());
  }

  virtual ~Flipper() {}

  double total_shares() {
    double total = 0.0;
    for (int i = 0; i < positions_.size(); i++) {
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
    for (int i = 0; i < positions_.size(); i++) {
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

protected:
  int num_stocks_;
  double delta_;
  double threshold_;
  std::vector<double> positions_;
  std::vector<double> prices_;
  std::default_random_engine generator_;
  std::normal_distribution<double> norm_dist_;

  virtual void rebalance() {}

  bool next_rand() {
    if (next_rand_index_ >= num_rand_bits_) {
      rand_value_ = rand();
      next_rand_index_ = 0;
    }

    bool val = false;
    if (rand_value_ & (1 << next_rand_index_)) {
      val = true;
    }
    next_rand_index_++;

    return val;
  }

  void adjust_prices() {
    if (false) {
      if (next_rand()) {
        prices_[0] *= delta_;
        prices_[1] /= delta_;
      } else {
        prices_[0] /= delta_;
        prices_[1] *= delta_;
      }
    } else {
      static constexpr double sigma = 0.02;
      static constexpr double sqrt_dt = 0.062994078834; // (1.0 / 252) ** 0.5

      for (int i = 0; i < prices_.size(); i++) {
        prices_[i] = prices_[i] +
                     prices_[i] * (sigma * sqrt_dt * norm_dist_(generator_));
      }
    }
  }

private:
  int rand_value_;
  int num_rand_bits_;
  int next_rand_index_;
};

class BuyAndHold : public Flipper {
public:
  BuyAndHold(int num_stocks, double delta, double threshold)
      : Flipper(num_stocks, delta, threshold) {}
  ~BuyAndHold() {}

protected:
  void rebalance() override {}
};

class WaveArbitrage : public Flipper {
public:
  WaveArbitrage(int num_stocks, double delta, double threshold)
      : Flipper(num_stocks, delta, threshold) {}
  ~WaveArbitrage() {}

protected:
  void rebalance() override {
    double dollars_per_stock = total_shares() / num_stocks_;
    double threshold = dollars_per_stock * 1.01;

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

    for (int i = 0; i < num_stocks_; i++) {
      positions_[i] = dollars_per_stock / prices_[i];
    }
  }
};

void run_experiment(int num_stocks, int flips, int num_bh_trials, int num_wave_trials) {
  const auto num_cpus = std::thread::hardware_concurrency();
  const int kBatchSize = 1;

  std::mutex mu;

  mu.lock();

  int bh_trials_remaining = num_bh_trials;
  bh_trials_remaining -= bh_trials_remaining % kBatchSize;

  int wave_trials_remaining = num_wave_trials;
  wave_trials_remaining -= wave_trials_remaining % kBatchSize;

  double delta = 1.001;
  double threshold = 1.0;

  std::vector<double> bh_g_accs(num_cpus, 0);
  std::vector<double> bh_val_accs(num_cpus, 0);
  std::vector<int> bh_trials(num_cpus, 0);

  std::vector<double> wave_g_accs(num_cpus, 0);
  std::vector<double> wave_val_accs(num_cpus, 0);
  std::vector<int> wave_trials(num_cpus, 0);
  mu.unlock();

  std::thread threads[num_cpus];
  for (int i = 0; i < num_cpus; i++) {
    threads[i] = std::thread(
        [&](int i) {
          double local_bh_g_acc = 0.0;
          double local_bh_val_acc = 0.0;
          double local_wave_g_acc = 0.0;
          double local_wave_val_acc = 0.0;
          int local_bh_trials = 0;
          int local_wave_trials = 0;

          while (true) {
            int bh_batch = 0;
            int wave_batch = 0;

            mu.lock();
            if (bh_trials_remaining > 0) {
              bh_batch = kBatchSize;
              bh_trials_remaining -= kBatchSize;
            }

            if (wave_trials_remaining > 0) {
              wave_batch = kBatchSize;
              wave_trials_remaining -= kBatchSize;
            }
            mu.unlock();

            if (bh_batch == 0 && wave_batch == 0) {
              break;
            }

            local_bh_trials += bh_batch;
            local_wave_trials += wave_batch;

            for (int trial = 0; trial < bh_batch; trial++) {
              BuyAndHold bh(num_stocks, delta, threshold);
              bh.simulate(flips);
              local_bh_g_acc += bh.g();
              local_bh_val_acc += bh.value();
            }

            for (int trial = 0; trial < wave_batch; trial++) {
              WaveArbitrage wave(num_stocks, delta, threshold);
              wave.simulate(flips);
              local_wave_g_acc += wave.g();
              local_wave_val_acc += wave.value();
            }
          }

          mu.lock();
          bh_g_accs[i] = local_bh_g_acc;
          bh_val_accs[i] = local_bh_val_acc;
          bh_trials[i] = local_bh_trials;

          wave_g_accs[i] = local_wave_g_acc;
          wave_val_accs[i] = local_wave_val_acc;
          wave_trials[i] = local_wave_trials;
          mu.unlock();
        },
        i);
  }

  for (int i = 0; i < num_cpus; i++) {
    threads[i].join();
  }

  double bh_g_acc = 0.0;
  double bh_val_acc = 0.0;
  double wave_g_acc = 0.0;
  double wave_val_acc = 0.0;
  int total_bh_trials = 0;
  int total_wave_trials = 0;
  for (int i = 0; i < num_cpus; i++) {
    bh_g_acc += bh_g_accs[i];
    bh_val_acc += bh_val_accs[i];
    total_bh_trials += bh_trials[i];

    wave_g_acc += wave_g_accs[i];
    wave_val_acc += wave_val_accs[i];
    total_wave_trials += wave_trials[i];
  }

  printf("%d,%.10lf,%.10lf,%.10lf,%.10lf\n", flips, bh_g_acc / total_bh_trials,
         bh_val_acc / total_bh_trials, wave_g_acc / total_wave_trials,
         wave_val_acc / total_wave_trials);
}

int main() {
  srand(time(NULL));
  setbuf(stdout, NULL);

  printf("flips,bh_g,bh_val,wave_g,wave_val\n");
  // for (int i = 1; i < 1000000000; i = i + 1 + i * 0.1) {
  //  run_experiment(i, 16000, 4000);
  //}
  run_experiment(/*num_stocks=*/10, /*flips=*/100000, /*num_bh_trials=*/10000,
                 /*num_wave_trials=*/10000);
}
