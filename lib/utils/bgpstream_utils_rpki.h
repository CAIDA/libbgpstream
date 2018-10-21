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

#include "bgpstream_elem.h"
#include <roafetchlib/roafetchlib.h>
#include <stdint.h>

#define RPKI_CMD_CNT 2048
#define RPKI_SSH_BUFLEN 2048
#define RPKI_INTERVAL_LEN 22

/** A BGPStream RPKI Input object */
typedef struct bgpstream_rpki_input {

  /** RPKI interval
   *
   * RPKI time interval for the validation
   */
  char rpki_interval[RPKI_INTERVAL_LEN];

  /** RPKI collectors
   *
   * RPKI collectors
   */
  char rpki_collectors[RPKI_CMD_CNT];

  /** RPKI SSH arguments
   *
   * RPKI SSH arguments to connect to a cache server via SSH
   */
  char rpki_ssh[RPKI_SSH_BUFLEN];

  /** RPKI SSH arguments (pointer)
   *
   * Pointer to the RPKI SSH arguments
   */
  char *rpki_ssh_ptr;

  /** RPKI mode
   *
   * Mode of the validation - historical(0) or live(1)
   */
  int rpki_live;

  /** RPKI unified
   *
   * Whether the validation is separate(0) or unified(1)
   */
  int rpki_unified;

  /** RPKI active
   *
   * Whether the RPKI support is active
   */
  int rpki_active;

} bgpstream_rpki_input_t;

/** Public Functions */

/** Create a BGPStream RPKI input struct instance
 *
 * @return             Pointer to the BGPStream RPKI input struct
 */
bgpstream_rpki_input_t *bgpstream_rpki_create_input();

/** Destroy a BGPStream RPKI input struct instance
 *
 * @param input        Pointer to the BGPStream RPKI input struct
 */
void bgpstream_rpki_destroy_input(bgpstream_rpki_input_t *input);

/** Parse the BGPStream RPKI window argument
 *
 * @param input          Pointer to the BGPStream RPKI input struct
 * @param interval_start Start of the time interval
 * @param interval_end   End of the time interval
 * @return               1 if the parsing process was valid - 0 otherwise
 */
int bgpstream_rpki_parse_interval(bgpstream_rpki_input_t *input, 
                                  uint32_t interval_start, 
                                  uint32_t interval_end);

/** Add the mode argument to the input struct and set RPKI active
 *
 * @param inp          Pointer to the BGPStream RPKI input struct
 */
void bgpstream_rpki_parse_live(bgpstream_rpki_input_t *inp);

/** Add the unified argument to the input struct
 *
 * @param inp          Pointer to the BGPStream RPKI input struct
 */
void bgpstream_rpki_parse_unified(bgpstream_rpki_input_t *inp);

/** Add the SSH arguments to the input struct
 *
 * @param optarg       Pointer to the arguments buffer
 * @param inp          Pointer to the BGPStream RPKI input struct
 */
void bgpstream_rpki_parse_ssh(char *optarg, bgpstream_rpki_input_t *inp);

/** Add the collectors arguments to the input struct
 *
 * @param optarg       Pointer to the arguments buffer
 * @param inp          Pointer to the BGPStream RPKI input struct
 */
void bgpstream_rpki_parse_collectors(char *optarg, bgpstream_rpki_input_t *inp);

/** If the default mode is active, set RPKI active without a specific collector
 *
 * @param inp          Pointer to the BGPStream RPKI input struct
 */
void bgpstream_rpki_parse_default(bgpstream_rpki_input_t *inp);

/** Set up the ROAFetchlib configuration
 *
 * @param input        Pointer to the BGPStream RPKI input struct
 * @return             Pointer to the ROAFetchlib configuration
 */
rpki_cfg_t *bgpstream_rpki_set_cfg(bgpstream_rpki_input_t *input);

/** Destroy the ROAFetchlib configuration
 *
 * @param cfg          Pointer to the ROAFetchlib configuration
 */
void bgpstream_rpki_destroy_cfg(rpki_cfg_t *cfg);

/** Validate a BGP elem with the ROAFetchlib if the Annoucement contains an
 * unique origin AS
 *
 * @param elem         Pointer to a BGPStream Elem instance
 * @param result       Pointer to buffer for the validation result
 * @param size         Size of the validation result buffer
 * @return             1 if the validation process was valid - 0 otherwise
 */
int bgpstream_rpki_validate(bgpstream_elem_t const *elem, char *result,
                            size_t size);

#endif /* __BGPSTREAM_UTILS_RPKI_H */
