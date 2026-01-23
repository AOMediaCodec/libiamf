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
 * @file iamf_obu_parser.h
 * @brief IAMF OBU Parser module.
 * @version 2.0.0
 * @date Created 12/11/2025
 **/

#ifndef __IAMF_OBU_PARSER_H__
#define __IAMF_OBU_PARSER_H__

#include "obu/iamf_obu.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Enumeration of IAMF OBU parser states.
 *
 * This enumeration defines the possible states of the IAMF OBU parser during
 * the parsing process. The parser transitions between these states based on
 * the processing results of individual OBUs and the callback function return
 * values.
 */
typedef enum EIamfParserState {
  /** Parser is idle - not initialized or ready for new parsing session. */
  ck_iamf_parser_state_idle,

  /** Parser is running and actively processing OBUs. */
  ck_iamf_parser_state_run,

  /** Parser has stopped processing due to error or completion. */
  ck_iamf_parser_state_stop,

  /** Parser detected new stream and requires calling module to handle
   *  switching.
   */
  ck_iamf_parser_state_switch,
} iamf_parser_state_t;

/**
 * @brief Callback function type for processing a parsed IAMF OBU.
 *
 * This function pointer defines the signature for a callback that will be
 * invoked by the parser for each successfully parsed OBU (excluding reserved
 * and temporal delimiter OBUs).
 *
 * @param obu Pointer to the parsed IAMF OBU structure. The callback function
 *            is responsible for managing the lifecycle of this OBU, including
 *            freeing its memory if necessary, using the appropriate
 * `iamf_obu_free` function.
 * @param user_data A pointer to user-provided data that was passed to
 *                  `iamf_obu_parser_create`. This allows the callback to access
 *                  custom context.
 * @return int An integer status code. Return IAMF_OK (0) for success,
 *             or an appropriate IAMF error code on failure. If an error
 *             is returned, the parsing process may be affected or halted
 *             depending on the implementation.
 */
typedef iamf_parser_state_t (*fun_obu_process_t)(iamf_obu_t *obu,
                                                 void *user_data);

/**
 * @brief Opaque structure for the IAMF OBU parser instance.
 *
 * This structure holds the internal state of the OBU parser, including the
 * profile, callback function, user data, and any extra parameters.
 */
typedef struct IamfObuParser iamf_obu_parser_t;

/**
 * @brief Creates and initializes an IAMF OBU parser instance.
 *
 * This function allocates memory for a new parser and initializes it with
 * the provided OBU processing callback, user data, and any extra parameters.
 * The parser is initially set to the default IAMF profile.
 *
 * @param obu_process A function pointer of type `fun_obu_process_t` that
 *                    will be called for each valid OBU parsed. This callback
 *                    is responsible for handling the parsed OBU data.
 * @param user_data A void pointer to arbitrary user data. This pointer will
 *                  be passed to the `obu_process` callback function, allowing
 *                  it to access external context or state.
 * @param extra_params A pointer to an `iamf_obu_extra_parameters_t` structure
 *                     containing additional configuration for OBU parsing.
 *                     This can be NULL if no extra parameters are needed.
 *                     The content of this structure is copied internally, so
 *                     the original structure can be freed or modified after
 *                     this call without affecting the parser.
 * @return iamf_obu_parser_t* On success, returns a pointer to the newly
 *                             created parser instance. On failure (e.g.,
 *                             memory allocation error), returns NULL.
 */
iamf_obu_parser_t *iamf_obu_parser_create(
    fun_obu_process_t obu_process, void *user_data,
    const iamf_obu_extra_parameters_t *extra_params);

/**
 * @brief Destroys an IAMF OBU parser instance and frees its resources.
 *
 * This function deallocates the memory associated with the parser instance
 * that was created by `iamf_obu_parser_create`. After calling this function,
 * the parser pointer should no longer be used.
 *
 * @param parser Pointer to the IAMF OBU parser instance to be destroyed.
 *               If NULL is passed, the function does nothing.
 */
void iamf_obu_parser_destroy(iamf_obu_parser_t *parser);

/**
 * @brief Parses a buffer containing IAMF OBU stream data.
 *
 * This function iteratively processes a buffer of raw IAMF bitstream data.
 * It identifies and parses individual OBUs within the buffer. For each
 * successfully parsed OBU (excluding reserved and temporal delimiter OBUs),
 * the registered `obu_process` callback is invoked. The function continues
 * to parse OBUs until the input buffer is exhausted or an incomplete OBU
 * is encountered at the end of the buffer.
 *
 * If an error occurs during the processing of a specific OBU (e.g., the
 * `obu_process` callback returns an error), an error message is logged,
 * but the parsing attempt continues with the next OBU in the buffer.
 * Invalid arguments to this function will result in 0 bytes being processed.
 *
 * @param parser Pointer to the IAMF OBU parser instance.
 * @param data Pointer to the buffer containing the IAMF OBU stream data.
 * @param size The size of the data buffer in bytes.
 * @return uint32_t The actual number of bytes consumed from the input `data`
 *                  buffer. This value represents the total size of all fully
 *                  processed OBUs. It can be less than `size` if the buffer
 *                  ends with a partial OBU. Returns 0 if `parser`, `data`, or
 *                  `size` are invalid, or if no OBUs could be processed.
 */
uint32_t iamf_obu_parser_parse(iamf_obu_parser_t *parser, const uint8_t *data,
                               uint32_t size);

/**
 * @brief Retrieves the current state of the IAMF OBU parser.
 *
 * This function returns the current parsing state of the IAMF OBU parser
 * instance. The state reflects the result of the most recent parsing operation
 * and indicates the current operational status of the parser. This can be used
 * to monitor the parsing progress, determine if the parser encountered errors,
 * or check if more data is needed to continue processing.
 *
 * The parser state is updated after each call to `iamf_obu_parser_parse` and
 * reflects the final state returned by the OBU processing callback function.
 * The state transitions are managed by the parser based on the processing
 * results of individual OBUs and the callback return values.
 *
 * @param parser Pointer to the IAMF OBU parser instance.
 * @return iamf_parser_state_t The current state of the parser.
 */
iamf_parser_state_t iamf_obu_parser_get_state(iamf_obu_parser_t *parser);

#ifdef __cplusplus
}
#endif

#endif  // __IAMF_OBU_PARSER_H__
