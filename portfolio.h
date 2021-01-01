#ifndef WAVE_ARBITRAGE_PORTFOLIO_H
#define WAVE_ARBITRAGE_PORTFOLIO_H

#include <algorithm>
#include <string>

#include <glog/logging.h>

using std::string;

class Portfolio {
public:
  static constexpr double kFeePerShare = 0.0009;

  Portfolio(double cash, std::vector<string> symbols)
      : cash_(cash), fees_(0.0), symbols_(symbols) {
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
    s += middle_indent + "fees: " + std::to_string(fees_) + ",\n";
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

  void buy(size_t symbol_index, double cash_to_spend, double price) {
    if (cash_to_spend > cash_) {
      cash_to_spend = cash_;
    }

    const double shares = cash_to_spend / (price + kFeePerShare);
    const double fee = shares * kFeePerShare;

    cash_ -= cash_to_spend;
    fees_ += fee;
    shares_[symbol_index] += shares;
  }

  void sell(size_t symbol_index, double quantity, double price) {
    DCHECK_LE(quantity, shares_[symbol_index]);

    shares_[symbol_index] -= quantity;
    const double fee = quantity * kFeePerShare;
    cash_ += quantity * price - fee;
    fees_ += fee;
  }

  void pay_dividend(size_t symbol_index, double per_share) {
    cash_ += shares_[symbol_index] * per_share;
  }

  void stock_split(size_t symbol_index, double ratio) {
    shares_[symbol_index] *= ratio;
  }

private:
  double cash_;
  double fees_;

  std::vector<string> symbols_;
  std::vector<double> shares_;
};

#endif // WAVE_ARBITRAGE_PORTFOLIO_H
