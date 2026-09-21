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

#ifndef SIMDJSON_RVV_SPARSE_ESCAPE_THRESHOLD
#define SIMDJSON_RVV_SPARSE_ESCAPE_THRESHOLD 2
#endif
#ifndef SIMDJSON_RVV_ENABLE_PACKED_CONTROL
#define SIMDJSON_RVV_ENABLE_PACKED_CONTROL 1
#endif
#ifndef SIMDJSON_RVV_ENABLE_PACKED_ESCAPE
#define SIMDJSON_RVV_ENABLE_PACKED_ESCAPE SIMDJSON_RVV_ENABLE_PACKED_CONTROL
#endif
#ifndef SIMDJSON_RVV_ENABLE_PACKED_QUOTE
#define SIMDJSON_RVV_ENABLE_PACKED_QUOTE SIMDJSON_RVV_ENABLE_PACKED_CONTROL
#endif
#ifndef SIMDJSON_RVV_ENABLE_PACKED_SHIFT
#define SIMDJSON_RVV_ENABLE_PACKED_SHIFT SIMDJSON_RVV_ENABLE_PACKED_CONTROL
#endif

// Sparse escape handling is event-driven: enumerate only actual backslashes
// using mask-first operations, then synthesize escaped-byte positions. This
// avoids the vcompress -> viota -> vrgather chain for the very sparsest JSON
// chunks. Medium/dense masks use the packed-bit control plane below; the old
// dense vector algorithm remains as the unrestricted-VLEN fallback.
static constexpr size_t SPARSE_ESCAPE_THRESHOLD =
    SIMDJSON_RVV_SPARSE_ESCAPE_THRESHOLD;

// Packed-mask control plane. The optimized path covers up to 512 active byte
// lanes, which includes the project's tested VLEN range through VLEN=1024 for
// both Stage 1 (e8m2) and minify (e8m4). Larger vectors remain fully correct by
// falling back to the vector algorithms below; this is a performance boundary,
// not a VLA semantic boundary.
static constexpr size_t PACKED_MASK_MAX_LANES = 512;
static constexpr size_t PACKED_MASK_WORDS = PACKED_MASK_MAX_LANES / 64;

struct packed_mask_words {
  uint64_t words[PACKED_MASK_WORDS];
};

simdjson_really_inline uint64_t valid_low_bits(size_t count) noexcept {
  return count >= 64 ? ~uint64_t(0) : ((uint64_t(1) << count) - 1);
}

simdjson_really_inline size_t packed_word_count(size_t vl) noexcept {
  return (vl + 63) / 64;
}

simdjson_really_inline size_t packed_word_valid_bits(
    size_t word_index, size_t vl) noexcept {
  const size_t consumed = word_index * 64;
  const size_t remaining = vl - consumed;
  return remaining < 64 ? remaining : 64;
}

simdjson_really_inline bool pack_mask(
    vbool4_t mask, size_t vl, packed_mask_words &out) noexcept {
  if (SIMDJSON_IS_BIG_ENDIAN || vl > PACKED_MASK_MAX_LANES) { return false; }
  const size_t words = packed_word_count(vl);
  for (size_t i = 0; i < words; ++i) { out.words[i] = 0; }
  __riscv_vsm_v_b4(reinterpret_cast<uint8_t *>(out.words), mask, vl);
  const size_t last_bits = packed_word_valid_bits(words - 1, vl);
  out.words[words - 1] &= valid_low_bits(last_bits);
  return true;
}

simdjson_really_inline bool pack_mask(
    vbool2_t mask, size_t vl, packed_mask_words &out) noexcept {
  if (SIMDJSON_IS_BIG_ENDIAN || vl > PACKED_MASK_MAX_LANES) { return false; }
  const size_t words = packed_word_count(vl);
  for (size_t i = 0; i < words; ++i) { out.words[i] = 0; }
  __riscv_vsm_v_b2(reinterpret_cast<uint8_t *>(out.words), mask, vl);
  const size_t last_bits = packed_word_valid_bits(words - 1, vl);
  out.words[words - 1] &= valid_low_bits(last_bits);
  return true;
}

simdjson_really_inline vbool4_t unpack_mask_b4(
    const packed_mask_words &in, size_t vl) noexcept {
  return __riscv_vlm_v_b4(
      reinterpret_cast<const uint8_t *>(in.words), vl);
}

