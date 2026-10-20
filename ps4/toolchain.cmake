# PS4LM's CMake toolchain: the PS4 payload SDK's own toolchain plus Jaguar flags.
#
#   cmake -DCMAKE_TOOLCHAIN_FILE=ps4/toolchain.cmake ...
#
# scripts/build-llama.sh passes it. PS4_PAYLOAD_SDK comes from scripts/env.sh.
# See docs/TOOLCHAIN.md.

if(NOT DEFINED ENV{PS4_PAYLOAD_SDK} AND NOT DEFINED PS4_PAYLOAD_SDK)
    message(FATAL_ERROR "PS4_PAYLOAD_SDK is not set: source scripts/env.sh")
endif()
if(NOT DEFINED PS4_PAYLOAD_SDK)
    set(PS4_PAYLOAD_SDK "$ENV{PS4_PAYLOAD_SDK}")
endif()

# Target triple x86_64-scie-ps4, sysroot, orbis-clang and friends, static
# libraries and position independent executables.
include("${PS4_PAYLOAD_SDK}/toolchain/orbis.cmake")

# The triple alone builds generic x86-64 with no AVX (docs/PORTING.md 1).
# btver2 is Jaguar: SSE4.2, AVX, F16C, BMI1; no AVX2, FMA or BMI2.
# PS4LM=1 gates every PS4 patch and shim.
set(PS4LM_FLAGS "-march=btver2 -mtune=btver2 -DPS4LM=1")
set(CMAKE_ASM_FLAGS_INIT "${PS4LM_FLAGS}")
# Every C and C++ file starts with compat/compat.h: sys/types.h, which the
# SDK's FreeBSD 9 headers expect but do not include, and the functions
# compat/compat.c adds (docs/PORTING.md).
set(PS4LM_COMPAT_H "${CMAKE_CURRENT_LIST_DIR}/compat/compat.h")
set(CMAKE_C_FLAGS_INIT "${PS4LM_FLAGS} -include ${PS4LM_COMPAT_H}")
set(CMAKE_CXX_FLAGS_INIT "${PS4LM_FLAGS} -include ${PS4LM_COMPAT_H}")
