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
#include <unistd.h>

#include "utils.h"

#include "bgpwatcher_consumer_interface.h"

#include "bwc_test.h"

#define NAME "test"

#define MAX_DUMP_SIZE 100

#define STATE					\
  (BWC_GET_STATE(consumer, test))

static bwc_t bwc_test = {
  BWC_ID_TEST,
  NAME,
  BWC_GENERATE_PTRS(test)
};

typedef struct bwc_test_state {

  /** The number of views we have processed */
  int view_cnt;

} bwc_test_state_t;

#if 0
/** Print usage information to stderr */
static void usage(bwc_t *consumer)
{
  fprintf(stderr,
	  "consumer usage: %s\n",
	  /*	  "       -c <level>    output compression level to use (default: %d)\n" */
	  consumer->name);
}
#endif

/** Parse the arguments given to the consumer */
static int parse_args(bwc_t *consumer, int argc, char **argv)
{
  /*int opt;*/

  assert(argc > 0 && argv != NULL);

  /* NB: remember to reset optind to 1 before using getopt! */
  optind = 1;

  /* remember the argv strings DO NOT belong to us */
#if 0
  while((opt = getopt(argc, argv, ":c:f:?")) >= 0)
    {
      switch(opt)
	{
	case 'c':
	  state->compress_level = atoi(optarg);
	  break;

	case 'f':
	  state->ascii_file = strdup(optarg);
	  break;

	case '?':
	case ':':
	default:
	  usage(consumer);
	  return -1;
	}
    }
#endif

  return 0;
}

bwc_t *bwc_test_alloc()
{
  return &bwc_test;
}

int bwc_test_init(bwc_t *consumer, int argc, char **argv)
{
  bwc_test_state_t *state = NULL;

  if((state = malloc_zero(sizeof(bwc_test_state_t))) == NULL)
    {
      return -1;
    }
  BWC_SET_STATE(consumer, state);

  /* set defaults here */

  /* parse the command line args */
  if(parse_args(consumer, argc, argv) != 0)
    {
      return -1;
    }

  /* react to args here */

  return 0;
}

void bwc_test_destroy(bwc_t *consumer)
{
  bwc_test_state_t *state = STATE;

  fprintf(stdout, "BWC-TEST: %d views processed\n",
	  STATE->view_cnt);

  if(state == NULL)
    {
      return;
    }

  /* destroy things here */

  free(state);

  BWC_SET_STATE(consumer, NULL);
}

int bwc_test_process_view(bwc_t *consumer, uint8_t interests,
			  bgpwatcher_view_t *view)
{
  fprintf(stdout, "BWC-TEST: Interests: ");
  bgpwatcher_consumer_interest_dump(interests);
  fprintf(stdout, "\n");

  /* only dump 'small' views, otherwise it is just obnoxious */
  if(bgpwatcher_view_pfx_size(view) < MAX_DUMP_SIZE)
    {
      bgpwatcher_view_dump(view);
    }
  else
    {
      fprintf(stdout, "BWC-TEST: Time:      %"PRIu32"\n",
	      bgpwatcher_view_time(view));
      fprintf(stdout, "BWC-TEST: IPv4-Pfxs: %"PRIu32"\n",
	      bgpwatcher_view_v4pfx_size(view));
      fprintf(stdout, "BWC-TEST: IPv6-Pfxs: %"PRIu32"\n",
	      bgpwatcher_view_v6pfx_size(view));
      fprintf(stdout, "--------------------\n");
    }

  timeseries_set_single(BWC_GET_TIMESERIES(consumer),
			"bwc-test.v4pfxs_cnt",
			bgpwatcher_view_v4pfx_size(view),
			bgpwatcher_view_time(view));

  STATE->view_cnt++;

  // ########################################################
  // Add some memory to the users pointers
  // ########################################################

  void *my_memory = NULL;
  bgpwatcher_view_iter_t *it;
  it = bgpwatcher_view_iter_create(view);

  // set destructors
  bgpwatcher_view_set_user_destructor(view, free);
  bgpwatcher_view_set_pfx_user_destructor(view, free);
  bgpwatcher_view_set_peer_user_destructor(view, free);
  bgpwatcher_view_set_pfx_peer_user_destructor(view, free);

  // view user memory allocation
  my_memory = malloc(sizeof(int));
  bgpwatcher_view_set_user(view, my_memory);
  my_memory = NULL;

  // per-peer user memory allocation
  for(bgpwatcher_view_iter_first(it, BGPWATCHER_VIEW_ITER_FIELD_PEER, BGPWATCHER_VIEW_FIELD_ACTIVE);
      !bgpwatcher_view_iter_is_end(it, BGPWATCHER_VIEW_ITER_FIELD_PEER, BGPWATCHER_VIEW_FIELD_ACTIVE);
      bgpwatcher_view_iter_next(it, BGPWATCHER_VIEW_ITER_FIELD_PEER, BGPWATCHER_VIEW_FIELD_ACTIVE))
    {
      my_memory = malloc(sizeof(int));
      bgpwatcher_view_iter_set_peer_user(it, my_memory);
      my_memory = NULL;            
    }

  // per-prefix user memory allocation
  for(bgpwatcher_view_iter_first(it, BGPWATCHER_VIEW_ITER_FIELD_V4PFX, BGPWATCHER_VIEW_FIELD_ACTIVE);
      !bgpwatcher_view_iter_is_end(it, BGPWATCHER_VIEW_ITER_FIELD_V4PFX, BGPWATCHER_VIEW_FIELD_ACTIVE);
      bgpwatcher_view_iter_next(it, BGPWATCHER_VIEW_ITER_FIELD_V4PFX, BGPWATCHER_VIEW_FIELD_ACTIVE))
    {
      my_memory = malloc(sizeof(int));
      bgpwatcher_view_iter_set_v4pfx_user(it, my_memory);
      my_memory = NULL;
      
      // per-prefix per-peer user memory allocation
      for(bgpwatcher_view_iter_first(it, BGPWATCHER_VIEW_ITER_FIELD_V4PFX_PEER, BGPWATCHER_VIEW_FIELD_ACTIVE);
          !bgpwatcher_view_iter_is_end(it, BGPWATCHER_VIEW_ITER_FIELD_V4PFX_PEER, BGPWATCHER_VIEW_FIELD_ACTIVE);
          bgpwatcher_view_iter_next(it, BGPWATCHER_VIEW_ITER_FIELD_V4PFX_PEER, BGPWATCHER_VIEW_FIELD_ACTIVE))
        {
          my_memory = malloc(sizeof(int));
          bgpwatcher_view_iter_set_v4pfx_pfxinfo_user(it, my_memory);
          my_memory = NULL;          
        }
    }

  
  bgpwatcher_view_iter_destroy(it);
  
  return 0;
}
