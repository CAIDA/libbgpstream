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

#include "bgpstream_di_mgr.h"
#include "bgpstream_debug.h"
#include "config.h"
#include "utils.h"
#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#ifdef WITH_DATA_INTERFACE_SINGLEFILE
#include "bgpstream_data_interface_singlefile.h"
#endif

#ifdef WITH_DATA_INTERFACE_CSVFILE
#include "bgpstream_data_interface_csvfile.h"
#endif

#ifdef WITH_DATA_INTERFACE_SQLITE
#include "bgpstream_data_interface_sqlite.h"
#endif

#ifdef WITH_DATA_INTERFACE_BROKER
#include "bgpstream_data_interface_broker.h"
#endif

/* After 10 retries, start exponential backoff */
#define DATA_INTERFACE_BLOCKING_RETRY_CNT 10
/* Wait at least 20 seconds if the broker has no new data for us */
#define DATA_INTERFACE_BLOCKING_MIN_WAIT 20
/* Wait at most 150 seconds if the broker has no new data for us */
#define DATA_INTERFACE_BLOCKING_MAX_WAIT 150

struct bgpstream_di_mgr {

  bsdi_t *interfaces[_BGPSTREAM_DATA_INTERFACE_CNT];

  bgpstream_data_interface_id_t active_di;

  // has the data interface been started yet?
  int started;

  // blocking query state
  int blocking;
  int backoff_time;
  int retry_cnt;

  // TODO: remove these
#if 0
#ifdef WITH_DATA_INTERFACE_SINGLEFILE
  bgpstream_di_singlefile_t *singlefile;
  char *singlefile_rib_mrtfile;
  char *singlefile_upd_mrtfile;
#endif

#ifdef WITH_DATA_INTERFACE_CSVFILE
  bgpstream_di_csvfile_t *csvfile;
  char *csvfile_file;
#endif

#ifdef WITH_DATA_INTERFACE_SQLITE
  bgpstream_di_sqlite_t *sqlite;
  char *sqlite_file;
#endif
#endif
};

/** Convenience typedef for the interface alloc function type */
typedef bsdi_t *(*di_alloc_func_t)();

/** Array of DI allocation functions.
 *
 * This MUST be kept in sync with the bgpstream_data_interface_id_t enum
 */
static const di_alloc_func_t di_alloc_functions[] = {

#ifdef WITH_DATA_INTERFACE_BROKER
  bsdi_broker_alloc,
#else
  NULL,
#endif

#ifdef WITH_DATA_INTERFACE_SINGLEFILE
  bsdi_singlefile_alloc,
#else
  NULL,
#endif

#ifdef WITH_DATA_INTERFACE_CSVFILE
  bsdi_csvfile_alloc,
#else
  NULL,
#endif

#ifdef WITH_DATA_INTERFACE_SQLITE
  bsdi_sqlite_alloc,
#else
  NULL,
#endif

};

#define GET_DEFAULT_STR_VALUE(var_store, default_value)                        \
  do {                                                                         \
    if (strcmp(default_value, "not-set") == 0) {                               \
      var_store = NULL;                                                        \
    } else {                                                                   \
      var_store = strdup(default_value);                                       \
    }                                                                          \
  } while (0)

#define GET_DEFAULT_INT_VALUE(var_store, default_value)                        \
  do {                                                                         \
    if (strcmp(default_value, "not-set") == 0) {                               \
      var_store = 0;                                                           \
    } else {                                                                   \
      var_store = atoi(default_value);                                         \
    }                                                                          \
  } while (0)

static bsdi_t *di_alloc(bgpstream_filter_mgr_t *filter_mgr,
                        bgpstream_data_interface_id_t id)
{
  bsdi_t *di;
  assert(ARR_CNT(di_alloc_functions) == _BGPSTREAM_DATA_INTERFACE_CNT);

  if (di_alloc_functions[id] == NULL) {
    return NULL;
  }

  /* first, create the struct */
  if ((di = malloc_zero(sizeof(bsdi_t))) == NULL) {
    return NULL;
  }

  /* get the core DI details (info, func ptrs) from the plugin */
  memcpy(di, di_alloc_functions[id - 1](), sizeof(bsdi_t));

  di->filter_mgr = filter_mgr;

  return di;
}

