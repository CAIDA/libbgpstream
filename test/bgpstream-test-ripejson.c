/*
 * Copyright (C) 2015 The Regents of the University of California.
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
 */

#include "bgpstream.h"
#include "bgpstream_test.h"
#include "bgpstream-test-ripejson.h"
#include "utils.h"
#include <stdio.h>
#include <string.h>

#define BUFFER_LEN 1024
char buffer[BUFFER_LEN];

bgpstream_t *bs;
bgpstream_record_t *rec;
bgpstream_elem_t *elem;
static bgpstream_data_interface_id_t di_id = 0;

static char buf[65536];
static int print_record(bgpstream_record_t *record)
{
  if (bgpstream_record_snprintf(buf, sizeof(buf), record) == NULL) {
    fprintf(stderr, "ERROR: Could not convert record to string\n");
    return -1;
  }

  printf("%s\n", buf);
  return 0;
}

int test_bgpstream_ripejson(){
  /* Declare BGPStream requirements */
  int rrc = 0, counter = 0, erc = 0;
  bgpstream_elem_t *bs_elem;
  bgpstream_t *bs = bgpstream_create();
  bgpstream_record_t *rec = NULL;
  SETUP;
  CHECK_SET_INTERFACE(singlefile);
  bgpstream_data_interface_option_t *option;
  if ((option = bgpstream_get_data_interface_option_by_name(bs, di_id, "upd-type"))==NULL){
    return -1;
  }
  if (bgpstream_set_data_interface_option(bs, option, "ripejson") != 0) {
    return -1;
  }
  if ((option = bgpstream_get_data_interface_option_by_name(bs, di_id, "upd-file"))==NULL){
    return -1;
  }
  if (bgpstream_set_data_interface_option(bs, option, "ris-live-stream.json") != 0) {
    return -1;
  }

  /* turn on interface */
  if (bgpstream_start(bs) < 0) {
    return -1;
  }

  int count = 0;
  while ((rrc = bgpstream_get_next_record(bs, &rec)) > 0) {
    if (rec->status != BGPSTREAM_RECORD_STATUS_VALID_RECORD) {
      fprintf(stderr, "ERC=%d; ERROR!\n", erc);
      fprintf(stderr, "\t%s\n", buf);
      return -1;
    }
    if (rec->status == BGPSTREAM_RECORD_STATUS_VALID_RECORD) {
      while ((erc = bgpstream_record_get_next_elem(rec, &elem)) > 0) {

        if (bgpstream_record_elem_snprintf(buf, sizeof(buf), rec, elem) == NULL) {
          fprintf(stderr, "ERROR: Could not convert record/elem to string\n");
          return -1;
        }

        if (strcmp(buf, valid_output[count]) != 0) {
          // Strings are identical
            fprintf(stderr, "DIFF: %d; RECORD_STATUS: %d\n", count, rec->status);
            fprintf(stderr, "\t%s\n", buf);
            fprintf(stderr, "\t%s\n", valid_output[count]);
          goto err;
        }
        fprintf(stderr, "DONE: %d; RECORD_STATUS: %d\n", count, rec->status);
        fprintf(stderr, "\t%s\n", buf);
        fprintf(stderr, "\t%s\n", valid_output[count]);
        count++;
        buf[0]='\0';
        // if(count>=6 ){
        //   return -1;
        //   goto end;
        // }
      }
    }
  }

 end:
  return 0;
 err:
  return -1;
}

int main()
{
  return test_bgpstream_ripejson();
}
