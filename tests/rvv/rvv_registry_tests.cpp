#include "rvv_test_common.h"
#include "simdjson/rvv/implementation.h"
#include "simdjson/internal/instruction_set.h"

#include <vector>

int main() {
  bool ok = true;
  const auto *rvv = rvv_test::rvv();
  if (rvv == nullptr) {
    std::cerr << "[FAIL] rvv implementation is not registered\n";
    return 1;
  }

  ok &= rvv->name() == "rvv";
  ok &= rvv->required_instruction_sets() == simdjson::internal::instruction_set::RVV;
  ok &= rvv->supported_by_runtime_system();

  {
    rvv_test::active_implementation_guard guard;
    simdjson::get_active_implementation() = rvv;
    simdjson::dom::parser parser;
    auto json = R"({"ok":true,"n":123,"s":"rvv"})"_padded;
    simdjson::dom::element doc;
    if (parser.parse(json).get(doc) != simdjson::SUCCESS) {
      std::cerr << "[FAIL] basic RVV parse failed\n";
      ok = false;
    } else {
      bool flag = false;
      int64_t n = 0;
      std::string_view s;
      ok &= doc["ok"].get(flag) == simdjson::SUCCESS && flag;
      ok &= doc["n"].get(n) == simdjson::SUCCESS && n == 123;
      ok &= doc["s"].get(s) == simdjson::SUCCESS && s == "rvv";
    }
  }

  ok &= rvv->validate_utf8("ASCII", 5);
  const std::string euro = "\xE2\x82\xAC";
  ok &= rvv->validate_utf8(euro.data(), euro.size());

  {
    const std::string input = " { \"x\" : \"a b\" , \"n\" : 1 } ";
    simdjson::error_code error = simdjson::SUCCESS;
    const std::string output = rvv_test::minify_with(rvv, input, error);
    ok &= error == simdjson::SUCCESS;
    ok &= output == R"({"x":"a b","n":1})";
  }

  return rvv_test::finish(ok, "rvv_registry_tests");
}
