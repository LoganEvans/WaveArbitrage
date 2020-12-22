#ifndef WAVE_ARBITRAGE_STRATEGY_H
#define WAVE_ARBITRAGE_STRATEGY_H

#include <string>

#include "portfolio.h"

using std::string;

class Strategy {
public:
  Strategy(double cash, std::vector<string> symbols)
      : folio_(cash, symbols) {}

  string to_string(const std::vector<double> &prices, int indent = 0) const {
    string top_indent = "";
    for (int i = 0; i < indent; i++) {
      top_indent += " ";
    }

    string middle_indent = "";
    for (int i = 0; i < indent + 2; i++) {
      middle_indent += " ";
    }

    string s = top_indent + strategy_name() + " {\n";
    s += middle_indent + "rebalances: " + std::to_string(rebalances_) + ",\n";
    s += middle_indent + portfolio().to_string(prices, indent + 2) + "\n";
    s += top_indent + "}";
    return s;
  }

  virtual string strategy_name() const = 0;

  virtual bool price_event(const std::vector<double>& prices) = 0;

  const Portfolio &portfolio() const { return folio_; }

  void rebalance(const std::vector<double> &prices) {
    for (size_t i = 0; i < prices.size(); i++) {
      CHECK_GE(portfolio().shares(i), 0.0) << rebalances_;
      folio_.sell(i, portfolio().shares(i), prices[i]);
    }

    double per_stock = portfolio().cash() / prices.size();
    for (size_t i = 0; i < prices.size(); i++) {
      folio_.buy(i, per_stock / prices[i], prices[i]);
    }
    rebalances_++;
  }

  void pay_dividend(string symbol, double per_share) {
    folio_.pay_dividend(portfolio().index(symbol), per_share);
  }

  void stock_split(string symbol, double ratio) {
    folio_.stock_split(portfolio().index(symbol), ratio);
  }

protected:
  Portfolio folio_;
  int rebalances_ = 0;
};

class BuyAndHold : public Strategy {
public:
  BuyAndHold(double cash, std::vector<string> symbols,
             const std::vector<double> &prices)
      : Strategy(cash, std::move(symbols)),
        rebalance_cash_(prices.size() * 0.01) {
    rebalance(prices);
  }

  string strategy_name() const { return "BuyAndHold"; }

  bool price_event(const std::vector<double> &prices) {
    if (portfolio().cash() < rebalance_cash_) {
      return false;
    }
    // Reinvest dividends.
    double per_share = portfolio().cash() / prices.size();
    for (size_t i = 0; i < prices.size(); i++) {
      folio_.buy(i, /*quantity=*/per_share / prices[i], /*price=*/prices[i]);
    }
    return true;
  }

protected:
  const double rebalance_cash_;
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
    if (portfolio().cash() != 0.0) {
      rebalance(prices);
      return true;
    }

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
