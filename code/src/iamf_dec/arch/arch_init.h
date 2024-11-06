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
 * @file arch_init.h
 * @brief Init CPU-specific function callbacks.
 * @version 0.1
 * @date Created 10/24/2024
 **/

#ifndef ARCH_INIT_H_
#define ARCH_INIT_H_

typedef struct ArchCallbacks Arch;

void arch_init(Arch* arch);

#endif /* ARCH_INIT_H_ */
