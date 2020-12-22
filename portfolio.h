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

  string to_string(const std::vector<double> &prices, int indent = 0) const {
    string top_indent = "";
    for (int i = 0; i < indent; i++) {
      top_indent += " ";
    }

    string middle_indent = "";
    for (int i = 0; i < indent + 2; i++) {
      middle_indent += " ";
    }

    string s = top_indent + "Portfolio {\n" + middle_indent + "cash: $" +
               std::to_string(cash_) + ",\n";
    s += middle_indent + "value: " + std::to_string(value(prices)) + ",\n";
    s += middle_indent + "g: " + std::to_string(g(prices)) + ",\n";
    s += middle_indent + "stocks: {";
    for (size_t i = 0; i < symbols_.size(); i++) {
      s += symbols_[i] + ": " + std::to_string(shares_[i]) + ", ";
    }
    s += "}\n" + top_indent + "}";
    return s;
  }

  int index(string symbol) const {
    auto found = std::find(symbols_.begin(), symbols_.end(), symbol);
    if (found == symbols_.end()) {
      return -1;
    }
    return std::distance(symbols_.begin(), found);
  }

  double cash() const { return cash_; }

  double shares(size_t symbol_index) const {
    DCHECK_LT(symbol_index, shares_.size());
    return shares_[symbol_index];
  }

  double value(const std::vector<double> &prices) const {
    DCHECK_EQ(prices.size(), shares_.size());

    double value = cash_;
    for (size_t i = 0; i < prices.size(); i++) {
      value += shares_[i] * prices[i];
    }
    return value;
  }

  double g(const std::vector<double> &prices) const {
    double total_value = value(prices);
    double prod = 1.0;
    for (auto price : prices) {
      prod *= total_value / price;
    }

    return pow(prod, 1.0 / prices.size());
  }

  void buy(size_t symbol_index, double quantity, double price) {
    const double real_cost = static_cast<int>(100 * quantity * price) / 100.0;
    const double real_quantity = real_cost / price;

    DCHECK_LE(real_cost, cash_);

    cash_ -= real_cost;
    shares_[symbol_index] += real_quantity;
  }

  void sell(size_t symbol_index, double quantity, double price) {
    DCHECK_LE(quantity, shares_[symbol_index]);

    shares_[symbol_index] -= quantity;
    cash_ += quantity * price;
  }

  void pay_dividend(size_t symbol_index, double per_share) {
    cash_ += shares_[symbol_index] * per_share;
  }

  void stock_split(size_t symbol_index, double ratio) {
    shares_[symbol_index] *= ratio;
  }

private:
  double cash_;

  std::vector<string> symbols_;
  std::vector<double> shares_;
};

#endif // WAVE_ARBITRAGE_PORTFOLIO_H
