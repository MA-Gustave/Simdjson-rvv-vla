set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR riscv64)

find_program(RVV_CLANG clang REQUIRED)
find_program(RVV_CLANGXX clang++ REQUIRED)
set(CMAKE_C_COMPILER "${RVV_CLANG}")
set(CMAKE_CXX_COMPILER "${RVV_CLANGXX}")
set(CMAKE_C_COMPILER_TARGET riscv64-linux-gnu)
set(CMAKE_CXX_COMPILER_TARGET riscv64-linux-gnu)

if(DEFINED ENV{RVV_MARCH} AND NOT "$ENV{RVV_MARCH}" STREQUAL "")
  set(RVV_MARCH "$ENV{RVV_MARCH}")
else()
  set(RVV_MARCH "rv64gcv")
endif()
if(DEFINED ENV{RVV_CPP_FLAGS})
  set(RVV_CPP_FLAGS "$ENV{RVV_CPP_FLAGS}")
else()
  set(RVV_CPP_FLAGS "")
endif()
# Debian's riscv64 cross libc lives under /usr/riscv64-linux-gnu, but that
# directory is a linker/runtime prefix, not a standalone Clang sysroot.
# Passing --sysroot=/usr/riscv64-linux-gnu makes GNU ld prefix absolute paths
# from its libc linker scripts a second time (e.g. .../usr/riscv64-linux-gnu/
# usr/riscv64-linux-gnu/lib/libc.so.6).  Let Clang/GNU ld use the Debian cross
# toolchain layout directly; RVV_SYSROOT is still used below as QEMU's -L prefix.
set(CMAKE_C_FLAGS_INIT "--target=riscv64-linux-gnu -march=${RVV_MARCH} -mabi=lp64d")
set(CMAKE_CXX_FLAGS_INIT "--target=riscv64-linux-gnu -march=${RVV_MARCH} -mabi=lp64d --gcc-toolchain=/usr ${RVV_CPP_FLAGS}")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

find_program(RVV_QEMU qemu-riscv64)
if(RVV_QEMU)
  if(DEFINED ENV{RVV_SYSROOT} AND NOT "$ENV{RVV_SYSROOT}" STREQUAL "")
    set(CMAKE_CROSSCOMPILING_EMULATOR "${RVV_QEMU};-L;$ENV{RVV_SYSROOT}")
  else()
    set(CMAKE_CROSSCOMPILING_EMULATOR "${RVV_QEMU}")
  endif()
endif()
