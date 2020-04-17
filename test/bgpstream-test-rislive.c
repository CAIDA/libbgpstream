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
#include "utils.h"
#include <stdio.h>
#include <string.h>

#define SETUP                                                                  \
  do {                                                                         \
    bs = bgpstream_create();                                                   \
  } while (0)

#define CHECK_SET_INTERFACE(interface)                                         \
  do {                                                                         \
    di_id = bgpstream_get_data_interface_id_by_name(bs, STR(interface));       \
    bgpstream_set_data_interface(bs, di_id);                                   \
  } while (0)

#define N_RECORDS 7

static const char *valid_output[] = {
  "U|A|1553627987.890000|singlefile|rrc00|||11708|72.22.223.9|45.161.192.0/23|72.22.223.9|11708 32097 1299 52320 263009 263009 263009 263009 263009 52993 268481 268481|268481|||",
  "U|S|1553625081.880000|singlefile|rrc01|||24931|195.66.224.59|||||||IDLE", // ris_peer_state
  "", // ris-live server-side error message
};

static char buf[65536];

/*
 * 1. update
 * 2. open
 * 3. notification
 * 4. keepalive
 * 5. ris_peer_state
 * 6. unsupported (change keepalive message's type to an unsupported one)
 * 7. corrupted (change keepalive messages's raw bytes)
 */
static int test_bgpstream_rislive()
{
  /* Declare BGPStream requirements */
  int rrc = 0, count = 0, rcount = 0, erc = 0;
  bgpstream_t *bs = bgpstream_create();
  bgpstream_elem_t *elem;
  bgpstream_record_t *rec = NULL;
  static bgpstream_data_interface_id_t di_id = 0;

  SETUP;
  CHECK_SET_INTERFACE(singlefile);
  bgpstream_data_interface_option_t *option;

  if ((option = bgpstream_get_data_interface_option_by_name(
         bs, di_id, "upd-type")) == NULL) {
    return -1;
  }
  if (bgpstream_set_data_interface_option(bs, option, "ris-live") != 0) {
    return -1;
  }
  if ((option = bgpstream_get_data_interface_option_by_name(
         bs, di_id, "upd-file")) == NULL) {
    return -1;
  }
  if (bgpstream_set_data_interface_option(bs, option, "ris-live-stream.json") !=
      0) {
    return -1;
  }

  /* turn on interface */
  if (bgpstream_start(bs) < 0) {
    return -1;
  }

  while ((rrc = bgpstream_get_next_record(bs, &rec)) > 0) {
    fprintf(stderr, "checking entry %d\n", rcount);
    switch (rec->status) {
    case BGPSTREAM_RECORD_STATUS_VALID_RECORD:

      while ((erc = bgpstream_record_get_next_elem(rec, &elem)) > 0) {

        if (bgpstream_record_elem_snprintf(buf, sizeof(buf), rec, elem) ==
            NULL) {
          fprintf(stderr, "ERROR: Could not convert record/elem to string\n");
          return -1;
        }

        if (strcmp(buf, valid_output[rcount]) != 0) {
          // Strings are not identical
          fprintf(stderr, "elem output different, rcount %d, count %d\n",
                  rcount, count);
          fprintf(stderr, "INVALID: %s\nCORRECT: %s\n", buf,
                  valid_output[rcount]);
          goto err;
        }
        fprintf(stderr, "VALID: %s\n", buf);
        count++;
        buf[0] = '\0';
      }
      fprintf(stderr, "correctly valid record %d\n\n", rcount);
      break;

    case BGPSTREAM_RECORD_STATUS_UNSUPPORTED_RECORD:
      if (rcount < 3) {
        // first three records should be supported
        fprintf(stderr, "record %d shouldn't be unsupported\n", rcount);
        goto err;
      }
      fprintf(stderr, "correctly unsupported record %d\n\n", rcount);
      break;

    case BGPSTREAM_RECORD_STATUS_CORRUPTED_RECORD:
      if (rcount < 6) {
        fprintf(stderr, "record %d shouldn't be corrupted\n", rcount);
        goto err;
      }
      fprintf(stderr, "correctly corrupted record %d\n", rcount);
      break;

    default:
      goto err;
      break;
    }

    // record test correctly passed
    rcount++;
  }

  if (rcount != N_RECORDS) {
    // if not all records passed
    fprintf(stderr, "there should be %d records, processed only %d records\n", N_RECORDS, rcount);
    return -1;
  }

  return 0;
err:
  return -1;
}

int main()
{
  int rc = test_bgpstream_rislive();
  ENDTEST;
  return rc;
}
