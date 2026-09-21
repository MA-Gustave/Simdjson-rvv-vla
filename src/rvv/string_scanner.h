#ifndef SIMDJSON_RVV_STRING_SCANNER_H
#define SIMDJSON_RVV_STRING_SCANNER_H

#ifndef SIMDJSON_CONDITIONAL_INCLUDE
#include "simdjson/rvv/base.h"
#include "simdjson/rvv/intrinsics.h"
#endif // SIMDJSON_CONDITIONAL_INCLUDE

namespace simdjson {
namespace rvv {
namespace stage1 {

struct string_state {
  bool in_string{false};
  bool next_is_escaped{false};
};

// Stage 1 version: e8m2 keeps the lane count compatible with e32m8, which
// lets the structural writer compact absolute uint32_t indexes directly.
simdjson_really_inline vbool4_t escaped_characters(
    vuint8m2_t bytes, size_t vl, string_state &state) noexcept {
  const bool incoming_escape = state.next_is_escaped;
  const vbool4_t backslash = __riscv_vmseq_vx_u8m2_b4(bytes, '\\', vl);
  if (simdjson_likely(!detail::any(backslash, vl))) {
    state.next_is_escaped = false;
    if (simdjson_likely(!incoming_escape)) {
      return __riscv_vmclr_m_b4(vl);
    }
    // Rare boundary-only escape: pay for lane indexes only here.
    const vuint16m4_t lanes = __riscv_vid_v_u16m4(vl);
    return __riscv_vmseq_vx_u16m4_b4(lanes, 0, vl);
  }

  const vuint16m4_t lanes = __riscv_vid_v_u16m4(vl);
  const vuint16m4_t zero = __riscv_vmv_v_x_u16m4(0, vl);
  const vuint16m4_t bs_flags = __riscv_vmerge_vxm_u16m4(zero, 1, backslash, vl);
  const vuint16m4_t prev_bs_flags = __riscv_vslide1up_vx_u16m4(bs_flags, 0, vl);
  const vbool4_t prev_backslash = __riscv_vmsne_vx_u16m4_b4(prev_bs_flags, 0, vl);
  const vbool4_t run_start = __riscv_vmand_mm_b4(
      backslash, __riscv_vmnot_m_b4(prev_backslash, vl), vl);

  // Map each backslash lane to the start of its run. viota is exclusive;
  // subtracting continuation flags converts it to the packed-start index.
  const vuint16m4_t packed_starts =
      __riscv_vcompress_vm_u16m4(lanes, run_start, vl);
  vuint16m4_t run_id = __riscv_viota_m_u16m4(run_start, vl);
  const vbool4_t continuation_backslash = __riscv_vmand_mm_b4(
      backslash, __riscv_vmnot_m_b4(run_start, vl), vl);
  const vuint16m4_t continuation_flags = __riscv_vmerge_vxm_u16m4(
      zero, 1, continuation_backslash, vl);
  run_id = __riscv_vsub_vv_u16m4(run_id, continuation_flags, vl);

  const vuint16m4_t start_for_lane =
      __riscv_vrgather_vv_u16m4(packed_starts, run_id, vl);
  const vuint16m4_t offset = __riscv_vsub_vv_u16m4(lanes, start_for_lane, vl);
  const vuint16m4_t offset_bit = __riscv_vand_vx_u16m4(offset, 1, vl);
  vbool4_t active_backslash = __riscv_vmand_mm_b4(
      backslash, __riscv_vmseq_vx_u16m4_b4(offset_bit, 0, vl), vl);

  // A run continuing from the previous vector has the opposite local parity.
  if (incoming_escape) {
    const vbool4_t first_run = __riscv_vmand_mm_b4(
        backslash, __riscv_vmseq_vx_u16m4_b4(run_id, 0, vl), vl);
    // Only lane-0 backslash can belong to a continuing run.
    const vbool4_t lane0 = __riscv_vmseq_vx_u16m4_b4(lanes, 0, vl);
    if (detail::any(__riscv_vmand_mm_b4(first_run, lane0, vl), vl)) {
      active_backslash = __riscv_vmxor_mm_b4(active_backslash, first_run, vl);
    }
  }

  const vuint16m4_t active_flags =
      __riscv_vmerge_vxm_u16m4(zero, 1, active_backslash, vl);
  const vuint16m4_t escaped_flags = __riscv_vslide1up_vx_u16m4(
      active_flags, incoming_escape ? 1 : 0, vl);
  const vbool4_t escaped = __riscv_vmsne_vx_u16m4_b4(escaped_flags, 0, vl);

  state.next_is_escaped = detail::mask_last(active_backslash, vl);
  return escaped;
}

simdjson_really_inline vbool4_t inside_string(
    vbool4_t unescaped_quote, size_t vl, string_state &state) noexcept {
  if (simdjson_likely(!detail::any(unescaped_quote, vl))) {
    return state.in_string ? __riscv_vmset_m_b4(vl) : __riscv_vmclr_m_b4(vl);
  }

  const vuint16m4_t zero = __riscv_vmv_v_x_u16m4(0, vl);
  const vuint16m4_t quote_flags =
      __riscv_vmerge_vxm_u16m4(zero, 1, unescaped_quote, vl);
  const vuint16m4_t quote_before = __riscv_viota_m_u16m4(unescaped_quote, vl);
  vuint16m4_t parity = __riscv_vand_vx_u16m4(
      __riscv_vadd_vv_u16m4(quote_before, quote_flags, vl), 1, vl);
  if (state.in_string) {
    parity = __riscv_vxor_vx_u16m4(parity, 1, vl);
  }
  const vbool4_t inside = __riscv_vmsne_vx_u16m4_b4(parity, 0, vl);
  state.in_string ^= ((__riscv_vcpop_m_b4(unescaped_quote, vl) & 1u) != 0);
  return inside;
}

// Minify version: e8m4 doubles the byte-lane count while u16m8 still gives
// enough lanes for run indexing. This is independent of Stage 1's index writer.
simdjson_really_inline vbool2_t escaped_characters_m4(
    vuint8m4_t bytes, size_t vl, string_state &state) noexcept {
  const bool incoming_escape = state.next_is_escaped;
  const vbool2_t backslash = __riscv_vmseq_vx_u8m4_b2(bytes, '\\', vl);
  if (simdjson_likely(!detail::any(backslash, vl))) {
    state.next_is_escaped = false;
    if (simdjson_likely(!incoming_escape)) {
      return __riscv_vmclr_m_b2(vl);
    }
    const vuint16m8_t lanes = __riscv_vid_v_u16m8(vl);
    return __riscv_vmseq_vx_u16m8_b2(lanes, 0, vl);
  }

  const vuint16m8_t lanes = __riscv_vid_v_u16m8(vl);
  const vuint16m8_t zero = __riscv_vmv_v_x_u16m8(0, vl);
  const vuint16m8_t bs_flags = __riscv_vmerge_vxm_u16m8(zero, 1, backslash, vl);
  const vuint16m8_t prev_bs_flags = __riscv_vslide1up_vx_u16m8(bs_flags, 0, vl);
  const vbool2_t prev_backslash = __riscv_vmsne_vx_u16m8_b2(prev_bs_flags, 0, vl);
  const vbool2_t run_start = __riscv_vmand_mm_b2(
      backslash, __riscv_vmnot_m_b2(prev_backslash, vl), vl);

  const vuint16m8_t packed_starts =
      __riscv_vcompress_vm_u16m8(lanes, run_start, vl);
  vuint16m8_t run_id = __riscv_viota_m_u16m8(run_start, vl);
  const vbool2_t continuation_backslash = __riscv_vmand_mm_b2(
      backslash, __riscv_vmnot_m_b2(run_start, vl), vl);
  const vuint16m8_t continuation_flags = __riscv_vmerge_vxm_u16m8(
      zero, 1, continuation_backslash, vl);
  run_id = __riscv_vsub_vv_u16m8(run_id, continuation_flags, vl);

  const vuint16m8_t start_for_lane =
      __riscv_vrgather_vv_u16m8(packed_starts, run_id, vl);
  const vuint16m8_t offset = __riscv_vsub_vv_u16m8(lanes, start_for_lane, vl);
  const vuint16m8_t offset_bit = __riscv_vand_vx_u16m8(offset, 1, vl);
  vbool2_t active_backslash = __riscv_vmand_mm_b2(
      backslash, __riscv_vmseq_vx_u16m8_b2(offset_bit, 0, vl), vl);

  if (incoming_escape) {
    const vbool2_t first_run = __riscv_vmand_mm_b2(
        backslash, __riscv_vmseq_vx_u16m8_b2(run_id, 0, vl), vl);
    const vbool2_t lane0 = __riscv_vmseq_vx_u16m8_b2(lanes, 0, vl);
    if (detail::any(__riscv_vmand_mm_b2(first_run, lane0, vl), vl)) {
      active_backslash = __riscv_vmxor_mm_b2(active_backslash, first_run, vl);
    }
  }

  const vuint16m8_t active_flags =
      __riscv_vmerge_vxm_u16m8(zero, 1, active_backslash, vl);
  const vuint16m8_t escaped_flags = __riscv_vslide1up_vx_u16m8(
      active_flags, incoming_escape ? 1 : 0, vl);
  const vbool2_t escaped = __riscv_vmsne_vx_u16m8_b2(escaped_flags, 0, vl);

  state.next_is_escaped = detail::mask_last(active_backslash, vl);
  return escaped;
}

simdjson_really_inline vbool2_t inside_string_m4(
    vbool2_t unescaped_quote, size_t vl, string_state &state) noexcept {
  if (simdjson_likely(!detail::any(unescaped_quote, vl))) {
    return state.in_string ? __riscv_vmset_m_b2(vl) : __riscv_vmclr_m_b2(vl);
  }

  const vuint16m8_t zero = __riscv_vmv_v_x_u16m8(0, vl);
  const vuint16m8_t quote_flags =
      __riscv_vmerge_vxm_u16m8(zero, 1, unescaped_quote, vl);
  const vuint16m8_t quote_before = __riscv_viota_m_u16m8(unescaped_quote, vl);
  vuint16m8_t parity = __riscv_vand_vx_u16m8(
      __riscv_vadd_vv_u16m8(quote_before, quote_flags, vl), 1, vl);
  if (state.in_string) {
    parity = __riscv_vxor_vx_u16m8(parity, 1, vl);
  }
  const vbool2_t inside = __riscv_vmsne_vx_u16m8_b2(parity, 0, vl);
  state.in_string ^= ((__riscv_vcpop_m_b2(unescaped_quote, vl) & 1u) != 0);
  return inside;
}

} // namespace stage1
} // namespace rvv
} // namespace simdjson

#endif // SIMDJSON_RVV_STRING_SCANNER_H
