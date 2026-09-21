#include "rvv_test_common.h"

int main() {
  if (!rvv_test::require_rvv_and_fallback()) { return 1; }
  bool ok = true;

  const size_t boundaries[] = {
    1, 2, 3, 7, 8, 15, 16, 31, 32, 33, 63, 64, 65,
    127, 128, 129, 255, 256, 257, 511, 512, 513, 1023, 1024, 1025
  };

  for (size_t boundary : boundaries) {
    {
      const std::string prefix = R"({"key")";
      const size_t pad = boundary > prefix.size() ? boundary - prefix.size() : 0;
      const std::string json = prefix + std::string(pad, ' ') + R"(: [1,2,3]})";
      ok &= rvv_test::compare_dom("structural-boundary-" + std::to_string(boundary), json);
    }
    {
      const std::string prefix = R"({"key":)";
      const size_t pad = boundary > prefix.size() ? boundary - prefix.size() : 0;
      const std::string json = prefix + std::string(pad, ' ') + R"("value with spaces"})";
      ok &= rvv_test::compare_dom("quote-boundary-" + std::to_string(boundary), json);
    }
    {
      std::string json = R"({"x":")";
      if (boundary > json.size()) { json.append(boundary - json.size(), 'a'); }
      json += " inside spaces ";
      json += R"(","y":1})";
      ok &= rvv_test::compare_dom("string-state-boundary-" + std::to_string(boundary), json);
    }

    for (size_t run = 1; run <= 16; ++run) {
      std::string json = R"({"x":")";
      if (boundary > json.size() + run) {
        json.append(boundary - json.size() - run, 'a');
      }
      json.append(run, '\\');
      json.push_back('"');
      json += R"(tail","y":1})";
      ok &= rvv_test::compare_dom(
          "backslash-run-b" + std::to_string(boundary) + "-n" + std::to_string(run), json);
    }
  }

  // Raw control bytes inside a quoted string are rejected by Stage 1.
  for (size_t boundary : boundaries) {
    if (boundary < 8) { continue; }
    std::string json = R"({"x":")";
    json.append(boundary - json.size(), 'a');
    json.push_back(char(0x01));
    json += R"("})";
    ok &= rvv_test::compare_dom("control-in-string-" + std::to_string(boundary), json);
  }

  return rvv_test::finish(ok, "rvv_stage1_boundary_tests");
}
