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
#include "utils.h"
#include "bgpstream-test-rpki.h"
#include "bgpstream_test.h"
#include "bgpstream_utils_rpki.h"

bgpstream_t *bs;
bgpstream_data_interface_id_t di_id = 0;

static int val_compare(const void *cmp1, const void *cmp2) {

  /* Compare string for the sorting process */
  return strcmp(*(const char **)cmp1, *(const char **)cmp2);
}

void sort(char *buf[], int n) {

  /* Sort the validation results */
  qsort(buf, n, sizeof(const char *), val_compare);
}

int split_result(char *val_result, char **val_result_buf) {

  /* Split the validation result into a string array */
  int cnt = 0;
  val_result_buf[cnt] = strtok(val_result, ";");
  while (val_result_buf[cnt] != NULL) {
    val_result_buf[++cnt] = strtok(NULL, ";");
  }
  return cnt;
}

int check_val_result(char *val_result, char *comp) {

  /* Check if the validation are equal regardless of the order */
  char *val_result_buf[1024];
  char *comp_buf[1024];
  int val_cnt = 0, comp_cnt = 0, check = 0;
  sort(val_result_buf, val_cnt = split_result(val_result, val_result_buf));
  sort(comp_buf, comp_cnt = split_result(comp, comp_buf));
  for (int i = 0; i < val_cnt; i++) {
    check += strcmp(val_result_buf[i], comp_buf[i]);
  }
  CHECK("Check Validation Result", !check);
  return 0;
}

int generate_rpki_windows(rpki_window_t *rpki_windows, const int *testcase,
                          int cnt) {

  /* Generate a RPKI Window Instance */
  int j = 0;
  for (int i = 0; i < cnt; i += 2) {
    rpki_windows[j].start = testcase[i];
    rpki_windows[j].end = testcase[i + 1];
    j++;
  }
  return j;
}

int test_rpki_create_input() {

  bgpstream_rpki_input_t *input = bgpstream_rpki_create_input();
  CHECK("Create Input", input != NULL);

  CHECK("Project/Collector Input",
        input->rpki_projects != NULL && input->rpki_collectors != NULL);
  CHECK("SSH/Window Input",
        input->rpki_ssh_arg != NULL && input->rpki_windows != NULL);
  CHECK("Binary Input",
        !input->rpki_historical && !input->rpki_unified && !input->rpki_ssh);
  CHECK("Meta Input", !input->rpki_options_cnt && !input->rpki_active &&
                          input->rpki_ptr == NULL && input->rpki_check == NULL);
  return 0;
}

int test_rpki_parse_input() {

  char *test_input = (char *)malloc(PARSING_SIZE * sizeof(char));

  /* Check Binary Input */
  snprintf(test_input, PARSING_SIZE, "%s", PARSING_BIN_TESTCASE_1);
  bgpstream_rpki_input_t *input = bgpstream_rpki_parse_input(test_input);
  CHECK("Parsing Bin Input #1",
        input->rpki_active == 1 && input->rpki_args_check == 1);
  snprintf(test_input, PARSING_SIZE, "%s", PARSING_BIN_TESTCASE_2);
  input = bgpstream_rpki_parse_input(test_input);
  CHECK("Parsing Bin Input #2",
        input->rpki_active == 1 && input->rpki_args_check == 1);
  snprintf(test_input, PARSING_SIZE, "%s", PARSING_BIN_TESTCASE_3);
  input = bgpstream_rpki_parse_input(test_input);
  CHECK("Parsing Bin Input #3",
        input->rpki_active == 1 && input->rpki_args_check == 1);

  PRINT_START;
  snprintf(test_input, PARSING_SIZE, "%s", PARSING_BIN_TESTCASE_4);
  input = bgpstream_rpki_parse_input(test_input);
  CHECK("Parsing Bin Input #4", input->rpki_args_check == 0);
  PRINT_END;

  PRINT_MID;

  /* Check SSH Input */
  snprintf(test_input, PARSING_SIZE, "%s", PARSING_SSH_TESTCASE_1);
  input = bgpstream_rpki_parse_input(test_input);
  CHECK("Parsing SSH Input #1", input->rpki_args_check == 1);

  PRINT_START;
  snprintf(test_input, PARSING_SIZE, "%s", PARSING_SSH_TESTCASE_2);
  input = bgpstream_rpki_parse_input(test_input);
  CHECK("Parsing SSH Input #2", input->rpki_args_check == 0);
  PRINT_END;

  PRINT_MID;

  /* Check Project and Collector Input */
  snprintf(test_input, PARSING_SIZE, "%s", PARSING_PCC_TESTCASE_1);
  input = bgpstream_rpki_parse_input(test_input);
  CHECK("Parsing PC Input #1",
        input->rpki_active == 1 && input->rpki_args_check == 1);
  snprintf(test_input, PARSING_SIZE, "%s", PARSING_PCC_TESTCASE_2);
  input = bgpstream_rpki_parse_input(test_input);
  CHECK("Parsing PC Input #2",
        input->rpki_active == 1 && input->rpki_args_check == 1);
  return 0;
}

