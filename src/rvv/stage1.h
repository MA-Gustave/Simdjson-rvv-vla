#ifndef SIMDJSON_RVV_STAGE1_H
#define SIMDJSON_RVV_STAGE1_H

#ifndef SIMDJSON_CONDITIONAL_INCLUDE
#include "simdjson/rvv/base.h"
#include "simdjson/rvv/intrinsics.h"
#include <rvv/string_scanner.h>
#include <rvv/utf8_validation.h>
#endif // SIMDJSON_CONDITIONAL_INCLUDE

namespace simdjson {
namespace rvv {
namespace stage1 {

#ifndef SIMDJSON_RVV_SPARSE_INDEX_THRESHOLD
#define SIMDJSON_RVV_SPARSE_INDEX_THRESHOLD 8
#endif
#ifndef SIMDJSON_RVV_ENABLE_PACKED_INDEX_WRITER
#define SIMDJSON_RVV_ENABLE_PACKED_INDEX_WRITER SIMDJSON_RVV_ENABLE_PACKED_CONTROL
#endif
#ifndef SIMDJSON_RVV_PACKED_INDEX_THRESHOLD
#define SIMDJSON_RVV_PACKED_INDEX_THRESHOLD 64
#endif

// Count trailing zeroes without making Zbb a correctness requirement. When
// Zbb is enabled the compiler maps the builtin to ctz; otherwise the small
// binary-search sequence stays entirely in scalar integer code. `value` must
// be non-zero.
simdjson_really_inline uint32_t trailing_zeroes64(uint64_t value) noexcept {
#if defined(__riscv_zbb)
  return uint32_t(__builtin_ctzll(value));
#else
  uint32_t count = 0;
  if ((value & uint64_t(0xffffffffu)) == 0) { count += 32; value >>= 32; }
  if ((value & uint64_t(0xffffu)) == 0) { count += 16; value >>= 16; }
  if ((value & uint64_t(0xffu)) == 0) { count += 8; value >>= 8; }
  if ((value & uint64_t(0xfu)) == 0) { count += 4; value >>= 4; }
  if ((value & uint64_t(0x3u)) == 0) { count += 2; value >>= 2; }
  if ((value & uint64_t(0x1u)) == 0) { count += 1; }
  return count;
#endif
}

simdjson_inline size_t trim_partial_utf8(const uint8_t *buf, size_t len) noexcept {
  if (simdjson_unlikely(len < 3)) {
    switch (len) {
      case 2:
        if (buf[len-1] >= 0xc0) { return len-1; }
        if (buf[len-2] >= 0xe0) { return len-2; }
        return len;
      case 1:
        if (buf[len-1] >= 0xc0) { return len-1; }
        return len;
      default:
        return len;
    }
  }
  if (buf[len-1] >= 0xc0) { return len-1; }
  if (buf[len-2] >= 0xe0) { return len-2; }
  if (buf[len-3] >= 0xf0) { return len-3; }
  return len;
}

simdjson_really_inline vbool4_t classify_whitespace(
    vuint8m2_t bytes, vuint8m2_t low, size_t vl) noexcept {
  // Same collision-free low-nibble table used by the upstream RVV-VLS
  // backend, applied directly to scalable vectors. RVV types are sizeless,
  // so masks must remain locals/return values and cannot be struct members.
  static const uint8_t ws_table[16] = {
    ' ', 100, 100, 100, 17, 100, 113, 2,
    100, '\t', '\n', 112, 100, '\r', 100, 100
  };
  const vuint8m1_t lookup = __riscv_vle8_v_u8m1(ws_table, 16);
  const vuint8m2_t candidate = detail::lookup16_u8m2(lookup, low);
  return __riscv_vmseq_vv_u8m2_b4(candidate, bytes, vl);
}

simdjson_really_inline vbool4_t classify_operator(
    vuint8m2_t bytes, vuint8m2_t low, size_t vl) noexcept {
  static const uint8_t op_table[16] = {
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, ':', '{', ',', '}', 0, 0
  };
  const vuint8m1_t lookup = __riscv_vle8_v_u8m1(op_table, 16);
  const vuint8m2_t candidate = detail::lookup16_u8m2(lookup, low);
  const vuint8m2_t curl = __riscv_vor_vx_u8m2(bytes, 0x20, vl);
  return __riscv_vmseq_vv_u8m2_b4(candidate, curl, vl);
}

// Streaming helpers are kept in the RVV Stage 1 namespace. The generic
// versions live under generic's anonymous namespace; using local names avoids
// namespace lookup collisions between the two distinct stage1 namespaces.
simdjson_inline bool rvv_ends_with_partial_scalar(
    dom_parser_implementation &parser, size_t len) noexcept {
  const uint8_t f =
      parser.buf[parser.structural_indexes[parser.n_structural_indexes - 1]];
  const uint8_t e = parser.buf[len - 1];
  return f != '{' && f != '[' && f != '}' && f != ']' && f != ':' &&
         f != ',' && f != '"' && e != ' ' && e != '\t' && e != '\n' &&
         e != '\r';
}

simdjson_inline uint32_t rvv_find_next_document_index(
    dom_parser_implementation &parser, bool defer_last = false) noexcept {
  if (parser.n_structural_indexes == 0) { return 0; }
  auto arr_cnt = 0;
  auto obj_cnt = 0;
  for (auto i = parser.n_structural_indexes - 1; i > 0; i--) {
    const auto idxb = parser.structural_indexes[i];
    switch (parser.buf[idxb]) {
      case ':':
      case ',': continue;
      case '}': obj_cnt--; continue;
      case ']': arr_cnt--; continue;
      case '{': obj_cnt++; break;
      case '[': arr_cnt++; break;
    }
    const auto idxa = parser.structural_indexes[i - 1];
    switch (parser.buf[idxa]) {
      case '{':
      case '[':
      case ':':
      case ',': continue;
    }
    if (!arr_cnt && !obj_cnt) {
      return defer_last ? i : parser.n_structural_indexes;
    }
    return i;
  }
  switch (parser.buf[parser.structural_indexes[0]]) {
    case '}': obj_cnt--; break;
    case ']': arr_cnt--; break;
    case '{': obj_cnt++; break;
    case '[': arr_cnt++; break;
  }
  if (!arr_cnt && !obj_cnt) {
    return defer_last ? 0 : parser.n_structural_indexes;
  }
  return 0;
}

class json_structural_indexer {
public:
  static simdjson_warn_unused error_code index(
      const uint8_t *buf, size_t len, dom_parser_implementation &parser,
      stage1_mode partial) noexcept {
    if (simdjson_unlikely(len > parser.capacity())) { return CAPACITY; }
    if (len == 0) { return EMPTY; }
    if (is_streaming(partial)) {
      len = trim_partial_utf8(buf, len);
      if (len == 0) { return UTF8_ERROR; }
    }

    json_structural_indexer indexer(parser.structural_indexes.get());
    size_t offset = 0;
    while (offset < len) {
      // e8m2 is intentional: e32m8 has exactly the same lane count, so
      // structural offsets can be compacted as uint32_t with no widening step.
      const size_t vl = __riscv_vsetvl_e8m2(len - offset);
      const vuint8m2_t bytes = __riscv_vle8_v_u8m2(buf + offset, vl);
      indexer.next(buf + offset, bytes, vl, uint32_t(offset));
      offset += vl;
    }
    return indexer.finish(parser, len, partial);
  }

private:
  explicit simdjson_inline json_structural_indexer(uint32_t *tail) noexcept
      : tail(tail) {}

