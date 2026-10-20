/* Included first in every PS4LM C and C++ file (ps4/toolchain.cmake, -include).
 *
 * The SDK's FreeBSD 9 headers use u_int and friends without including
 * sys/types.h themselves, and declare none of the FreeBSD 10 functions
 * llama.cpp calls. compat.c implements those on ones libkernel exports.
 * docs/PORTING.md has one entry per function.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include <sys/types.h>

#if defined(PS4LM) && !defined(__ASSEMBLER__)
#ifdef __cplusplus
extern "C" {
#endif

int pipe2(int fds[2], int flags);

#ifdef __cplusplus
}
#endif
#endif
