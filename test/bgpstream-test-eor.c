/*
 * Copyright (C) 2014 The Regents of the University of California.
 *
 * This file is part of libbgpstream.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "bgpstream.h"
#include "bgpstream_test.h"
#include "utils.h"
#include <stdio.h>
#include <string.h>

/*
 * End-of-RIB detection test.
 *
 * The fixture eor-stream.json feeds a series of RIS Live UPDATE messages (as
 * raw on-the-wire BGP bytes) through the full decode pipeline and checks that
 * each is or is not classified as an RFC 4724 End-of-RIB marker. The raw bytes
 * were hand-built per RFC 4271 (UPDATE) / RFC 4760 (MP_(UN)REACH_NLRI) / RFC
 * 4724 (End-of-RIB) and then independently cross-checked with scapy's BGP
 * dissector, which shares no code with libbgpstream / libparsebgp.
 *
 * The set deliberately mixes genuine EoR markers with non-EoR messages that
 * sit close to the detection boundary (including the false-positive cases fixed
 * during review), to guard against both missed and spurious EoR classification:
 *
 *  0. empty IPv4 UPDATE                  -> EoR (0.0.0.0/0)
 *       FF*16 0017 02 0000 0000
 *  1. empty MP_UNREACH, IPv6 unicast     -> EoR (::/0)
 *       ... 800F 03 0002 01
 *  2. empty MP_UNREACH, IPv4 unicast     -> EoR (0.0.0.0/0)
 *       ... 800F 03 0001 01
 *  3. native IPv4 withdrawal 10.0.0.0/8  -> WITHDRAWAL (NOT EoR)
 *       a withdrawal-only UPDATE must not be mistaken for an EoR
 *  4. MP_UNREACH withdrawal 2001:db8::/32 -> WITHDRAWAL (NOT EoR)
 *       a non-empty MP_UNREACH must not be mistaken for an EoR
 *  5. empty MP_UNREACH, IPv6 multicast (SAFI=2) -> no elems (NOT EoR)
 *       EoR is restricted to the unicast SAFI
 *  6. empty MP_UNREACH unicast + extra ORIGIN attr -> no elems (NOT EoR)
 *       EoR requires MP_UNREACH to be the only path attribute
 *  7. normal IPv4 announcement 192.0.2.0/24 -> ANNOUNCEMENT (NOT EoR)
 *       a routine announcement must not be mistaken for an EoR
 */

#define MAX_ELEMS 2

typedef struct {
  const char *desc;
  int n_elems;
  bgpstream_elem_type_t types[MAX_ELEMS];
  const char *prefixes[MAX_ELEMS];
} expected_record_t;

static const expected_record_t expected[] = {
  { "empty IPv4 UPDATE -> EoR", 1,
    { BGPSTREAM_ELEM_TYPE_END_OF_RIB }, { "0.0.0.0/0" } },
  { "MP_UNREACH IPv6 unicast empty -> EoR", 1,
    { BGPSTREAM_ELEM_TYPE_END_OF_RIB }, { "::/0" } },
  { "MP_UNREACH IPv4 unicast empty -> EoR", 1,
    { BGPSTREAM_ELEM_TYPE_END_OF_RIB }, { "0.0.0.0/0" } },
  { "native IPv4 withdrawal -> W (not EoR)", 1,
    { BGPSTREAM_ELEM_TYPE_WITHDRAWAL }, { "10.0.0.0/8" } },
  { "MP_UNREACH IPv6 withdrawal -> W (not EoR)", 1,
    { BGPSTREAM_ELEM_TYPE_WITHDRAWAL }, { "2001:db8::/32" } },
  { "MP_UNREACH non-unicast -> no elems (not EoR)", 0, { 0 }, { NULL } },
  { "MP_UNREACH + extra attr -> no elems (not EoR)", 0, { 0 }, { NULL } },
  { "IPv4 announcement -> A (not EoR)", 1,
    { BGPSTREAM_ELEM_TYPE_ANNOUNCEMENT }, { "192.0.2.0/24" } },
};

#define N_RECORDS (int)(sizeof(expected) / sizeof(expected[0]))

static char buf[1024];

static int test_eor(void)
{
  int rrc, erc, rcount = 0;
  bgpstream_t *bs = bgpstream_create();
  bgpstream_elem_t *elem;
  bgpstream_record_t *rec = NULL;
  bgpstream_data_interface_id_t di_id;
  bgpstream_data_interface_option_t *option;

  di_id = bgpstream_get_data_interface_id_by_name(bs, "singlefile");
  bgpstream_set_data_interface(bs, di_id);

  option = bgpstream_get_data_interface_option_by_name(bs, di_id, "upd-type");
  CHECK("get upd-type option", option != NULL);
  CHECK("set upd-type ris-live",
        bgpstream_set_data_interface_option(bs, option, "ris-live") == 0);

  option = bgpstream_get_data_interface_option_by_name(bs, di_id, "upd-file");
  CHECK("get upd-file option", option != NULL);
  CHECK("set upd-file",
        bgpstream_set_data_interface_option(bs, option, "eor-stream.json") == 0);

  CHECK("stream start", bgpstream_start(bs) == 0);

  while ((rrc = bgpstream_get_next_record(bs, &rec)) > 0) {
    if (rec->status != BGPSTREAM_RECORD_STATUS_VALID_RECORD) {
      continue;
    }

    CHECK("record index within expected range", rcount < N_RECORDS);
    if (rcount >= N_RECORDS) {
      break;
    }
    const expected_record_t *exp = &expected[rcount];

    int e = 0;
    while ((erc = bgpstream_record_get_next_elem(rec, &elem)) > 0) {
      CHECK_MSG(exp->desc, "more elems than expected", e < exp->n_elems);
      if (e >= exp->n_elems) {
        e++;
        continue;
      }
      CHECK_MSG(exp->desc, "elem type", elem->type == exp->types[e]);
      CHECK(exp->desc,
            bgpstream_pfx_snprintf(buf, sizeof(buf), &elem->prefix) != NULL);
      CHECK_MSG(exp->desc, exp->prefixes[e],
                strcmp(buf, exp->prefixes[e]) == 0);
      e++;
    }
    CHECK_MSG(exp->desc, "no elem error", erc == 0);
    CHECK_MSG(exp->desc, "elem count", e == exp->n_elems);
    rcount++;
  }

  CHECK("final return code", rrc == 0);
  CHECK("read all records", rcount == N_RECORDS);

  bgpstream_destroy(bs);
  return 0;
}

int main(void)
{
  test_eor();
  ENDTEST;
  return 0;
}
