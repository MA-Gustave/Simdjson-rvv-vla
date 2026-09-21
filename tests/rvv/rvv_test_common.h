#ifndef SIMDJSON_TESTS_RVV_TEST_COMMON_H
#define SIMDJSON_TESTS_RVV_TEST_COMMON_H

#include "simdjson.h"

#include <cmath>
#include <cstdint>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace rvv_test {

inline const simdjson::implementation *implementation(const char *name) {
  return simdjson::get_available_implementations()[name];
}

inline const simdjson::implementation *rvv() {
  return implementation("rvv");
}

inline const simdjson::implementation *fallback() {
  return implementation("fallback");
}

inline bool require_implementation(const char *name) {
  const auto *impl = implementation(name);
  if (impl == nullptr) {
    std::cerr << "[SETUP] implementation '" << name << "' is not compiled in\n";
    return false;
  }
  if (!impl->supported_by_runtime_system()) {
    std::cerr << "[SETUP] implementation '" << name << "' is not supported by the runtime\n";
    return false;
  }
  return true;
}

inline bool require_rvv_and_fallback() {
  return require_implementation("rvv") && require_implementation("fallback");
}

class active_implementation_guard {
public:
  active_implementation_guard() : previous(simdjson::get_active_implementation()) {}
  ~active_implementation_guard() { simdjson::get_active_implementation() = previous; }
  active_implementation_guard(const active_implementation_guard &) = delete;
  active_implementation_guard &operator=(const active_implementation_guard &) = delete;
private:
  const simdjson::implementation *previous;
};

inline bool dom_equal(simdjson::dom::element a, simdjson::dom::element b,
                      std::string &reason) {
  if (a.type() != b.type()) {
    reason = "element type mismatch";
    return false;
  }
  switch (a.type()) {
    case simdjson::dom::element_type::ARRAY: {
      auto aa = a.get_array();
      auto ab = b.get_array();
      if (aa.size() != ab.size()) {
        reason = "array size mismatch";
        return false;
      }
      auto ia = aa.begin();
      auto ib = ab.begin();
      size_t index = 0;
      for (; ia != aa.end(); ++ia, ++ib, ++index) {
        if (!dom_equal(*ia, *ib, reason)) {
          reason = "array[" + std::to_string(index) + "]: " + reason;
          return false;
        }
      }
      return true;
    }
    case simdjson::dom::element_type::OBJECT: {
      auto oa = a.get_object();
      auto ob = b.get_object();
      if (oa.size() != ob.size()) {
        reason = "object size mismatch";
        return false;
      }
      auto ia = oa.begin();
      auto ib = ob.begin();
      for (; ia != oa.end(); ++ia, ++ib) {
        const std::string_view ka = (*ia).key;
        const std::string_view kb = (*ib).key;
        if (ka != kb) {
          reason = "object key mismatch";
          return false;
        }
        if (!dom_equal((*ia).value, (*ib).value, reason)) {
          reason = "object['" + std::string(ka) + "']: " + reason;
          return false;
        }
      }
      return true;
    }
    case simdjson::dom::element_type::INT64:
      if (int64_t(a) != int64_t(b)) { reason = "int64 mismatch"; return false; }
      return true;
    case simdjson::dom::element_type::UINT64:
      if (uint64_t(a) != uint64_t(b)) { reason = "uint64 mismatch"; return false; }
      return true;
    case simdjson::dom::element_type::DOUBLE: {
      const double da = double(a);
      const double db = double(b);
      if (da != db && !(std::isnan(da) && std::isnan(db))) {
        reason = "double mismatch";
        return false;
      }
      return true;
    }
    case simdjson::dom::element_type::STRING:
      if (std::string_view(a) != std::string_view(b)) { reason = "string mismatch"; return false; }
      return true;
    case simdjson::dom::element_type::BIGINT:
      if (a.get_bigint().value_unsafe() != b.get_bigint().value_unsafe()) {
        reason = "bigint mismatch";
        return false;
      }
      return true;
    case simdjson::dom::element_type::BOOL:
      if (bool(a) != bool(b)) { reason = "bool mismatch"; return false; }
      return true;
    case simdjson::dom::element_type::NULL_VALUE:
      return true;
  }
  reason = "unknown element type";
  return false;
}

