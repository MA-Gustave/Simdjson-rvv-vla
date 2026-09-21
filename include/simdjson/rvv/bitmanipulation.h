#ifndef SIMDJSON_RVV_BITMANIPULATION_H
#define SIMDJSON_RVV_BITMANIPULATION_H

#ifndef SIMDJSON_CONDITIONAL_INCLUDE
#include "simdjson/rvv/base.h"
#endif // SIMDJSON_CONDITIONAL_INCLUDE

namespace simdjson {
namespace rvv {
namespace {

// Keep the scalar bit helpers required by generic number parsing and builders
// local to the RVV implementation. These operations are on scalar parser
// metadata, not on the Stage 1 byte stream, so RVV intrinsics would not help.
SIMDJSON_NO_SANITIZE_UNDEFINED
SIMDJSON_NO_SANITIZE_MEMORY
simdjson_inline int trailing_zeroes(uint64_t input_num) {
  return __builtin_ctzll(input_num);
}

simdjson_inline uint64_t clear_lowest_bit(uint64_t input_num) {
  return input_num & (input_num - 1);
}

simdjson_inline int leading_zeroes(uint64_t input_num) {
  return __builtin_clzll(input_num);
}

simdjson_inline long long int count_ones(uint64_t input_num) {
  return __builtin_popcountll(input_num);
}

simdjson_inline bool add_overflow(
    uint64_t value1, uint64_t value2, uint64_t *result) {
  return __builtin_uaddll_overflow(
      value1, value2, reinterpret_cast<unsigned long long *>(result));
}

} // unnamed namespace
} // namespace rvv
} // namespace simdjson

#endif // SIMDJSON_RVV_BITMANIPULATION_H