/* ========== PUBLIC FUNCTIONS BELOW HERE ========== */

bgpstream_di_mgr_t *bgpstream_di_mgr_create(bgpstream_filter_mgr_t *filter_mgr)
{
  bgpstream_di_mgr_t *mgr;
  int id;

  if ((mgr = malloc_zero(sizeof(bgpstream_di_mgr_t))) == NULL) {
    return NULL; // can't allocate memory
  }

  // default values
  mgr->active_di = BGPSTREAM_DATA_INTERFACE_BROKER;
  mgr->backoff_time = DATA_INTERFACE_BLOCKING_MIN_WAIT;

  /* allocate the interfaces (some may/will be NULL) */
  for (id = 0; id <= _BGPSTREAM_DATA_INTERFACE_CNT; id++) {
    mgr->interfaces[id] = di_alloc(filter_mgr, id);
  }

  return mgr;
}

int bgpstream_di_mgr_set_data_interface(bgpstream_di_mgr_t *di_mgr,
                                         bgpstream_data_interface_id_t di_id)
{
  if (di_mgr->interfaces[di_id] == NULL) {
    return -1;
  }
  di_mgr->active_di = di_id;
  return 0;
}

bgpstream_data_interface_id_t
bgpstream_di_mgr_get_data_interface_id(bgpstream_di_mgr_t *di_mgr)
{
  return di_mgr->active_di;
}

int bgpstream_di_mgr_set_data_interface_option(
  bgpstream_di_mgr_t *di_mgr,
  const bgpstream_data_interface_option_t *option_type,
  const char *option_value)
{
  // this option has no effect if the data interface selected is not using this
  // option
  switch (option_type->if_id) {

#ifdef WITH_DATA_INTERFACE_SINGLEFILE
  case BGPSTREAM_DATA_INTERFACE_SINGLEFILE:
    switch (option_type->id) {
    case 0:
      if (di_mgr->singlefile_rib_mrtfile != NULL) {
        free(di_mgr->singlefile_rib_mrtfile);
      }
      di_mgr->singlefile_rib_mrtfile = strdup(option_value);
      break;
    case 1:
      if (di_mgr->singlefile_upd_mrtfile != NULL) {
        free(di_mgr->singlefile_upd_mrtfile);
      }
      di_mgr->singlefile_upd_mrtfile = strdup(option_value);
      break;
    }
    break;
#endif

#ifdef WITH_DATA_INTERFACE_CSVFILE
  case BGPSTREAM_DATA_INTERFACE_CSVFILE:
    switch (option_type->id) {
    case 0:
      if (di_mgr->csvfile_file != NULL) {
        free(di_mgr->csvfile_file);
      }
      di_mgr->csvfile_file = strdup(option_value);
      break;
    }
    break;
#endif

#ifdef WITH_DATA_INTERFACE_SQLITE
  case BGPSTREAM_DATA_INTERFACE_SQLITE:
    switch (option_type->id) {
    case 0:
      if (di_mgr->sqlite_file != NULL) {
        free(di_mgr->sqlite_file);
      }
      di_mgr->sqlite_file = strdup(option_value);
      break;
    }
#endif

#ifdef WITH_DATA_INTERFACE_BROKER
  case BGPSTREAM_DATA_INTERFACE_BROKER:
    switch (option_type->id) {
    case 0:
      if (di_mgr->broker_url != NULL) {
        free(di_mgr->broker_url);
      }
      di_mgr->broker_url = strdup(option_value);
      break;

    case 1:
      if ((di_mgr->broker_params = realloc(
             di_mgr->broker_params,
             sizeof(char *) * (di_mgr->broker_params_cnt + 1))) == NULL) {
        return -1;
      }
      di_mgr->broker_params[di_mgr->broker_params_cnt++] = strdup(option_value);
      break;
    }
    break;
#endif

  default:
    fprintf(stderr, "Invalid data interface (are all interfaces built?\n");
    return -1;
  }
  return 0;
}

