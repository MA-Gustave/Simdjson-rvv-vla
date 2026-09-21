#include "rvv_test_common.h"

int main() {
  if (!rvv_test::require_implementation("rvv")) { return 1; }
  rvv_test::active_implementation_guard guard;
  simdjson::get_active_implementation() = rvv_test::rvv();
  bool ok = true;

  std::string source;
  for (int i = 0; i < 200; ++i) {
    source += "{\"id\":" + std::to_string(i) + ",\"s\":\"row " +
              std::to_string(i) + "\"}\n";
  }
  const simdjson::padded_string input(source);
  const size_t windows[] = {32, 63, 64, 65, 127, 128, 129, 255, 256, 511, 1024, 4096};

  for (size_t window : windows) {
    simdjson::dom::parser parser;
    simdjson::dom::document_stream stream;
    const simdjson::error_code start = parser.parse_many(input, window).get(stream);
    if (start != simdjson::SUCCESS) {
      std::cerr << "[FAIL] parse_many setup window=" << window << " error=" << start << "\n";
      ok = false;
      continue;
    }
    size_t count = 0;
    for (auto result : stream) {
      simdjson::dom::element doc;
      const simdjson::error_code error = result.get(doc);
      if (error != simdjson::SUCCESS) {
        std::cerr << "[FAIL] parse_many document window=" << window << " index=" << count
                  << " error=" << error << "\n";
        ok = false;
        break;
      }
      int64_t id = -1;
      if (doc["id"].get(id) != simdjson::SUCCESS || id != int64_t(count)) {
        std::cerr << "[FAIL] parse_many value mismatch window=" << window
                  << " index=" << count << "\n";
        ok = false;
        break;
      }
      ++count;
    }
    if (count != 200) {
      std::cerr << "[FAIL] parse_many count window=" << window << " got=" << count << "\n";
      ok = false;
    }
  }

  return rvv_test::finish(ok, "rvv_streaming_tests");
}
