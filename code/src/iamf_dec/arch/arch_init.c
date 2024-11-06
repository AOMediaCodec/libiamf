/*
 * Copyright (c) 2024, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */

/**
 * @file arch_init.c
 * @brief Init CPU-specific function callbacks.
 * @version 0.1
 * @date Created 10/24/2024
 **/

#include "arch_init.h"

#include <string.h>

#include "arch.h"
#include "arch_common.h"

// To add new architecture 'ABC', follow the following pattern:
// * add arch/ABC subfolder, with detect_ABC.h
// * inside detect_ABC.h, if ABC target detected, define IAMF_ARCH_DETECTED_ABC,
//   and implement arch_override(...) function
// * all files under arch/ABC must include detect_ABC.h, and be surrounded with
//   #if defined(IAMF_ARCH_DETECTED_ABC)
#include "arm/detect_arm.h"
#include "x86/detect_x86.h"
#if defined(IAMF_ARCH_DETECTED_ARM) || defined(IAMF_ARCH_DETECTED_X86)
#define HAS_ARCH_OVERRIDE (1)
#endif

#if defined(HAS_ARCH_OVERRIDE)
// Implemented in each architecture-specific subfolder,
// behind IAMF_ARCH_DETECTED_... ifdef
void arch_override(Arch* arch);
#endif

void arch_init(Arch* arch) {
  memset(arch, 0x0, sizeof(Arch));

  // Fill with reference implementations

  // arch->myfn = &myfn_c; // myfn_c is in arch_common.h/.c
  // TODO: Remove these lines when first function added!

#if defined(HAS_ARCH_OVERRIDE)
  // Override with platform-specific functions, if available
  arch_override(arch);
#endif
}