simdjson_really_inline vbool2_t unpack_mask_b2(
    const packed_mask_words &in, size_t vl) noexcept {
  return __riscv_vlm_v_b2(
      reinterpret_cast<const uint8_t *>(in.words), vl);
}

// Cumulative XOR used by simdjson's fixed-width backends. This produces the
// inclusive quote parity needed by generic string-tail semantics.
simdjson_really_inline uint64_t prefix_xor_word(uint64_t bits) noexcept {
#if __riscv_zbc
  return __riscv_clmul_64(bits, ~uint64_t(0));
#else
  bits ^= bits << 1;
  bits ^= bits << 2;
  bits ^= bits << 4;
  bits ^= bits << 8;
  bits ^= bits << 16;
  bits ^= bits << 32;
  return bits;
#endif
}

// The same escape-run algebra used by simdjson's generic 64-byte Stage 1,
// applied one packed mask word at a time. Carry is one bit: whether lane 0 of
// the next word is escaped by an active trailing backslash.
simdjson_really_inline uint64_t escaped_word(
    uint64_t backslash, size_t valid_bits, bool incoming_escape,
    bool &outgoing_escape) noexcept {
  static constexpr uint64_t ODD_BITS = 0xAAAAAAAAAAAAAAAAULL;
  const uint64_t valid = valid_low_bits(valid_bits);
  backslash &= valid;
  const uint64_t incoming = incoming_escape ? 1u : 0u;
  const uint64_t potential_escape = backslash & ~incoming;
  const uint64_t maybe_escaped = potential_escape << 1;
  const uint64_t escape_and_terminal =
      ((maybe_escaped | ODD_BITS) - potential_escape) ^ ODD_BITS;
  const uint64_t escaped =
      (escape_and_terminal ^ (backslash | incoming)) & valid;
  const uint64_t active_escape = escape_and_terminal & backslash & valid;
  outgoing_escape =
      ((active_escape >> (valid_bits - 1)) & uint64_t(1)) != 0;
  return escaped;
}

simdjson_really_inline bool escaped_characters_packed(
    vbool4_t backslash, size_t vl, string_state &state,
    vbool4_t &escaped) noexcept {
  if (!SIMDJSON_RVV_ENABLE_PACKED_ESCAPE) { return false; }
  packed_mask_words input{};
  if (!pack_mask(backslash, vl, input)) { return false; }
  packed_mask_words output{};
  bool carry = state.next_is_escaped;
  const size_t words = packed_word_count(vl);
  for (size_t i = 0; i < words; ++i) {
    const size_t valid_bits = packed_word_valid_bits(i, vl);
    bool next = false;
    output.words[i] = escaped_word(input.words[i], valid_bits, carry, next);
    carry = next;
  }
  state.next_is_escaped = carry;
  escaped = unpack_mask_b4(output, vl);
  return true;
}

simdjson_really_inline bool escaped_characters_packed_m4(
    vbool2_t backslash, size_t vl, string_state &state,
    vbool2_t &escaped) noexcept {
  if (!SIMDJSON_RVV_ENABLE_PACKED_ESCAPE) { return false; }
  packed_mask_words input{};
  if (!pack_mask(backslash, vl, input)) { return false; }
  packed_mask_words output{};
  bool carry = state.next_is_escaped;
  const size_t words = packed_word_count(vl);
  for (size_t i = 0; i < words; ++i) {
    const size_t valid_bits = packed_word_valid_bits(i, vl);
    bool next = false;
    output.words[i] = escaped_word(input.words[i], valid_bits, carry, next);
    carry = next;
  }
  state.next_is_escaped = carry;
  escaped = unpack_mask_b2(output, vl);
  return true;
}

simdjson_really_inline bool inside_string_packed(
    vbool4_t unescaped_quote, size_t vl, string_state &state,
    vbool4_t &inside) noexcept {
  if (!SIMDJSON_RVV_ENABLE_PACKED_QUOTE) { return false; }
  packed_mask_words quote{};
  if (!pack_mask(unescaped_quote, vl, quote)) { return false; }
  packed_mask_words output{};
  bool carry = state.in_string;
  const size_t words = packed_word_count(vl);
  for (size_t i = 0; i < words; ++i) {
    const size_t valid_bits = packed_word_valid_bits(i, vl);
    const uint64_t valid = valid_low_bits(valid_bits);
    uint64_t prefix = prefix_xor_word(quote.words[i]) & valid;
    if (carry) { prefix ^= valid; }
    output.words[i] = prefix;
    carry = ((prefix >> (valid_bits - 1)) & uint64_t(1)) != 0;
  }
  state.in_string = carry;
  inside = unpack_mask_b4(output, vl);
  return true;
}

