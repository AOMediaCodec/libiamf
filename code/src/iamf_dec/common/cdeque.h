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
 * @file cdeque.h
 * @brief Double-ended queue APIs.
 * @version 0.1
 * @date Created 12/11/2025
 **/

#ifndef __CDEQUE_H__
#define __CDEQUE_H__

#include "cvalue.h"

typedef struct CDeque deque_t;

deque_t *deque_new();
void deque_free(deque_t *dq, func_value_wrap_ptr_free_t);
int deque_is_empty(deque_t *dq);
int deque_length(deque_t *dq);
int deque_push_front(deque_t *dq, value_wrap_t data);
int deque_push_back(deque_t *dq, value_wrap_t data);
int deque_pop_front(deque_t *dq, value_wrap_t *data);
int deque_pop_back(deque_t *dq, value_wrap_t *data);
value_wrap_t *deque_at(deque_t *dq, int index);
#endif  // __CDEQUE_H__
