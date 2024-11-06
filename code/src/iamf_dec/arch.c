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
 * @file arch.c
 * @brief Collection of CPU-specific functions.
 * @version 0.1
 * @date Created 10/24/2024
 **/

#include "arch.h"

#include <assert.h>

#include "IAMF_utils.h"
#include "arch/arch_init.h"

Arch* arch_create() {
  Arch* arch = IAMF_MALLOCZ(Arch, 1);
  memset(arch, 0x0, sizeof(Arch));

  arch_init(arch);

  return arch;
}

void arch_destroy(Arch* arch) { IAMF_FREE(arch); }