inline bool compare_dom(const std::string &name, const std::string &json) {
  const auto *ref = fallback();
  const auto *target = rvv();
  if (ref == nullptr || target == nullptr) {
    std::cerr << "[SETUP] fallback and rvv must both be compiled for differential tests\n";
    return false;
  }

  active_implementation_guard guard;
  simdjson::padded_string ref_input(json);
  simdjson::padded_string rvv_input(json);

  simdjson::get_active_implementation() = ref;
  simdjson::dom::parser ref_parser;
  simdjson::dom::element ref_doc;
  const simdjson::error_code ref_error = ref_parser.parse(ref_input).get(ref_doc);

  simdjson::get_active_implementation() = target;
  simdjson::dom::parser rvv_parser;
  simdjson::dom::element rvv_doc;
  const simdjson::error_code rvv_error = rvv_parser.parse(rvv_input).get(rvv_doc);

  // Error-code selection is not identical across simdjson implementations for
  // malformed JSON. What must agree here is acceptance vs rejection; for valid
  // JSON we then compare the DOM exactly.
  const bool ref_success = ref_error == simdjson::SUCCESS;
  const bool rvv_success = rvv_error == simdjson::SUCCESS;
  if (ref_success != rvv_success) {
    std::cerr << "[FAIL] " << name << ": validity mismatch fallback="
              << ref_error << " rvv=" << rvv_error << "\n";
    return false;
  }
  if (!ref_success) {
    return true;
  }

  std::string reason;
  if (!dom_equal(ref_doc, rvv_doc, reason)) {
    std::cerr << "[FAIL] " << name << ": " << reason << "\n";
    return false;
  }
  return true;
}

inline std::string minify_with(const simdjson::implementation *impl,
                               const std::string &input,
                               simdjson::error_code &error) {
  std::vector<uint8_t> out(input.size() + simdjson::SIMDJSON_PADDING);
  size_t out_len = 0;
  error = impl->minify(reinterpret_cast<const uint8_t *>(input.data()), input.size(),
                       out.data(), out_len);
  return std::string(reinterpret_cast<const char *>(out.data()), out_len);
}

inline bool compare_minify(const std::string &name, const std::string &input) {
  const auto *ref = fallback();
  const auto *target = rvv();
  if (ref == nullptr || target == nullptr) {
    std::cerr << "[SETUP] fallback and rvv must both be compiled for differential tests\n";
    return false;
  }
  simdjson::error_code ref_error = simdjson::SUCCESS;
  simdjson::error_code rvv_error = simdjson::SUCCESS;
  const std::string a = minify_with(ref, input, ref_error);
  const std::string b = minify_with(target, input, rvv_error);
  if (ref_error != rvv_error) {
    std::cerr << "[FAIL] " << name << ": minify error mismatch fallback_error="
              << ref_error << " rvv_error=" << rvv_error << "\n";
    return false;
  }

  // dst_len/output on an error is implementation-dependent in simdjson
  // (notably fallback vs generic vector minifiers). Only compare bytes when
  // minification succeeds.
  if (ref_error != simdjson::SUCCESS) {
    return true;
  }
  if (a != b) {
    std::cerr << "[FAIL] " << name << ": minify output mismatch"
              << " fallback_len=" << a.size() << " rvv_len=" << b.size() << "\n";
    return false;
  }
  return true;
}

inline bool compare_utf8(const std::string &name, const std::string &input) {
  const auto *ref = fallback();
  const auto *target = rvv();
  if (ref == nullptr || target == nullptr) {
    std::cerr << "[SETUP] fallback and rvv must both be compiled for differential tests\n";
    return false;
  }
  const bool a = ref->validate_utf8(input.data(), input.size());
  const bool b = target->validate_utf8(input.data(), input.size());
  if (a != b) {
    std::cerr << "[FAIL] " << name << ": UTF-8 mismatch fallback=" << a
              << " rvv=" << b << " length=" << input.size() << "\n";
    return false;
  }
  return true;
}

inline int finish(bool ok, const char *suite) {
  if (ok) {
    std::cout << "[PASS] " << suite << "\n";
    return 0;
  }
  std::cerr << "[FAIL] " << suite << "\n";
  return 1;
}

} // namespace rvv_test

#endif // SIMDJSON_TESTS_RVV_TEST_COMMON_H
