#ifndef SIMDJSON_RVV_UTF8_VALIDATION_H
#define SIMDJSON_RVV_UTF8_VALIDATION_H

#ifndef SIMDJSON_CONDITIONAL_INCLUDE
#include "simdjson/rvv/base.h"
#include "simdjson/rvv/intrinsics.h"
#endif // SIMDJSON_CONDITIONAL_INCLUDE

namespace simdjson {
namespace rvv {
namespace stage1 {

// Scalable RVV adaptation of simdjson's lookup4 UTF-8 algorithm. The hot path
// is fully vectorized; only the last three bytes are retained as scalar carry
// between arbitrary VLA chunks.
struct utf8_state {
  uint8_t last1{0};
  uint8_t last2{0};
  uint8_t last3{0};
  uint8_t tail_len{0};
  bool error{false};

  simdjson_really_inline bool previous_tail_may_be_incomplete() const noexcept {
    return (tail_len >= 1 && last1 >= 0xc0) ||
           (tail_len >= 2 && last2 >= 0xe0) ||
           (tail_len >= 3 && last3 >= 0xf0);
  }

  simdjson_really_inline void update_tail(const uint8_t *src, size_t vl) noexcept {
    if (vl >= 3) {
      last3 = src[vl - 3];
      last2 = src[vl - 2];
      last1 = src[vl - 1];
      tail_len = 3;
      return;
    }
    if (vl == 2) {
      last3 = (tail_len >= 1) ? last1 : 0;
      last2 = src[0];
      last1 = src[1];
      tail_len = uint8_t(tail_len >= 1 ? 3 : 2);
      return;
    }
    if (vl == 1) {
      last3 = (tail_len >= 2) ? last2 : 0;
      last2 = (tail_len >= 1) ? last1 : 0;
      last1 = src[0];
      tail_len = uint8_t(tail_len < 3 ? tail_len + 1 : 3);
    }
  }

  simdjson_really_inline static vuint8m2_t check_special_cases(
      vuint8m2_t input, vuint8m2_t prev1, size_t vl) noexcept {
    static const uint8_t byte1_high_table[16] = {
      2, 2, 2, 2, 2, 2, 2, 2,
      128, 128, 128, 128, 33, 1, 21, 73
    };
    static const uint8_t byte1_low_table[16] = {
      231, 163, 131, 131, 139, 203, 203, 203,
      203, 203, 203, 203, 203, 219, 203, 203
    };
    static const uint8_t byte2_high_table[16] = {
      1, 1, 1, 1, 1, 1, 1, 1,
      230, 174, 186, 186, 1, 1, 1, 1
    };

    const vuint8m1_t t1 = __riscv_vle8_v_u8m1(byte1_high_table, 16);
    const vuint8m1_t t2 = __riscv_vle8_v_u8m1(byte1_low_table, 16);
    const vuint8m1_t t3 = __riscv_vle8_v_u8m1(byte2_high_table, 16);

    const vuint8m2_t prev1_hi = __riscv_vsrl_vx_u8m2(prev1, 4, vl);
    const vuint8m2_t prev1_lo = __riscv_vand_vx_u8m2(prev1, 0x0f, vl);
    const vuint8m2_t input_hi = __riscv_vsrl_vx_u8m2(input, 4, vl);

    const vuint8m2_t a = detail::lookup16_u8m2(t1, prev1_hi);
    const vuint8m2_t b = detail::lookup16_u8m2(t2, prev1_lo);
    const vuint8m2_t c = detail::lookup16_u8m2(t3, input_hi);
    return __riscv_vand_vv_u8m2(
        __riscv_vand_vv_u8m2(a, b, vl), c, vl);
  }

  simdjson_really_inline void consume_chunk(
      const uint8_t *src, vuint8m2_t input, size_t vl) noexcept {
    if (simdjson_unlikely(error)) { return; }

    const vint8m2_t signed_input = __riscv_vreinterpret_v_u8m2_i8m2(input);
    const bool has_non_ascii = detail::any(
        __riscv_vmslt_vx_i8m2_b4(signed_input, 0, vl), vl);

    // ASCII is the overwhelmingly common path. It is safe to skip lookup4 only
    // when the preceding chunk did not end with a potentially incomplete lead.
    if (simdjson_likely(!has_non_ascii && !previous_tail_may_be_incomplete())) {
      update_tail(src, vl);
      return;
    }

    const vuint8m2_t prev1 = __riscv_vslide1up_vx_u8m2(input, last1, vl);
    const vuint8m2_t prev2 = __riscv_vslide1up_vx_u8m2(prev1, last2, vl);
    const vuint8m2_t prev3 = __riscv_vslide1up_vx_u8m2(prev2, last3, vl);

    const vuint8m2_t special = check_special_cases(input, prev1, vl);
    const vuint8m2_t third_byte = __riscv_vssubu_vx_u8m2(prev2, 0x60, vl);
    const vuint8m2_t fourth_byte = __riscv_vssubu_vx_u8m2(prev3, 0x70, vl);
    const vuint8m2_t must23 = __riscv_vor_vv_u8m2(third_byte, fourth_byte, vl);
    const vuint8m2_t must23_80 = __riscv_vand_vx_u8m2(must23, 0x80, vl);
    const vuint8m2_t bad = __riscv_vxor_vv_u8m2(must23_80, special, vl);

    error = detail::any(__riscv_vmsne_vx_u8m2_b4(bad, 0, vl), vl);
    update_tail(src, vl);
  }

  simdjson_really_inline bool finish() const noexcept {
    return !error && !previous_tail_may_be_incomplete();
  }
};

simdjson_inline bool validate_utf8(const char *buf, size_t len) noexcept {
  utf8_state state{};
  const uint8_t *src = reinterpret_cast<const uint8_t *>(buf);
  size_t remaining = len;
  while (remaining > 0 && simdjson_likely(!state.error)) {
    const size_t vl = __riscv_vsetvl_e8m2(remaining);
    const vuint8m2_t input = __riscv_vle8_v_u8m2(src, vl);
    state.consume_chunk(src, input, vl);
    src += vl;
    remaining -= vl;
  }
  return state.finish();
}

} // namespace stage1
} // namespace rvv
} // namespace simdjson

#endif // SIMDJSON_RVV_UTF8_VALIDATION_H