simdjson_really_inline bool inside_string_packed_m4(
    vbool2_t unescaped_quote, size_t vl, string_state &state,
    vbool2_t &inside) noexcept {
  if (!SIMDJSON_RVV_ENABLE_PACKED_QUOTE) { return false; }
  packed_mask_words quote{};
  if (!pack_mask(unescaped_quote, vl, quote)) { return false; }
  packed_mask_words output{};
  bool carry = state.in_string;
  const size_t words = packed_word_count(vl);
  for (size_t i = 0; i < words; ++i) {
    const size_t valid_bits = packed_word_valid_bits(i, vl);
    const uint64_t valid = valid_low_bits(valid_bits);
    uint64_t prefix = prefix_xor_word(quote.words[i]) & valid;
    if (carry) { prefix ^= valid; }
    output.words[i] = prefix;
    carry = ((prefix >> (valid_bits - 1)) & uint64_t(1)) != 0;
  }
  state.in_string = carry;
  inside = unpack_mask_b2(output, vl);
  return true;
}

// Shift a predicate by one lane, injecting a scalar carry into lane 0. This
// replaces byte expansion + vslide1up when the logical operation is only a
// one-bit predicate shift. `last` returns the final active input bit.
simdjson_really_inline bool shift_mask_left_one_packed(
    vbool4_t input, size_t vl, bool incoming, vbool4_t &shifted,
    bool &last) noexcept {
  if (!SIMDJSON_RVV_ENABLE_PACKED_SHIFT) { return false; }
  packed_mask_words source{};
  if (!pack_mask(input, vl, source)) { return false; }
  packed_mask_words output{};
  bool carry = incoming;
  const size_t words = packed_word_count(vl);
  for (size_t i = 0; i < words; ++i) {
    const size_t valid_bits = packed_word_valid_bits(i, vl);
    const uint64_t valid = valid_low_bits(valid_bits);
    const uint64_t word = source.words[i] & valid;
    output.words[i] = ((word << 1) | uint64_t(carry)) & valid;
    carry = ((word >> (valid_bits - 1)) & uint64_t(1)) != 0;
  }
  last = carry;
  shifted = unpack_mask_b4(output, vl);
  return true;
}

simdjson_really_inline vbool4_t escaped_characters_sparse(
    vbool4_t backslash, size_t count, size_t vl, bool incoming_escape,
    bool &outgoing_escape) noexcept {
  // Byte lane IDs are enough through lane 255. The caller only selects this
  // path when vl <= 256; larger VLENs retain the fully scalable dense path.
  const vuint8m2_t lanes = __riscv_vid_v_u8m2(vl);
  vbool4_t escaped = __riscv_vmclr_m_b4(vl);
  if (incoming_escape) {
    escaped = __riscv_vmor_mm_b4(
        escaped, __riscv_vmseq_vx_u8m2_b4(lanes, 0, vl), vl);
  }

  vbool4_t pending = backslash;
  long previous = -2;
  bool previous_active = false;
  outgoing_escape = false;

  for (size_t i = 0; i < count; ++i) {
    const long position = __riscv_vfirst_m_b4(pending, vl);
    bool active = (position == previous + 1) ? !previous_active : true;
    if (i == 0 && position == 0 && incoming_escape) { active = false; }

    if (active) {
      if (size_t(position + 1) < vl) {
        escaped = __riscv_vmor_mm_b4(
            escaped,
            __riscv_vmseq_vx_u8m2_b4(
                lanes, uint8_t(position + 1), vl),
            vl);
      } else {
        outgoing_escape = true;
      }
    }

    previous = position;
    previous_active = active;
    const vbool4_t first = __riscv_vmsof_m_b4(pending, vl);
    pending = __riscv_vmxor_mm_b4(pending, first, vl);
  }
  return escaped;
}

