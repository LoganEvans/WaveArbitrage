#ifndef WAVE_ARBITRAGE_STRATEGY_H
#define WAVE_ARBITRAGE_STRATEGY_H

#include "portfolio.h"

using std::string;

class Strategy {
public:
  Strategy(double cash, std::vector<string> symbols)
      : folio_(cash, symbols) {}

  string to_string() const {
    string s = absl::StrCat(strategy_name(), " {\n");
    absl::StrAppend(&s, "  rebalances: ", rebalances_, ",\n");
    absl::StrAppend(&s, portfolio().to_string(2), "\n");
    absl::StrAppend(&s, "}");
    return s;
  }

  virtual string strategy_name() const = 0;

  virtual bool price_event(const std::vector<double>& prices) = 0;

  const Portfolio &portfolio() const { return folio_; }

  void rebalance(const std::vector<double> &prices) {
    for (size_t i = 0; i < prices.size(); i++) {
      folio_.sell(i, folio_.shares(i), prices[i]);
    }
    double per_stock = portfolio().cash() / prices.size();
    for (size_t i = 0; i < prices.size(); i++) {
      folio_.buy(i, per_stock / prices[i], prices[i]);
    }
    rebalances_++;
  }

protected:
  Portfolio folio_;
  int rebalances_ = 0;
};

class BuyAndHold : public Strategy {
public:
  BuyAndHold(double cash, std::vector<string> symbols,
             const std::vector<double> &prices)
      : Strategy(cash, std::move(symbols)) {
    rebalance(prices);
  }

  string strategy_name() const { return "BuyAndHold"; }

  bool price_event(const std::vector<double> &prices) { return false; }
};

class WaveArbitrage : public Strategy {
public:
  WaveArbitrage(double cash, std::vector<string> symbols,
                const std::vector<double> &prices, double rebalance_threshold)
      : Strategy(cash, std::move(symbols)),
        rebalance_threshold_(rebalance_threshold) {
    rebalance(prices);
  }

  string strategy_name() const { return "WaveArbitrage"; }

  bool price_event(const std::vector<double> &prices) {
    const double shares0 = portfolio().shares(0);
    double min_value = shares0 * prices[0];
    double max_value = shares0 * prices[0];
    for (size_t i = 1; i < prices.size(); i++) {
      double value = portfolio().shares(i) * prices[i];
      if (value < min_value) {
        min_value = value;
      }
      if (value > min_value) {
        max_value = value;
      }
      if (max_value / min_value > rebalance_threshold_) {
        rebalance(prices);
        return true;
      }
    }

    return false;
  }

protected:
  const double rebalance_threshold_;
};


#endif // WAVE_ARBITRAGE_STRATEGY_H
