#ifndef SIMDJSON_RVV_INTRINSICS_H
#define SIMDJSON_RVV_INTRINSICS_H

#ifndef SIMDJSON_CONDITIONAL_INCLUDE
#include "simdjson/rvv/base.h"
#endif // SIMDJSON_CONDITIONAL_INCLUDE

#include <riscv_vector.h>

namespace simdjson {
namespace rvv {
namespace detail {

// RVV has no cross-register 16-entry byte gather for LMUL > 1. Split the
// index group into m1 pieces, gather from the same 16-byte table, and rebuild
// the original LMUL group. All indexes passed to these helpers are <= 15.
simdjson_really_inline vuint8m2_t lookup16_u8m2(
    vuint8m1_t table, vuint8m2_t index) noexcept {
  const size_t vl1 = __riscv_vsetvlmax_e8m1();
  return __riscv_vcreate_v_u8m1_u8m2(
      __riscv_vrgather_vv_u8m1(
          table, __riscv_vget_v_u8m2_u8m1(index, 0), vl1),
      __riscv_vrgather_vv_u8m1(
          table, __riscv_vget_v_u8m2_u8m1(index, 1), vl1));
}

simdjson_really_inline vuint8m4_t lookup16_u8m4(
    vuint8m1_t table, vuint8m4_t index) noexcept {
  const size_t vl1 = __riscv_vsetvlmax_e8m1();
  return __riscv_vcreate_v_u8m1_u8m4(
      __riscv_vrgather_vv_u8m1(
          table, __riscv_vget_v_u8m4_u8m1(index, 0), vl1),
      __riscv_vrgather_vv_u8m1(
          table, __riscv_vget_v_u8m4_u8m1(index, 1), vl1),
      __riscv_vrgather_vv_u8m1(
          table, __riscv_vget_v_u8m4_u8m1(index, 2), vl1),
      __riscv_vrgather_vv_u8m1(
          table, __riscv_vget_v_u8m4_u8m1(index, 3), vl1));
}

simdjson_really_inline bool any(vbool4_t mask, size_t vl) noexcept {
  return __riscv_vfirst_m_b4(mask, vl) >= 0;
}

simdjson_really_inline bool any(vbool2_t mask, size_t vl) noexcept {
  return __riscv_vfirst_m_b2(mask, vl) >= 0;
}

simdjson_really_inline bool mask_last(vbool4_t mask, size_t vl) noexcept {
  const vuint8m2_t zero = __riscv_vmv_v_x_u8m2(0, vl);
  const vuint8m2_t flags = __riscv_vmerge_vxm_u8m2(zero, 1, mask, vl);
  const vuint8m2_t last = __riscv_vslidedown_vx_u8m2(flags, vl - 1, 1);
  return __riscv_vmv_x_s_u8m2_u8(last) != 0;
}

simdjson_really_inline bool mask_last(vbool2_t mask, size_t vl) noexcept {
  const vuint8m4_t zero = __riscv_vmv_v_x_u8m4(0, vl);
  const vuint8m4_t flags = __riscv_vmerge_vxm_u8m4(zero, 1, mask, vl);
  const vuint8m4_t last = __riscv_vslidedown_vx_u8m4(flags, vl - 1, 1);
  return __riscv_vmv_x_s_u8m4_u8(last) != 0;
}

} // namespace detail
} // namespace rvv
} // namespace simdjson

#endif // SIMDJSON_RVV_INTRINSICS_H
