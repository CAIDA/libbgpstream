/*
 * bgpwatcher
 *
 * Alistair King, CAIDA, UC San Diego
 * corsaro-info@caida.org
 *
 * Copyright (C) 2012 The Regents of the University of California.
 *
 * This file is part of bgpwatcher.
 *
 * bgpwatcher is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * bgpwatcher is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with bgpwatcher.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <assert.h>
#include <stdio.h>

#include "utils.h"
#include "parse_cmd.h"

#include "bgpwatcher_consumer_manager.h"
#include "bgpwatcher_consumer_interface.h"

/* include all consumers here */

/* test consumer */
#include "bwc_test.h"

/* Per-AS Visibility consumer */
/*#include "bwc_perasvisibility.h"*/

/* ==================== PRIVATE DATA STRUCTURES ==================== */

#define MAXOPTS 1024

struct bw_consumer_manager {

  /** Array of consumers
   * @note index of consumer is given by (bwc_id_t - 1)
   */
  bwc_t *consumers[BWC_ID_LAST];

};

/** Convenience typedef for the backend alloc function type */
typedef bwc_t* (*consumer_alloc_func_t)();

/** Array of backend allocation functions.
 *
 * @note the indexes of these functions must exactly match the ID in
 * timeseries_backend_id_t. The element at index 0 MUST be NULL.
 */
static const consumer_alloc_func_t consumer_alloc_functions[] = {

  /** Pointer to test backend alloc function */
  bwc_test_alloc,

  /** Pointer to (disabled) per-as vis function */
  NULL,

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

static bwc_t *consumer_alloc(bwc_id_t id)
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

/* ==================== PUBLIC MANAGER FUNCTIONS ==================== */

bw_consumer_manager_t *bw_consumer_manager_create()
{
  bw_consumer_manager_t *mgr;
  int id;

  /* allocate some memory for our state */
  if((mgr = malloc_zero(sizeof(bw_consumer_manager_t))) == NULL)
    {
      return NULL;
    }

  /* allocate the consumers (some may/will be NULL) */
  for(id = BWC_ID_FIRST; id <= BWC_ID_LAST; id++)
    {
      mgr->consumers[id-1] = consumer_alloc(id);
    }

  return mgr;
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

  fprintf(stderr, "enabling consumer (%s)", consumer->name);

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

bwc_t **bw_consumer_manager_get_all_backends(bw_consumer_manager_t *mgr)
{
  return mgr->consumers;
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
