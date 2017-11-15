/*
 * This file is part of bgpstream
 *
 * CAIDA, UC San Diego
 * bgpstream-info@caida.org
 *
 * Copyright (C) 2012 The Regents of the University of California.
 * Authors: Alistair King, Chiara Orsini
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
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

bgpstream_t *bs;
bgpstream_record_t *rec;
bgpstream_data_interface_id_t di_id = 0;
bgpstream_data_interface_option_t *option;

#define RUN(interface)                                                         \
  do {                                                                         \
    int ret;                                                                   \
    int counter = 0;                                                           \
    CHECK("stream start (" STR(interface) ")", bgpstream_start(bs) == 0);      \
    while ((ret = bgpstream_get_next_record(bs, &rec)) > 0) {                   \
      if (rec->status == BGPSTREAM_RECORD_STATUS_VALID_RECORD) {               \
        counter++;                                                             \
      }                                                                        \
    }                                                                          \
    CHECK("final return code (" STR(interface) ")",                         \
          ret == 0);                                                    \
    CHECK("read records (" STR(interface) ")",                                 \
          counter == interface##_RECORDS);                              \
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

int test_bgpstream()
{
  CHECK("BGPStream create", (bs = bgpstream_create()) != NULL);

  TEARDOWN;
  return 0;
}

int test_singlefile()
{
  SETUP;

  CHECK_SET_INTERFACE(singlefile);

  CHECK("get option (rib-file)",
        (option = bgpstream_get_data_interface_option_by_name(
           bs, di_id, "rib-file")) != NULL);
  CHECK("set option (rib-file)",
        bgpstream_set_data_interface_option(bs, option,
                                            "routeviews.route-views.jinx.ribs.1427846400.bz2") == 0);

  CHECK("get option (upd-file)",
        (option = bgpstream_get_data_interface_option_by_name(
           bs, di_id, "upd-file")) != NULL);
  CHECK("set option (upd-file)",
        bgpstream_set_data_interface_option(bs, option,
                                            "ris.rrc06.updates.1427846400.gz") == 0);

  RUN(singlefile);

  TEARDOWN;
  return 0;
}

int test_csvfile()
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

int test_sqlite()
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

int test_broker()
{
  SETUP;

  CHECK_SET_INTERFACE(broker);

  /* test http connectivity */
  io_t *file = wandio_create(BGPSTREAM_DI_BROKER_URL);
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

  return 0;
}