  simdjson_really_inline void write_indexes(
      vbool4_t structural, size_t vl, uint32_t base) noexcept {
    const size_t count = __riscv_vcpop_m_b4(structural, vl);
    if (simdjson_likely(count == 0)) { return; }

    // W2 sparse/event writer. On current RVV implementations, especially the
    // SpacemiT X60, high-LMUL vcompress can be far more expensive than mask
    // operations plus a handful of scalar event extractions. Keep this path
    // bounded by the number of actual structurals: it never scans bytes.
    //
    // The threshold is intentionally conservative until native benchmarks can
    // tune the W2/W1 crossover. vmsof selects only the first active mask bit;
    // XOR removes that bit while preserving all remaining event positions.
    static constexpr size_t SPARSE_INDEX_THRESHOLD =
        SIMDJSON_RVV_SPARSE_INDEX_THRESHOLD;
    if (simdjson_likely(count <= SPARSE_INDEX_THRESHOLD)) {
      vbool4_t pending = structural;
      for (size_t i = 0; i < count; ++i) {
        const long index = __riscv_vfirst_m_b4(pending, vl);
        *tail++ = base + uint32_t(index);
        const vbool4_t first = __riscv_vmsof_m_b4(pending, vl);
        pending = __riscv_vmxor_mm_b4(pending, first, vl);
      }
      return;
    }

    // W1 packed-mask writer. Export the predicate once with vsm.v, enumerate
    // set bits with scalar integer operations, and write absolute uint32_t
    // indexes directly. This removes vid/vcompress from the medium-density
    // regime while keeping a dense vector fallback below. At VLEN=256, the
    // Stage 1 e8m2 chunk is exactly 64 bytes, so the complete structural mask
    // fits in one uint64_t word.
    static constexpr size_t PACKED_INDEX_THRESHOLD =
        SIMDJSON_RVV_PACKED_INDEX_THRESHOLD;
    if (SIMDJSON_RVV_ENABLE_PACKED_INDEX_WRITER &&
        simdjson_likely(count <= PACKED_INDEX_THRESHOLD)) {
      packed_mask_words bits{};
      if (simdjson_likely(pack_mask(structural, vl, bits))) {
        const size_t words = packed_word_count(vl);
        for (size_t word_index = 0; word_index < words; ++word_index) {
          uint64_t word = bits.words[word_index];
          const uint32_t word_base =
              base + uint32_t(word_index * size_t(64));
          while (word != 0) {
            const uint32_t bit = trailing_zeroes64(word);
            *tail++ = word_base + bit;
            word &= word - 1;
          }
        }
        return;
      }
    }

    // W0 dense vector writer. e32m8 and e8m2 have identical VLMAX, so
    // output-width offsets can be compacted directly without widening. Keep
    // this path for dense structural blocks where one vector compaction can
    // amortize its relatively high setup/permutation cost.
    const vuint32m8_t lanes = __riscv_vid_v_u32m8(vl);
    const vuint32m8_t packed = __riscv_vcompress_vm_u32m8(lanes, structural, vl);
    const vuint32m8_t absolute = __riscv_vadd_vx_u32m8(packed, base, count);
    __riscv_vse32_v_u32m8(tail, absolute, count);
    tail += count;
  }

