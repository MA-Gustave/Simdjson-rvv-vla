#include "rvv_test_common.h"

#include <limits>

int main() {
  if (!rvv_test::require_rvv_and_fallback()) { return 1; }
  bool ok = true;

  const std::vector<std::pair<std::string, std::string>> cases = {
    {"empty-object", "{}"},
    {"empty-array", "[]"},
    {"primitives", R"([null,true,false,0,-1,1,1.25,1e30])"},
    {"object", R"({"a":1,"b":"text","c":[1,2,3]})"},
    {"nested", R"({"a":[{"b":[{"c":"d"}]}],"z":null})"},
    {"escapes", R"(["\\","\"","line\nfeed","\u20ac","a b c"] )"},
    {"unicode", std::string("[\"Bj\xC3\xB6rk\",\"\xE2\x82\xAC\",\"\xF0\x9F\x98\x80\"]")},
    {"pretty", " { \n \t\"a\" : [ 1 , 2 , 3 ] , \r\n \"b\" : \"space kept\" } "},
    {"bad-trailing-comma", R"({"a":1,})"},
    {"bad-array", "[1,2,3"},
    {"bad-number", "[01]"},
    {"bad-unquoted-key", "{a:1}"},
    {"unclosed-string", R"({"a":"unterminated})"}
  };

  for (const auto &c : cases) {
    ok &= rvv_test::compare_dom(c.first, c.second);
  }

  std::string large = "[";
  for (int i = 0; i < 3000; ++i) {
    if (i) { large.push_back(','); }
    large += "{\"id\":" + std::to_string(i) + ",\"s\":\"value " +
             std::to_string(i) + "\",\"b\":" + (i % 2 ? "true" : "false") + "}";
  }
  large.push_back(']');
  ok &= rvv_test::compare_dom("large-array", large);

  return rvv_test::finish(ok, "rvv_dom_equivalence_tests");
}
