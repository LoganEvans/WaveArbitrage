#ifndef WAVE_ARBITRAGE_STRATEGY_H
#define WAVE_ARBITRAGE_STRATEGY_H

#include <string>

#include "portfolio.h"

using std::string;

class Strategy {
public:
  Strategy(double cash, std::vector<string> symbols)
      : rebalance_cash_(symbols.size() * 0.01), folio_(cash, symbols) {}

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
    s += middle_indent + "rebalances: " + std::to_string(num_rebalances_) +
         ",\n";
    s += middle_indent + "dividends: " + std::to_string(num_dividends_) + ",\n";
    s += middle_indent + "splits: " + std::to_string(num_splits_) + ",\n";
    s += portfolio().to_string(prices, indent + 2) + "\n";
    s += top_indent + "}";
    return s;
  }

  virtual string strategy_name() const = 0;

  virtual bool price_event(const std::vector<double> &prices) = 0;

  const Portfolio &portfolio() const { return folio_; }

  void rebalance(const std::vector<double> &prices) {
    // This mostly ignores fees. However, the difference between the
    // positions should diminish as the portfolio continually rebalances.

    std::vector<double> values;
    double total = portfolio().cash();
    for (size_t i = 0; i < prices.size(); i++) {
      double value = portfolio().shares(i) * prices[i];
      values.push_back(value);
      total += value;
    }

    const double desired = total / prices.size();
    for (size_t i = 0; i < prices.size(); i++) {
      if (values[i] > desired) {
        folio_.sell(i, (values[i] - desired) / prices[i], prices[i]);
      }
      DCHECK_GE(portfolio().shares(i), 0.0) << num_rebalances_;
    }

    for (size_t i = 0; i < prices.size(); i++) {
      if (values[i] < desired) {
        folio_.buy(i, desired - values[i], prices[i]);
      }
    }

    num_rebalances_++;
  }

  void pay_dividend(string symbol, double per_share) {
    folio_.pay_dividend(portfolio().index(symbol), per_share);
    num_dividends_ += 1;
  }

  void stock_split(string symbol, double ratio) {
    folio_.stock_split(portfolio().index(symbol), ratio);
    num_splits_ += 1;
  }

protected:
  const double rebalance_cash_;
  Portfolio folio_;
  int num_rebalances_ = 0;
  int num_dividends_ = 0;
  int num_splits_ = 0;
};

class BuyAndHold : public Strategy {
public:
  BuyAndHold(double cash, std::vector<string> symbols,
             const std::vector<double> &prices)
      : Strategy(cash, std::move(symbols)) {
    rebalance(prices);
  }

  string strategy_name() const { return "BuyAndHold"; }

  bool price_event(const std::vector<double> &prices) {
    if (portfolio().cash() < rebalance_cash_) {
      return false;
    }
    // Reinvest dividends.
    double per_stock = portfolio().cash() / prices.size();
    for (size_t i = 0; i < prices.size(); i++) {
      folio_.buy(i, /*cash_to_spend=*/per_stock, /*price=*/prices[i]);
    }
    return true;
  }
};

class WaveArbitrage : public Strategy {
public:
  WaveArbitrage(double cash, std::vector<string> symbols,
                const std::vector<double> &prices, double rebalance_threshold)
      : Strategy(cash, std::move(symbols)),
        rebalance_threshold_(rebalance_threshold) {
    rebalance_down_.resize(2);
    rebalance_up_.resize(2);
    rebalance(prices);
  }

  string strategy_name() const { return "WaveArbitrage"; }

  bool price_event(const std::vector<double> &prices) {
    bool do_rebalance = false;
    std::vector<double> rebalance_prices = prices;
    // Handle dividend events.
    if (portfolio().cash() > rebalance_cash_) {
      do_rebalance = true;
    }

    for (size_t i = 0; i < prices.size(); i++) {
      // This should help avoid slippage. Since the last price event, only one
      // of the prices will have changed (generally speaking). This models
      // putting in a limit order. On the past price event, we calculated the
      // thresholds, and we will use those thresholds for the rebalance.
      if (prices[i] < rebalance_down_[i]) {
        do_rebalance = true;
        rebalance_prices[i] = rebalance_down_[i] + 0.01;
      } else if (prices[i] > rebalance_up_[i]) {
        do_rebalance = true;
        rebalance_prices[i] = rebalance_up_[i] - 0.01;
      }
    }

    // TODO(lpe): Assuming 2 asset types here.
    std::vector<double> values = {portfolio().shares(0) * prices[0],
                                  portfolio().shares(1) * prices[1]};
    double total = values[0] + values[1];

    for (size_t i = 0; i < prices.size(); i++) {
      // Set the threshold a penny away to model situations where some limit
      // orders put in at a certain price might have executed, but ours might
      // be at the bottom of the pile.
      rebalance_down_[i] =
          (total / 2.0) / rebalance_threshold_ / portfolio().shares(i) - 0.01;
      rebalance_up_[i] =
          (total / 2.0) * rebalance_threshold_ / portfolio().shares(i) + 0.01;
    }

    if (do_rebalance) {
      rebalance(prices);
    }

    return do_rebalance;;
  }

protected:
  const double rebalance_threshold_;

  std::vector<double> rebalance_down_;
  std::vector<double> rebalance_up_;
};

#endif // WAVE_ARBITRAGE_STRATEGY_H
