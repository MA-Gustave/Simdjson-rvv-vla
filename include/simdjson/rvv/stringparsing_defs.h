#ifndef SIMDJSON_RVV_STRINGPARSING_DEFS_H
#define SIMDJSON_RVV_STRINGPARSING_DEFS_H

#ifndef SIMDJSON_CONDITIONAL_INCLUDE
#include "simdjson/rvv/base.h"
#include "simdjson/rvv/intrinsics.h"
#endif // SIMDJSON_CONDITIONAL_INCLUDE

namespace simdjson {
namespace rvv {
namespace {

// Generic Stage 2 consumes a semantic 64-byte window. e8m4 guarantees at
// least 64 byte lanes for the RVV 1.0 baseline VLEN >= 128, so copy and event
// detection are one vector load/store instead of a loop over smaller vectors.
struct backslash_and_quote {
public:
  static constexpr uint32_t BYTES_PROCESSED = 64;

  simdjson_inline backslash_and_quote() noexcept
      : backslash(BYTES_PROCESSED), quote(BYTES_PROCESSED) {}
  simdjson_inline backslash_and_quote(uint32_t bs, uint32_t q) noexcept
      : backslash(bs), quote(q) {}

  simdjson_inline backslash_and_quote copy_and_find(
      const uint8_t *src, uint8_t *dst) const noexcept {
    static_assert(SIMDJSON_PADDING >= (BYTES_PROCESSED - 1),
                  "backslash and quote finder must process fewer than SIMDJSON_PADDING bytes");
    constexpr size_t vl = BYTES_PROCESSED;
    const vuint8m4_t v = __riscv_vle8_v_u8m4(src, vl);
    __riscv_vse8_v_u8m4(dst, v, vl);

    const long bs = __riscv_vfirst_m_b2(
        __riscv_vmseq_vx_u8m4_b2(v, '\\', vl), vl);
    const long q = __riscv_vfirst_m_b2(
        __riscv_vmseq_vx_u8m4_b2(v, '"', vl), vl);
    return {
      bs < 0 ? BYTES_PROCESSED : uint32_t(bs),
      q < 0 ? BYTES_PROCESSED : uint32_t(q)
    };
  }

  simdjson_inline bool has_quote_first() const noexcept { return quote < backslash; }
  simdjson_inline bool has_backslash() const noexcept { return backslash < quote; }
  simdjson_inline int quote_index() const noexcept { return int(quote); }
  simdjson_inline int backslash_index() const noexcept { return int(backslash); }

  uint32_t backslash;
  uint32_t quote;
};

struct escaping {
  static constexpr uint32_t BYTES_PROCESSED = 64;

  simdjson_inline escaping() noexcept : index(BYTES_PROCESSED) {}
  simdjson_inline escaping(uint32_t i) noexcept : index(i) {}

  simdjson_inline static escaping copy_and_find(
      const uint8_t *src, uint8_t *dst) noexcept {
    static_assert(SIMDJSON_PADDING >= (BYTES_PROCESSED - 1),
                  "escaping finder must process fewer than SIMDJSON_PADDING bytes");
    constexpr size_t vl = BYTES_PROCESSED;
    const vuint8m4_t v = __riscv_vle8_v_u8m4(src, vl);
    __riscv_vse8_v_u8m4(dst, v, vl);

    vbool2_t m = __riscv_vmseq_vx_u8m4_b2(v, '"', vl);
    m = __riscv_vmor_mm_b2(m, __riscv_vmseq_vx_u8m4_b2(v, '\\', vl), vl);
    m = __riscv_vmor_mm_b2(m, __riscv_vmsltu_vx_u8m4_b2(v, 0x20, vl), vl);
    const long i = __riscv_vfirst_m_b2(m, vl);
    return { i < 0 ? BYTES_PROCESSED : uint32_t(i) };
  }

  simdjson_inline bool has_escape() const noexcept { return index < BYTES_PROCESSED; }
  simdjson_inline int escape_index() const noexcept { return int(index); }

  uint32_t index;
};

} // unnamed namespace
} // namespace rvv
} // namespace simdjson

#endif // SIMDJSON_RVV_STRINGPARSING_DEFS_H
