#include <stdio.h>

#include <algorithm>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include "market_data.pb.h"

std::vector<std::string> get_processed(std::string symbol) {
  std::vector<std::string> res;
  std::string comparison = "processed/" + symbol;
  for (const auto & f : std::filesystem::directory_iterator("processed")) {
    if (0 == f.path().string().compare(0, comparison.size(), comparison)) {
      res.push_back(f.path());
    }
  }

  std::sort(res.begin(), res.end());

  return res;
}


int main() {
  GOOGLE_PROTOBUF_VERIFY_VERSION;
  market_data::Events events;
  for (auto f : get_processed("AMZN")) {
    std::fstream input(f, std::ios::in | std::ios::binary);
    events.ParseFromIstream(&input);
    for (auto event : events.events()) {
      // XXX Check that it's actually a trade! event.has_trade()
      if (event.has_trade()) {
        printf("%lf\n", event.trade().price() / 10000.0);
      }
    }
  }

  return 0;
}
