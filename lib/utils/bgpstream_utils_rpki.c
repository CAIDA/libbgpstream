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

#include <stdio.h>
#include <string.h>
#include "config.h"
#include "utils.h"
#include "bgpstream_utils_rpki.h"

bgpstream_rpki_input_t *bgpstream_rpki_create_input()
{
  /* Create a BGPStream RPKI input struct instance */
  bgpstream_rpki_input_t *input = NULL;
  size_t input_size = sizeof(bgpstream_rpki_input_t);
  if ((input = (bgpstream_rpki_input_t *)malloc_zero(input_size)) == NULL) {
    return NULL;
  }

  return input;
}

void bgpstream_rpki_destroy_input(bgpstream_rpki_input_t *input)
{
  /* Destroy a BGPStream RPKI input struct instance if necessary */
  if (input == NULL) {
    return;
  }
  free(input);
}

rpki_cfg_t *bgpstream_rpki_set_cfg(bgpstream_rpki_input_t *inp)
{
  /* Set up the ROAFetchlib configuration */
  return rpki_set_config(inp->rpki_collectors, inp->rpki_windows,
      inp->rpki_unified, !inp->rpki_live, RPKI_BROKER, inp->rpki_ssh_ptr);
}

void bgpstream_rpki_destroy_cfg(rpki_cfg_t *cfg)
{
  /* Destroy the ROAFetchlib configuration if necessary */
  if (cfg == NULL) {
    return;
  }
  rpki_destroy_config(cfg);
}

int bgpstream_rpki_parse_windows(bgpstream_rpki_input_t *input,
                                 rpki_window_t windows[WINDOW_CMD_CNT],
                                 int windows_cnt)
{
  /* Parse all BGPReader window arguments (assuming a unix timestamp len of 10)
     Format: WD_1-WD2,WD3-WD4, chk_len calculates the string length */
  int rst = 0;
  int chk_len = (windows_cnt * 2) * 10 + (2 * windows_cnt - 1);
  for (int i = 0; i < windows_cnt; i++) {
    size_t size = strlen(input->rpki_windows);
    rst+= snprintf(input->rpki_windows + size, sizeof(input->rpki_windows)
                       - size, i < windows_cnt - 1 ? "%"PRIu32 "-%"PRIu32 "," :
                       "%"PRIu32 "-%"PRIu32, windows[i].start, windows[i].end);
  }

  return rst == chk_len;
}

void bgpstream_rpki_parse_live(bgpstream_rpki_input_t *inp)
{
  /* Add the mode parameter to the input struct and set RPKI active */
  inp->rpki_active = 1;
  inp->rpki_live = 1;
}

void bgpstream_rpki_parse_unified(bgpstream_rpki_input_t *inp)
{
  /* Add the unified parameter to the input struct */
  inp->rpki_unified = 1;
}

void bgpstream_rpki_parse_ssh(char *optarg, bgpstream_rpki_input_t *inp)
{
  /* Add the SSH parameters to the input struct */
  inp->rpki_ssh_ptr = inp->rpki_ssh;
  snprintf(inp->rpki_ssh_ptr, sizeof(inp->rpki_ssh), "%s", optarg);
}

void bgpstream_rpki_parse_collectors(char *optarg, bgpstream_rpki_input_t *inp)
{
  /* Add the collectors parameter to the input struct and set RPKI active */
  inp->rpki_active = 1;
  snprintf(inp->rpki_collectors, sizeof(inp->rpki_collectors), "%s", optarg);
}

void bgpstream_rpki_parse_default(bgpstream_rpki_input_t *inp)
{
  /* If the default mode is active, set RPKI active without a spec. collector */
  inp->rpki_active = 1;
}

int bgpstream_rpki_validate(bgpstream_elem_t const *elem, char *result,
                            size_t size)
{
  /* Validate a BGP elem with the ROAFetchlib */
  int val_rst = 0;
  char prefix[INET6_ADDRSTRLEN];
  bgpstream_addr_ntop(prefix, INET6_ADDRSTRLEN,
                      &(((bgpstream_pfx_t *)&(elem->prefix))->address));
  uint32_t asn = 0;

  /* Validate the BGP elem only if the origin ASN is a simple ASN value
     (i.e. not a set). If the validation function of the ROAFetchlib
     returns 0 -> a valid result (val_rst = 1) is available */
  if (!bgpstream_as_path_get_origin_val(elem->as_path, &asn)) {
    if (!rpki_validate(elem->annotations.cfg, elem->annotations.timestamp, asn,
                       prefix, elem->prefix.mask_len, result, size)) {
      val_rst = 1;
    }
  }

  return val_rst;
}
