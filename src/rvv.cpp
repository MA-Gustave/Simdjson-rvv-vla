#ifndef SIMDJSON_SRC_RVV_CPP
#define SIMDJSON_SRC_RVV_CPP

#ifndef SIMDJSON_CONDITIONAL_INCLUDE
#include <base.h>
#endif // SIMDJSON_CONDITIONAL_INCLUDE

#include <simdjson/rvv.h>
#include <simdjson/rvv/implementation.h>

#include <simdjson/rvv/begin.h>
#include <generic/stage1/find_next_document_index.h>
#include <generic/stage2/amalgamated.h>
#include <rvv/string_scanner.h>
#include <rvv/utf8_validation.h>
#include <rvv/stage1.h>
#include <rvv/minify.h>

namespace simdjson {
namespace rvv {

simdjson_warn_unused error_code implementation::create_dom_parser_implementation(
    size_t capacity,
    size_t max_depth,
    std::unique_ptr<internal::dom_parser_implementation>& dst) const noexcept {
  dst.reset(new (std::nothrow) dom_parser_implementation());
  if (!dst) { return MEMALLOC; }
  if (auto err = dst->set_capacity(capacity)) { return err; }
  if (auto err = dst->set_max_depth(max_depth)) { return err; }
  return SUCCESS;
}

simdjson_warn_unused error_code implementation::minify(
    const uint8_t *buf, size_t len, uint8_t *dst, size_t &dst_len) const noexcept {
  return rvv::stage1::minify(buf, len, dst, dst_len);
}

simdjson_warn_unused bool implementation::validate_utf8(
    const char *buf, size_t len) const noexcept {
  return rvv::stage1::validate_utf8(buf, len);
}

simdjson_warn_unused error_code dom_parser_implementation::stage1(
    const uint8_t *_buf, size_t _len, stage1_mode streaming) noexcept {
  this->buf = _buf;
  this->len = _len;
  return rvv::stage1::json_structural_indexer::index(buf, len, *this, streaming);
}

simdjson_warn_unused error_code dom_parser_implementation::stage2(dom::document &_doc) noexcept {
  return stage2::tape_builder::parse_document<false>(*this, _doc);
}

simdjson_warn_unused error_code dom_parser_implementation::stage2_next(dom::document &_doc) noexcept {
  return stage2::tape_builder::parse_document<true>(*this, _doc);
}

SIMDJSON_NO_SANITIZE_MEMORY
simdjson_warn_unused uint8_t *dom_parser_implementation::parse_string(
    const uint8_t *src, uint8_t *dst, bool allow_replacement) const noexcept {
  return rvv::stringparsing::parse_string(src, dst, allow_replacement);
}

simdjson_warn_unused uint8_t *dom_parser_implementation::parse_wobbly_string(
    const uint8_t *src, uint8_t *dst) const noexcept {
  return rvv::stringparsing::parse_wobbly_string(src, dst);
}

simdjson_warn_unused error_code dom_parser_implementation::parse(
    const uint8_t *_buf, size_t _len, dom::document &_doc) noexcept {
  auto error = stage1(_buf, _len, stage1_mode::regular);
  if (error) { return error; }
  return stage2(_doc);
}

} // namespace rvv
} // namespace simdjson

#include <simdjson/rvv/end.h>

#endif // SIMDJSON_SRC_RVV_CPP
