#include "rvv_test_common.h"

#include <cmath>

int main() {
  if (!rvv_test::require_implementation("rvv")) { return 1; }
  rvv_test::active_implementation_guard guard;
  simdjson::get_active_implementation() = rvv_test::rvv();
  bool ok = true;

  {
    simdjson::ondemand::parser parser;
    auto json = R"({"i":123,"d":3.14,"s":"text value","b":true,"n":null})"_padded;
    simdjson::ondemand::document doc;
    ok &= parser.iterate(json).get(doc) == simdjson::SUCCESS;
    int64_t i = 0;
    double d = 0;
    std::string_view s;
    bool b = false;
    bool is_null = false;
    ok &= doc["i"].get(i) == simdjson::SUCCESS && i == 123;
    ok &= doc["d"].get(d) == simdjson::SUCCESS && std::abs(d - 3.14) < 1e-12;
    ok &= doc["s"].get(s) == simdjson::SUCCESS && s == "text value";
    ok &= doc["b"].get(b) == simdjson::SUCCESS && b;
    ok &= doc["n"].is_null().get(is_null) == simdjson::SUCCESS && is_null;
  }

  {
    simdjson::ondemand::parser parser;
    auto json = R"([{"id":1,"skip":[1,2,3]},{"id":2,"skip":{"x":"y"}},{"id":3,"value":"keep"}])"_padded;
    simdjson::ondemand::document doc;
    ok &= parser.iterate(json).get(doc) == simdjson::SUCCESS;
    simdjson::ondemand::array array;
    ok &= doc.get_array().get(array) == simdjson::SUCCESS;
    size_t count = 0;
    for (auto item : array) {
      ++count;
      if (count == 3) {
        int64_t id = 0;
        std::string_view value;
        ok &= item["id"].get(id) == simdjson::SUCCESS && id == 3;
        ok &= item["value"].get(value) == simdjson::SUCCESS && value == "keep";
      }
    }
    ok &= count == 3;
  }

  {
    std::string text(2049, 'a');
    text.replace(1000, 1, " ");
    std::string source = "{\"payload\":\"" + text + "\",\"tail\":42}";
    simdjson::padded_string json(source);
    simdjson::ondemand::parser parser;
    simdjson::ondemand::document doc;
    ok &= parser.iterate(json).get(doc) == simdjson::SUCCESS;
    std::string_view payload;
    int64_t tail = 0;
    ok &= doc["payload"].get(payload) == simdjson::SUCCESS && payload == text;
    ok &= doc["tail"].get(tail) == simdjson::SUCCESS && tail == 42;
  }

  return rvv_test::finish(ok, "rvv_ondemand_tests");
}
