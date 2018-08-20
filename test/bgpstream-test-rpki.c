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

#include "bgpstream-test-rpki.h"
#include "bgpstream_test.h"
#include "bgpstream_utils_rpki.h"
#include "utils.h"
#include <stdio.h>
#include <string.h>

bgpstream_t *bs;
bgpstream_data_interface_id_t di_id = 0;

int check_val_result(char *val_result, char *comp, int cnt)
{

  /* Check whether the validation results are equal */
  char test_num[VALIDATION_BUF] = {0};
  snprintf(test_num, sizeof(test_num), "Check Validation Result #%i", cnt);
  CHECK_RPKI_RESULT(test_num, !strcmp(val_result, comp));
  return strcmp(val_result, comp);
}

int generate_rpki_windows(rpki_window_t *rpki_windows, const int *testcase,
                          int cnt)
{

  /* Generate a RPKI Window Instance */
  int j = 0;
  for (int i = 0; i < cnt; i += 2) {
    rpki_windows[j].start = testcase[i];
    rpki_windows[j].end = testcase[i + 1];
    j++;
  }
  return j;
}

int test_rpki_create_input()
{

  bgpstream_rpki_input_t *input = bgpstream_rpki_create_input();
  CHECK_RPKI_RESULT("Create Input", input != NULL);
  CHECK_RPKI_RESULT("Collectors arguments", !strlen(input->rpki_collectors));
  CHECK_RPKI_RESULT("SSH arguments", !strlen(input->rpki_ssh));
  CHECK_RPKI_RESULT("Window arguments", !strlen(input->rpki_windows));
  CHECK_RPKI_RESULT("Live argument", !input->rpki_live && !input->rpki_unified);
  CHECK_RPKI_RESULT("Unified argument",
                    !input->rpki_live && !input->rpki_unified);
  CHECK_RPKI_RESULT("Meta flags",
                    !input->rpki_active && input->rpki_ssh_ptr == NULL);
  return 0;
}

int test_rpki_parse_input()
{

  /* Check live/historical mode argument */
  bgpstream_rpki_input_t *input = bgpstream_rpki_create_input();
  bgpstream_rpki_parse_live(input);
  CHECK_RPKI_RESULT("Parsing RPKI mode parameter",
                    input->rpki_active && input->rpki_live);
  bgpstream_rpki_destroy_input(input);

  /* Check unified argument */
  input = bgpstream_rpki_create_input();
  bgpstream_rpki_parse_unified(input);
  CHECK_RPKI_RESULT("Parsing RPKI unified parameter", input->rpki_unified == 1);
  bgpstream_rpki_destroy_input(input);

  /* Check SSH arguments */
  input = bgpstream_rpki_create_input();
  bgpstream_rpki_parse_ssh(PARSING_SSH_TESTCASE_1, input);
  CHECK_RPKI_RESULT("Parsing RPKI SSH arguments",
                    !strcmp(input->rpki_ssh, PARSING_SSH_TESTCASE_1) &&
                      input->rpki_ssh_ptr != NULL);
  bgpstream_rpki_destroy_input(input);

  /* Check collectors arguments */
  input = bgpstream_rpki_create_input();
  bgpstream_rpki_parse_collectors(PARSING_PCC_TESTCASE_1, input);
  CHECK_RPKI_RESULT("Parsing RPKI collectors arguments",
                    !strcmp(input->rpki_collectors, PARSING_PCC_TESTCASE_1) &&
                      input->rpki_active);
  bgpstream_rpki_destroy_input(input);

  /* Check default arguments */
  input = bgpstream_rpki_create_input();
  bgpstream_rpki_parse_default(input);
  CHECK_RPKI_RESULT("Parsing RPKI default parameter", input->rpki_active);
  bgpstream_rpki_destroy_input(input);

  return 0;
}

