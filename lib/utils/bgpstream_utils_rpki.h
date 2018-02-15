/*
 * Copyright (C) 2014 The Regents of the University of California.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Authors:
 *   Samir Al-Sheikh (s.al-sheikh@fu-berlin.de)
 */

#ifndef __BGPSTREAM_UTILS_RPKI_H
#define __BGPSTREAM_UTILS_RPKI_H

#ifdef WITH_RPKI
#include "bgpstream_elem_int.h"
#include <inttypes.h>
#include <roafetchlib/roafetchlib.h>
#include <stdint.h>

// Note the copy of the BGPStream - Window Command Count !!
#define WINDOW_CMD_CNT 1024

#define RPKI_CMD_CNT 2048
#define RPKI_SSH_BUFLEN 2048
#define RPKI_WINDOW_LEN 24
#define RPKI_SSH_CNT 3

/** A BGPStream RPKI Window object */
typedef struct rpki_window {
  uint32_t start;
  uint32_t end;
} rpki_window_t;

/** A BGPStream RPKI Input object */
typedef struct bgpstream_rpki_input {

  /** RPKI projects
   *
   * RPKI projects
   */
  char rpki_projects[RPKI_CMD_CNT];

  /** RPKI collectors
   *
   * RPKI collectors
   */
  char rpki_collectors[RPKI_CMD_CNT];

  /** RPKI arguments
   *
   * RPKI arguments to connect to a cache server via SSH
   */
  char rpki_ssh_arg[RPKI_SSH_BUFLEN];

  /** RPKI windows
   *
   * RPKI time interval windows for the validation
   */
  char rpki_windows[WINDOW_CMD_CNT * RPKI_WINDOW_LEN];

  /** RPKI historical
   *
   * Mode of the validation - live(0) or historical(1)
   */
  int rpki_historical;

  /** RPKI unified
   *
   * Whether the validation is distinct(0) or unified(1)
   */
  int rpki_unified;

  /** RPKI SSH
   *
   * Whether the connection is ssh-based(1) or not(0)
   */
  int rpki_ssh;

  /** RPKI validation status
   *
   * Number of RPKI-related arguments
   */
  int rpki_options_cnt;

  /** RPKI active
   *
   * If the RPKI support is active
   */
  int rpki_active;

  /** RPKI parse pointer
   *
   * Temporarily RPKI parse pointer
   */
  char *rpki_ptr;

  /** RPKI check
   *
   * Checks whether all binary-based arguments are valid
   */
  char *rpki_check;

  /** RPKI args check
   *
   * Checks whether all RPKI-based arguments are valid
   */
  int rpki_args_check;

} bgpstream_rpki_input_t;


/** Public Functions */

/** Create a BGPStream RPKI input struct instance
 *
 * @return                Pointer to the BGPStream RPKI input struct
 */
bgpstream_rpki_input_t *bgpstream_rpki_create_input();

/** Destroy a BGPStream RPKI input struct instance
 *
 * @param input           Pointer to the BGPStream RPKI input struct
 */
void bgpstream_rpki_destroy_input(bgpstream_rpki_input_t *input);

/** Parse all RPKI-related input arguments
 *
 * @param optarg          Pointer to the arguments buffer
 * @return                Pointer to the BGPStream RPKI input struct
 */
bgpstream_rpki_input_t *bgpstream_rpki_parse_input(char *optarg);

/** Parse all BGPStream RPKI window arguments
 *
 * @param input           Pointer to the BGPStream RPKI input struct
 * @param windows         Array of structs of type rpki_window_t
 * @param windows_cnt     Number of BGPStream RPKI interval windows
 * @return                Pointer to the BGPStream RPKI input struct
 */
bgpstream_rpki_input_t *
bgpstream_rpki_parse_windows(bgpstream_rpki_input_t *input,
                             rpki_window_t windows[WINDOW_CMD_CNT],
                             int windows_cnt);

/** Set up the ROAFetchlib configuration
 *
 * @param input           Pointer to the BGPStream RPKI input struct
 * @return                Pointer to the ROAFetchlib configuration
 */
rpki_cfg_t *bgpstream_rpki_set_cfg(bgpstream_rpki_input_t *input);

/** Validate a BGP elem with the ROAFetchlib if the Annoucement contains an
 * unique origin AS
 *
 * @param elem            Pointer to a BGPStream Elem instance
 * @param result          Pointer to buffer for the validation result
 * @param size            Size of the validation result buffer
 * @return                1 if the validation process was valid - 0 otherwise
 */
int bgpstream_rpki_validate(bgpstream_elem_t const *elem, char *result,
                            size_t size);

#endif
#endif /* __BGPSTREAM_UTILS_H */
