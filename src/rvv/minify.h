#ifndef SIMDJSON_RVV_MINIFY_H
#define SIMDJSON_RVV_MINIFY_H

#ifndef SIMDJSON_CONDITIONAL_INCLUDE
#include "simdjson/rvv/base.h"
#include "simdjson/rvv/intrinsics.h"
#include <rvv/string_scanner.h>
#endif // SIMDJSON_CONDITIONAL_INCLUDE

namespace simdjson {
namespace rvv {
namespace stage1 {

simdjson_really_inline vbool2_t whitespace_mask_m4(
    vuint8m4_t bytes, size_t vl) noexcept {
  vbool2_t whitespace = __riscv_vmseq_vx_u8m4_b2(bytes, ' ', vl);
  whitespace = __riscv_vmor_mm_b2(
      whitespace, __riscv_vmseq_vx_u8m4_b2(bytes, '\t', vl), vl);
  whitespace = __riscv_vmor_mm_b2(
      whitespace, __riscv_vmseq_vx_u8m4_b2(bytes, '\n', vl), vl);
  return __riscv_vmor_mm_b2(
      whitespace, __riscv_vmseq_vx_u8m4_b2(bytes, '\r', vl), vl);
}

simdjson_inline error_code minify(
    const uint8_t *buf, size_t len, uint8_t *dst, size_t &dst_len) noexcept {
  string_state strings{};
  const uint8_t *src = buf;
  uint8_t *out = dst;
  size_t remaining = len;

  while (remaining > 0) {
    // m4 is the widest useful byte LMUL here: its lane count can still be
    // paired with u16m8 for escape-run indexing, and VLEN=128 already covers
    // a full 64-byte semantic block per iteration.
    const size_t vl = __riscv_vsetvl_e8m4(remaining);
    const vuint8m4_t bytes = __riscv_vle8_v_u8m4(src, vl);

    const vbool2_t escaped = escaped_characters_m4(bytes, vl, strings);
    const vbool2_t raw_quote = __riscv_vmseq_vx_u8m4_b2(bytes, '"', vl);
    const vbool2_t quote = __riscv_vmand_mm_b2(
        raw_quote, __riscv_vmnot_m_b2(escaped, vl), vl);
    const vbool2_t inside = inside_string_m4(quote, vl, strings);

    const vbool2_t whitespace = whitespace_mask_m4(bytes, vl);
    const vbool2_t remove = __riscv_vmand_mm_b2(
        whitespace, __riscv_vmnot_m_b2(inside, vl), vl);

    // If there is nothing to remove, bypass vcompress entirely.
    if (simdjson_likely(!detail::any(remove, vl))) {
      __riscv_vse8_v_u8m4(out, bytes, vl);
      out += vl;
    } else {
      const vbool2_t keep = __riscv_vmnot_m_b2(remove, vl);
      const size_t count = __riscv_vcpop_m_b2(keep, vl);
      if (count != 0) {
        const vuint8m4_t packed = __riscv_vcompress_vm_u8m4(bytes, keep, vl);
        __riscv_vse8_v_u8m4(out, packed, count);
        out += count;
      }
    }

    src += vl;
    remaining -= vl;
  }

  if (simdjson_unlikely(strings.in_string)) {
    dst_len = 0;
    return UNCLOSED_STRING;
  }
  dst_len = size_t(out - dst);
  return SUCCESS;
}

} // namespace stage1
} // namespace rvv
} // namespace simdjson

#endif // SIMDJSON_RVV_MINIFY_H