int test_rpki_parse_windows()
{

  /* Check RPKI Window Input Case 1 */
  struct rpki_window rpki_windows[WINDOW_CMD_CNT];
  int cnt = ARR_CNT(PARSING_WND_TESTCASE_1);
  int j = generate_rpki_windows(rpki_windows, PARSING_WND_TESTCASE_1, cnt);
  bgpstream_rpki_input_t *input = bgpstream_rpki_create_input();
  int rst = bgpstream_rpki_parse_windows(input, rpki_windows, j);
  CHECK_RPKI_RESULT("Parsing Window Input #1",
                    !strcmp(input->rpki_windows, PARSING_WND_TESTCASE_1_RST) &&
                      rst);
  bgpstream_rpki_destroy_input(input);

  /* Check RPKI Window Input Case 2 */
  input = bgpstream_rpki_create_input();
  cnt = ARR_CNT(PARSING_WND_TESTCASE_2);
  j = generate_rpki_windows(rpki_windows, PARSING_WND_TESTCASE_2, cnt);
  rst = bgpstream_rpki_parse_windows(input, rpki_windows, j);
  CHECK_RPKI_RESULT("Parsing Window Input #2",
                    !strcmp(input->rpki_windows, PARSING_WND_TESTCASE_2_RST) &&
                      rst);
  bgpstream_rpki_destroy_input(input);

  return 0;
}

int test_rpki_validate()
{

  /* Declare BGPStream requirements */
  int rrc = 0, counter = 0, erc = 0;
  bgpstream_elem_t *bs_elem;
  bgpstream_t *bs = bgpstream_create();
  bgpstream_record_t *rec = NULL;
  SETUP;
  CHECK_SET_INTERFACE(singlefile);
  bgpstream_data_interface_option_t *option;
  option = bgpstream_get_data_interface_option_by_name(bs, di_id, "upd-file");
  bgpstream_set_data_interface_option(bs, option,
                                      "ris.rrc06.updates.1427846400.gz");
  bgpstream_start(bs);

  /* Create an input instance for hitorical validation (CC01) */
  bgpstream_rpki_input_t *input = bgpstream_rpki_create_input();
  bgpstream_rpki_parse_collectors(VALIDATE_TESTCASE_1, input);

  /* Create a RPKI window instance matching the testfile */
  int VALIDATE_WND[2] = {1427846400, 1427846500};
  struct rpki_window rpki_windows[WINDOW_CMD_CNT];
  int j =
    generate_rpki_windows(rpki_windows, VALIDATE_WND, ARR_CNT(VALIDATE_WND));
  int rst = bgpstream_rpki_parse_windows(input, rpki_windows, j);
  bgpstream_add_interval_filter(bs, rpki_windows[0].start, rpki_windows[0].end);

  /* Set up a ROAFetchlib configuration */
  rpki_cfg_t *cfg = bgpstream_rpki_set_cfg(input);
  char val_result[VALIDATION_BUF];
  char val_comp[VALIDATION_BUF];

  /* Process every BGPStream elem and check the validation */
  j = 0;
  while ((rrc = bgpstream_get_next_record(bs, &rec)) > 0) {
    if (rec->status == BGPSTREAM_RECORD_STATUS_VALID_RECORD) {
      while ((erc = bgpstream_record_get_next_elem(rec, &bs_elem)) > 0) {
        if (bs_elem->type == BGPSTREAM_ELEM_TYPE_ANNOUNCEMENT) {
          bs_elem->annotations.cfg = cfg;
          bs_elem->annotations.rpki_active = input->rpki_active;
          bs_elem->annotations.timestamp = rec->time_sec;
          bgpstream_rpki_validate(bs_elem, val_result, sizeof(val_result));
          snprintf(val_comp, sizeof(val_comp), "%s",
                   VALIDATE_TESTCASE_1_RST[counter]);
          if (check_val_result(val_result, val_comp, j++) != 0) {
            return -1;
          }
          counter++;
        }
      }
    }
  }
  return 0;
}

int main()
{
#ifdef WITH_RPKI
  CHECK_RPKI_SECTION("RPKI Input", !test_rpki_create_input());
  CHECK_RPKI_SECTION("RPKI Parsing", !test_rpki_parse_input());
  CHECK_RPKI_SECTION("RPKI Window Parsing", !test_rpki_parse_windows());
  CHECK_RPKI_SECTION("RPKI Validation", !test_rpki_validate());
#endif
  return 0;
}