int test_rpki_parse_windows() {

  /* Check RPKI Window Input */
  struct rpki_window rpki_windows[WINDOW_CMD_CNT];
  int cnt = ELEMS(PARSING_WND_TESTCASE_1);
  int j = generate_rpki_windows(rpki_windows, PARSING_WND_TESTCASE_1, cnt);
  bgpstream_rpki_input_t *input = bgpstream_rpki_create_input();
  input = bgpstream_rpki_parse_windows(input, rpki_windows, j);
  CHECK("Parsing Window Input #1",
        !strcmp(input->rpki_windows, PARSING_WND_TESTCASE_1_RST));

  cnt = ELEMS(PARSING_WND_TESTCASE_2);
  j = generate_rpki_windows(rpki_windows, PARSING_WND_TESTCASE_2, cnt);
  input = bgpstream_rpki_parse_windows(bgpstream_rpki_create_input(),
                                       rpki_windows, j);
  CHECK("Parsing Window Input #2",
        !strcmp(input->rpki_windows, PARSING_WND_TESTCASE_2_RST));
  return 0;
}

int test_rpki_validate() {

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
  char *test_input = (char *)malloc(PARSING_SIZE * sizeof(char));
  snprintf(test_input, PARSING_SIZE, "%s", VALIDATE_TESTCASE_1);
  bgpstream_rpki_input_t *input = bgpstream_rpki_parse_input(test_input);

  /* Create a RPKI window instance matching the testfile */
  int VALIDATE_WND[2] = {1427846400, 1427846500};
  struct rpki_window rpki_windows[WINDOW_CMD_CNT];
  int j =
      generate_rpki_windows(rpki_windows, VALIDATE_WND, ELEMS(VALIDATE_WND));
  input = bgpstream_rpki_parse_windows(input, rpki_windows, j);
  bgpstream_add_interval_filter(bs, rpki_windows[0].start, rpki_windows[0].end);

  /* Set up a ROAFetchlib configuration */
  rpki_cfg_t *cfg = bgpstream_rpki_set_cfg(input);
  char val_result[VALIDATION_BUF];
  char val_comp[VALIDATION_BUF];

  /* Process every BGPStream elem and check the validation */
  while ((rrc = bgpstream_get_next_record(bs, &rec)) > 0) {
    if (rec->status == BGPSTREAM_RECORD_STATUS_VALID_RECORD) {
      while ((erc = bgpstream_record_get_next_elem(rec, &bs_elem)) > 0) {
        if (bs_elem->type == BGPSTREAM_ELEM_TYPE_ANNOUNCEMENT) {
          bs_elem->annotations.cfg = cfg;
          bs_elem->annotations.rpki_active = input->rpki_active;
          bs_elem->annotations.timestamp = rec->time_sec;
          bgpstream_rpki_validate(bs_elem, val_result, sizeof(val_result));
          snprintf(val_comp, sizeof(val_comp), "%s",
                   VALIDATE_TESTCASE_RST2[counter]);
          check_val_result(val_result, val_comp);
          counter++;
        }
      }
    }
  }
  return 0;
}

int main() {
#ifdef WITH_RPKI
  CHECK_SECTION("RPKI Input", !test_rpki_create_input());
  CHECK_SECTION("RPKI Parsing", !test_rpki_parse_input());
  CHECK_SECTION("RPKI Window Parsing", !test_rpki_parse_windows());
  CHECK_SECTION("RPKI Validation", !test_rpki_validate());
#endif
  return 0;
}
