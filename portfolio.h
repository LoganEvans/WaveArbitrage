#ifndef WAVE_ARBITRAGE_PORTFOLIO_H
#define WAVE_ARBITRAGE_PORTFOLIO_H

#include <algorithm>
#include <string>

#include "absl/strings/str_join.h"
#include <glog/logging.h>

using std::string;

class Portfolio {
public:
  Portfolio(double cash, std::vector<string> symbols)
      : cash_(cash), symbols_(symbols) {
    shares_.resize(symbols_.size());
  }

  string to_string(int indent = 0) const {
    string top_indent = "";
    for (int i = 0; i < indent; i++) {
      absl::StrAppend(&top_indent, " ");
    }

    string middle_indent = "";
    for (int i = 0; i < indent + 2; i++) {
      absl::StrAppend(&middle_indent, " ");
    }

    string s = absl::StrCat(top_indent, "Portfolio {\n", middle_indent,
                            "cash: $", cash_, ",\n");
    for (size_t i = 0; i < symbols_.size(); i++) {
      absl::StrAppend(&s, middle_indent, symbols_[i], ": ", shares_[i], ",\n");
    }
    absl::StrAppend(&s, top_indent, "}");
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
    CHECK_LT(symbol_index, shares_.size());
    return shares_[symbol_index];
  }

  double value(const std::vector<double> &prices) const {
    CHECK_EQ(prices.size(), shares_.size());

    double value = cash_;
    for (size_t i = 0; i < prices.size(); i++) {
      value += shares_[i] * prices[i];
    }
    return value;
  }

  void buy(size_t symbol_index, double quantity, double price) {
    double cost = quantity * price;

    CHECK_LE(cost, cash_);

    cash_ -= cost;
    shares_[symbol_index] += quantity;
  }

  void sell(size_t symbol_index, double quantity, double price) {
    CHECK_LE(quantity, shares_[symbol_index]);

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
