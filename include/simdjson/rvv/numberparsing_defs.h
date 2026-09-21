#ifndef SIMDJSON_RVV_NUMBERPARSING_DEFS_H
#define SIMDJSON_RVV_NUMBERPARSING_DEFS_H

#ifndef SIMDJSON_CONDITIONAL_INCLUDE
#include "simdjson/rvv/base.h"
#include "simdjson/rvv/intrinsics.h"
#include "simdjson/internal/numberparsing_tables.h"
#endif // SIMDJSON_CONDITIONAL_INCLUDE

#include <cstring>

#ifdef JSON_TEST_NUMBERS
void found_invalid_number(const uint8_t *buf);
void found_integer(int64_t result, const uint8_t *buf);
void found_unsigned_integer(uint64_t result, const uint8_t *buf);
void found_float(double result, const uint8_t *buf);
#endif

namespace simdjson {
namespace rvv {
namespace numberparsing {

static simdjson_inline uint32_t parse_eight_digits_unrolled(const char *chars) {
  uint64_t val;
#if defined(__riscv_misaligned_fast) && __riscv_misaligned_fast
  memcpy(&val, chars, sizeof(uint64_t));
#else
  // Avoid a potentially expensive unaligned scalar 64-bit load. Eight byte
  // lanes fit in u8mf2 at the minimum supported VLEN and are moved to xlen
  // without touching a temporary memory buffer.
  const vuint8mf2_t bytes =
      __riscv_vle8_v_u8mf2(reinterpret_cast<const uint8_t *>(chars), 8);
  const vuint8m1_t wide = __riscv_vlmul_ext_v_u8mf2_u8m1(bytes);
  val = __riscv_vmv_x_s_u64m1_u64(__riscv_vreinterpret_v_u8m1_u64m1(wide));
#endif
  val = (val & 0x0F0F0F0F0F0F0F0FULL) * 2561 >> 8;
  val = (val & 0x00FF00FF00FF00FFULL) * 6553601 >> 16;
  return uint32_t((val & 0x0000FFFF0000FFFFULL) * 42949672960001ULL >> 32);
}

static simdjson_inline uint32_t parse_eight_digits_unrolled(const uint8_t *chars) {
  return parse_eight_digits_unrolled(reinterpret_cast<const char *>(chars));
}

simdjson_inline internal::value128 full_multiplication(
    uint64_t value1, uint64_t value2) {
  internal::value128 answer;
  __uint128_t r = (static_cast<__uint128_t>(value1)) * value2;
  answer.low = uint64_t(r);
  answer.high = uint64_t(r >> 64);
  return answer;
}

} // namespace numberparsing
} // namespace rvv
} // namespace simdjson

#define SIMDJSON_SWAR_NUMBER_PARSING 1

#endif // SIMDJSON_RVV_NUMBERPARSING_DEFS_H
