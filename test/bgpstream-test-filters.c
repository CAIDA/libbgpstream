/*
 * Copyright (C) 2016 The Regents of the University of California.
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

#include "bgpstream_test.h"

#include "utils.h"

#include <stdio.h>
#include <string.h>
#include <wandio.h>

#ifdef WITH_DATA_INTERFACE_BROKER
static bgpstream_t *bs;
static bgpstream_record_t *rec;
static bgpstream_elem_t *elem;

static bgpstream_data_interface_id_t di_id = 0;
// static bgpstream_data_interface_option_t *option;

static const char *expected_results[] = {
  "U|A|1427846850.000000|ris|rrc06|||25152|202.249.2.185|202.70.88.0/21|202.249.2.185|25152 2914 15412 9304 23752|23752|2914:410 2914:1408 2914:2401 2914:3400||",
  "U|A|1427846860.000000|ris|rrc06|||25152|202.249.2.185|202.70.88.0/21|202.249.2.185|25152 2914 15412 9304 23752|23752|2914:410 2914:1408 2914:2401 2914:3400||",
  "U|A|1427846871.000000|ris|rrc06|||25152|2001:200:0:fe00::6249:0|2620:110:9004::/48|2001:200:0:fe00::6249:0|25152 2914 3356 13620|13620|2914:420 2914:1001 2914:2000 2914:3000||",
  "U|A|1427846874.000000|routeviews|route-views.jinx|||37105|196.223.14.46|154.73.136.0/24|196.223.14.84|37105 37549|37549|37105:300||",
  "U|A|1427846874.000000|routeviews|route-views.jinx|||37105|196.223.14.46|154.73.137.0/24|196.223.14.84|37105 37549|37549|37105:300||",
  "U|A|1427846874.000000|routeviews|route-views.jinx|||37105|196.223.14.46|154.73.138.0/24|196.223.14.84|37105 37549|37549|37105:300||",
  "U|A|1427846874.000000|routeviews|route-views.jinx|||37105|196.223.14.46|154.73.139.0/24|196.223.14.84|37105 37549|37549|37105:300||",
  NULL};

static const char *mangled_expected_results =
  "|A|1427846874.000000|routeviews|route-views.jinx|||37105||154.73.139.0/203|196.223.14.84|37105 37549|37549|37105:300||";

#define SETUP                                                                  \
  do {                                                                         \
    bs = bgpstream_create();                                                   \
  } while (0)

#define TEARDOWN                                                               \
  do {                                                                         \
    bgpstream_destroy(bs);                                                     \
    bs = NULL;                                                                 \
  } while (0)

#define CHECK_SET_INTERFACE(interface)                                         \
  do {                                                                         \
    CHECK("get data interface ID (" STR(interface) ")",                        \
          (di_id = bgpstream_get_data_interface_id_by_name(                    \
             bs, STR(interface))) != 0);                                       \
    bgpstream_set_data_interface(bs, di_id);                                   \
  } while (0)

static void process_records()
{
  int ret;
  int counter = 0;

  CHECK("stream start (" STR(interface) ")", bgpstream_start(bs) == 0);

  while ((ret = bgpstream_get_next_record(bs, &rec)) > 0) {
    if (rec->status == BGPSTREAM_RECORD_STATUS_VALID_RECORD) {
      while (bgpstream_record_get_next_elem(rec, &elem) > 0) {

        /* make sure we haven't reached end of expected_results */
        CHECK("elem partial count", expected_results[counter]);
        if (!expected_results[counter]) break;

        CHECK_SNPRINTF("elem equality",
          expected_results[counter], 65536, CHAR_P,
          bgpstream_record_elem_snprintf(cs_buf, cs_len, rec, elem));

        // try mangling some values in the last elem to see if printer breaks
        if (!expected_results[counter+1]) {
          rec->type = 201;
          elem->peer_ip.version = 202;
          elem->prefix.mask_len = 203;
          CHECK_SNPRINTF("mangled elem equality",
            mangled_expected_results, 65536, CHAR_P,
            bgpstream_record_elem_snprintf(cs_buf, cs_len, rec, elem));
        }

        counter++;
      }
    }
  }

  /* make sure we have reached end of expected_results */
  CHECK("elem total count", !expected_results[counter]);
}

static int test_bgpstream_filters()
{
  SETUP;

  CHECK_SET_INTERFACE(broker);

  bgpstream_add_filter(bs, BGPSTREAM_FILTER_TYPE_COLLECTOR, "rrc06");
  bgpstream_add_filter(bs, BGPSTREAM_FILTER_TYPE_COLLECTOR, "route-views.jinx");

  bgpstream_add_filter(bs, BGPSTREAM_FILTER_TYPE_RECORD_TYPE, "updates");

  bgpstream_add_interval_filter(bs, 1427846847, 1427846874);

  bgpstream_add_filter(bs, BGPSTREAM_FILTER_TYPE_ELEM_PEER_ASN, "25152");
  bgpstream_add_filter(bs, BGPSTREAM_FILTER_TYPE_ELEM_PEER_ASN, "37105");

  bgpstream_add_filter(bs, BGPSTREAM_FILTER_TYPE_ELEM_PREFIX,
                       "2620:110:9004::/40");
  bgpstream_add_filter(bs, BGPSTREAM_FILTER_TYPE_ELEM_PREFIX,
                       "154.73.128.0/17");
  bgpstream_add_filter(bs, BGPSTREAM_FILTER_TYPE_ELEM_PREFIX, "202.70.88.0/21");

  bgpstream_add_filter(bs, BGPSTREAM_FILTER_TYPE_ELEM_COMMUNITY, "2914:*");
  bgpstream_add_filter(bs, BGPSTREAM_FILTER_TYPE_ELEM_COMMUNITY, "*:300");

  process_records();

  TEARDOWN;


  SETUP;

  CHECK_SET_INTERFACE(broker);

  bgpstream_add_interval_filter(bs, 1427846847, 1427846874);
  CHECK("filter string", bgpstream_parse_filter_string(bs,
    "collector rrc06 route-views.jinx and "
    "type updates and "
    "peer 25152 37105 and "
    "prefix 2620:110:9004::/40 154.73.128.0/17 202.70.88.0/21 and "
    "comm \"2914:*\" *:300"));

  process_records();

  TEARDOWN;
  return 0;
}

#endif

int main()
{
  int rc = 0;

#ifdef WITH_DATA_INTERFACE_BROKER
  rc = test_bgpstream_filters();
#else
  SKIPPED_SECTION("broker data interface filters");
#endif

  ENDTEST;
  return rc;
}
