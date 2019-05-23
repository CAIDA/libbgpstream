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

#include "bsdi_csvfile.h"
#include "bgpstream_log.h"
#include "config.h"
#include "utils.h"
#include "libcsv/csv.h"
#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <wandio.h>

#define STATE (BSDI_GET_STATE(di, csvfile))
#define TIF filter_mgr->time_interval

/* ---------- START CLASS DEFINITION ---------- */

/* define the internal option ID values */
enum {
  OPTION_CSV_FILE,
};

/* define the options this data interface accepts */
static bgpstream_data_interface_option_t options[] = {
  /* CSV file name */
  {
    BGPSTREAM_DATA_INTERFACE_CSVFILE, // interface ID
    OPTION_CSV_FILE,                  // internal ID
    "csv-file",                       // name
    "csv file listing the mrt data to read (default: " STR(
      BGPSTREAM_DI_CSVFILE_CSV_FILE) ")",
  },
};

/* create the class structure for this data interface */
BSDI_CREATE_CLASS(csvfile, BGPSTREAM_DATA_INTERFACE_CSVFILE,
                  "Retrieve metadata information from a csv file", options)

/* ---------- END CLASS DEFINITION ---------- */

typedef struct bsdi_csvfile_state {
  /* user-provided options */

  // Path to a CSV file to read
  char *csv_file;

  /* internal state: */

  // CSV parser state
  struct csv_parser parser;

  // The current field being parsed
  int current_field;

  /* record metadata: */
  char filename[BGPSTREAM_DUMP_MAX_LEN];
  char project[BGPSTREAM_PAR_MAX_LEN];
  bgpstream_record_type_t record_type;
  char collector[BGPSTREAM_PAR_MAX_LEN];
  uint32_t filetime;
  uint32_t time_span;
  uint32_t timestamp;

  /* maximum timestamp processed in the current file */
  uint32_t max_ts_infile;
  /* maximum timestamp processed in the past file */
  uint32_t last_processed_ts;
  /* maximum timestamp accepted in the current round */
  uint32_t max_accepted_ts;
} bsdi_csvfile_state_t;

enum {
  CSVFILE_PATH = 0,
  CSVFILE_PROJECT = 1,
  CSVFILE_BGPTYPE = 2,
  CSVFILE_COLLECTOR = 3,
  CSVFILE_FILETIME = 4,
  CSVFILE_TIMESPAN = 5,
  CSVFILE_TIMESTAMP = 6,

  CSVFILE_FIELDCNT = 7
};

static int filters_match(bsdi_t *di)
{
  bgpstream_filter_mgr_t *filter_mgr = BSDI_GET_FILTER_MGR(di);
  int all_false;

  char *f;

  // projects
  all_false = 1;
  if (filter_mgr->projects != NULL) {
    bgpstream_str_set_rewind(filter_mgr->projects);
    while ((f = bgpstream_str_set_next(filter_mgr->projects)) != NULL) {
      if (strcmp(f, STATE->project) == 0) {
        all_false = 0;
        break;
      }
    }
    if (all_false != 0) {
      return 0;
    }
  }

  // collectors
  all_false = 1;
  if (filter_mgr->collectors != NULL) {
    bgpstream_str_set_rewind(filter_mgr->collectors);
    while ((f = bgpstream_str_set_next(filter_mgr->collectors)) != NULL) {
      if (strcmp(f, STATE->collector) == 0) {
        all_false = 0;
        break;
      }
    }
    if (all_false != 0) {
      return 0;
    }
  }

  // bgp_types
  all_false = 1;
  if (filter_mgr->bgp_types != NULL) {
    bgpstream_str_set_rewind(filter_mgr->bgp_types);
    while ((f = bgpstream_str_set_next(filter_mgr->bgp_types)) != NULL) {
      if ((STATE->record_type == BGPSTREAM_UPDATE &&
           strcmp("updates", f) == 0) ||
          (STATE->record_type == BGPSTREAM_RIB && strcmp("ribs", f) == 0)) {
        all_false = 0;
        break;
      }
    }
    if (all_false != 0) {
      return 0;
    }
  }

  // time_interval
  if (TIF != NULL) {
    // filetime (we consider 15 mins before to consider routeviews updates
    // and 120 seconds to have some margins)
    if (!(STATE->filetime >= (TIF->begin_time - (15 * 60) - 120) &&
          (TIF->end_time == BGPSTREAM_FOREVER ||
           STATE->filetime <= TIF->end_time))) {
      return 0;
    }
  }

  // if all the filters are matched
  return 1;
}

