#include "rvv_test_common.h"

#include <random>

int main() {
  if (!rvv_test::require_rvv_and_fallback()) { return 1; }
  bool ok = true;

  struct utf8_case { const char *name; std::string bytes; bool valid; };
  const std::vector<utf8_case> cases = {
    {"empty", "", true},
    {"ascii", "plain ascii", true},
    {"two-byte", "\xC3\xB1", true},
    {"three-byte", "\xE2\x82\xAC", true},
    {"four-byte", "\xF0\x9F\x98\x80", true},
    {"truncated-two", "\xC3", false},
    {"truncated-three-1", "\xE2", false},
    {"truncated-three-2", "\xE2\x82", false},
    {"truncated-four-3", "\xF0\x9F\x98", false},
    {"overlong-two", "\xC0\xAF", false},
    {"overlong-three", "\xE0\x80\x80", false},
    {"surrogate-min", "\xED\xA0\x80", false},
    {"surrogate-max", "\xED\xBF\xBF", false},
    {"too-large", "\xF4\x90\x80\x80", false},
    {"five-byte-lead", "\xF8\x88\x80\x80\x80", false},
    {"stray-cont", "\x80", false}
  };

  for (const auto &c : cases) {
    const bool got = rvv_test::rvv()->validate_utf8(c.bytes.data(), c.bytes.size());
    if (got != c.valid) {
      std::cerr << "[FAIL] explicit UTF-8 case " << c.name << "\n";
      ok = false;
    }
    ok &= rvv_test::compare_utf8(c.name, c.bytes);
  }

  const size_t boundaries[] = {1, 2, 3, 7, 8, 15, 16, 31, 32, 63, 64, 65,
                               127, 128, 129, 255, 256, 257, 511, 512, 513,
                               1023, 1024, 1025};
  const std::string euro = "\xE2\x82\xAC";
  const std::string emoji = "\xF0\x9F\x98\x80";
  for (size_t boundary : boundaries) {
    for (size_t split = 0; split <= 3; ++split) {
      if (boundary < split) { continue; }
      std::string s(boundary - split, 'a');
      s += euro;
      s.append(17, 'b');
      ok &= rvv_test::compare_utf8(
          "euro-boundary-" + std::to_string(boundary) + "-s" + std::to_string(split), s);
    }
    for (size_t split = 0; split <= 4; ++split) {
      if (boundary < split) { continue; }
      std::string s(boundary - split, 'a');
      s += emoji;
      s.append(17, 'c');
      ok &= rvv_test::compare_utf8(
          "emoji-boundary-" + std::to_string(boundary) + "-s" + std::to_string(split), s);
    }
  }

  std::mt19937_64 rng(0x5256564c41555446ULL);
  for (size_t i = 0; i < 1500; ++i) {
    const size_t len = size_t(rng() % 2049);
    std::string bytes(len, '\0');
    for (size_t j = 0; j < len; ++j) {
      bytes[j] = char(rng() & 0xffu);
    }
    if (!rvv_test::compare_utf8("random-" + std::to_string(i), bytes)) {
      ok = false;
      break;
    }
  }

  return rvv_test::finish(ok, "rvv_utf8_tests");
}