simdjson_really_inline vbool2_t escaped_characters_sparse_m4(
    vbool2_t backslash, size_t count, size_t vl, bool incoming_escape,
    bool &outgoing_escape) noexcept {
  // Same event algorithm for minify. Restrict the optimized path to <=256
  // lanes so byte lane IDs remain exact; the dense u16m8 path handles wider
  // vectors without imposing any VLEN correctness limit.
  const vuint8m4_t lanes = __riscv_vid_v_u8m4(vl);
  vbool2_t escaped = __riscv_vmclr_m_b2(vl);
  if (incoming_escape) {
    escaped = __riscv_vmor_mm_b2(
        escaped, __riscv_vmseq_vx_u8m4_b2(lanes, 0, vl), vl);
  }

  vbool2_t pending = backslash;
  long previous = -2;
  bool previous_active = false;
  outgoing_escape = false;

  for (size_t i = 0; i < count; ++i) {
    const long position = __riscv_vfirst_m_b2(pending, vl);
    bool active = (position == previous + 1) ? !previous_active : true;
    if (i == 0 && position == 0 && incoming_escape) { active = false; }

    if (active) {
      if (size_t(position + 1) < vl) {
        escaped = __riscv_vmor_mm_b2(
            escaped,
            __riscv_vmseq_vx_u8m4_b2(
                lanes, uint8_t(position + 1), vl),
            vl);
      } else {
        outgoing_escape = true;
      }
    }

    previous = position;
    previous_active = active;
    const vbool2_t first = __riscv_vmsof_m_b2(pending, vl);
    pending = __riscv_vmxor_mm_b2(pending, first, vl);
  }
  return escaped;
}

// Stage 1 version: e8m2 keeps the lane count compatible with e32m8, which
// lets the structural writer compact absolute uint32_t indexes directly.
simdjson_really_inline vbool4_t escaped_characters(
    vuint8m2_t bytes, size_t vl, string_state &state) noexcept {
  const bool incoming_escape = state.next_is_escaped;
  const vbool4_t backslash = __riscv_vmseq_vx_u8m2_b4(bytes, '\\', vl);
  const size_t backslash_count = __riscv_vcpop_m_b4(backslash, vl);
  if (simdjson_likely(backslash_count == 0)) {
    state.next_is_escaped = false;
    if (simdjson_likely(!incoming_escape)) {
      return __riscv_vmclr_m_b4(vl);
    }
    // Rare boundary-only escape: pay for lane indexes only here.
    const vuint16m4_t lanes = __riscv_vid_v_u16m4(vl);
    return __riscv_vmseq_vx_u16m4_b4(lanes, 0, vl);
  }

  if (simdjson_likely(
          backslash_count <= SPARSE_ESCAPE_THRESHOLD && vl <= 256)) {
    bool outgoing_escape = false;
    const vbool4_t escaped = escaped_characters_sparse(
        backslash, backslash_count, vl, incoming_escape, outgoing_escape);
    state.next_is_escaped = outgoing_escape;
    return escaped;
  }

  // For medium/dense escape masks in the common VLEN range, move only the
  // predicate bits to the scalar control plane. This avoids high-LMUL
  // vcompress/viota/vrgather while preserving exact cross-word carry.
  vbool4_t packed_escaped;
  if (simdjson_likely(
          escaped_characters_packed(backslash, vl, state, packed_escaped))) {
    return packed_escaped;
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

  vbool4_t packed_inside;
  if (simdjson_likely(
          inside_string_packed(unescaped_quote, vl, state, packed_inside))) {
    return packed_inside;
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
  const size_t backslash_count = __riscv_vcpop_m_b2(backslash, vl);
  if (simdjson_likely(backslash_count == 0)) {
    state.next_is_escaped = false;
    if (simdjson_likely(!incoming_escape)) {
      return __riscv_vmclr_m_b2(vl);
    }
    const vuint16m8_t lanes = __riscv_vid_v_u16m8(vl);
    return __riscv_vmseq_vx_u16m8_b2(lanes, 0, vl);
  }

  if (simdjson_likely(
          backslash_count <= SPARSE_ESCAPE_THRESHOLD && vl <= 256)) {
    bool outgoing_escape = false;
    const vbool2_t escaped = escaped_characters_sparse_m4(
        backslash, backslash_count, vl, incoming_escape, outgoing_escape);
    state.next_is_escaped = outgoing_escape;
    return escaped;
  }

  vbool2_t packed_escaped;
  if (simdjson_likely(
          escaped_characters_packed_m4(backslash, vl, state, packed_escaped))) {
    return packed_escaped;
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

  vbool2_t packed_inside;
  if (simdjson_likely(
          inside_string_packed_m4(unescaped_quote, vl, state, packed_inside))) {
    return packed_inside;
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
