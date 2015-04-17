/*
 * This file is part of bgpwatcher
 *
 * Copyright (C) 2015 The Regents of the University of California.
 * Authors: Alistair King, Chiara Orsini
 *
 * All rights reserved.
 *
 * This code has been developed by CAIDA at UC San Diego.
 * For more information, contact bgpstream-info@caida.org
 *
 * This source code is proprietary to the CAIDA group at UC San Diego and may
 * not be redistributed, published or disclosed without prior permission from
 * CAIDA.
 *
 * Report any bugs, questions or comments to bgpstream-info@caida.org
 *
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "utils.h"
#include "parse_cmd.h"

#include "bgpwatcher_consumer_manager.h"
#include "bgpwatcher_consumer_interface.h"

/* include all consumers here */

/* test consumer */
#include "bwc_test.h"

/* test consumer */
#include "bwc_perfmonitor.h"

/* Visibility consumer */
#include "bwc_visibility.h"

/* Per-AS Visibility consumer */
#include "bwc_perasvisibility.h"

/* Per-Geo Visibility consumer */
#include "bwc_pergeovisibility.h"

/* ==================== PRIVATE DATA STRUCTURES ==================== */

#define MAXOPTS 1024

struct bw_consumer_manager {

  /** Array of consumers
   * @note index of consumer is given by (bwc_id_t - 1)
   */
  bwc_t *consumers[BWC_ID_LAST];

  /** Borrowed pointer to a libtimeseries instance */
  timeseries_t *timeseries;

  /** State structure that is passed along with each view */
  bwc_chain_state_t chain_state;
};

/** Convenience typedef for the backend alloc function type */
typedef bwc_t* (*consumer_alloc_func_t)();

/** Array of backend allocation functions.
 *
 * @note the indexes of these functions must exactly match the ID-1 in
 * timeseries_backend_id_t.
 */
static const consumer_alloc_func_t consumer_alloc_functions[] = {

  /** Pointer to test backend alloc function */
  bwc_test_alloc,

  /** Pointer to performance monitor function */
  bwc_perfmonitor_alloc,

  /** Pointer to visibility alloc function */
  bwc_visibility_alloc,

  /** Pointer to per-as vis alloc function */
  bwc_perasvisibility_alloc,

  /** Pointer to per-geo vis alloc function */
  bwc_pergeovisibility_alloc,

  /** Sample conditional consumer. If enabled, point to the alloc function,
      otherwise a NULL pointer to indicate the consumer is unavailable */
  /*
    #ifdef WITH_<NAME>
    bwc_<name>_alloc,
    #else
    NULL,
    #endif
  */

};

/* ==================== PRIVATE FUNCTIONS ==================== */

static bwc_t *consumer_alloc(timeseries_t *timeseries,
                             bwc_chain_state_t *chain_state,
                             bwc_id_t id)
{
  bwc_t *consumer;
  assert(ARR_CNT(consumer_alloc_functions) == BWC_ID_LAST);

  if(consumer_alloc_functions[id-1] == NULL)
    {
      return NULL;
    }

  /* first, create the struct */
  if((consumer = malloc_zero(sizeof(bwc_t))) == NULL)
    {
      return NULL;
    }

  /* get the core consumer details (id, name, func ptrs) from the plugin */
  memcpy(consumer, consumer_alloc_functions[id-1](), sizeof(bwc_t));

  consumer->timeseries = timeseries;

  consumer->chain_state = chain_state;

  return consumer;
}

static int consumer_init(bwc_t *consumer, int argc, char **argv)
{
  assert(consumer != NULL);

  /* if it has already been initialized, then we simply return */
  if(bwc_is_enabled(consumer))
    {
      return 0;
    }

  /* otherwise, we need to init this plugin */

  /* ask the consumer to initialize. */
  if(consumer->init(consumer, argc, argv) != 0)
    {
      return -1;
    }

  consumer->enabled = 1;

  return 0;
}

static void consumer_destroy(bwc_t **consumer_p)
{
  assert(consumer_p != NULL);
  bwc_t *consumer = *consumer_p;
  *consumer_p = NULL;

  if(consumer == NULL)
    {
      return;
    }

  /* only free everything if we were enabled */
  if(bwc_is_enabled(consumer))
    {
      /* ask the backend to free it's own state */
      consumer->destroy(consumer);
    }

  /* finally, free the actual backend structure */
  free(consumer);

  return;
}

static int
init_bwc_chain_state(bw_consumer_manager_t *mgr)
{
  strcpy(mgr->chain_state.metric_prefix, BGPWATCHER_METRIC_PREFIX_DEFAULT);

  for(int i=0; i< BGPSTREAM_MAX_IP_VERSION_IDX; i++)
    {
      mgr->chain_state.full_feed_peer_ids[i] = bgpstream_id_set_create();
      mgr->chain_state.peer_ids_cnt[i] = 0;
      mgr->chain_state.full_feed_peer_asns_cnt[i] = 0;
      mgr->chain_state.usable_table_flag[i] = 0;
    }
  return 0;
}

static void
destroy_bwc_chain_state(bw_consumer_manager_t *mgr)
{
  for(int i=0; i< BGPSTREAM_MAX_IP_VERSION_IDX; i++)
    {
      if(mgr->chain_state.full_feed_peer_ids[i] != NULL)
        {
          bgpstream_id_set_destroy(mgr->chain_state.full_feed_peer_ids[i]);
          mgr->chain_state.full_feed_peer_ids[i] = NULL;
        }
    }
}


/* ==================== PUBLIC MANAGER FUNCTIONS ==================== */

