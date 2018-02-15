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
#include "bgpstream_utils.h"
#include "bgpstream_constants.h"
#include "bgpstream_utils_rpki.h"

#define MIN_OPT_CNT 4
#define MIN_SSH_CNT 7

#ifdef WITH_RPKI

bgpstream_rpki_input_t *bgpstream_rpki_create_input() {

  /* Create input */
  bgpstream_rpki_input_t *input = NULL;
  size_t input_size = sizeof(bgpstream_rpki_input_t);
  if ((input = (bgpstream_rpki_input_t *)malloc(input_size)) == NULL) {
    return NULL;
  } else {
    memset(input, 0, input_size);
  }

  /* Initializing all members */
  memset(input->rpki_projects, 0, sizeof(input->rpki_projects));
  memset(input->rpki_collectors, 0, sizeof(input->rpki_collectors));
  memset(input->rpki_ssh_arg, 0, sizeof(input->rpki_ssh_arg));
  memset(input->rpki_windows, 0, sizeof(input->rpki_windows));
  input->rpki_historical = 0;
  input->rpki_unified = 0;
  input->rpki_ssh = 0;
  input->rpki_options_cnt = 0;
  input->rpki_active = 0;
  input->rpki_ptr = NULL;
  input->rpki_check = NULL;

  return input;
}

void bgpstream_rpki_destroy_input(bgpstream_rpki_input_t *input) {

  /* Destroy input if necessary */
  if (input == NULL) {
    return;
  }
  free(input);
}

bgpstream_rpki_input_t *bgpstream_rpki_parse_input(char *optarg) {

  /* Create input */
  bgpstream_rpki_input_t *inp = bgpstream_rpki_create_input();

  char* optarg_dup = strdup(optarg);

  /* Check if enough RPKI arguments are present before parsing */
  int count_options = 0;
  const char *tmp = optarg_dup;
  while(tmp = strstr(tmp, ",")) { count_options++; tmp++; }
  if(count_options < MIN_OPT_CNT) {
    fprintf(stderr, "ERROR:  Invalid RPKI options\n");
    fprintf(stderr, " * FORMAT: historical,unified,ssh,project,collector\n");
    inp->rpki_args_check = 0;
    return inp;
  }


  /* Parse all binary based input arguments */
  inp->rpki_historical =
      strtol(inp->rpki_check = strtok(optarg, ","), &(inp->rpki_ptr), 10);
  inp->rpki_historical =
      (inp->rpki_ptr == inp->rpki_check ? -1 : inp->rpki_historical);
  inp->rpki_unified =
      strtol(inp->rpki_check = strtok(NULL, ","), &(inp->rpki_ptr), 10);
  inp->rpki_unified =
      (inp->rpki_ptr == inp->rpki_check ? -1 : inp->rpki_unified);
  inp->rpki_ssh =
      strtol(inp->rpki_check = strtok(NULL, ","), &(inp->rpki_ptr), 10);
  inp->rpki_ssh = (inp->rpki_ptr == inp->rpki_check ? -1 : inp->rpki_ssh);

  /* Check if all necessary arguments are valid */
  if ((inp->rpki_historical != 0 && inp->rpki_historical != 1) ||
      (inp->rpki_unified != 0 && inp->rpki_unified != 1) ||
      (inp->rpki_ssh != 0 && inp->rpki_ssh != 1)) {
    fprintf(stderr, "ERROR: Invalid RPKI options\n");
    inp->rpki_args_check = 0;
    return inp;
  }
  
  /* Check if enough SSH arguments are present before parsing */
  if(inp->rpki_ssh == 1) {
    int count_ssh_options = 0;
    tmp = optarg_dup;
    while(tmp = strstr(tmp, ",")) { count_ssh_options++; tmp++; }
    if(count_ssh_options < MIN_SSH_CNT) {
      fprintf(stderr, "ERROR:  Invalid RPKI SSH options\n");
      fprintf(stderr, " * FORMAT: user,hostkey,private_key,project,collector\n");
      inp->rpki_args_check = 0;
      return inp;
    }
    free(optarg_dup);
  }

  /* Parse all SSH related input arguments */
  if (inp->rpki_ssh && !(inp->rpki_historical)) {
    for (int i = 0; i < RPKI_SSH_CNT; i++) {
      strncat(inp->rpki_ssh_arg, !strlen(inp->rpki_ssh_arg) ? "" : ",",
              sizeof(inp->rpki_ssh_arg) - 1);
      strncat(inp->rpki_ssh_arg, strtok(NULL, ","),
              sizeof(inp->rpki_ssh_arg) - 1);
    }
  }

  /* Parse all project and collector arguments */
  while ((inp->rpki_ptr = strtok(NULL, ",")) != NULL) {
    if ((inp->rpki_options_cnt)++ % 2) {
      strncat(inp->rpki_collectors, !strlen(inp->rpki_collectors) ? "" : ",",
              sizeof(inp->rpki_collectors) - 1);
      strncat(inp->rpki_collectors, inp->rpki_ptr, RPKI_CMD_CNT - 1);
    } else {
      strncat(inp->rpki_projects, !strlen(inp->rpki_projects) ? "" : ",",
              sizeof(inp->rpki_projects) - 1);
      strncat(inp->rpki_projects, inp->rpki_ptr, RPKI_CMD_CNT - 1);
    }
  }

  inp->rpki_active = 1;
  inp->rpki_args_check = 1;

  return inp;
}

bgpstream_rpki_input_t *
bgpstream_rpki_parse_windows(bgpstream_rpki_input_t *input,
                             rpki_window_t windows[WINDOW_CMD_CNT],
                             int windows_cnt) {

  /* Parse all BGPReader window arguments */
  char rpki_window[RPKI_WINDOW_LEN];
  for (int i = 0; i < windows_cnt; i++) {
    snprintf(rpki_window, sizeof(rpki_window), "%" PRIu32 "-%" PRIu32 ",",
             windows[i].start, windows[i].end);
    strcat(input->rpki_windows, rpki_window);
  }
  input->rpki_windows[strlen(input->rpki_windows) - 1] = '\0';

  return input;
}

rpki_cfg_t *bgpstream_rpki_set_cfg(bgpstream_rpki_input_t *inp) {

  /* Set up the ROAFetchlib configuration */
  rpki_cfg_t *cfg = NULL;

  return rpki_set_config(
      inp->rpki_projects, inp->rpki_collectors, inp->rpki_windows,
      inp->rpki_unified, inp->rpki_historical, RPKI_BROKER,
      (inp->rpki_ssh && !inp->rpki_historical) ? inp->rpki_ssh_arg : NULL);
}

int bgpstream_rpki_validate(bgpstream_elem_t const *elem, char *result,
                            size_t size) {

  /* Validate a BGP elem with the ROAFetchlib */
  int val_rst = 0;
  char prefix[INET6_ADDRSTRLEN];
  bgpstream_addr_ntop(prefix, INET6_ADDRSTRLEN,
                      &(((bgpstream_pfx_t *)&(elem->prefix))->address));
  uint32_t asn = 0;

  if (!bgpstream_as_path_get_origin_val(elem->as_path, &asn)) {
    if (!rpki_validate(elem->annotations.cfg, elem->annotations.timestamp, asn,
                       prefix, elem->prefix.mask_len, result, size)) {
      val_rst = 1;
    }
  }

  return val_rst;
}

#endif
