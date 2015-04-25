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

#include "utils.h"
#include "bgpstream_utils.h"
#include "czmq.h"

#include "bgpwatcher_consumer_interface.h"

#include "bwc_perfmonitor.h"




#define NAME                        "perfmonitor"

#define BUFFER_LEN 1024
#define META_METRIC_PREFIX_FORMAT  "%s.meta.bgpwatcher.consumer."NAME



#define DUMP_METRIC(value, time, fmt, ...)                      \
do {                                                            \
 char buf[1024];                                                \
 snprintf(buf,1024, META_METRIC_PREFIX_FORMAT"."fmt, __VA_ARGS__);		\
 timeseries_set_single(BWC_GET_TIMESERIES(consumer),		\
                       buf, value, time);                       \
 } while(0)                                                     \


#define STATE					\
  (BWC_GET_STATE(consumer, perfmonitor))

#define CHAIN_STATE                             \
  (BWC_GET_CHAIN_STATE(consumer))

static bwc_t bwc_perfmonitor = {
  BWC_ID_PERFMONITOR,
  NAME,
  BWC_GENERATE_PTRS(perfmonitor)
};

typedef struct bwc_perfmonitor_state {

  /** The number of views we have processed */
  int view_cnt;

  /** Timeseries Key Package (general) */
  timeseries_kp_t *kp_gen;

} bwc_perfmonitor_state_t;

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


static char *graphite_safe(char *p)
{
  if(p == NULL)
    {
      return p;
    }

  char *r = p;
  while(*p != '\0')
    {
      if(*p == '.')
	{
	  *p = '_';
	}
      if(*p == '*')
	{
	  *p = '-';
	}
      p++;
    }
  return r;
}



/** Parse the arguments given to the consumer */
static int parse_args(bwc_t *consumer, int argc, char **argv)
{
  /*int opt;*/

  assert(argc > 0 && argv != NULL);

  /* NB: remember to reset optind to 1 before using getopt! */
  optind = 1;

  
  return 0;
}

bwc_t *bwc_perfmonitor_alloc()
{
  return &bwc_perfmonitor;
}

int bwc_perfmonitor_init(bwc_t *consumer, int argc, char **argv)
{
  bwc_perfmonitor_state_t *state = NULL;

  if((state = malloc_zero(sizeof(bwc_perfmonitor_state_t))) == NULL)
    {
      return -1;
    }
  BWC_SET_STATE(consumer, state);

  /* set defaults here */

  state->view_cnt = 0;

  /* parse the command line args */
  if(parse_args(consumer, argc, argv) != 0)
    {
      return -1;
    }

  /* react to args here */

  return 0;
}

void bwc_perfmonitor_destroy(bwc_t *consumer)
{
  bwc_perfmonitor_state_t *state = STATE;

  fprintf(stderr, "BWC-TEST: %d views processed\n",
	  STATE->view_cnt);

  if(state == NULL)
    {
      return;
    }

  /* destroy things here */

  free(state);

  BWC_SET_STATE(consumer, NULL);
}

int bwc_perfmonitor_process_view(bwc_t *consumer, uint8_t interests,
				 bgpwatcher_view_t *view)
{
  // view arrival delay, i.e. now - table ts
  
  DUMP_METRIC(zclock_time()/1000- bgpwatcher_view_get_time(view),
              bgpwatcher_view_get_time(view),
	      "%s", CHAIN_STATE->metric_prefix, "view_arrival_delay");

  
  // get the number of peers and their table size

  bgpwatcher_view_iter_t *it;

  /* create a new iterator */
  if((it = bgpwatcher_view_iter_create(view)) == NULL)
    {
      return -1;
    }

  bgpstream_peer_sig_t *sig;
  unsigned long long pfx4_cnt;
  unsigned long long pfx6_cnt;
  unsigned long long peer_on = 1;

  char addr[INET6_ADDRSTRLEN] = "";

  for(bgpwatcher_view_iter_first_peer(it, BGPWATCHER_VIEW_FIELD_ACTIVE);
      bgpwatcher_view_iter_has_more_peer(it);
      bgpwatcher_view_iter_next_peer(it))
    {
      /* grab the peer id */
      sig = bgpwatcher_view_iter_peer_get_sig(it);
      assert(sig != NULL);
      pfx4_cnt =
        bgpwatcher_view_iter_peer_get_pfx_cnt(it,
                                              BGPSTREAM_ADDR_VERSION_IPV4,
                                              BGPWATCHER_VIEW_FIELD_ACTIVE);
      pfx6_cnt =
        bgpwatcher_view_iter_peer_get_pfx_cnt(it,
                                              BGPSTREAM_ADDR_VERSION_IPV6,
                                              BGPWATCHER_VIEW_FIELD_ACTIVE);

      bgpstream_addr_ntop(addr, INET6_ADDRSTRLEN, &(sig->peer_ip_addr));
      graphite_safe(addr);
      DUMP_METRIC(peer_on,
		  bgpwatcher_view_get_time(view),
		  "peers.%s.%s.peer_on", CHAIN_STATE->metric_prefix, sig->collector_str, addr);

      DUMP_METRIC(pfx4_cnt,
		  bgpwatcher_view_get_time(view),
		  "peers.%s.%s.ipv4_cnt", CHAIN_STATE->metric_prefix, sig->collector_str, addr);

      DUMP_METRIC(pfx6_cnt,
		  bgpwatcher_view_get_time(view),
		  "peers.%s.%s.ipv6_cnt", CHAIN_STATE->metric_prefix, sig->collector_str, addr);      
    }

  /* destroy the view iterator */
  bgpwatcher_view_iter_destroy(it);

  STATE->view_cnt++;

  return 0;
}

