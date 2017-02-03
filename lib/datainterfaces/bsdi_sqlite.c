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

#include "bsdi_sqlite.h"
#include "bgpstream_debug.h"
#include "utils.h"
#include <assert.h>
#include <inttypes.h>
#include <sqlite3.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#define STATE (BSDI_GET_STATE(di, sqlite))

/* ---------- START CLASS DEFINITION ---------- */

/* define the internal option ID values */
enum {
  OPTION_DB_FILE,
};

/* define the options this data interface accepts */
static bgpstream_data_interface_option_t options[] = {
  /* SQLITE database file name */
  {
    BGPSTREAM_DATA_INTERFACE_SQLITE, // interface ID
    OPTION_DB_FILE, // internal ID
    "db-file", // name
    "SQLite database file (default: " STR(BGPSTREAM_DI_SQLITE_DB_FILE) ")",
  },
};

/* create the class structure for this data interface */
BSDI_CREATE_CLASS(
  sqlite,
  BGPSTREAM_DATA_INTERFACE_SQLITE,
  "Retrieve metadata information from an SQLite database",
  options
);

/* ---------- END CLASS DEFINITION ---------- */

#define MAX_QUERY_LEN 2048

typedef struct bsdi_sqlite_state {
  /* user-provided options: */

  char *db_file;

  /* internal state: */

  // DB handle
  sqlite3 *db;

  // statement handle
  sqlite3_stmt *stmt;

  // buffer for building queries XXX
  char query_buf[MAX_QUERY_LEN];

  // current timestamp
  uint32_t current_ts;

  // last timestamp
  uint32_t last_ts;

} bsdi_sqlite_state_t;

#define MAX_INTERVAL_LEN 16

#define APPEND_STR(str)                                                        \
  do {                                                                         \
    size_t len = strlen(str);                                                  \
    if (rem_buf_space < len + 1) {                                             \
      goto err;                                                         \
    }                                                                          \
    strncat(STATE->query_buf, str, rem_buf_space);                            \
    rem_buf_space -= len;                                                      \
  } while (0)

static int prepare_db(bsdi_t *di)
{
  if (sqlite3_open_v2(STATE->db_file, &STATE->db, SQLITE_OPEN_READONLY, NULL)
      != SQLITE_OK) {
    bgpstream_log_err("SQLite can't open database: %s",
                      sqlite3_errmsg(STATE->db));
    return -1;
  }

  if (sqlite3_prepare_v2(STATE->db, STATE->query_buf, -1, &STATE->stmt, NULL)
      != SQLITE_OK) {
    bgpstream_log_err("SQLite failed to prepare statement: %s",
                      sqlite3_errmsg(STATE->db));
    return -1;
  }
  return 0;
}

/* ========== PUBLIC METHODS BELOW HERE ========== */

int bsdi_sqlite_init(bsdi_t *di)
{
  bsdi_sqlite_state_t *state;

  if ((state = malloc_zero(sizeof(bsdi_sqlite_state_t))) == NULL) {
    goto err;
  }
  BSDI_SET_STATE(di, state);

  /* set default state */
  // none

  return 0;
err:
  bsdi_sqlite_destroy(di);
  return -1;
}

