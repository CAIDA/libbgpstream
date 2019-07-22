/*
 * Copyright (C) 2017 The Regents of the University of California.
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

#include "bgpstream_di_mgr.h"
#include "bgpstream_log.h"
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

#ifdef WITH_DATA_INTERFACE_KAFKA
#include "bsdi_kafka.h"
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

#ifdef WITH_DATA_INTERFACE_BETABMP
#include "bsdi_betabmp.h"
#endif

#ifdef WITH_DATA_INTERFACE_RISLIVE
#include "bsdi_rislive.h"
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

  // ID of the DI that is active
  bgpstream_data_interface_id_t active_di;

  // resource queue manager
  bgpstream_resource_mgr_t *res_mgr;

  // has the data interface been started yet?
  int started;

  // blocking query state
  int blocking;
  int backoff_time;
  int retry_cnt;

  // polling state when mixing streams and batch resources
  int next_poll;
  int poll_freq;
  int poll_cnt;
};

/** Convenience typedef for the interface alloc function type */
typedef bsdi_t *(*di_alloc_func_t)(void);

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

#ifdef WITH_DATA_INTERFACE_KAFKA
  bsdi_kafka_alloc,
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

#ifdef WITH_DATA_INTERFACE_BETABMP
  bsdi_betabmp_alloc,
#else
  NULL,
#endif

#ifdef WITH_DATA_INTERFACE_RISLIVE
  bsdi_rislive_alloc,
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
                        bgpstream_resource_mgr_t *res_mgr,
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
  di->res_mgr = res_mgr;

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
  bgpstream_data_interface_id_t id;

  if ((mgr = malloc_zero(sizeof(bgpstream_di_mgr_t))) == NULL) {
    return NULL; // can't allocate memory
  }

  // default values
  if ((mgr->res_mgr = bgpstream_resource_mgr_create(filter_mgr)) == NULL) {
    goto err;
  }
  mgr->active_di = BGPSTREAM_DATA_INTERFACE_BROKER;
  mgr->backoff_time = DATA_INTERFACE_BLOCKING_MIN_WAIT;
  mgr->poll_freq = DATA_INTERFACE_BLOCKING_MIN_WAIT;

  /* allocate the interfaces (some may/will be NULL) */
  for (id = 0; id < _BGPSTREAM_DATA_INTERFACE_CNT; id++) {
    if ((mgr->available_dis = realloc(
           mgr->available_dis, sizeof(bgpstream_data_interface_id_t) *
                                 (mgr->available_dis_cnt + 1))) == NULL) {
      goto err;
    }
    mgr->available_dis[mgr->available_dis_cnt++] = id;
    mgr->interfaces[id] = di_alloc(filter_mgr, mgr->res_mgr, id);
  }

  return mgr;

err:
  bgpstream_di_mgr_destroy(mgr);
  return NULL;
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
  bgpstream_data_interface_id_t id;

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

int bgpstream_di_mgr_get_next_record(bgpstream_di_mgr_t *di_mgr,
                                     bgpstream_record_t **record)
{
  // this function is responsible for blocking if we're in live mode
  int rc;

  while (1) {
    // if our queue is empty, or we only have stream resources and the
    // poll timer has expired, then ask the DI for more resources
    if (bgpstream_resource_mgr_empty(di_mgr->res_mgr) != 0 ||
        (bgpstream_resource_mgr_stream_only(di_mgr->res_mgr) != 0 &&
         epoch_sec() >= di_mgr->next_poll)) {

      if (ACTIVE_DI->update_resources(ACTIVE_DI) != 0) {
        // an error occurred
        return -1;
      }

      if (bgpstream_resource_mgr_stream_only(di_mgr->res_mgr) == 0) {
        // we now have some non-stream resources, so reset the
        // polling frequency
        di_mgr->poll_freq = DATA_INTERFACE_BLOCKING_MIN_WAIT;
      } else {
        // still stream-only, so let's consider backing off our polling
        if (di_mgr->poll_cnt >= DATA_INTERFACE_BLOCKING_RETRY_CNT) {
          // we've made >= 10 polls without getting anything, so back off
          di_mgr->poll_freq *= 2;
          if (di_mgr->poll_freq > DATA_INTERFACE_BLOCKING_MAX_WAIT) {
            // we've backed off our polling frequency to > 150
            // seconds, so let's revert to 150
            di_mgr->poll_freq = DATA_INTERFACE_BLOCKING_MAX_WAIT;
          }
        }
        di_mgr->poll_cnt++;
      }
      di_mgr->next_poll = epoch_sec() + di_mgr->poll_freq;
    }

    // if the queue is not empty, then grab a record
    if (bgpstream_resource_mgr_empty(di_mgr->res_mgr) == 0) {
      if ((rc = bgpstream_resource_mgr_get_record(di_mgr->res_mgr, record)) <
          0) {
        // an error occurred
        return -1;
      }
      if (rc > 0) {
        break;
      }
      // must be EOS, try immediately to refill the queue
      continue;
    } else if (di_mgr->blocking == 0) {
      // queue is empty after a fill attempt, and we're not in blocking mode, so
      // signal EOS
      rc = 0;
      break;
    }

    // either the queue was empty, or it is now
    assert(bgpstream_resource_mgr_empty(di_mgr->res_mgr) != 0);

    // we're in blocking mode, so we sleep
    if (sleep(di_mgr->backoff_time) != 0) {
      // interrupted
      return -1;
    }
    // adjust our sleep time, perhaps
    if (di_mgr->retry_cnt >= DATA_INTERFACE_BLOCKING_RETRY_CNT) {
      di_mgr->backoff_time *= 2;
      if (di_mgr->backoff_time > DATA_INTERFACE_BLOCKING_MAX_WAIT) {
        di_mgr->backoff_time = DATA_INTERFACE_BLOCKING_MAX_WAIT;
      }
    }
    di_mgr->retry_cnt++;
  }

  di_mgr->backoff_time = DATA_INTERFACE_BLOCKING_MIN_WAIT;
  di_mgr->retry_cnt = 0;

  return rc;
}

void bgpstream_di_mgr_destroy(bgpstream_di_mgr_t *di_mgr)
{
  if (di_mgr == NULL) {
    return;
  }

  bgpstream_resource_mgr_destroy(di_mgr->res_mgr);
  di_mgr->res_mgr = NULL;

  free(di_mgr->available_dis);
  di_mgr->available_dis = NULL;
  di_mgr->available_dis_cnt = 0;

  bgpstream_data_interface_id_t id;
  for (id = 0; id < _BGPSTREAM_DATA_INTERFACE_CNT; id++) {
    di_destroy(di_mgr->interfaces[id]);
    di_mgr->interfaces[id] = NULL;
  }

  free(di_mgr);
  return;
}
