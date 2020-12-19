#include <stdio.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "market_data.pb.h"

std::vector<std::string> get_processed(std::string symbol) {
  static const std::string kProcessedDir =
      "/home/logan/data/processed/";
  std::string comparison = kProcessedDir + symbol;
  std::vector<std::string> res;
  for (const auto &f : std::filesystem::directory_iterator(kProcessedDir)) {
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
  for (auto f : get_processed("PPL")) {
    std::fstream input(f, std::ios::in | std::ios::binary);
    events.ParseFromIstream(&input);
    for (auto event : events.events()) {
      if (event.has_trade()) {
        printf("%lf\n", event.trade().price() / 10000.0);
      } else {
        printf("here: %s\n", event.DebugString().c_str());
      }
    }
  }

  return 0;
}