static int build_query(bsdi_t *di)
{
  size_t rem_buf_space = MAX_QUERY_LEN;
  char interval_str[MAX_INTERVAL_LEN];

  /* reset the query buffer. probably unnecessary, but lets do it anyway */
  STATE->query_buf[0] = '\0';

  APPEND_STR(
    "SELECT bgp_data.file_path, collectors.project, collectors.name, "
    "bgp_types.name, time_span.time_span, bgp_data.file_time, bgp_data.ts "
    "FROM  collectors JOIN bgp_data JOIN bgp_types JOIN time_span "
    "WHERE bgp_data.collector_id = collectors.id  AND "
    "bgp_data.collector_id = time_span.collector_id AND "
    "bgp_data.type_id = bgp_types.id AND "
    "bgp_data.type_id = time_span.bgp_type_id ");

  // projects, collectors, bgp_types, and time_intervals are used as filters
  // only if they are provided by the user
  bgpstream_filter_mgr_t *filter_mgr = BSDI_GET_FILTER_MGR(di);
  bgpstream_interval_filter_t *tif;
  int first;
  char *f;

  // projects
  first = 1;
  if (filter_mgr->projects != NULL) {
    APPEND_STR(" AND collectors.project IN (");
    bgpstream_str_set_rewind(filter_mgr->projects);
    while ((f = bgpstream_str_set_next(filter_mgr->projects)) != NULL) {
      if (!first) {
        APPEND_STR(", ");
      }
      APPEND_STR("'");
      APPEND_STR(f);
      APPEND_STR("'");
      first = 0;
    }
    APPEND_STR(" ) ");
  }

  // collectors
  first = 1;
  if (filter_mgr->collectors != NULL) {
    APPEND_STR(" AND collectors.name IN (");
    bgpstream_str_set_rewind(filter_mgr->collectors);
    while ((f = bgpstream_str_set_next(filter_mgr->collectors)) != NULL) {
      if (!first) {
        APPEND_STR(", ");
      }
      APPEND_STR("'");
      APPEND_STR(f);
      APPEND_STR("'");
      first = 0;
    }
    APPEND_STR(" ) ");
  }

  // bgp_types
  first = 1;
  if (filter_mgr->bgp_types != NULL) {
    APPEND_STR(" AND bgp_types.name IN (");
    bgpstream_str_set_rewind(filter_mgr->bgp_types);
    while ((f = bgpstream_str_set_next(filter_mgr->bgp_types)) != NULL) {
      if (!first) {
        APPEND_STR(", ");
      }
      APPEND_STR("'");
      APPEND_STR(f);
      APPEND_STR("'");
      first = 0;
    }
    APPEND_STR(" ) ");
  }

  // time_intervals
  int written = 0;
  if (filter_mgr->time_intervals != NULL) {
    tif = filter_mgr->time_intervals;
    APPEND_STR(" AND ( ");

    while (tif != NULL) {
      APPEND_STR(" ( ");

      // BEGIN TIME
      APPEND_STR(" (bgp_data.file_time >=  ");
      interval_str[0] = '\0';
      if ((written = snprintf(interval_str, MAX_INTERVAL_LEN, "%" PRIu32,
                              tif->begin_time)) < MAX_INTERVAL_LEN) {
        APPEND_STR(interval_str);
      }
      APPEND_STR("  - time_span.time_span - 120 )");
      APPEND_STR("  AND  ");

      // END TIME
      if (tif->end_time != BGPSTREAM_FOREVER) {
        APPEND_STR(" (bgp_data.file_time <=  ");
        interval_str[0] = '\0';
        if ((written = snprintf(interval_str, MAX_INTERVAL_LEN, "%" PRIu32,
                                tif->end_time)) < MAX_INTERVAL_LEN) {
          APPEND_STR(interval_str);
        }
        APPEND_STR(") ");
        APPEND_STR(" ) ");
      }

      tif = tif->next;
      if (tif != NULL) {
        APPEND_STR(" OR ");
      }
    }
    APPEND_STR(" )");
  }

  /*  comment on 120 seconds: */
  /*  sometimes it happens that ribs or updates carry a filetime which is not */
  /*  compliant with the expected filetime (e.g. : */
  /*   rib.23.59 instead of rib.00.00 */
  /*  in order to compensate for this kind of situations we  */
  /*  retrieve data that are 120 seconds older than the requested  */

  // minimum timestamp and current timestamp are the two placeholders
  APPEND_STR(" AND bgp_data.ts > ? AND bgp_data.ts <= ?");
  // order by filetime and bgptypes in reverse order: this way the
  // input insertions are always "head" insertions, i.e. queue insertion is
  // faster
  APPEND_STR(" ORDER BY file_time DESC, bgp_types.name DESC");

  return 0;

 err:
  return -1;
}

int bsdi_sqlite_start(bsdi_t *di)
{
  /* check user-provided options */
  if (!STATE->db_file) {
    fprintf(stderr, "ERROR: The 'db-file' option must be set\n");
    return -1;
  }

  if (build_query(di) != 0) {
    return -1;
  }

  return prepare_db(di);
}

int bsdi_sqlite_set_option(bsdi_t *di,
                           const bgpstream_data_interface_option_t *option_type,
                           const char *option_value)
{
  switch (option_type->id) {
  case OPTION_DB_FILE:
    // replaces our current DB file
    if (STATE->db_file != NULL) {
      free(STATE->db_file);
      STATE->db_file = NULL;
    }
    if ((STATE->db_file = strdup(option_value)) == NULL) {
      return -1;
    }
    break;

  default:
    return -1;
  }

  return 0;
}

void bsdi_sqlite_destroy(bsdi_t *di)
{
  if (di == NULL || STATE == NULL) {
    return;
  }

  free(STATE->db_file);
  STATE->db_file = NULL;

  sqlite3_finalize(STATE->stmt);
  sqlite3_close(STATE->db);

  free(STATE);
  BSDI_SET_STATE(di, NULL);
}

int bsdi_sqlite_get_queue(bsdi_t *di, bgpstream_input_mgr_t *input_mgr)
{
  int rc;
  int queue_len = 0;

  STATE->last_ts = STATE->current_ts;

  // update current_timestamp - we always ask for data 1 second old at least
  STATE->current_ts = epoch_sec() - 1; // now() - 1 second

  sqlite3_bind_int(STATE->stmt, 1, STATE->last_ts);
  sqlite3_bind_int(STATE->stmt, 2, STATE->current_ts);

  while ((rc = sqlite3_step(STATE->stmt)) != SQLITE_DONE) {
    if (rc != SQLITE_ROW) {
      bgpstream_log_err(
        "\t\tBSDS_SQLITE: error while stepping through results");
      return -1;
    }

    queue_len += bgpstream_input_mgr_push_sorted_input(input_mgr,
        strdup((const char *)sqlite3_column_text(STATE->stmt, 0)) /* path */,
        strdup(
          (const char *)sqlite3_column_text(STATE->stmt, 1)) /* project */,
        strdup(
          (const char *)sqlite3_column_text(STATE->stmt, 2)) /* collector */,
        strdup((const char *)sqlite3_column_text(STATE->stmt, 3)) /* type */,
        sqlite3_column_int(STATE->stmt, 5) /* file time */,
        sqlite3_column_int(STATE->stmt, 4) /* time span */);
  }
  sqlite3_reset(STATE->stmt);

  return queue_len;
}

