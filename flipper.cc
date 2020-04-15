#include <stdio.h>
#include <time.h>

#include <cassert>
#include <cmath>
#include <cstdlib>
#include <vector>

class Flipper {
  public:
    Flipper(int num_stocks, double delta, double threshold) :
        num_stocks_(num_stocks),
        delta_(delta),
        threshold_(threshold),
        positions_(num_stocks, 1.0),
        prices_(num_stocks, 1.0) {

        assert(num_stocks == 2);

        unsigned int v = RAND_MAX; // count the number of bits set in v
        unsigned int c; // c accumulates the total bits set in v
        for (c = 0; v; v >>= 1) {
              c += v & 1;
        }
        num_rand_bits_ = c;
        next_rand_index_ = c;
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
        for (auto price: prices_) {
            prod *= total / price;
        }

        return pow(prod, 1.0 / num_stocks_);
    }

    void simulate(int flips) {
        for (int i = 0; i < flips; i++) {
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

    virtual void rebalance() {}

    bool next_rand() {
        bool val = false;
        if (next_rand_index_ >= num_rand_bits_) {
            rand_value_ = rand();
            next_rand_index_ = 0;
        }

        if (rand_value_ & (1 << next_rand_index_)) {
            val = true;
        }
        next_rand_index_++;

        return val;
    }

    void adjust_prices() {
        if (next_rand()) {
            prices_[0] *= delta_;
            prices_[1] /= delta_;
        } else {
            prices_[0] /= delta_;
            prices_[1] *= delta_;
        }
    }

  private:
    int rand_value_;
    int num_rand_bits_;
    int next_rand_index_;
};

class BuyAndHold : public Flipper {
  public:
    BuyAndHold(int num_stocks, double delta, double threshold) : Flipper(num_stocks, delta, threshold) {}
    ~BuyAndHold() {}

  protected:
    void rebalance() override {}
};

class WaveArbitrage: public Flipper {
  public:
    WaveArbitrage(int num_stocks, double delta, double threshold) : Flipper(num_stocks, delta, threshold) {}
    ~WaveArbitrage() {}

  protected:
    void rebalance() override {
        double total = total_shares();
        double target_val = total / num_stocks_;
        for (int i = 0; i < num_stocks_; i++) {
            double val = positions_[i] * prices_[i];
            static bool kSkipThreshold = true;
            if (kSkipThreshold || val / target_val > threshold_) {
                for (int j = 0; j < num_stocks_; j++) {
                    double v = positions_[j] * prices_[j];
                    positions_[j] = total / num_stocks_ / prices_[j];
                }
                break;
            }
        }
    }
};

int main() {
    srand(time(NULL));
    int num_stocks = 2;
    int flips = 10000000;
    int trials = 1000;
    double delta = 1.001;
    double threshold = 1.0;

    double bh_acc = 0.0;
    double wave_acc = 0.0;
    double bh_acc_geo = 1.0;
    double wave_acc_geo = 1.0;

    for (int trial = 0; trial < trials; trial++) {
        BuyAndHold bh(num_stocks, delta, threshold);
        bh.simulate(flips);
        bh_acc += bh.g();
        bh_acc_geo *= pow(bh.g(), 1.0 / trials);

        WaveArbitrage wave(num_stocks, delta, threshold);
        wave.simulate(flips);
        wave_acc += wave.g();
        wave_acc_geo *= pow(wave.g(), 1.0 / trials);
    }

    printf("arithmetic: bh: %lf, wave: %lf\n", bh_acc / trials, wave_acc / trials);
    printf("geometric: bh: %lf, wave: %lf\n", bh_acc_geo, wave_acc_geo);
}
