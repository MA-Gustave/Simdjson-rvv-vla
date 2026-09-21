#ifndef SIMDJSON_CONDITIONAL_INCLUDE
#include "simdjson/rvv/base.h"
#endif // SIMDJSON_CONDITIONAL_INCLUDE

#if !SIMDJSON_CAN_ALWAYS_RUN_RVV
#if defined(SIMDJSON_RVV_GCC_FUNCTION_TARGET_ACTIVE)
#undef SIMDJSON_RVV_GCC_FUNCTION_TARGET_ACTIVE
#pragma pop_macro("simdjson_warn_unused")
#pragma pop_macro("simdjson_never_inline")
#pragma pop_macro("simdjson_really_inline")
#pragma pop_macro("simdjson_inline")
#endif
#endif

#undef SIMDJSON_IMPLEMENTATION