  simdjson_really_inline void next(
      const uint8_t *src, vuint8m2_t bytes, size_t vl, uint32_t base) noexcept {
    // UTF-8 is validated from the already-loaded byte vector. ASCII chunks pay
    // only a sign-bit mask check; multibyte input stays on RVV lookup4.
    utf8.consume_chunk(src, bytes, vl);

    const vbool4_t escaped = escaped_characters(bytes, vl, strings);
    const vbool4_t raw_quote = __riscv_vmseq_vx_u8m2_b4(bytes, '"', vl);
    const vbool4_t quote = __riscv_vmand_mm_b4(
        raw_quote, __riscv_vmnot_m_b4(escaped, vl), vl);
    const vbool4_t inside = inside_string(quote, vl, strings);
    const vbool4_t string_tail = __riscv_vmxor_mm_b4(inside, quote, vl);

    const vuint8m2_t low = __riscv_vand_vx_u8m2(bytes, 0x0f, vl);
    const vbool4_t whitespace = classify_whitespace(bytes, low, vl);
    const vbool4_t op = classify_operator(bytes, low, vl);
    const vbool4_t op_or_ws = __riscv_vmor_mm_b4(op, whitespace, vl);
    const vbool4_t scalar = __riscv_vmnot_m_b4(op_or_ws, vl);
    const vbool4_t nonquote_scalar = __riscv_vmand_mm_b4(
        scalar, __riscv_vmnot_m_b4(quote, vl), vl);

    vbool4_t follows_nonquote_scalar;
    bool last_nonquote_scalar = false;
    if (simdjson_likely(shift_mask_left_one_packed(
            nonquote_scalar, vl, prev_nonquote_scalar,
            follows_nonquote_scalar, last_nonquote_scalar))) {
      // Packed predicate shift: no element widening/slide is required.
    } else {
      // Fully scalable fallback for vector lengths above the packed-control
      // optimization window or for non-little-endian targets.
      const vuint8m2_t zero = __riscv_vmv_v_x_u8m2(0, vl);
      const vuint8m2_t scalar_flags =
          __riscv_vmerge_vxm_u8m2(zero, 1, nonquote_scalar, vl);
      const vuint8m2_t follows_flags = __riscv_vslide1up_vx_u8m2(
          scalar_flags, prev_nonquote_scalar ? 1 : 0, vl);
      follows_nonquote_scalar =
          __riscv_vmsne_vx_u8m2_b4(follows_flags, 0, vl);
      last_nonquote_scalar = detail::mask_last(nonquote_scalar, vl);
    }

    const vbool4_t scalar_start = __riscv_vmand_mm_b4(
        scalar, __riscv_vmnot_m_b4(follows_nonquote_scalar, vl), vl);
    const vbool4_t potential_structural =
        __riscv_vmor_mm_b4(op, scalar_start, vl);
    const vbool4_t structural = __riscv_vmand_mm_b4(
        potential_structural, __riscv_vmnot_m_b4(string_tail, vl), vl);

    const vbool4_t control = __riscv_vmsltu_vx_u8m2_b4(bytes, 0x20, vl);
    const vbool4_t bad_control = __riscv_vmand_mm_b4(control, inside, vl);
    unescaped_control_error |= detail::any(bad_control, vl);

    prev_nonquote_scalar = last_nonquote_scalar;
    write_indexes(structural, vl, base);
  }

