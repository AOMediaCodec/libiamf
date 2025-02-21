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
 * @file detect_x86.h
 * @brief Detect x86 architecture during compilation.
 * @version 0.1
 * @date Created 10/24/2024
 **/

#ifndef DETECT_X86_H_
#define DETECT_X86_H_

#if defined(__x86_64__) || defined(__i386__)
#define IAMF_ARCH_DETECTED_X86 (1)
#endif

#endif /* DETECT_X86_H_ */
