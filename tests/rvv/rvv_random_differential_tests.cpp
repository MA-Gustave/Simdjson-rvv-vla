#include "rvv_test_common.h"

#include <random>

static std::string random_json_string(std::mt19937_64 &rng, size_t len) {
  std::string out = "\"";
  for (size_t i = 0; i < len; ++i) {
    switch (rng() % 12) {
      case 0: out += " "; break;
      case 1: out += "\\\\"; break;
      case 2: out += "\\\""; break;
      case 3: out += "\\n"; break;
      case 4: out += "\\t"; break;
      default: out.push_back(char('a' + (rng() % 26))); break;
    }
  }
  out += "\"";
  return out;
}

static std::string make_document(std::mt19937_64 &rng, size_t index) {
  std::string out = "{\"id\":" + std::to_string(index) + ",\"name\":";
  out += random_json_string(rng, size_t(rng() % 300));
  out += ",\"items\":[";
  const size_t count = size_t(rng() % 24);
  for (size_t i = 0; i < count; ++i) {
    if (i) { out += (rng() & 1) ? "," : " , \n "; }
    out += std::to_string(int64_t(rng() % 2000000) - 1000000);
  }
  out += "],\"flag\":";
  out += (rng() & 1) ? "true" : "false";
  out += ",\"nested\":{\"x\":" + std::to_string(rng() % 100000) + "}}";
  return out;
}

int main() {
  if (!rvv_test::require_rvv_and_fallback()) { return 1; }
  bool ok = true;
  std::mt19937_64 rng(0x53494d444a534f4eULL);

  for (size_t i = 0; i < 800; ++i) {
    const std::string json = make_document(rng, i);
    if (!rvv_test::compare_dom("random-valid-" + std::to_string(i), json)) {
      ok = false;
      break;
    }
    if (!rvv_test::compare_minify("random-minify-" + std::to_string(i), json)) {
      ok = false;
      break;
    }
  }

  for (size_t i = 0; i < 200 && ok; ++i) {
    std::string json = make_document(rng, i);
    switch (i % 4) {
      case 0: json.pop_back(); break;
      case 1: json += ",}"; break;
      case 2: json.insert(json.size() / 2, "\""); break;
      case 3: json.insert(json.size() / 3, std::string(1, char(0x01))); break;
    }
    if (!rvv_test::compare_dom("random-invalid-" + std::to_string(i), json)) {
      ok = false;
      break;
    }
  }

  return rvv_test::finish(ok, "rvv_random_differential_tests");
}