  simdjson_warn_unused simdjson_inline error_code finish(
      dom_parser_implementation &parser, size_t len, stage1_mode partial) noexcept {
    const error_code string_error = strings.in_string ? UNCLOSED_STRING : SUCCESS;
    const bool should_exit = is_streaming(partial)
      ? ((string_error != SUCCESS) && (string_error != UNCLOSED_STRING))
      : (string_error != SUCCESS);
    const bool have_unclosed_string = (string_error == UNCLOSED_STRING);

    parser.n_structural_indexes = uint32_t(tail - parser.structural_indexes.get());
    parser.structural_indexes[parser.n_structural_indexes] = uint32_t(len);
    parser.structural_indexes[parser.n_structural_indexes + 1] = uint32_t(len);
    parser.structural_indexes[parser.n_structural_indexes + 2] = 0;
    parser.next_structural_index = 0;

    if (simdjson_unlikely(should_exit)) { return string_error; }
    if (simdjson_unlikely(unescaped_control_error)) { return UNESCAPED_CHARS; }
    if (simdjson_unlikely(parser.n_structural_indexes == 0u)) { return EMPTY; }
    if (simdjson_unlikely(
        parser.structural_indexes[parser.n_structural_indexes - 1] > len)) {
      return UNEXPECTED_ERROR;
    }

    if (partial == stage1_mode::streaming_partial) {
      if (have_unclosed_string) {
        parser.n_structural_indexes--;
        if (simdjson_unlikely(parser.n_structural_indexes == 0u)) { return CAPACITY; }
      }
      auto new_structural_indexes = rvv_find_next_document_index(
          parser, !have_unclosed_string && rvv_ends_with_partial_scalar(parser, len));
      if (new_structural_indexes == 0 && parser.n_structural_indexes > 0) {
        if (parser.structural_indexes[0] == 0) {
          return CAPACITY;
        }
        parser.n_structural_indexes = 0;
        return EMPTY;
      }
      parser.n_structural_indexes = new_structural_indexes;
    } else if (partial == stage1_mode::streaming_final) {
      if (have_unclosed_string) { parser.n_structural_indexes--; }
      parser.n_structural_indexes = rvv_find_next_document_index(parser);
      parser.structural_indexes[parser.n_structural_indexes + 1] =
          parser.structural_indexes[parser.n_structural_indexes];
      parser.structural_indexes[parser.n_structural_indexes] = uint32_t(len);
      if (simdjson_unlikely(parser.n_structural_indexes == 0u)) { return EMPTY; }
    }

    if (simdjson_unlikely(!utf8.finish())) { return UTF8_ERROR; }
    return SUCCESS;
  }

  uint32_t *tail;
  string_state strings{};
  utf8_state utf8{};
  bool prev_nonquote_scalar{false};
  bool unescaped_control_error{false};
};

} // namespace stage1
} // namespace rvv
} // namespace simdjson

#endif // SIMDJSON_RVV_STAGE1_H
