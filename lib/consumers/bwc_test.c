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
#include <bgpstream_utils_pfx_set.h>

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

  if(state == NULL)
    {
      return;
    }

  fprintf(stdout, "BWC-TEST: %d views processed\n",
	  STATE->view_cnt);

  /* destroy things here */
  free(state);

  BWC_SET_STATE(consumer, NULL);
}

int bwc_test_process_view(bwc_t *consumer, uint8_t interests,
			  bgpwatcher_view_t *view)
{
  bwc_test_state_t *state = STATE;

  fprintf(stdout, "BWC-TEST: Interests: ");
  bgpwatcher_consumer_interest_dump(interests);
  fprintf(stdout, "\n");

  /* only dump 'small' views, otherwise it is just obnoxious */
  if(bgpwatcher_view_pfx_cnt(view, BGPWATCHER_VIEW_FIELD_ACTIVE)
     < MAX_DUMP_SIZE)
    {
      bgpwatcher_view_dump(view);
    }
  else
    {
      fprintf(stdout, "BWC-TEST: Time:      %"PRIu32"\n",
	      bgpwatcher_view_get_time(view));
      fprintf(stdout, "BWC-TEST: IPv4-Pfxs: %"PRIu32"\n",
	      bgpwatcher_view_v4pfx_cnt(view, BGPWATCHER_VIEW_FIELD_ACTIVE));
      fprintf(stdout, "BWC-TEST: IPv6-Pfxs: %"PRIu32"\n",
	      bgpwatcher_view_v6pfx_cnt(view, BGPWATCHER_VIEW_FIELD_ACTIVE));
      fprintf(stdout, "--------------------\n");
    }

  timeseries_set_single(BWC_GET_TIMESERIES(consumer),
			"bwc-test.v4pfxs_cnt",
			bgpwatcher_view_v4pfx_cnt(view,
						  BGPWATCHER_VIEW_FIELD_ACTIVE),
			bgpwatcher_view_get_time(view));

  state->view_cnt++;

  return 0;
  
  /** End of old test consumer **/

  // TEST: add some memory to the users pointers
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
  for(bgpwatcher_view_iter_first_peer(it, BGPWATCHER_VIEW_FIELD_ACTIVE);
      bgpwatcher_view_iter_has_more_peer(it);
      bgpwatcher_view_iter_next_peer(it))
    {
      my_memory = malloc(sizeof(int));
      *(int *)my_memory = bgpwatcher_view_iter_peer_get_peer_id(it) + 100;
      bgpwatcher_view_iter_peer_set_user(it, my_memory);
      
      my_memory = NULL;            
    }

  // per-prefix user memory allocation 
  for(bgpwatcher_view_iter_first_pfx(it, BGPSTREAM_ADDR_VERSION_IPV4, BGPWATCHER_VIEW_FIELD_ACTIVE);
      bgpwatcher_view_iter_has_more_pfx(it);
      bgpwatcher_view_iter_next_pfx(it))
    {
      my_memory = malloc(sizeof(int));
      bgpwatcher_view_iter_pfx_set_user(it, my_memory);
      my_memory = NULL;
      
      // per-prefix per-peer user memory allocation
      for(bgpwatcher_view_iter_pfx_first_peer(it, BGPWATCHER_VIEW_FIELD_ACTIVE);
          bgpwatcher_view_iter_pfx_has_more_peer(it);
          bgpwatcher_view_iter_pfx_next_peer(it))
        {
          my_memory = malloc(sizeof(int));
          *(int *)my_memory = bgpwatcher_view_iter_peer_get_peer_id(it);
          bgpwatcher_view_iter_pfx_peer_set_user(it, my_memory);
          my_memory = NULL;          
        }
    }

  // TEST: use seek in peers
  /* for(int i = 1; i <= 3 ; i++) */
  /*   { */
  /*     if(!bgpwatcher_view_iter_seek_peer(it, i, BGPWATCHER_VIEW_FIELD_ALL_VALID)) */
  /*       { */
  /*         fprintf(stderr,"Peer %d not found\n",i); */
  /*       } */
  /*     else */
  /*       { */
  /*         if(bgpwatcher_view_iter_seek_peer(it, i, BGPWATCHER_VIEW_FIELD_ACTIVE)) */
  /*           { */
  /*             fprintf(stderr,"Peer %d found - [ACTIVE]\n",i); */
  /*           } */
  /*         if(bgpwatcher_view_iter_seek_peer(it, i, BGPWATCHER_VIEW_FIELD_INACTIVE)) */
  /*           { */
  /*             fprintf(stderr,"Peer %d found - [ACTIVE]\n",i); */
  /*           } */
  /*       } */
  /*   } */

  int d = 0;
  
  // TEST: check pfx-peers iterator and deactivate
  d = 0;
  for(bgpwatcher_view_iter_first_pfx_peer(it, BGPSTREAM_ADDR_VERSION_IPV4,
                                          BGPWATCHER_VIEW_FIELD_ACTIVE,
                                          BGPWATCHER_VIEW_FIELD_ACTIVE);
      bgpwatcher_view_iter_has_more_pfx_peer(it);
      bgpwatcher_view_iter_next_pfx_peer(it))
    {
      // TEST: memory is correct
      /* fprintf(stderr, "Peer id: %d, %d\n", */
      /*         *(int *)bgpwatcher_view_iter_peer_get_user(it), */
      /*         *(int *)bgpwatcher_view_iter_pfx_peer_get_user(it)); */
      if(rand()%10 > 5)
        {
          bgpwatcher_view_iter_pfx_deactivate_peer(it);
          d++;
        }
    }

  // dump view after random pfx-peer deactivation
  fprintf(stderr,"Deactivated %d pfx-peers\n", d);
  bgpwatcher_view_dump(view);

  // TEST: check pfx iterator and deactivate
  /* d = 0; */
  /* for(bgpwatcher_view_iter_first_pfx(it, BGPSTREAM_ADDR_VERSION_IPV4, */
  /*                                    BGPWATCHER_VIEW_FIELD_ACTIVE); */
  /*     bgpwatcher_view_iter_has_more_pfx(it); */
  /*     bgpwatcher_view_iter_next_pfx(it)) */
  /*   { */
  /*     if(rand()%10 > 5) */
  /*       { */
  /*         bgpwatcher_view_iter_deactivate_pfx(it); */
  /*         d++; */
  /*       } */
  /*   } */

  /* // dump view after random pfx deactivation */
  /* fprintf(stderr,"Deactivated %d pfxs\n", d); */
  /* bgpwatcher_view_dump(view); */

  // TEST: check peer iterator and deactivate
  d = 0;
  for(bgpwatcher_view_iter_first_peer(it, BGPWATCHER_VIEW_FIELD_ACTIVE);
      bgpwatcher_view_iter_has_more_peer(it);
      bgpwatcher_view_iter_next_peer(it))
    {
      if(rand()%10 > 5)
        {
          bgpwatcher_view_iter_deactivate_peer(it);
          d++;
        }
    }

  // dump view after random peer deactivation
  fprintf(stderr,"Deactivated %d peers\n", d);
  bgpwatcher_view_dump(view);



  // TEST REMOVE
    // TEST: check pfx-peers iterator and remove
  d = 0;
  for(bgpwatcher_view_iter_first_pfx_peer(it, BGPSTREAM_ADDR_VERSION_IPV4,
                                          BGPWATCHER_VIEW_FIELD_ACTIVE,
                                          BGPWATCHER_VIEW_FIELD_ACTIVE);
      bgpwatcher_view_iter_has_more_pfx_peer(it);
      bgpwatcher_view_iter_next_pfx_peer(it))
    {
      // TEST: memory is correct
      /* fprintf(stderr, "Peer id: %d, %d\n", */
      /*         *(int *)bgpwatcher_view_iter_peer_get_user(it), */
      /*         *(int *)bgpwatcher_view_iter_pfx_peer_get_user(it)); */
      if(rand()%10 > 5)
        {
          bgpwatcher_view_iter_pfx_remove_peer(it);
          d++;
        }
    }

  // dump view after random pfx-peer deactivation
  fprintf(stderr,"Removed %d pfx-peers\n", d);
  bgpwatcher_view_dump(view);

  // TEST: check pfx iterator and remove
  /* d = 0; */
  /* for(bgpwatcher_view_iter_first_pfx(it, BGPSTREAM_ADDR_VERSION_IPV4, */
  /*                                    BGPWATCHER_VIEW_FIELD_ALL_VALID); */
  /*     bgpwatcher_view_iter_has_more_pfx(it); */
  /*     bgpwatcher_view_iter_next_pfx(it)) */
  /*   { */
  /*     if(rand()%10 > 5) */
  /*       { */
  /*         bgpwatcher_view_iter_remove_pfx(it); */
  /*         d++; */
  /*       } */
  /*   } */

  /* // dump view after random pfx deactivation */
  /* fprintf(stderr,"Removed %d pfxs\n", d); */
  /* bgpwatcher_view_dump(view); */

  // TEST: check peer iterator and remove
  d = 0;
  for(bgpwatcher_view_iter_first_peer(it, BGPWATCHER_VIEW_FIELD_ALL_VALID);
      bgpwatcher_view_iter_has_more_peer(it);
      bgpwatcher_view_iter_next_peer(it))
    {
      if(rand()%10 > 5)
        {
          bgpwatcher_view_iter_remove_peer(it);
          d++;
        }
    }

  // dump view after random peer deactivation
  fprintf(stderr,"Removed %d peers\n", d);
  bgpwatcher_view_dump(view);

  // garbage collect the view
  fprintf(stderr, "Running garbage collector\n");
  bgpwatcher_view_gc(view);
  bgpwatcher_view_dump(view);
  
  // TEST: bgpview clear
  /* bgpwatcher_view_clear(view); */
  /* fprintf(stderr,"Cleared view \n"); */
  /* bgpwatcher_view_dump(view); */

  fprintf(stderr,"End of test\n");
  
  bgpwatcher_view_iter_destroy(it);

    return 0;
}
