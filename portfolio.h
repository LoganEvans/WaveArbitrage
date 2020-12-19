#ifndef WAVE_ARBITRAGE_PORTFOLIO_H
#define WAVE_ARBITRAGE_PORTFOLIO_H

#include <algorithm>
#include <string>

#include <glog/logging.h>


using std::string;

class Portfolio {
public:
  Portfolio(double cash, std::vector<string> symbols)
      : cash_(cash), symbols_(symbols) {
    shares_.resize(symbols_.size());
  }

  int index(string symbol) {
    auto found = std::find(symbols_.begin(), symbols_.end(), symbol);
    CHECK(found != symbols_.end());
    return std::distance(symbols_.begin(), found);
  }

  double cash() { return cash_; }

  double shares(int symbol_index) { return shares_[symbol_index]; }

  double value(const std::vector<double>& prices) {
    CHECK_EQ(prices.size(), shares_.size());

    double value = cash_;
    for (size_t i = 0; i < prices.size(); i++) {
      value += shares_[i] * prices[i];
    }
    return value;
  }

  void buy(int symbol_index, double quantity, double price) {
    double cost = quantity * price;

    CHECK_LE(cost, cash_);

    cash_ -= cost;
    shares_[symbol_index] += quantity;
  }

  void sell(int symbol_index, double quantity, double price) {
    CHECK_LE(quantity, shares_[symbol_index]);

    shares_[symbol_index] -= quantity;
    cash_ += quantity * price;
  }

private:
  double cash_;

  std::vector<string> symbols_;
  std::vector<double> shares_;
};

#endif // WAVE_ARBITRAGE_PORTFOLIO_H
