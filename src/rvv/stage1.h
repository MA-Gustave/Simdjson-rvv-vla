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

    // Sparse fast path avoids setting up vid/vcompress for a single event.
    if (simdjson_likely(count == 1)) {
      const long index = __riscv_vfirst_m_b4(structural, vl);
      *tail++ = base + uint32_t(index);
      return;
    }

    // e32m8 and e8m2 have identical VLMAX. Generate output-width offsets
    // directly, compact them under the Stage 1 predicate, add the chunk base,
    // and store. This avoids the old u16 -> u32 widening path entirely.
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

    const vuint8m2_t zero = __riscv_vmv_v_x_u8m2(0, vl);
    const vuint8m2_t scalar_flags =
        __riscv_vmerge_vxm_u8m2(zero, 1, nonquote_scalar, vl);
    const vuint8m2_t follows_flags = __riscv_vslide1up_vx_u8m2(
        scalar_flags, prev_nonquote_scalar ? 1 : 0, vl);
    const vbool4_t follows_nonquote_scalar =
        __riscv_vmsne_vx_u8m2_b4(follows_flags, 0, vl);

    const vbool4_t scalar_start = __riscv_vmand_mm_b4(
        scalar, __riscv_vmnot_m_b4(follows_nonquote_scalar, vl), vl);
    const vbool4_t potential_structural =
        __riscv_vmor_mm_b4(op, scalar_start, vl);
    const vbool4_t structural = __riscv_vmand_mm_b4(
        potential_structural, __riscv_vmnot_m_b4(string_tail, vl), vl);

    const vbool4_t control = __riscv_vmsltu_vx_u8m2_b4(bytes, 0x20, vl);
    const vbool4_t bad_control = __riscv_vmand_mm_b4(control, inside, vl);
    unescaped_control_error |= detail::any(bad_control, vl);

    prev_nonquote_scalar = detail::mask_last(nonquote_scalar, vl);
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
