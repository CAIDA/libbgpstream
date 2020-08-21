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
 */

#include "bgpstream_test.h"

#include "utils.h"

#include <stdio.h>
#include <string.h>
#include <wandio.h>

#define singlefile_RECORDS 537347
#define csvfile_RECORDS 559424
#define sqlite_RECORDS 538308
#define broker_RECORDS 2153

static bgpstream_t *bs;
static bgpstream_record_t *rec;
static bgpstream_data_interface_id_t di_id = 0;
static bgpstream_data_interface_option_t *option;

#define RUN(interface)                                                         \
  do {                                                                         \
    int ret;                                                                   \
    int counter = 0;                                                           \
    CHECK("stream start (" STR(interface) ")", bgpstream_start(bs) == 0);      \
    while ((ret = bgpstream_get_next_record(bs, &rec)) > 0) {                  \
      if (rec->status == BGPSTREAM_RECORD_STATUS_VALID_RECORD) {               \
        counter++;                                                             \
      }                                                                        \
    }                                                                          \
    CHECK("final return code (" STR(interface) ")", ret == 0);                 \
    CHECK("read records (" STR(interface) ")",                                 \
          counter == interface##_RECORDS);                                     \
  } while (0)

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

static int test_bgpstream()
{
  CHECK("BGPStream create", (bs = bgpstream_create()) != NULL);

  TEARDOWN;
  return 0;
}

#ifdef WITH_DATA_INTERFACE_SINGLEFILE
static int test_singlefile()
{
  SETUP;

  CHECK_SET_INTERFACE(singlefile);

  CHECK("get option (rib-file)",
        (option = bgpstream_get_data_interface_option_by_name(
           bs, di_id, "rib-file")) != NULL);
  CHECK("set option (rib-file)",
        bgpstream_set_data_interface_option(
          bs, option, "routeviews.route-views.jinx.ribs.1427846400.bz2") == 0);

  CHECK("get option (upd-file)",
        (option = bgpstream_get_data_interface_option_by_name(
           bs, di_id, "upd-file")) != NULL);
  CHECK("set option (upd-file)",
        bgpstream_set_data_interface_option(
          bs, option, "ris.rrc06.updates.1427846400.gz") == 0);

  RUN(singlefile);

  TEARDOWN;
  return 0;
}
#endif

#ifdef WITH_DATA_INTERFACE_CSVFILE
static int test_csvfile()
{
  SETUP;

  CHECK_SET_INTERFACE(csvfile);

  CHECK("get option (csv-file)",
        (option = bgpstream_get_data_interface_option_by_name(
           bs, di_id, "csv-file")) != NULL);
  bgpstream_set_data_interface_option(bs, option, "csv_test.csv");

  bgpstream_add_filter(bs, BGPSTREAM_FILTER_TYPE_COLLECTOR, "rrc06");

  RUN(csvfile);

  TEARDOWN;
  return 0;
}
#endif

#ifdef WITH_DATA_INTERFACE_SQLITE
static int test_sqlite()
{
  SETUP;

  CHECK_SET_INTERFACE(sqlite);

  CHECK("get option (db-file)",
        (option = bgpstream_get_data_interface_option_by_name(
           bs, di_id, "db-file")) != NULL);
  bgpstream_set_data_interface_option(bs, option, "sqlite_test.db");

  bgpstream_add_filter(bs, BGPSTREAM_FILTER_TYPE_PROJECT, "routeviews");

  RUN(sqlite);

  TEARDOWN;
  return 0;
}
#endif

#ifdef WITH_DATA_INTERFACE_BROKER
#define BGPSTREAM_DI_BROKER_TEST_URL BGPSTREAM_DI_BROKER_URL "/meta/projects"
static int test_broker()
{
  SETUP;

  CHECK_SET_INTERFACE(broker);

  /* test http connectivity */
  io_t *file = wandio_create(BGPSTREAM_DI_BROKER_TEST_URL);
  CHECK_MSG("HTTP connectivity to broker",
            "Failed to connect to BGPStream Broker via HTTP.\n"
            "Maybe wandio is built without HTTP support, "
            "or there is no Internet connectivity\n",
            file != NULL);

  bgpstream_add_filter(bs, BGPSTREAM_FILTER_TYPE_COLLECTOR, "route-views6");
  bgpstream_add_filter(bs, BGPSTREAM_FILTER_TYPE_RECORD_TYPE, "updates");
  bgpstream_add_interval_filter(bs, 1427846550, 1427846700);

  RUN(broker);

  TEARDOWN;
  return 0;
}
#endif

int main()
{
  CHECK_SECTION("BGPStream", test_bgpstream() == 0);

#ifdef WITH_DATA_INTERFACE_SINGLEFILE
  CHECK_SECTION("singlefile data interface", test_singlefile() == 0);
#else
  SKIPPED_SECTION("singlefile data interface");
#endif

#ifdef WITH_DATA_INTERFACE_CSVFILE
  CHECK_SECTION("csvfile data interface", test_csvfile() == 0);
#else
  SKIPPED_SECTION("csvfile data interface");
#endif

#ifdef WITH_DATA_INTERFACE_SQLITE
  CHECK_SECTION("sqlite data interface", test_sqlite() == 0);
#else
  SKIPPED_SECTION("sqlite data interface");
#endif

#ifdef WITH_DATA_INTERFACE_BROKER
  CHECK_SECTION("broker data interface", test_broker() == 0);
#else
  SKIPPED_SECTION("broker data interface");
#endif

  ENDTEST;
  return 0;
}
