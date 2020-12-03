#include <iostream>
#include <fstream>
#include <memory>
#include <string>
#include <stdio.h>

#include "market_data.pb.h"


int main() {
  GOOGLE_PROTOBUF_VERIFY_VERSION;
  market_data::Events events;
  std::fstream input("processed/ZTS_20161227",
                     std::ios::in | std::ios::binary);
  events.ParseFromIstream(&input);

  for (auto event : events.events()) {
    printf("??? %s\n", events.DebugString().c_str());
  }
  printf("%u\n", events.events().size());

  return 0;
}