int bgpstream_di_mgr_start(bgpstream_di_mgr_t *di_mgr)
{
  bgpstream_debug("\tBSDS_MGR: init start");
  if (di_mgr == NULL) {
    return -1; // no manager
  }

  void *ds = NULL;

  switch (di_mgr->di_id) {
#ifdef WITH_DATA_INTERFACE_SINGLEFILE
  case BGPSTREAM_DATA_INTERFACE_SINGLEFILE:
    di_mgr->singlefile =
      bgpstream_di_singlefile_create(filter_mgr, di_mgr->singlefile_rib_mrtfile,
                                     di_mgr->singlefile_upd_mrtfile);
    ds = (void *)di_mgr->singlefile;
    break;
#endif

#ifdef WITH_DATA_INTERFACE_CSVFILE
  case BGPSTREAM_DATA_INTERFACE_CSVFILE:
    di_mgr->csvfile =
      bgpstream_di_csvfile_create(filter_mgr, di_mgr->csvfile_file);
    ds = (void *)di_mgr->csvfile;
    break;
#endif

#ifdef WITH_DATA_INTERFACE_SQLITE
  case BGPSTREAM_DATA_INTERFACE_SQLITE:
    di_mgr->sqlite =
      bgpstream_di_sqlite_create(filter_mgr, di_mgr->sqlite_file);
    ds = (void *)di_mgr->sqlite;
    break;
#endif

#ifdef WITH_DATA_INTERFACE_BROKER
  case BGPSTREAM_DATA_INTERFACE_BROKER:
    di_mgr->broker = bgpstream_di_broker_create(filter_mgr, di_mgr->broker_url,
                                                di_mgr->broker_params,
                                                di_mgr->broker_params_cnt);
    ds = (void *)di_mgr->broker;
    break;
#endif

  default:
    ds = NULL;
  }

  if (ds == NULL) {
    return -1;
  }

  bgpstream_debug("\tBSDS_MGR: init end");
  return 0;
}

void bgpstream_di_mgr_set_blocking(bgpstream_di_mgr_t *di_mgr)
{
  bgpstream_debug("\tBSDS_MGR: set blocking start");
  if (di_mgr == NULL) {
    return; // no manager
  }
  di_mgr->blocking = 1;
  bgpstream_debug("\tBSDS_MGR: set blocking end");
}

int bgpstream_di_mgr_get_queue(bgpstream_di_mgr_t *di_mgr,
                               bgpstream_input_mgr_t *input_mgr)
{
  bgpstream_debug("\tBSDS_MGR: get data start");
  if (di_mgr == NULL) {
    return -1; // no data interface manager
  }
  int results = -1;

  do {
    switch (di_mgr->di_id) {
#ifdef WITH_DATA_INTERFACE_SINGLEFILE
    case BGPSTREAM_DATA_INTERFACE_SINGLEFILE:
      results = bgpstream_di_singlefile_update_input_queue(di_mgr->singlefile,
                                                           input_mgr);
      break;
#endif

#ifdef WITH_DATA_INTERFACE_CSVFILE
    case BGPSTREAM_DATA_INTERFACE_CSVFILE:
      results =
        bgpstream_di_csvfile_update_input_queue(di_mgr->csvfile, input_mgr);
      break;
#endif

#ifdef WITH_DATA_INTERFACE_SQLITE
    case BGPSTREAM_DATA_INTERFACE_SQLITE:
      results =
        bgpstream_di_sqlite_update_input_queue(di_mgr->sqlite, input_mgr);
      break;
#endif

#ifdef WITH_DATA_INTERFACE_BROKER
    case BGPSTREAM_DATA_INTERFACE_BROKER:
      results =
        bgpstream_di_broker_update_input_queue(di_mgr->broker, input_mgr);
      break;
#endif

    default:
      fprintf(stderr, "Invalid data interface\n");
      break;
    }
    if (results == 0 && di_mgr->blocking) {
      // results = 0 => 2+ time and database did not give any error
      sleep(di_mgr->backoff_time);
      if (di_mgr->retry_cnt >= DATA_INTERFACE_BLOCKING_RETRY_CNT) {
        di_mgr->backoff_time = di_mgr->backoff_time * 2;
        if (di_mgr->backoff_time > DATA_INTERFACE_BLOCKING_MAX_WAIT) {
          di_mgr->backoff_time = DATA_INTERFACE_BLOCKING_MAX_WAIT;
        }
      }
      di_mgr->retry_cnt++;
    }
    bgpstream_debug("\tBSDS_MGR: got %d (blocking: %d)", results,
                    di_mgr->blocking);
  } while (di_mgr->blocking && results == 0);

  di_mgr->backoff_time = DATA_INTERFACE_BLOCKING_MIN_WAIT;
  di_mgr->retry_cnt = 0;

  bgpstream_debug("\tBSDS_MGR: get data end");
  return results;
}

