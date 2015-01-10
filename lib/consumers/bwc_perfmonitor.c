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
#include "bl_bgp_utils.h"
#include "czmq.h"

#include "bgpwatcher_consumer_interface.h"

#include "bwc_perfmonitor.h"

#define NAME "perfmonitor"

#define METRIC_PREFIX "bgp.meta.bgpwatcher.consumer"

#define DUMP_METRIC(value, time, fmt, ...)                      \
do {                                                            \
 char buf[1024];                                                \
 snprintf(buf,1024, METRIC_PREFIX"."fmt, __VA_ARGS__);		\
 timeseries_set_single(BWC_GET_TIMESERIES(consumer),		\
                       buf, value, time);                       \
 } while(0)                                                     \


#define STATE					\
  (BWC_GET_STATE(consumer, perfmonitor))

static bwc_t bwc_perfmonitor = {
  BWC_ID_PERFMONITOR,
  NAME,
  BWC_GENERATE_PTRS(perfmonitor)
};

typedef struct bwc_perfmonitor_state {

  /** The number of views we have processed */
  int view_cnt;

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

int bwc_perfmonitor_process_view(bwc_t *consumer, uint8_t interests,
				 bgpwatcher_view_t *view)
{
  // view arrival delay, i.e. now - table ts
  
  DUMP_METRIC(zclock_time()/1000- bgpwatcher_view_time(view),
              bgpwatcher_view_time(view),
	      "%s", "view_arrival_delay");

  
  // get the number of peers and their table size

  bgpwatcher_view_iter_t *it;

  /* create a new iterator */
  if((it = bgpwatcher_view_iter_create(view)) == NULL)
    {
      return -1;
    }

  bl_peer_signature_t *sig;
  unsigned long long pfx4_cnt;
  unsigned long long pfx6_cnt;
  unsigned long long peer_on = 1;

  char *addr;

  for(bgpwatcher_view_iter_first(it, BGPWATCHER_VIEW_ITER_FIELD_PEER);
      !bgpwatcher_view_iter_is_end(it, BGPWATCHER_VIEW_ITER_FIELD_PEER);
      bgpwatcher_view_iter_next(it, BGPWATCHER_VIEW_ITER_FIELD_PEER))
    {
      /* grab the peer id */
      sig = bgpwatcher_view_iter_get_peersig(it);
      pfx4_cnt = bgpwatcher_view_iter_get_peer_v4pfx_cnt(it);
      pfx6_cnt = bgpwatcher_view_iter_get_peer_v6pfx_cnt(it);

      /** @todo Chiara all these bl_print_* functions should probably take a
	  char buffer and a length so that they don't have to do mallocs every
	  time */
      addr = graphite_safe(bl_print_addr_storage(&(sig->peer_ip_addr)));
      DUMP_METRIC(peer_on,
		  bgpwatcher_view_time(view),
		  "peers.%s.%s.peer_on", sig->collector_str, addr);

      DUMP_METRIC(pfx4_cnt,
		  bgpwatcher_view_time(view),
		  "peers.%s.%s.ipv4_cnt", sig->collector_str, addr);

      DUMP_METRIC(pfx6_cnt,
		  bgpwatcher_view_time(view),
		  "peers.%s.%s.ipv6_cnt", sig->collector_str, addr);
      free(addr);
    }

  /* destroy the view iterator */
  bgpwatcher_view_iter_destroy(it);

  STATE->view_cnt++;

  return 0;
}

