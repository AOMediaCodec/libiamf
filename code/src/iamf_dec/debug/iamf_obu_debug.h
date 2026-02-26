/*
 * Copyright (c) 2025, Alliance for Open Media. All rights reserved
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
 * @file iamf_obu_debug.h
 * @brief IAMF OBU Debug APIs.
 * @version 2.0.0
 * @date Created 12/11/2025
 **/

#ifndef __IAMF_OBU_DEBUG_H__
#define __IAMF_OBU_DEBUG_H__

#include <stdio.h>

#define def_dfn_size 32

#define obu_dump(data, size, type)                                  \
  do {                                                              \
    static int _obu_count = 0;                                      \
    static char dfn[def_dfn_size];                                  \
    snprintf(dfn, def_dfn_size, "/tmp/obu/obu_%02d_%06d.dat", type, \
             _obu_count);                                           \
    FILE *f = fopen(dfn, "w");                                      \
    if (f) {                                                        \
      fwrite(data, 1, size, f);                                     \
      fclose(f);                                                    \
    }                                                               \
    ++_obu_count;                                                   \
  } while (0)

#endif /* __IAMF_OBU_DEBUG_H__ */