static void bgpstream_di_mgr_close(bgpstream_di_mgr_t *di_mgr)
{
  bgpstream_debug("\tBSDS_MGR: close start");
  if (di_mgr == NULL) {
    return; // no manager to destroy
  }
  switch (di_mgr->di_id) {
#ifdef WITH_DATA_INTERFACE_SINGLEFILE
  case BGPSTREAM_DATA_INTERFACE_SINGLEFILE:
    bgpstream_di_singlefile_destroy(di_mgr->singlefile);
    di_mgr->singlefile = NULL;
    break;
#endif

#ifdef WITH_DATA_INTERFACE_CSVFILE
  case BGPSTREAM_DATA_INTERFACE_CSVFILE:
    bgpstream_di_csvfile_destroy(di_mgr->csvfile);
    di_mgr->csvfile = NULL;
    break;
#endif

#ifdef WITH_DATA_INTERFACE_SQLITE
  case BGPSTREAM_DATA_INTERFACE_SQLITE:
    bgpstream_di_sqlite_destroy(di_mgr->sqlite);
    di_mgr->sqlite = NULL;
    break;
#endif

#ifdef WITH_DATA_INTERFACE_BROKER
  case BGPSTREAM_DATA_INTERFACE_BROKER:
    bgpstream_di_broker_destroy(di_mgr->broker);
    di_mgr->broker = NULL;
    break;
#endif

  default:
    assert(0);
    break;
  }
  bgpstream_debug("\tBSDS_MGR: close end");
}

void bgpstream_di_mgr_destroy(bgpstream_di_mgr_t *di_mgr)
{
  bgpstream_debug("\tBSDS_MGR: destroy start");
  if (di_mgr == NULL) {
    return; // no manager to destroy
  }

  bgpstream_di_mgr_close(di_mgr);

// destroy any active data interface (if they have not been destroyed before)
#ifdef WITH_DATA_INTERFACE_SINGLEFILE
  bgpstream_di_singlefile_destroy(di_mgr->singlefile);
  di_mgr->singlefile = NULL;
  free(di_mgr->singlefile_rib_mrtfile);
  free(di_mgr->singlefile_upd_mrtfile);
#endif

#ifdef WITH_DATA_INTERFACE_CSVFILE
  bgpstream_di_csvfile_destroy(di_mgr->csvfile);
  di_mgr->csvfile = NULL;
  free(di_mgr->csvfile_file);
#endif

#ifdef WITH_DATA_INTERFACE_SQLITE
  bgpstream_di_sqlite_destroy(di_mgr->sqlite);
  di_mgr->sqlite = NULL;
  free(di_mgr->sqlite_file);
#endif

#ifdef WITH_DATA_INTERFACE_BROKER
  bgpstream_di_broker_destroy(di_mgr->broker);
  di_mgr->broker = NULL;
  free(di_mgr->broker_url);
  int i;
  for (i = 0; i < di_mgr->broker_params_cnt; i++) {
    free(di_mgr->broker_params[i]);
    di_mgr->broker_params[i] = NULL;
  }
  free(di_mgr->broker_params);
  di_mgr->broker_params = NULL;
  di_mgr->broker_params_cnt = 0;
#endif

  free(di_mgr);
  bgpstream_debug("\tBSDS_MGR: destroy end");
}
