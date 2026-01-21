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
 * @file animated_parameter.h
 * @brief Animated parameter APIs.
 * @version 2.0.0
 * @date Created 12/11/2025
 **/

#ifndef __ANIMATED_PARAMETER_H__
#define __ANIMATED_PARAMETER_H__

#include <stdint.h>

#include "animation.h"
#include "iorw.h"

/**
 * Animated Parameter definitions.
 */

typedef enum EIamfAnimationType {
  ck_iamf_animation_type_step,
  ck_iamf_animation_type_linear,
  ck_iamf_animation_type_bezier,
  ck_iamf_animation_type_inter_linear,
  ck_iamf_animation_type_inter_bezier
} iamf_animation_type_t;

#define def_animated_parameter_data_type_definition(T1, T2) \
  typedef struct AnimatedParameterData_##T1##_##T2 {        \
    T1 start;                                               \
    T1 end;                                                 \
    T1 control;                                             \
    T2 control_relative_time;                               \
  } animated_parameter_data_##T1##_##T2##_t

#define def_animated_parameter_data_type(T1, T2) \
  animated_parameter_data_##T1##_##T2##_t

def_animated_parameter_data_type_definition(uint32_t, uint32_t);
def_animated_parameter_data_type_definition(int16_t, float);

#define apd_u32_t def_animated_parameter_data_type(uint32_t, uint32_t)

#define def_animated_parameter_type_definition(prefix, T1, T2) \
  typedef struct AnimatedParameter_##T1##_##T2 {               \
    animation_type_t animation_type;                           \
    def_animated_parameter_data_type(T1, T2) data;             \
  } prefix##_animated_parameter_##T1##_##T2##_t

def_animated_parameter_type_definition(obu, uint32_t, uint32_t);
def_animated_parameter_type_definition(obu, int16_t, float);

#define def_animated_parameter_type(prefix, T1, T2) \
  prefix##_animated_parameter_##T1##_##T2##_t

#define ap_t(prefix, T1, T2) def_animated_parameter_type(prefix, T1, T2)
#define ap_u32_t ap_t(obu, uint32_t, uint32_t)

int animated_parameter_data_read_bits(bits_io_context_t *bits_r,
                                      uint32_t n_bits,
                                      iamf_animation_type_t type,
                                      apd_u32_t *param);

#define def_animated_parameter_data_convert_bits_function(dst, src, num_bits, \
                                                          convert_func)       \
  do {                                                                        \
    (dst)->start = convert_func((src)->start, (num_bits));                    \
    (dst)->end = convert_func((src)->end, (num_bits));                        \
    (dst)->control = convert_func((src)->control, (num_bits));                \
    (dst)->control_relative_time =                                            \
        iamf_divide_128f((src)->control_relative_time);                       \
  } while (0)

#endif  // __ANIMATED_PARAMETER_H__
