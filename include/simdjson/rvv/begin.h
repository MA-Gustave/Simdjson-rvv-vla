#define SIMDJSON_IMPLEMENTATION rvv

#include "simdjson/rvv/base.h"

#if !SIMDJSON_CAN_ALWAYS_RUN_RVV
// GCC 14 on RISC-V supports function target("arch=+v"), but not
// #pragma GCC target regions. When the enclosing TU is baseline rv64gc,
// temporarily make simdjson's function-declaration macros RVV-targeted.
// This keeps runtime dispatch honest: only RVV functions require V.
#if defined(__riscv) && !defined(__riscv_vector) && defined(__GNUC__) && !defined(__clang__)
#pragma push_macro("simdjson_inline")
#pragma push_macro("simdjson_really_inline")
#pragma push_macro("simdjson_never_inline")
#pragma push_macro("simdjson_warn_unused")

#undef simdjson_inline
#undef simdjson_really_inline
#undef simdjson_never_inline
#undef simdjson_warn_unused

#define simdjson_inline inline __attribute__((target("arch=+v")))
#define simdjson_really_inline inline __attribute__((target("arch=+v")))
#define simdjson_never_inline __attribute__((noinline, target("arch=+v")))
#define simdjson_warn_unused __attribute__((warn_unused_result, target("arch=+v")))

#define SIMDJSON_RVV_GCC_FUNCTION_TARGET_ACTIVE 1
#endif
#endif
#include "simdjson/rvv/intrinsics.h"
#include "simdjson/rvv/bitmanipulation.h"


#include "simdjson/rvv/numberparsing_defs.h"
#include "simdjson/rvv/stringparsing_defs.h"
