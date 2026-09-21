#include "rvv_test_common.h"

int main() {
  if (!rvv_test::require_rvv_and_fallback()) { return 1; }
  bool ok = true;

  const std::vector<std::string> cases = {
    "0", "-0", "1", "-1", "12345678", "123456789",
    "9223372036854775807", "-9223372036854775808", "18446744073709551615",
    "1.0", "-1.0", "3.141592653589793", "1e-300", "1e300",
    "2.2250738585072014e-308", "1.7976931348623157e308",
    "[0,1,12,123,1234,12345,123456,1234567,12345678,123456789]",
    "[1.25,-3.5,6.02214076e23,9.1093837015e-31]",
    "01", "1.", ".1", "1e", "--1", "18446744073709551616"
  };

  for (size_t i = 0; i < cases.size(); ++i) {
    ok &= rvv_test::compare_dom("number-" + std::to_string(i), cases[i]);
  }

  std::string dense = "[";
  for (int i = 0; i < 5000; ++i) {
    if (i) { dense.push_back(','); }
    dense += std::to_string(10000000 + i);
  }
  dense.push_back(']');
  ok &= rvv_test::compare_dom("dense-eight-digit-numbers", dense);

  return rvv_test::finish(ok, "rvv_number_tests");
}
