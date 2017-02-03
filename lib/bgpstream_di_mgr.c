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
#include "bsdi_singlefile.h"
#endif

#ifdef WITH_DATA_INTERFACE_CSVFILE
#include "bsdi_csvfile.h"
#endif

#ifdef WITH_DATA_INTERFACE_SQLITE
#include "bsdi_sqlite.h"
#endif

#ifdef WITH_DATA_INTERFACE_BROKER
#include "bsdi_broker.h"
#endif

/* After 10 retries, start exponential backoff */
#define DATA_INTERFACE_BLOCKING_RETRY_CNT 10
/* Wait at least 20 seconds if the broker has no new data for us */
#define DATA_INTERFACE_BLOCKING_MIN_WAIT 20
/* Wait at most 150 seconds if the broker has no new data for us */
#define DATA_INTERFACE_BLOCKING_MAX_WAIT 150

#define ACTIVE_DI (di_mgr->interfaces[di_mgr->active_di])

struct bgpstream_di_mgr {

  bsdi_t *interfaces[_BGPSTREAM_DATA_INTERFACE_CNT];

  bgpstream_data_interface_id_t *available_dis;
  int available_dis_cnt;

  bgpstream_data_interface_id_t active_di;

  // has the data interface been started yet?
  int started;

  // blocking query state
  int blocking;
  int backoff_time;
  int retry_cnt;

  // TODO: remove these
#if 0

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

  /* INVALID DATA INTERFACE */
  NULL,

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

static void di_destroy(bsdi_t *di)
{
  if (di == NULL) {
    return;
  }

  /* ask the interface to free it's own state */
  di->destroy(di);

  /* finally, free the actual interface structure */
  free(di);

  return;
}

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
  memcpy(di, di_alloc_functions[id](), sizeof(bsdi_t));

  di->filter_mgr = filter_mgr;

  /* call the init function to allow the plugin to create state */
  if (di->init(di) != 0) {
    di_destroy(di);
    return NULL;
  }

  return di;
}

static bsdi_t *get_di(bgpstream_di_mgr_t *di_mgr,
                      bgpstream_data_interface_id_t id)
{
  if (id > 0 && id < _BGPSTREAM_DATA_INTERFACE_CNT) {
    return di_mgr->interfaces[id]; // could be NULL
  }
  return NULL;
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
  for (id = 0; id < _BGPSTREAM_DATA_INTERFACE_CNT; id++) {
    if ((mgr->available_dis = realloc(mgr->available_dis,
                                      sizeof(bgpstream_data_interface_id_t)*
                                      (mgr->available_dis_cnt+1))) == NULL) {
      bgpstream_di_mgr_destroy(mgr);
      return NULL;
    }
    mgr->available_dis[mgr->available_dis_cnt++] = id;
    mgr->interfaces[id] = di_alloc(filter_mgr, id);
  }

  return mgr;
}

int bgpstream_di_mgr_get_data_interfaces(bgpstream_di_mgr_t *di_mgr,
                                         bgpstream_data_interface_id_t **if_ids)
{
  *if_ids = di_mgr->available_dis;
  return di_mgr->available_dis_cnt;
}

bgpstream_data_interface_id_t
bgpstream_di_mgr_get_data_interface_id_by_name(bgpstream_di_mgr_t *di_mgr,
                                               const char *name)

{
  int id;

  for (id = 1; id < _BGPSTREAM_DATA_INTERFACE_CNT; id++) {
    if (bgpstream_di_mgr_get_data_interface_info(di_mgr, id) != NULL &&
        strcmp(bgpstream_di_mgr_get_data_interface_info(di_mgr, id)->name,
               name) == 0) {
      return id;
    }
  }

  return 0;
}

bgpstream_data_interface_info_t *
bgpstream_di_mgr_get_data_interface_info(bgpstream_di_mgr_t *di_mgr,
                                         bgpstream_data_interface_id_t if_id)
{
  if (get_di(di_mgr, if_id) != NULL) {
    return &get_di(di_mgr, if_id)->info;
  }
  return NULL;
}

int bgpstream_di_mgr_get_data_interface_options(
  bgpstream_di_mgr_t *di_mgr, bgpstream_data_interface_id_t if_id,
  bgpstream_data_interface_option_t **opts)
{
  if (get_di(di_mgr, if_id) != NULL) {
    *opts = get_di(di_mgr, if_id)->opts;
    return get_di(di_mgr, if_id)->opts_cnt;
  }
  *opts = NULL;
  return 0;
}

int bgpstream_di_mgr_set_data_interface(bgpstream_di_mgr_t *di_mgr,
                                         bgpstream_data_interface_id_t di_id)
{
  fprintf(stderr, "DEBUG: Setting data interface to %d\n", di_id);
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

int bgpstream_di_mgr_set_data_interface_option(bgpstream_di_mgr_t *di_mgr,
                           const bgpstream_data_interface_option_t *option_type,
                           const char *option_value)
{
  bsdi_t *di;

  /* The requested interface is unavailable */
  if ((di = get_di(di_mgr, option_type->if_id)) == NULL) {
    return -1;
  }
  return di->set_option(di, option_type, option_value);
}

int bgpstream_di_mgr_start(bgpstream_di_mgr_t *di_mgr)
{
  if (di_mgr == NULL || ACTIVE_DI == NULL) {
    return -1;
  }
  return ACTIVE_DI->start(ACTIVE_DI);
}

void bgpstream_di_mgr_set_blocking(bgpstream_di_mgr_t *di_mgr)
{
  di_mgr->blocking = 1;
}

int bgpstream_di_mgr_get_queue(bgpstream_di_mgr_t *di_mgr,
                               bgpstream_input_mgr_t *input_mgr)
{
  int queue_len;

  do {
    if ((queue_len = ACTIVE_DI->get_queue(ACTIVE_DI, input_mgr)) < 0) {
      return -1;
    }

    if (queue_len == 0 && di_mgr->blocking != 0) {
      /* go into polling mode and wait for data */
      sleep(di_mgr->backoff_time);
      if (di_mgr->retry_cnt >= DATA_INTERFACE_BLOCKING_RETRY_CNT) {
        di_mgr->backoff_time = di_mgr->backoff_time * 2;
        if (di_mgr->backoff_time > DATA_INTERFACE_BLOCKING_MAX_WAIT) {
          di_mgr->backoff_time = DATA_INTERFACE_BLOCKING_MAX_WAIT;
        }
      }
      di_mgr->retry_cnt++;
    }
  } while (queue_len == 0 && di_mgr->blocking != 0);

  /* reset polling state */
  di_mgr->backoff_time = DATA_INTERFACE_BLOCKING_MIN_WAIT;
  di_mgr->retry_cnt = 0;

  return queue_len;
}

void bgpstream_di_mgr_destroy(bgpstream_di_mgr_t *di_mgr)
{
  if (di_mgr == NULL) {
    return;
  }

  free(di_mgr->available_dis);
  di_mgr->available_dis = NULL;
  di_mgr->available_dis_cnt = 0;

  int id;
  for (id = 0; id < _BGPSTREAM_DATA_INTERFACE_CNT; id++) {
    di_destroy(di_mgr->interfaces[id]);
    di_mgr->interfaces[id] = NULL;
  }

  free(di_mgr);
  return;
}
