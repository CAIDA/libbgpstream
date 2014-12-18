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
static int parse_args(bwc_t *backend, int argc, char **argv)
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
	  usage(backend);
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
  if(bgpwatcher_view_size(view) < MAX_DUMP_SIZE)
    {
      bgpwatcher_view_dump(view);
    }
  else
    {
      fprintf(stdout, "BWC-TEST: Time:      %"PRIu32"\n",
	      bgpwatcher_view_time(view));
      fprintf(stdout, "BWC-TEST: IPv4-Pfxs: %"PRIu32"\n",
	      bgpwatcher_view_v4size(view));
      fprintf(stdout, "BWC-TEST: IPv6-Pfxs: %"PRIu32"\n",
	      bgpwatcher_view_v6size(view));
    }

  fprintf(stdout, "--------------------\n");

  STATE->view_cnt++;

  return 0;
}
