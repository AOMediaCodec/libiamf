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
 * @file override_x86.c
 * @brief Override with x86 function implementations.
 * @version 0.1
 * @date Created 10/24/2024
 **/

#include "detect_x86.h"

#if defined(IAMF_ARCH_DETECTED_X86)

#include "../../arch.h"

void arch_override(Arch* arch) {
  // Override functions with x86 implementations here
}

#endif /* IAMF_ARCH_DETECTED_X86 */
