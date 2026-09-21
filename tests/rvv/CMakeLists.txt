# RVV-VLA backend-specific correctness tests.
# These tests are intentionally opt-in so unrelated host builds are unaffected.

set(RVV_TESTS
  rvv_registry_tests
  rvv_dom_equivalence_tests
  rvv_stage1_boundary_tests
  rvv_minify_tests
  rvv_utf8_tests
  rvv_number_tests
  rvv_ondemand_tests
  rvv_streaming_tests
  rvv_random_differential_tests
  rvv_corpus_tests
)

foreach(test_name IN LISTS RVV_TESTS)
  add_cpp_test(${test_name} LABELS rvv per_implementation)
  target_include_directories(${test_name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
endforeach()

target_compile_definitions(rvv_corpus_tests PRIVATE RVV_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
