#include "rvv_test_common.h"

int main() {
  if (!rvv_test::require_rvv_and_fallback()) { return 1; }
  bool ok = true;

  const std::vector<std::pair<std::string, std::string>> exact = {
    {"", ""},
    {"   \t\r\n", ""},
    {" { } ", "{}"},
    {" { \"key\" : 1 , \"list\" : [ 1 , 2 ] } ", "{\"key\":1,\"list\":[1,2]}"},
    {"{ \"x\" : \"a b c\" }", "{\"x\":\"a b c\"}"},
    {R"json({ "x" : "escaped quote: \" and slash: \\" , "n" : 1 })json",
     R"json({"x":"escaped quote: \" and slash: \\","n":1})json"}
  };

  for (size_t i = 0; i < exact.size(); ++i) {
    simdjson::error_code error = simdjson::SUCCESS;
    const std::string got = rvv_test::minify_with(rvv_test::rvv(), exact[i].first, error);
    if (error != simdjson::SUCCESS || got != exact[i].second) {
      std::cerr << "[FAIL] exact minify case " << i << "\n";
      ok = false;
    }
    ok &= rvv_test::compare_minify("exact-" + std::to_string(i), exact[i].first);
  }

  const size_t boundaries[] = {1, 15, 16, 31, 32, 63, 64, 65, 127, 128, 129,
                               255, 256, 257, 511, 512, 513, 1024};
  for (size_t n : boundaries) {
    const std::string input = " { \"x\" : \"" + std::string(n, 'a') +
                              " space kept \" , \"y\" : [ 1 , 2 , 3 ] } ";
    ok &= rvv_test::compare_minify("boundary-" + std::to_string(n), input);
  }

  for (size_t n : boundaries) {
    for (size_t run = 1; run <= 8; ++run) {
      std::string input = "{ \"x\" : \"";
      input.append(n, 'a');
      input.append(run, '\\');
      input += "\" tail \" , \"z\" : 7 }";
      ok &= rvv_test::compare_minify(
          "backslash-boundary-" + std::to_string(n) + "-" + std::to_string(run), input);
    }
  }

  {
    const std::string unclosed = "{ \"x\" : \"unterminated  ";
    ok &= rvv_test::compare_minify("unclosed-string", unclosed);
  }

  return rvv_test::finish(ok, "rvv_minify_tests");
}