bw_consumer_manager_t *bw_consumer_manager_create(timeseries_t *timeseries)
{
  bw_consumer_manager_t *mgr;
  int id;

  /* allocate some memory for our state */
  if((mgr = malloc_zero(sizeof(bw_consumer_manager_t))) == NULL)
    {
      goto err;
    }

  mgr->timeseries = timeseries;

  if(init_bwc_chain_state(mgr) < 0)
    {
      goto err;
    }

  /* allocate the consumers (some may/will be NULL) */
  for(id = BWC_ID_FIRST; id <= BWC_ID_LAST; id++)
    {
      mgr->consumers[id-1] = consumer_alloc(timeseries, &mgr->chain_state, id);
    }

  return mgr;
 err:
  bw_consumer_manager_destroy(&mgr);
  return NULL;
}

void
bw_consumer_manager_set_metric_prefix(bw_consumer_manager_t *mgr, char *metric_prefix)
{
  if(metric_prefix == NULL || strlen(metric_prefix) >= BGPWATCHER_METRIC_PREFIX_LEN)
    {
      return;
    }
  strcpy(mgr->chain_state.metric_prefix, metric_prefix);
}

void bw_consumer_manager_destroy(bw_consumer_manager_t **mgr_p)
{
  assert(mgr_p != NULL);
  bw_consumer_manager_t *mgr = *mgr_p;
  *mgr_p = NULL;
  int id;
  
  /* loop across all backends and free each one */
  for(id = BWC_ID_FIRST; id <= BWC_ID_LAST; id++)
  {
    consumer_destroy(&mgr->consumers[id-1]);
  }

  destroy_bwc_chain_state(mgr);

  free(mgr);
  return;
}

int bw_consumer_manager_enable_consumer(bwc_t *consumer, const char *options)
{
  char *local_args = NULL;
  char *process_argv[MAXOPTS];
  int len;
  int process_argc = 0;
  int rc;

  fprintf(stderr, "INFO: Enabling consumer '%s'\n", consumer->name);

  /* first we need to parse the options */
  if(options != NULL && (len = strlen(options)) > 0)
    {
      local_args = strndup(options, len);
      parse_cmd(local_args, &process_argc, process_argv, MAXOPTS,
		consumer->name);
    }
  else
    {
      process_argv[process_argc++] = (char*)consumer->name;
    }

  /* we just need to pass this along to the consumer framework */
  rc = consumer_init(consumer, process_argc, process_argv);

  if(local_args != NULL)
    {
      free(local_args);
    }

  return rc;
}

bwc_t *bw_consumer_manager_enable_consumer_from_str(bw_consumer_manager_t *mgr,
						    const char *cmd)
{
  char *strcpy = NULL;
  char *args = NULL;

  bwc_t *consumer;

  if((strcpy = strdup(cmd)) == NULL)
    {
      goto err;
    }

  if((args = strchr(strcpy, ' ')) != NULL)
    {
      /* set the space to a nul, which allows cmd to be used for the backend
	 name, and then increment args ptr to point to the next character, which
	 will be the start of the arg string (or at worst case, the terminating
	 \0 */
      *args = '\0';
      args++;
    }

  if((consumer = bw_consumer_manager_get_consumer_by_name(mgr, cmd)) == NULL)
    {
      fprintf(stderr, "ERROR: Invalid consumer name (%s)\n", cmd);
      goto err;
    }

  if(bw_consumer_manager_enable_consumer(consumer, args) != 0)
    {
      fprintf(stderr, "ERROR: Failed to initialize consumer (%s)\n", cmd);
      goto err;
    }

  free(strcpy);

  return consumer;

 err:
  if(strcpy != NULL)
    {
      free(strcpy);
    }
  return NULL;
}

bwc_t *bw_consumer_manager_get_consumer_by_id(bw_consumer_manager_t *mgr,
					      bwc_id_t id)
{
  assert(mgr != NULL);
  if(id < BWC_ID_FIRST || id > BWC_ID_LAST)
    {
      return NULL;
    }
  return mgr->consumers[id - 1];
}

bwc_t *bw_consumer_manager_get_consumer_by_name(bw_consumer_manager_t *mgr,
						const char *name)
{
  bwc_t *consumer;
  int id;

  for(id = BWC_ID_FIRST; id <= BWC_ID_LAST; id++)
    {
      if((consumer = bw_consumer_manager_get_consumer_by_id(mgr, id)) != NULL &&
	 strncasecmp(consumer->name, name, strlen(consumer->name)) == 0)
	{
	  return consumer;
	}
    }

  return NULL;
}

bwc_t **bw_consumer_manager_get_all_consumers(bw_consumer_manager_t *mgr)
{
  return mgr->consumers;
}

int bw_consumer_manager_process_view(bw_consumer_manager_t *mgr,
				     uint8_t interests,
				     bgpwatcher_view_t *view)
{
  int id;
  bwc_t *consumer;
  assert(mgr != NULL);

  for(id = BWC_ID_FIRST; id <= BWC_ID_LAST; id++)
  {
    if((consumer = bw_consumer_manager_get_consumer_by_id(mgr, id)) == NULL ||
       bwc_is_enabled(consumer) == 0)
      {
	continue;
      }
    if(consumer->process_view(consumer, interests, view) != 0)
      {
	return -1;
      }
  }

  return 0;
}


/* ==================== CONSUMER ACCESSOR FUNCTIONS ==================== */

int bwc_is_enabled(bwc_t *consumer)
{
  return consumer->enabled;
}

bwc_id_t bwc_get_id(bwc_t *consumer)
{
  return consumer->id;
}

const char *bwc_get_name(bwc_t *consumer)
{
  return consumer->name;
}