static void parse_field(void *field, size_t i, void *user_data)
{

  char *field_str = (char *)field;
  bsdi_t *di = (bsdi_t *)user_data;

  switch (STATE->current_field) {
  case CSVFILE_PATH:
    assert(i < BGPSTREAM_DUMP_MAX_LEN - 1);
    strncpy(STATE->filename, field_str, i);
    STATE->filename[i] = '\0';
    break;
  case CSVFILE_PROJECT:
    assert(i < BGPSTREAM_PAR_MAX_LEN - 1);
    strncpy(STATE->project, field_str, i);
    STATE->project[i] = '\0';
    break;
  case CSVFILE_BGPTYPE:
    assert(i < BGPSTREAM_PAR_MAX_LEN - 1);
    if (strcmp("ribs", field_str) == 0) {
      STATE->record_type = BGPSTREAM_RIB;
    } else if (strcmp("updates", field_str) == 0) {
      STATE->record_type = BGPSTREAM_UPDATE;
    } else {
      assert(0 && "Invalid record type");
    }
    break;
  case CSVFILE_COLLECTOR:
    assert(i < BGPSTREAM_PAR_MAX_LEN - 1);
    strncpy(STATE->collector, field_str, i);
    STATE->collector[i] = '\0';
    break;
  case CSVFILE_FILETIME:
    STATE->filetime = atoi(field_str);
    break;
  case CSVFILE_TIMESPAN:
    STATE->time_span = atoi(field_str);
    break;
  case CSVFILE_TIMESTAMP:
    STATE->timestamp = atoi(field_str);
    break;
  }

  /* one more field read */
  STATE->current_field++;
}

static void parse_rowend(int c, void *user_data)
{
  bsdi_t *di = (bsdi_t *)user_data;

  /* if there was an empty line, skip it */
  if (STATE->current_field == 0) {
    return;
  }

  /* ensure fields read is compliant with the expected file format */
  assert(STATE->current_field == CSVFILE_FIELDCNT);

  /* check if the timestamp is acceptable */
  if (STATE->timestamp > STATE->last_processed_ts &&
      STATE->timestamp <= STATE->max_accepted_ts) {
    /* update max in file timestamp */
    if (STATE->timestamp > STATE->max_ts_infile) {
      STATE->max_ts_infile = STATE->timestamp;
    }
    if (filters_match(di) != 0) {
      if (bgpstream_resource_mgr_push(
            BSDI_GET_RES_MGR(di), BGPSTREAM_RESOURCE_TRANSPORT_FILE,
            BGPSTREAM_RESOURCE_FORMAT_MRT, STATE->filename, STATE->filetime,
            STATE->time_span, STATE->project, STATE->collector,
            STATE->record_type, NULL) < 0) {
        assert(0);
      }
    }
  }
  STATE->current_field = 0;
}

/* ========== PUBLIC METHODS BELOW HERE ========== */

int bsdi_csvfile_init(bsdi_t *di)
{
  bsdi_csvfile_state_t *state;

  if ((state = malloc_zero(sizeof(bsdi_csvfile_state_t))) == NULL) {
    goto err;
  }
  BSDI_SET_STATE(di, state);

  /* set default state */

  /* initialize the CSV parser */
  if (csv_init(&(STATE->parser), (CSV_STRICT | CSV_REPALL_NL | CSV_STRICT_FINI |
                                  CSV_APPEND_NULL | CSV_EMPTY_IS_NULL)) != 0) {
    goto err;
  }
  STATE->current_field = CSVFILE_PATH;

  return 0;
err:
  bsdi_csvfile_destroy(di);
  return -1;
}

int bsdi_csvfile_start(bsdi_t *di)
{
  if (STATE->csv_file) {
    return 0;
  } else {
    bgpstream_log(BGPSTREAM_LOG_ERR, "The 'csv-file' option must be set");
    return -1;
  }
}

int bsdi_csvfile_set_option(
  bsdi_t *di, const bgpstream_data_interface_option_t *option_type,
  const char *option_value)
{
  switch (option_type->id) {
  case OPTION_CSV_FILE:
    // replaces our current CSV file
    if (STATE->csv_file != NULL) {
      free(STATE->csv_file);
      STATE->csv_file = NULL;
    }
    if ((STATE->csv_file = strdup(option_value)) == NULL) {
      return -1;
    }
    break;

  default:
    return -1;
  }

  return 0;
}

void bsdi_csvfile_destroy(bsdi_t *di)
{
  if (di == NULL || STATE == NULL) {
    return;
  }

  free(STATE->csv_file);
  STATE->csv_file = NULL;

  csv_free(&STATE->parser);

  free(STATE);
  BSDI_SET_STATE(di, NULL);
}

int bsdi_csvfile_update_resources(bsdi_t *di)
{
  io_t *file_io = NULL;
#define BUFFER_LEN 1024
  char buffer[BUFFER_LEN];
  int read = 0;

  /* we accept all timestamp earlier than now() - 1 second */
  STATE->max_accepted_ts = epoch_sec() - 1;

  STATE->max_ts_infile = 0;

  if ((file_io = wandio_create(STATE->csv_file)) == NULL) {
    bgpstream_log(BGPSTREAM_LOG_ERR, "can't open file %s", STATE->csv_file);
    goto err;
  }

  while ((read = wandio_read(file_io, &buffer, BUFFER_LEN)) > 0) {
    if (csv_parse(&(STATE->parser), buffer, read, parse_field, parse_rowend,
                  di) != read) {
      bgpstream_log(BGPSTREAM_LOG_ERR, "CSV parsing error %s",
                    csv_strerror(csv_error(&STATE->parser)));
      goto err;
    }
  }

  if (csv_fini(&(STATE->parser), parse_field, parse_rowend, di) != 0) {
    bgpstream_log(BGPSTREAM_LOG_ERR, "CSV parsing error %s",
                  csv_strerror(csv_error(&STATE->parser)));
    goto err;
  }

  wandio_destroy(file_io);

  STATE->last_processed_ts = STATE->max_ts_infile;
  return 0;

err:
  return -1;
}
