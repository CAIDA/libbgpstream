/*
 * bgpcorsaro
 *
 * Alistair King, CAIDA, UC San Diego
 * corsaro-info@caida.org
 *
 * Copyright (C) 2012 The Regents of the University of California.
 *
 * This file is part of bgpcorsaro.
 *
 * bgpcorsaro is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * bgpcorsaro is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with bgpcorsaro.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "config.h"
#include "bgpcorsaro_int.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "bgpstream_lib.h"
#include "bgpdump_util.h"

#include "utils.h"
#include "wandio_utils.h"

#include "bgpcorsaro_io.h"
#include "bgpcorsaro_log.h"
#include "bgpcorsaro_plugin.h"

#include "bgpcorsaro_dump.h"

/** @file
 *
 * @brief Bgpcorsaro Dump plugin implementation
 *
 * @author Alistair King
 *
 */

/** The number of output file pointers to support non-blocking close at the end
    of an interval. If the wandio buffers are large enough that it takes more
    than 1 interval to drain the buffers, consider increasing this number */
#define OUTFILE_POINTERS 2

/** The name of this plugin */
#define PLUGIN_NAME "dump"

/** The version of this plugin */
#define PLUGIN_VERSION "0.1"

/** Common plugin information across all instances */
static bgpcorsaro_plugin_t bgpcorsaro_dump_plugin = {
  PLUGIN_NAME,                                  /* name */
  PLUGIN_VERSION,                               /* version */
  BGPCORSARO_PLUGIN_ID_DUMP,                    /* id */
  BGPCORSARO_PLUGIN_GENERATE_PTRS(bgpcorsaro_dump), /* func ptrs */
  BGPCORSARO_PLUGIN_GENERATE_TAIL,
};

enum dump_mode {
  DUMP_MODE_HUMAN         = 0,
  DUMP_MODE_MACHINE_HUMAN = 1,
  DUMP_MODE_MACHINE_UNIX  = 2,
};

enum timestamp_mode {
  TIMESTAMP_MODE_DUMP     = 0,
  TIMESTAMP_MODE_CHANGE   = 1,
};

/** Holds the state for an instance of this plugin */
struct bgpcorsaro_dump_state_t {
  /** The outfile for the plugin */
  iow_t *outfile;
  /** A set of pointers to outfiles to support non-blocking close */
  iow_t *outfile_p[OUTFILE_POINTERS];
  /** The current outfile */
  int outfile_n;

  /** The dump mode that we are operating in */
  enum dump_mode dump_mode;
  /** The timestamp mode that we are operating in */
  enum timestamp_mode timestamp_mode;
};

/** Extends the generic plugin state convenience macro in bgpcorsaro_plugin.h */
#define STATE(bgpcorsaro)						\
  (BGPCORSARO_PLUGIN_STATE(bgpcorsaro, dump, BGPCORSARO_PLUGIN_ID_DUMP))
/** Extends the generic plugin plugin convenience macro in bgpcorsaro_plugin.h */
#define PLUGIN(bgpcorsaro)						\
  (BGPCORSARO_PLUGIN_PLUGIN(bgpcorsaro, BGPCORSARO_PLUGIN_ID_DUMP))

/** Print usage information to stderr */
static void usage(bgpcorsaro_plugin_t *plugin)
{
  fprintf(stderr,
	  "plugin usage: %s [-HmM] [-t mode]\n"
	  "       -H         multi-line, human-readable (default)\n"
	  "       -m         one-line per entry with unix timestamps\n"
	  "       -M         one-line per entry with human readable timestamps (and some other differences that no human could ever comprehend)\n"
	  "       -t dump    timestamps for RIB dumps reflect the time of the dump (default)\n"
	  "       -t change  timestamps for RIB dumps reflect the last route modification\n",
	  plugin->argv[0]);
}

/** Parse the arguments given to the plugin */
static int parse_args(bgpcorsaro_t *bgpcorsaro)
{
  bgpcorsaro_plugin_t *plugin = PLUGIN(bgpcorsaro);
  struct bgpcorsaro_dump_state_t *state = STATE(bgpcorsaro);
  int opt;

  if(plugin->argc <= 0)
    {
      return 0;
    }

  /* NB: remember to reset optind to 1 before using getopt! */
  optind = 1;

  while((opt = getopt(plugin->argc, plugin->argv, ":t:HmM?")) >= 0)
    {
      switch(opt)
	{
	case 'H':
	  state->dump_mode = DUMP_MODE_HUMAN;
	  break;

	case 'm':
	  state->dump_mode = DUMP_MODE_MACHINE_HUMAN;
	  break;

	case 'M':
	  state->dump_mode = DUMP_MODE_MACHINE_UNIX;
	  break;

	case 't':
	  if(strcmp(optarg,"dump")==0){
	    state->timestamp_mode = TIMESTAMP_MODE_DUMP;
	  } else if(strcmp(optarg,"change")==0){
	    state->timestamp_mode = TIMESTAMP_MODE_CHANGE;
	  } else {
	    fprintf(stderr, "Invalid argument to -t (%s)\n", optarg);
	    usage(plugin);
	    return -1;
	  }
	  break;

	case '?':
	case ':':
	default:
	  usage(plugin);
	  return -1;
	}
    }

  /* dump doesn't take any arguments */
  if(optind != plugin->argc)
    {
      usage(plugin);
      return -1;
    }

  return 0;
}

/* this code is hacked out of bgpdump.c
 * Alistair did not write it.
 * He wants as little to do with it as possible.
 */

const char *bgp_state_name[] = {
  "Unknown",
  "Idle",
  "Connect",
  "Active",
  "Opensent",
  "Openconfirm",
  "Established",
  NULL
};

/* If no aspath was present as a string in the packet, return an empty string
 * so everything stays machine parsable */
static char *attr_aspath(attributes_t *a) {
  if(a->flag & ATTR_FLAG_BIT(BGP_ATTR_AS_PATH) &&
     a->aspath && a->aspath->str) {
    return a->aspath->str;
  }
  return "";
}

#ifdef BGPDUMP_HAVE_IPV6
static void table_line_announce6(bgpcorsaro_t *bgpcorsaro,
				 struct mp_nlri *prefix,
				 int count,
				 BGPDUMP_ENTRY *entry,
				 char *time_str)
{
  struct bgpcorsaro_dump_state_t *state = STATE(bgpcorsaro);

  int idx  ;
  char buf[128];
  char buf1[128];
  char buf2[128];
  char tmp1[20];
  char tmp2[20];
  unsigned int npref;
  unsigned int nmed;

  switch (entry->attr->origin)
    {

    case 0 :
      sprintf(tmp1,"IGP");
      break;
    case 1:
      sprintf(tmp1,"EGP");
      break;
    case 2:
    default:
      sprintf(tmp1,"INCOMPLETE");
      break;
    }
  if (entry->attr->flag & ATTR_FLAG_BIT(BGP_ATTR_ATOMIC_AGGREGATE))
    sprintf(tmp2,"AG");
  else
    sprintf(tmp2,"NAG");

  for (idx=0;idx<count;idx++)
    {
      if (state->dump_mode == 1)
	{
	  switch(entry->body.zebra_message.address_family)
	    {
	    case AFI_IP6:

	      npref=entry->attr->local_pref;
	      if((entry->attr->flag & ATTR_FLAG_BIT(BGP_ATTR_LOCAL_PREF)) ==0)
		npref=0;
	      nmed=entry->attr->med;
	      if((entry->attr->flag & ATTR_FLAG_BIT(BGP_ATTR_MULTI_EXIT_DISC))
		 ==0)
		nmed=0;

	      wandio_printf(state->outfile,
			    "BGP4MP|%ld|A|%s|%u|%s/%d|%s|%s|%s|%u|%u|",
			    entry->time,
			    bgpdump_fmt_ipv6(entry->body.zebra_message.source_ip,buf1),
			    entry->body.zebra_message.source_as,
			    bgpdump_fmt_ipv6(prefix->nlri[idx].address,buf2),
			    prefix->nlri[idx].len,
			    attr_aspath(entry->attr),
			    tmp1,
			    bgpdump_fmt_ipv6(prefix->nexthop,buf),
			    npref,
			    nmed);
	      break;
	    case AFI_IP:
	    default:

	      npref=entry->attr->local_pref;
	      if((entry->attr->flag & ATTR_FLAG_BIT(BGP_ATTR_LOCAL_PREF)) ==0)
		npref=0;
	      nmed=entry->attr->med;
	      if((entry->attr->flag & ATTR_FLAG_BIT(BGP_ATTR_MULTI_EXIT_DISC))
		 ==0)
		nmed=0;

	      //wandio_printf(state->outfile, "%s|%d|%d|",inet_ntoa(entry->attr->nexthop),nprof,nmed);
	      wandio_printf(state->outfile, "BGP4MP|%ld|A|%s|%u|%s/%d|%s|%s|%s|%u|%u|",
		     entry->time,
		     bgpdump_fmt_ipv4(entry->body.zebra_message.source_ip,buf1),
		     entry->body.zebra_message.source_as,
		     bgpdump_fmt_ipv6(prefix->nlri[idx].address,buf2),
		     prefix->nlri[idx].len,attr_aspath(entry->attr),
		     tmp1,
		     bgpdump_fmt_ipv6(prefix->nexthop,buf),
		     npref,
		     nmed);
	      break;
	    }
	  if((entry->attr->flag & ATTR_FLAG_BIT(BGP_ATTR_COMMUNITIES)) !=0)
	    wandio_printf(state->outfile, "%s|%s|",
		   entry->attr->community->str+1,
		   tmp2);
	  else
	    wandio_printf(state->outfile, "|%s|",tmp2);


	  if (entry->attr->aggregator_addr.s_addr != -1)
	    wandio_printf(state->outfile, "%u %s|\n",
		   entry->attr->aggregator_as,
		   inet_ntoa(entry->attr->aggregator_addr));
	  else
	    wandio_printf(state->outfile, "|\n");

	}
      else
	{
	  switch(entry->body.zebra_message.address_family)
	    {
	    case AFI_IP6:
	      wandio_printf(state->outfile, "BGP4MP|%s|A|%s|%u|%s/%d|%s|%s\n",time_str,bgpdump_fmt_ipv6(entry->body.zebra_message.source_ip,buf1),entry->body.zebra_message.source_as,bgpdump_fmt_ipv6(prefix->nlri[idx].address,buf),prefix->nlri[idx].len,attr_aspath(entry->attr),tmp1);
	      break;
	    case AFI_IP:
	    default:
	      wandio_printf(state->outfile, "BGP4MP|%s|A|%s|%u|%s/%d|%s|%s\n",time_str,bgpdump_fmt_ipv4(entry->body.zebra_message.source_ip,buf1),entry->body.zebra_message.source_as,bgpdump_fmt_ipv6(prefix->nlri[idx].address,buf),prefix->nlri[idx].len,attr_aspath(entry->attr),tmp1);
	      break;
	    }
	}

    }

}
#endif

static void table_line_announce_1(bgpcorsaro_t *bgpcorsaro,
				  struct mp_nlri *prefix,
				  int count,
				  BGPDUMP_ENTRY *entry,
				  char *time_str)
{
  struct bgpcorsaro_dump_state_t *state = STATE(bgpcorsaro);

  int idx  ;
  char buf[128];
  //char buf1[128];
  //char buf2[128];
  char tmp1[20];
  char tmp2[20];
  unsigned int npref;
  unsigned int nmed;

  switch (entry->attr->origin)
    {

    case 0 :
      sprintf(tmp1,"IGP");
      break;
    case 1:
      sprintf(tmp1,"EGP");
      break;
    case 2:
    default:
      sprintf(tmp1,"INCOMPLETE");
      break;
    }
  if (entry->attr->flag & ATTR_FLAG_BIT(BGP_ATTR_ATOMIC_AGGREGATE))
    sprintf(tmp2,"AG");
  else
    sprintf(tmp2,"NAG");

  for (idx=0;idx<count;idx++)
    {
      if (state->dump_mode == 1)
	{
	  if (entry->attr->flag & ATTR_FLAG_BIT(BGP_ATTR_MP_REACH_NLRI))
	    {
	      switch(entry->body.zebra_message.address_family)
		{
#ifdef BGPDUMP_HAVE_IPV6
		case AFI_IP6:
		  wandio_printf(state->outfile, "BGP4MP|%ld|A|%s|%u|",entry->time,bgpdump_fmt_ipv6(entry->body.zebra_message.source_ip,buf),entry->body.zebra_message.source_as);
		  break;
#endif
		case AFI_IP:
		default:
		  wandio_printf(state->outfile, "BGP4MP|%ld|A|%s|%u|",entry->time,inet_ntoa(entry->body.zebra_message.source_ip.v4_addr),entry->body.zebra_message.source_as);
		  break;
		}
	      wandio_printf(state->outfile, "%s/%d|%s|%s|",inet_ntoa(prefix->nlri[idx].address.v4_addr),prefix->nlri[idx].len,attr_aspath(entry->attr),tmp1);

	      npref=entry->attr->local_pref;
	      if( (entry->attr->flag & ATTR_FLAG_BIT(BGP_ATTR_LOCAL_PREF) ) ==0)
		npref=0;
	      nmed=entry->attr->med;
	      if( (entry->attr->flag & ATTR_FLAG_BIT(BGP_ATTR_MULTI_EXIT_DISC) ) ==0)
		nmed=0;

	      wandio_printf(state->outfile, "%s|%d|%d|",inet_ntoa(entry->attr->nexthop),npref,nmed);
	      //wandio_printf(state->outfile, "%s|%d|%d|",inet_ntoa(prefix->nexthop.v4_addr),entry->attr->local_pref,entry->attr->med);
	      if( (entry->attr->flag & ATTR_FLAG_BIT(BGP_ATTR_COMMUNITIES) ) !=0)
		wandio_printf(state->outfile, "%s|%s|",entry->attr->community->str+1,tmp2);
	      else
		wandio_printf(state->outfile, "|%s|",tmp2);

	    }
	  else
	    {
	      switch(entry->body.zebra_message.address_family)
		{
#ifdef BGPDUMP_HAVE_IPV6
		case AFI_IP6:
		  wandio_printf(state->outfile, "BGP4MP|%ld|A|%s|%u|",entry->time,bgpdump_fmt_ipv6(entry->body.zebra_message.source_ip,buf),entry->body.zebra_message.source_as);
		  break;
#endif
		case AFI_IP:
		default:
		  wandio_printf(state->outfile, "BGP4MP|%ld|A|%s|%u|",entry->time,inet_ntoa(entry->body.zebra_message.source_ip.v4_addr),entry->body.zebra_message.source_as);
		  break;
		}
	      wandio_printf(state->outfile, "%s/%d|%s|%s|",inet_ntoa(prefix->nlri[idx].address.v4_addr),prefix->nlri[idx].len,attr_aspath(entry->attr),tmp1);

	      npref=entry->attr->local_pref;
	      if( (entry->attr->flag & ATTR_FLAG_BIT(BGP_ATTR_LOCAL_PREF) ) ==0)
		npref=0;
	      nmed=entry->attr->med;
	      if( (entry->attr->flag & ATTR_FLAG_BIT(BGP_ATTR_MULTI_EXIT_DISC) ) ==0)
		nmed=0;

	      wandio_printf(state->outfile, "%s|%d|%d|",inet_ntoa(entry->attr->nexthop),npref,nmed);
	      //wandio_printf(state->outfile, "%s|%d|%d|",inet_ntoa(entry->attr->nexthop),entry->attr->local_pref,entry->attr->med);
	      if( (entry->attr->flag & ATTR_FLAG_BIT(BGP_ATTR_COMMUNITIES) ) !=0)
		wandio_printf(state->outfile, "%s|%s|",entry->attr->community->str+1,tmp2);
	      else
		wandio_printf(state->outfile, "|%s|",tmp2);


	    }
	  if (entry->attr->aggregator_addr.s_addr != -1)
	    wandio_printf(state->outfile, "%u %s|\n",entry->attr->aggregator_as,inet_ntoa(entry->attr->aggregator_addr));
	  else
	    wandio_printf(state->outfile, "|\n");
	}
      else
	{
	  switch(entry->body.zebra_message.address_family)
	    {
#ifdef BGPDUMP_HAVE_IPV6
	    case AFI_IP6:
	      wandio_printf(state->outfile, "BGP4MP|%s|A|%s|%u|",time_str,bgpdump_fmt_ipv6(entry->body.zebra_message.source_ip,buf),entry->body.zebra_message.source_as);
	      break;
#endif
	    case AFI_IP:
	    default:
	      wandio_printf(state->outfile, "BGP4MP|%s|A|%s|%u|",time_str,inet_ntoa(entry->body.zebra_message.source_ip.v4_addr),entry->body.zebra_message.source_as);
	      break;
	    }
	  wandio_printf(state->outfile, "%s/%d|%s|%s\n",inet_ntoa(prefix->nlri[idx].address.v4_addr),prefix->nlri[idx].len,attr_aspath(entry->attr),tmp1);

	}
    }

}

static void table_line_announce(bgpcorsaro_t *bgpcorsaro,
				struct prefix *prefix,
				int count,
				BGPDUMP_ENTRY *entry,
				char *time_str)
{
  struct bgpcorsaro_dump_state_t *state = STATE(bgpcorsaro);

  int idx  ;
  char buf[128];
  //char buf1[128];
  //char buf2[128];
  char tmp1[20];
  char tmp2[20];
  unsigned int npref;
  unsigned int nmed;

  switch (entry->attr->origin)
    {

    case 0 :
      sprintf(tmp1,"IGP");
      break;
    case 1:
      sprintf(tmp1,"EGP");
      break;
    case 2:
    default:
      sprintf(tmp1,"INCOMPLETE");
      break;
    }
  if (entry->attr->flag & ATTR_FLAG_BIT(BGP_ATTR_ATOMIC_AGGREGATE))
    sprintf(tmp2,"AG");
  else
    sprintf(tmp2,"NAG");

  for (idx=0;idx<count;idx++)
    {
      if (state->dump_mode == 1)
	{
	  switch(entry->body.zebra_message.address_family)
	    {
#ifdef BGPDUMP_HAVE_IPV6
	    case AFI_IP6:
	      wandio_printf(state->outfile, "BGP4MP|%ld|A|%s|%u|",entry->time,bgpdump_fmt_ipv6(entry->body.zebra_message.source_ip,buf),entry->body.zebra_message.source_as);
	      break;
#endif
	    case AFI_IP:
	    default:
	      wandio_printf(state->outfile, "BGP4MP|%ld|A|%s|%u|",entry->time,inet_ntoa(entry->body.zebra_message.source_ip.v4_addr),entry->body.zebra_message.source_as);
	      break;
	    }
	  wandio_printf(state->outfile, "%s/%d|%s|%s|",inet_ntoa(prefix[idx].address.v4_addr),prefix[idx].len,attr_aspath(entry->attr),tmp1);
	  npref=entry->attr->local_pref;
	  if( (entry->attr->flag & ATTR_FLAG_BIT(BGP_ATTR_LOCAL_PREF) ) ==0)
	    npref=0;
	  nmed=entry->attr->med;
	  if( (entry->attr->flag & ATTR_FLAG_BIT(BGP_ATTR_MULTI_EXIT_DISC) ) ==0)
	    nmed=0;

	  wandio_printf(state->outfile, "%s|%u|%u|",inet_ntoa(entry->attr->nexthop),npref,nmed);
	  if( (entry->attr->flag & ATTR_FLAG_BIT(BGP_ATTR_COMMUNITIES) ) !=0)
	    wandio_printf(state->outfile, "%s|%s|",entry->attr->community->str+1,tmp2);
	  else
	    wandio_printf(state->outfile, "|%s|",tmp2);

	  if (entry->attr->aggregator_addr.s_addr != -1)
	    wandio_printf(state->outfile, "%u %s|\n",entry->attr->aggregator_as,inet_ntoa(entry->attr->aggregator_addr));
	  else
	    wandio_printf(state->outfile, "|\n");
	}
      else
	{
	  switch(entry->body.zebra_message.address_family)
	    {
#ifdef BGPDUMP_HAVE_IPV6
	    case AFI_IP6:
	      wandio_printf(state->outfile, "BGP4MP|%s|A|%s|%u|",time_str,bgpdump_fmt_ipv6(entry->body.zebra_message.source_ip,buf),entry->body.zebra_message.source_as);
	      break;
#endif
	    case AFI_IP:
	    default:
	      wandio_printf(state->outfile, "BGP4MP|%s|A|%s|%u|",time_str,inet_ntoa(entry->body.zebra_message.source_ip.v4_addr),entry->body.zebra_message.source_as);
	      break;
	    }
	  wandio_printf(state->outfile, "%s/%d|%s|%s\n",inet_ntoa(prefix[idx].address.v4_addr),prefix[idx].len,attr_aspath(entry->attr),tmp1);

	}
    }

}

static void table_line_withdraw(bgpcorsaro_t *bgpcorsaro,
				struct prefix *prefix,
				int count,
				BGPDUMP_ENTRY *entry,
				char *time_str)
{
  struct bgpcorsaro_dump_state_t *state = STATE(bgpcorsaro);

  int idx;
  char buf[128];

  for (idx=0;idx<count;idx++)
    {
      if (state->dump_mode==1)
	{
	  switch(entry->body.zebra_message.address_family)
	    {
#ifdef BGPDUMP_HAVE_IPV6
	    case AFI_IP6:
	      wandio_printf(state->outfile, "BGP4MP|%ld|W|%s|%u|",
		     entry->time,
		     bgpdump_fmt_ipv6(entry->body.zebra_message.source_ip,buf),
		     entry->body.zebra_message.source_as);
	      break;
#endif
	    case AFI_IP:
	    default:
	      wandio_printf(state->outfile, "BGP4MP|%ld|W|%s|%u|",
		     entry->time,
		     inet_ntoa(entry->body.zebra_message.source_ip.v4_addr),
		     entry->body.zebra_message.source_as);
	      break;
	    }
	  wandio_printf(state->outfile, "%s/%d\n",inet_ntoa(prefix[idx].address.v4_addr),prefix[idx].len);
	}
      else
	{
	  switch(entry->body.zebra_message.address_family)
	    {
#ifdef BGPDUMP_HAVE_IPV6
	    case AFI_IP6:
	      wandio_printf(state->outfile, "BGP4MP|%s|W|%s|%u|",
		     time_str,
		     bgpdump_fmt_ipv6(entry->body.zebra_message.source_ip,buf),
		     entry->body.zebra_message.source_as);
	      break;
#endif
	    case AFI_IP:
	    default:
	      wandio_printf(state->outfile, "BGP4MP|%s|W|%s|%u|",
		     time_str,
		     inet_ntoa(entry->body.zebra_message.source_ip.v4_addr),
		     entry->body.zebra_message.source_as);
	      break;
	    }
	  wandio_printf(state->outfile, "%s/%d\n",inet_ntoa(prefix[idx].address.v4_addr),prefix[idx].len);
	}

    }
}

#ifdef BGPDUMP_HAVE_IPV6

static void table_line_withdraw6(bgpcorsaro_t *bgpcorsaro,
				 struct prefix *prefix,
				 int count,
				 BGPDUMP_ENTRY *entry,
				 char *time_str)
{
  struct bgpcorsaro_dump_state_t *state = STATE(bgpcorsaro);

  int idx;
  char buf[128];
  char buf1[128];

  for (idx=0;idx<count;idx++)
    {
      if (state->dump_mode==1)
	{
	  switch(entry->body.zebra_message.address_family)
	    {
	    case AFI_IP6:
	      wandio_printf(state->outfile, "BGP4MP|%ld|W|%s|%u|%s/%d\n",
		     entry->time,
		     bgpdump_fmt_ipv6(entry->body.zebra_message.source_ip,buf1),
		     entry->body.zebra_message.source_as,
		     bgpdump_fmt_ipv6(prefix[idx].address,buf),prefix[idx].len);
	      break;
	    case AFI_IP:
	    default:
	      wandio_printf(state->outfile, "BGP4MP|%ld|W|%s|%u|%s/%d\n",
		     entry->time,
		     bgpdump_fmt_ipv4(entry->body.zebra_message.source_ip,buf1),
		     entry->body.zebra_message.source_as,
		     bgpdump_fmt_ipv6(prefix[idx].address,buf),prefix[idx].len);
	      break;
	    }
	}
      else
	{
	  switch(entry->body.zebra_message.address_family)
	    {
	    case AFI_IP6:
	      wandio_printf(state->outfile, "BGP4MP|%s|W|%s|%u|%s/%d\n",
		     time_str,
		     bgpdump_fmt_ipv6(entry->body.zebra_message.source_ip,buf1),
		     entry->body.zebra_message.source_as,
		     bgpdump_fmt_ipv6(prefix[idx].address,buf),prefix[idx].len);
	      break;
	    case AFI_IP:
	    default:
	      wandio_printf(state->outfile, "BGP4MP|%s|W|%s|%u|%s/%d\n",
		     time_str,
		     bgpdump_fmt_ipv4(entry->body.zebra_message.source_ip,buf1),
		     entry->body.zebra_message.source_as,
		     bgpdump_fmt_ipv6(prefix[idx].address,buf),prefix[idx].len);
	      break;
	    }
	}

    }
}
#endif

void show_prefixes(bgpcorsaro_t *bgpcorsaro,
		   int count,
		   struct prefix *prefix)
{
  struct bgpcorsaro_dump_state_t *state = STATE(bgpcorsaro);
  int i;
  for(i=0;i<count;i++)
    {
      wandio_printf(state->outfile,
		    "  %s/%d\n",
		    inet_ntoa(prefix[i].address.v4_addr),
		    prefix[i].len);
    }
}

#ifdef BGPDUMP_HAVE_IPV6
void show_prefixes6(bgpcorsaro_t* bgpcorsaro,
		    int count,
		    struct prefix *prefix)
{
  struct bgpcorsaro_dump_state_t *state = STATE(bgpcorsaro);
  int i;
 char buf[128];

 for (i=0;i<count;i++)
   wandio_printf(state->outfile, "  %s/%d\n",bgpdump_fmt_ipv6(prefix[i].address,buf),prefix[i].len);
}
#endif

static char *describe_origin(int origin) {
  if(origin == 0) return "IGP";
  if(origin == 1) return "EGP";
  return "INCOMPLETE";
}

static void table_line_dump_v2_prefix(bgpcorsaro_t *bgpcorsaro,
				      BGPDUMP_TABLE_DUMP_V2_PREFIX *e,
				      BGPDUMP_ENTRY *entry)
{
  struct bgpcorsaro_dump_state_t *state = STATE(bgpcorsaro);

  struct tm *date = NULL;
  unsigned int npref;
  unsigned int nmed;
  char  time_str[20];
  char peer[BGPDUMP_ADDRSTRLEN], prefix[BGPDUMP_ADDRSTRLEN], nexthop[BGPDUMP_ADDRSTRLEN];

  int i;

  for(i = 0; i < e->entry_count; i++) {
    attributes_t *attr = e->entries[i].attr;
    if(! attr)
      continue;

    char *origin = describe_origin(attr->origin);
    char *aspath_str = (attr->aspath) ? attr->aspath->str: "";
    char *aggregate = attr->flag & ATTR_FLAG_BIT(BGP_ATTR_ATOMIC_AGGREGATE) ? "AG" : "NAG";

    if(e->entries[i].peer.afi == AFI_IP){
      bgpdump_fmt_ipv4(e->entries[i].peer.peer_ip, peer);
#ifdef BGPDUMP_HAVE_IPV6
    } else if(e->entries[i].peer.afi == AFI_IP6){
      bgpdump_fmt_ipv6(e->entries[i].peer.peer_ip, peer);
#endif
    }

    if(e->afi == AFI_IP) {
      bgpdump_fmt_ipv4(e->prefix, prefix);
#ifdef BGPDUMP_HAVE_IPV6
    } else if(e->afi == AFI_IP6) {
      bgpdump_fmt_ipv6(e->prefix, prefix);
#endif
    }

    if (state->dump_mode == 1)
      {
	if(state->timestamp_mode==0){
	  wandio_printf(state->outfile, "TABLE_DUMP2|%ld|B|%s|%u|",entry->time,peer,e->entries[i].peer.peer_as);
	}else if(state->timestamp_mode==1){
	  wandio_printf(state->outfile, "TABLE_DUMP2|%u|B|%s|%u|",e->entries[i].originated_time,peer,e->entries[i].peer.peer_as);
	}
	wandio_printf(state->outfile, "%s/%d|%s|%s|",prefix,e->prefix_length,aspath_str,origin);

	npref=attr->local_pref;
	if( (attr->flag & ATTR_FLAG_BIT(BGP_ATTR_LOCAL_PREF) ) ==0)
	  npref=0;
	nmed=attr->med;
	if( (attr->flag & ATTR_FLAG_BIT(BGP_ATTR_MULTI_EXIT_DISC) ) ==0)
	  nmed=0;

#ifdef BGPDUMP_HAVE_IPV6
	if ((attr->flag & ATTR_FLAG_BIT(BGP_ATTR_MP_REACH_NLRI)) && attr->mp_info->announce[AFI_IP6][SAFI_UNICAST])
	  {
	    bgpdump_fmt_ipv6(attr->mp_info->announce[AFI_IP6][SAFI_UNICAST]->nexthop,nexthop);
	  }
	else
#endif
	  {
	    strncpy(nexthop, inet_ntoa(attr->nexthop), BGPDUMP_ADDRSTRLEN);
	  }
	wandio_printf(state->outfile, "%s|%u|%u|",nexthop,npref,nmed);

	if( (attr->flag & ATTR_FLAG_BIT(BGP_ATTR_COMMUNITIES) ) !=0)
	  wandio_printf(state->outfile, "%s|%s|",attr->community->str+1,aggregate);
	else
	  wandio_printf(state->outfile, "|%s|",aggregate);

	if (attr->aggregator_addr.s_addr != -1)
	  wandio_printf(state->outfile, "%u %s|\n",attr->aggregator_as,inet_ntoa(attr->aggregator_addr));
	else
	  wandio_printf(state->outfile, "|\n");
      }
    else
      {
	if(state->timestamp_mode==0){
	  date=gmtime(&entry->time);
	}else if(state->timestamp_mode==1){
	  time_t time_temp = (time_t)((e->entries[i]).originated_time);
	  date=gmtime(&time_temp);
	}
	bgpdump_time2str(date,time_str);
	wandio_printf(state->outfile, "TABLE_DUMP_V2|%s|A|%s|%u|",time_str,peer,e->entries[i].peer.peer_as);
	wandio_printf(state->outfile, "%s/%d|%s|%s\n",prefix,e->prefix_length,aspath_str,origin);

      }
  }

}

static void table_line_mrtd_route(bgpcorsaro_t *bgpcorsaro,
				  BGPDUMP_MRTD_TABLE_DUMP *route,
				  BGPDUMP_ENTRY *entry)
{
  struct bgpcorsaro_dump_state_t *state = STATE(bgpcorsaro);

  struct tm *date = NULL;
  char tmp1[20];
  char tmp2[20];
  unsigned int npref;
  unsigned int nmed;
  char  time_str[20];
  char peer[BGPDUMP_ADDRSTRLEN], prefix[BGPDUMP_ADDRSTRLEN], nexthop[BGPDUMP_ADDRSTRLEN];

  switch (entry->attr->origin)
    {

    case 0 :
      sprintf(tmp1,"IGP");
      break;
    case 1:
      sprintf(tmp1,"EGP");
      break;
    case 2:
    default:
      sprintf(tmp1,"INCOMPLETE");
      break;
    }
  if (entry->attr->flag & ATTR_FLAG_BIT(BGP_ATTR_ATOMIC_AGGREGATE))
    sprintf(tmp2,"AG");
  else
    sprintf(tmp2,"NAG");

#ifdef BGPDUMP_HAVE_IPV6
  if (entry->subtype == AFI_IP6)
    {
      bgpdump_fmt_ipv6(route->peer_ip,peer);
      bgpdump_fmt_ipv6(route->prefix,prefix);
    }
  else
#endif
    {
      strncpy(peer, inet_ntoa(route->peer_ip.v4_addr), BGPDUMP_ADDRSTRLEN);
      strncpy(prefix, inet_ntoa(route->prefix.v4_addr), BGPDUMP_ADDRSTRLEN);
    }

  if (state->dump_mode == 1)
    {
      if(state->timestamp_mode==0){
	wandio_printf(state->outfile, "TABLE_DUMP|%ld|B|%s|%u|",entry->time,peer,route->peer_as);
      }else if(state->timestamp_mode==1){
	wandio_printf(state->outfile, "TABLE_DUMP|%ld|B|%s|%u|",route->uptime,peer,route->peer_as);
      }
      wandio_printf(state->outfile, "%s/%d|%s|%s|",prefix,route->mask,attr_aspath(entry->attr),tmp1);

      npref=entry->attr->local_pref;
      if( (entry->attr->flag & ATTR_FLAG_BIT(BGP_ATTR_LOCAL_PREF) ) ==0)
	npref=0;
      nmed=entry->attr->med;
      if( (entry->attr->flag & ATTR_FLAG_BIT(BGP_ATTR_MULTI_EXIT_DISC) ) ==0)
	nmed=0;

#ifdef BGPDUMP_HAVE_IPV6
      if ((entry->attr->flag & ATTR_FLAG_BIT(BGP_ATTR_MP_REACH_NLRI)) && entry->attr->mp_info->announce[AFI_IP6][SAFI_UNICAST])
	{
	  bgpdump_fmt_ipv6(entry->attr->mp_info->announce[AFI_IP6][SAFI_UNICAST]->nexthop,nexthop);
	}
      else
#endif
	{
	  strncpy(nexthop, inet_ntoa(entry->attr->nexthop), BGPDUMP_ADDRSTRLEN);
	}
      wandio_printf(state->outfile, "%s|%u|%u|",nexthop,npref,nmed);

      if( (entry->attr->flag & ATTR_FLAG_BIT(BGP_ATTR_COMMUNITIES) ) !=0)
	wandio_printf(state->outfile, "%s|%s|",entry->attr->community->str+1,tmp2);
      else
	wandio_printf(state->outfile, "|%s|",tmp2);

      if (entry->attr->aggregator_addr.s_addr != -1)
	wandio_printf(state->outfile, "%u %s|\n",entry->attr->aggregator_as,inet_ntoa(entry->attr->aggregator_addr));
      else
	wandio_printf(state->outfile, "|\n");
    }
  else
    {
      if(state->timestamp_mode==0){
	date=gmtime(&entry->time);
      }else if(state->timestamp_mode==1){
	date=gmtime(&route->uptime);
      }
      bgpdump_time2str(date,time_str);
      wandio_printf(state->outfile, "TABLE_DUMP|%s|A|%s|%u|",time_str,peer,route->peer_as);
      wandio_printf(state->outfile, "%s/%d|%s|%s\n",prefix,route->mask,attr_aspath(entry->attr),tmp1);

    }

}

static void show_attr(bgpcorsaro_t *bgpcorsaro, attributes_t *attr) {
  struct bgpcorsaro_dump_state_t *state = STATE(bgpcorsaro);

  if(attr != NULL) {

    if( (attr->flag & ATTR_FLAG_BIT (BGP_ATTR_ORIGIN) ) !=0 )
      {
	switch (attr->origin)
	  {
	  case 0:
	    wandio_printf(state->outfile, "ORIGIN: IGP\n");
	    break;
	  case 1:
	    wandio_printf(state->outfile, "ORIGIN: EGP\n");
	    break;
	  case 2:
	    wandio_printf(state->outfile, "ORIGIN: INCOMPLETE\n");

	  }

      }

    if( (attr->flag & ATTR_FLAG_BIT(BGP_ATTR_AS_PATH) ) !=0)
      wandio_printf(state->outfile, "ASPATH: %s\n",attr->aspath->str);

    if( (attr->flag & ATTR_FLAG_BIT(BGP_ATTR_NEXT_HOP) ) !=0)
      wandio_printf(state->outfile, "NEXT_HOP: %s\n",inet_ntoa(attr->nexthop));

    if( (attr->flag & ATTR_FLAG_BIT(BGP_ATTR_MULTI_EXIT_DISC) ) !=0)
      wandio_printf(state->outfile, "MULTI_EXIT_DISC: %u\n",attr->med);

    if( (attr->flag & ATTR_FLAG_BIT(BGP_ATTR_LOCAL_PREF) ) !=0)
      wandio_printf(state->outfile, "LOCAL_PREF: %u\n",attr->local_pref);

    if( (attr->flag & ATTR_FLAG_BIT(BGP_ATTR_ATOMIC_AGGREGATE) ) !=0)
      wandio_printf(state->outfile, "ATOMIC_AGGREGATE\n");

    if( (attr->flag & ATTR_FLAG_BIT(BGP_ATTR_AGGREGATOR) ) !=0)
      wandio_printf(state->outfile, "AGGREGATOR: AS%u %s\n",attr->aggregator_as,inet_ntoa(attr->aggregator_addr));

    if( (attr->flag & ATTR_FLAG_BIT(BGP_ATTR_ORIGINATOR_ID) ) !=0)
      wandio_printf(state->outfile, "ORIGINATOR_ID: %s\n",inet_ntoa(attr->originator_id));

    if( (attr->flag & ATTR_FLAG_BIT(BGP_ATTR_CLUSTER_LIST) ) !=0)
      {
	int cluster_index;

	wandio_printf(state->outfile, "CLUSTER_LIST: ");

	for (cluster_index = 0;cluster_index<attr->cluster->length;cluster_index++)
	  wandio_printf(state->outfile, "%s ",inet_ntoa(attr->cluster->list[cluster_index]));
	wandio_printf(state->outfile, "\n");
      }

    int idx;
    for (idx=0;idx<attr->unknown_num;idx++)
      {
	struct unknown_attr *unknown = attr->unknown + idx;
	wandio_printf(state->outfile, "   UNKNOWN_ATTR(%i, %i, %i):", unknown->flag, unknown->type, unknown->len);
	int b;
	for(b = 0; b < unknown->len; ++b)
	  wandio_printf(state->outfile, " %02x", unknown->raw[b]);
	wandio_printf(state->outfile, "\n");
      }

    if( (attr->flag & ATTR_FLAG_BIT(BGP_ATTR_MP_REACH_NLRI) )!=0)
      {
	wandio_printf(state->outfile, "MP_REACH_NLRI");
#ifdef BGPDUMP_HAVE_IPV6
	if (attr->mp_info->announce[AFI_IP6][SAFI_UNICAST] || attr->mp_info->announce[AFI_IP6][SAFI_MULTICAST] || attr->mp_info->announce[AFI_IP6][SAFI_UNICAST_MULTICAST])

	  {
	    char buf[128];

	    if (attr->mp_info->announce[AFI_IP6][SAFI_UNICAST])
	      {
		wandio_printf(state->outfile, "(IPv6 Unicast)\n");
		wandio_printf(state->outfile, "NEXT_HOP: %s\n",bgpdump_fmt_ipv6(attr->mp_info->announce[AFI_IP6][SAFI_UNICAST]->nexthop,buf));
		if (attr->mp_info->announce[AFI_IP6][SAFI_UNICAST]->nexthop_len==32)
		  wandio_printf(state->outfile, "NEXT_HOP: %s\n",bgpdump_fmt_ipv6(attr->mp_info->announce[AFI_IP6][SAFI_UNICAST]->nexthop_local,buf));
	      }
	    else if (attr->mp_info->announce[AFI_IP6][SAFI_MULTICAST])
	      {
		wandio_printf(state->outfile, "(IPv6 Multicast)\n");
		wandio_printf(state->outfile, "NEXT_HOP: %s\n",bgpdump_fmt_ipv6(attr->mp_info->announce[AFI_IP6][SAFI_MULTICAST]->nexthop,buf));
		if (attr->mp_info->announce[AFI_IP6][SAFI_MULTICAST]->nexthop_len==32)
		  wandio_printf(state->outfile, "NEXT_HOP: %s\n",bgpdump_fmt_ipv6(attr->mp_info->announce[AFI_IP6][SAFI_MULTICAST]->nexthop_local,buf));

	      }
	    else
	      {
		wandio_printf(state->outfile, "(IPv6 Both unicast and multicast)\n");
		wandio_printf(state->outfile, "NEXT_HOP: %s\n",bgpdump_fmt_ipv6(attr->mp_info->announce[AFI_IP6][SAFI_UNICAST_MULTICAST]->nexthop,buf));
		if (attr->mp_info->announce[AFI_IP6][SAFI_UNICAST_MULTICAST]->nexthop_len==32)
		  wandio_printf(state->outfile, "NEXT_HOP: %s\n",bgpdump_fmt_ipv6(attr->mp_info->announce[AFI_IP6][SAFI_UNICAST_MULTICAST]->nexthop_local,buf));


	      }
	  }
	else
#endif
	  {

	    if (attr->mp_info->announce[AFI_IP][SAFI_UNICAST])
	      {
		wandio_printf(state->outfile, "(IPv4 Unicast)\n");
		wandio_printf(state->outfile, "NEXT_HOP: %s\n",inet_ntoa(attr->mp_info->announce[AFI_IP][SAFI_UNICAST]->nexthop.v4_addr));
		if (attr->mp_info->announce[AFI_IP][SAFI_UNICAST]->nexthop_len==32)
		  wandio_printf(state->outfile, "NEXT_HOP: %s\n",inet_ntoa(attr->mp_info->announce[AFI_IP][SAFI_UNICAST]->nexthop_local.v4_addr));

	      }
	    else if (attr->mp_info->announce[AFI_IP][SAFI_MULTICAST])
	      {
		wandio_printf(state->outfile, "(IPv4 Multicast)\n");
		wandio_printf(state->outfile, "NEXT_HOP: %s\n",inet_ntoa(attr->mp_info->announce[AFI_IP][SAFI_MULTICAST]->nexthop.v4_addr));
		if (attr->mp_info->announce[AFI_IP][SAFI_MULTICAST]->nexthop_len==32)
		  wandio_printf(state->outfile, "NEXT_HOP: %s\n",inet_ntoa(attr->mp_info->announce[AFI_IP][SAFI_MULTICAST]->nexthop_local.v4_addr));


	      }
	    else if (attr->mp_info->announce[AFI_IP][SAFI_UNICAST_MULTICAST])
	      {
		wandio_printf(state->outfile, "(IPv4 Both unicast and multicast)\n");
		wandio_printf(state->outfile, "NEXT_HOP: %s\n",inet_ntoa(attr->mp_info->announce[AFI_IP][SAFI_UNICAST_MULTICAST]->nexthop.v4_addr));
		if (attr->mp_info->announce[AFI_IP][SAFI_UNICAST_MULTICAST]->nexthop_len==32)
		  wandio_printf(state->outfile, "NEXT_HOP: %s\n",inet_ntoa(attr->mp_info->announce[AFI_IP][SAFI_UNICAST_MULTICAST]->nexthop_local.v4_addr));


	      }

	  }
      }

    if( (attr->flag & ATTR_FLAG_BIT(BGP_ATTR_MP_UNREACH_NLRI) )!=0)
      {
	wandio_printf(state->outfile, "MP_UNREACH_NLRI");
#ifdef BGPDUMP_HAVE_IPV6
	if (attr->mp_info->withdraw[AFI_IP6][SAFI_UNICAST] || attr->mp_info->withdraw[AFI_IP6][SAFI_MULTICAST] || attr->mp_info->withdraw[AFI_IP6][SAFI_UNICAST_MULTICAST])

	  {

	    if (attr->mp_info->withdraw[AFI_IP6][SAFI_UNICAST])
	      {
		wandio_printf(state->outfile, "(IPv6 Unicast)\n");
	      }
	    else if (attr->mp_info->withdraw[AFI_IP6][SAFI_MULTICAST])
	      {
		wandio_printf(state->outfile, "(IPv6 Multicast)\n");

	      }
	    else
	      {
		wandio_printf(state->outfile, "(IPv6 Both unicast and multicast)\n");


	      }
	  }
	else
#endif
	  {

	    if (attr->mp_info->withdraw[AFI_IP][SAFI_UNICAST])
	      {
		wandio_printf(state->outfile, "(IPv4 Unicast)\n");

	      }
	    else if (attr->mp_info->withdraw[AFI_IP][SAFI_MULTICAST])
	      {
		wandio_printf(state->outfile, "(IPv4 Multicast)\n");


	      }
	    else if (attr->mp_info->withdraw[AFI_IP][SAFI_UNICAST_MULTICAST])
	      {
		wandio_printf(state->outfile, "(IPv4 Both unicast and multicast)\n");


	      }

	  }
      }
    if( (attr->flag & ATTR_FLAG_BIT(BGP_ATTR_COMMUNITIES) ) !=0)
      wandio_printf(state->outfile, "COMMUNITY:%s\n",attr->community->str);
  }

}

static void process(bgpcorsaro_t *bgpcorsaro, BGPDUMP_ENTRY *entry)
{
  struct bgpcorsaro_dump_state_t *state = STATE(bgpcorsaro);

  struct tm *date;
  char time_str[128];
  char time_str2[128];
  char time_str_fixed[128];
  char prefix[BGPDUMP_ADDRSTRLEN];

  date=gmtime(&entry->time);
  bgpdump_time2str(date,time_str);
  bgpdump_time2str(date,time_str_fixed);
  if (state->dump_mode==0)
    {
      wandio_printf(state->outfile, "TIME: %s\n", time_str);
    }
  //wandio_printf(state->outfile, "TIME: %s",asctime(gmtime(&entry->time)));
  //wandio_printf(state->outfile, "LENGTH          : %u\n", entry->length);
  switch(entry->type) {
  case BGPDUMP_TYPE_MRTD_TABLE_DUMP:
    if(state->dump_mode==0){
      const char *prefix_str = NULL;
      switch(entry->subtype){
#ifdef BGPDUMP_HAVE_IPV6
      case BGPDUMP_SUBTYPE_MRTD_TABLE_DUMP_AFI_IP6:
	wandio_printf(state->outfile, "TYPE: TABLE_DUMP/INET6\n");
	prefix_str = bgpdump_fmt_ipv6(entry->body.mrtd_table_dump.prefix,prefix);
	break;

      case BGPDUMP_SUBTYPE_MRTD_TABLE_DUMP_AFI_IP6_32BIT_AS:
	wandio_printf(state->outfile, "TYPE: TABLE_DUMP/INET6_32BIT_AS\n");
	prefix_str = bgpdump_fmt_ipv6(entry->body.mrtd_table_dump.prefix,prefix);
	break;

#endif
      case BGPDUMP_SUBTYPE_MRTD_TABLE_DUMP_AFI_IP:
	wandio_printf(state->outfile, "TYPE: TABLE_DUMP/INET\n");
	prefix_str = inet_ntoa(entry->body.mrtd_table_dump.prefix.v4_addr);
	break;

      case BGPDUMP_SUBTYPE_MRTD_TABLE_DUMP_AFI_IP_32BIT_AS:
	wandio_printf(state->outfile, "TYPE: TABLE_DUMP/INET_32BIT_AS\n");
	prefix_str = inet_ntoa(entry->body.mrtd_table_dump.prefix.v4_addr);
	break;

      default:
	wandio_printf(state->outfile, "Error: unknown table type %d\n", entry->subtype);
	return;

      }
      wandio_printf(state->outfile, "VIEW: %d\n",entry->body.mrtd_table_dump.view);
      wandio_printf(state->outfile, "SEQUENCE: %d\n",entry->body.mrtd_table_dump.sequence);
      wandio_printf(state->outfile, "PREFIX: %s/%d\n",prefix_str,entry->body.mrtd_table_dump.mask);
      wandio_printf(state->outfile, "FROM:");
      switch(entry->subtype)
	{
#ifdef BGPDUMP_HAVE_IPV6
	case BGPDUMP_SUBTYPE_MRTD_TABLE_DUMP_AFI_IP6:
	case BGPDUMP_SUBTYPE_MRTD_TABLE_DUMP_AFI_IP6_32BIT_AS:

	  bgpdump_fmt_ipv6(entry->body.mrtd_table_dump.peer_ip,prefix);
	  wandio_printf(state->outfile, "%s ",prefix);
	  break;
#endif
	case BGPDUMP_SUBTYPE_MRTD_TABLE_DUMP_AFI_IP:
	case BGPDUMP_SUBTYPE_MRTD_TABLE_DUMP_AFI_IP_32BIT_AS:
	  if (entry->body.mrtd_table_dump.peer_ip.v4_addr.s_addr != 0x00000000L)
	    wandio_printf(state->outfile, "%s ",inet_ntoa(entry->body.mrtd_table_dump.peer_ip.v4_addr));
	  else
	    wandio_printf(state->outfile, "N/A ");

	}
      wandio_printf(state->outfile, "AS%u\n",entry->body.mrtd_table_dump.peer_as);

      //wandio_printf(state->outfile, "FROM: %s AS%d\n",inet_ntoa(entry->body.mrtd_table_dump.peer_ip.v4_addr),entry->body.mrtd_table_dump.peer_as);
      //time2str(localtime(&entry->body.mrtd_table_dump.uptime),time_str2);
      bgpdump_time2str(gmtime(&entry->body.mrtd_table_dump.uptime),time_str2);
      wandio_printf(state->outfile, "ORIGINATED: %s\n",time_str2);
      if (entry->attr && entry->attr->len)
	show_attr(bgpcorsaro, entry->attr);

      wandio_printf(state->outfile, "STATUS: 0x%x\n",entry->body.mrtd_table_dump.status);


      //wandio_printf(state->outfile, "    UPTIME      : %s",asctime(gmtime(&entry->body.mrtd_table_dump.uptime)));
      //wandio_printf(state->outfile, "    PEER IP     : %s\n",inet_ntoa(entry->body.mrtd_table_dump.peer_ip));
      //wandio_printf(state->outfile, "    PEER IP     : %s\n",inet_ntoa(entry->body.mrtd_table_dump.peer_ip.v4_addr));
      //wandio_printf(state->outfile, "    PEER AS     : %d\n",entry->body.mrtd_table_dump.peer_as);
    }
    else if (state->dump_mode ==1 || state->dump_mode ==2) // -m -M
      {
	table_line_mrtd_route(bgpcorsaro, &entry->body.mrtd_table_dump,entry);
      }
    break;

  case BGPDUMP_TYPE_TABLE_DUMP_V2:
    if(state->dump_mode == 0){
      char peer_ip[BGPDUMP_ADDRSTRLEN];
      //char time_str[30];
      int i;

      BGPDUMP_TABLE_DUMP_V2_PREFIX *e;
      e = &entry->body.mrtd_table_dump_v2_prefix;

      if(e->afi == AFI_IP){
	strncpy(prefix, inet_ntoa(e->prefix.v4_addr), BGPDUMP_ADDRSTRLEN);
#ifdef BGPDUMP_HAVE_IPV6
      } else if(e->afi == AFI_IP6){
	bgpdump_fmt_ipv6(e->prefix, prefix);
#endif
      }

      for(i = 0; i < e->entry_count; i++){
	// This is slightly nasty - as we want to print multiple entries
	// for multiple peers, we may need to print another TIME ourselves
	if(i) wandio_printf(state->outfile, "\nTIME: %s\n",time_str_fixed);
	if(e->afi == AFI_IP){
	  wandio_printf(state->outfile, "TYPE: TABLE_DUMP_V2/IPV4_UNICAST\n");
#ifdef BGPDUMP_HAVE_IPV6
	} else if(e->afi == AFI_IP6){
	  wandio_printf(state->outfile, "TYPE: TABLE_DUMP_V2/IPV6_UNICAST\n");
#endif
	}
	wandio_printf(state->outfile, "PREFIX: %s/%d\n",prefix, e->prefix_length);
	wandio_printf(state->outfile, "SEQUENCE: %d\n",e->seq);

	if(e->entries[i].peer.afi == AFI_IP){
	  bgpdump_fmt_ipv4(e->entries[i].peer.peer_ip, peer_ip);
#ifdef BGPDUMP_HAVE_IPV6
	} else if (e->entries[i].peer.afi == AFI_IP6){
	  bgpdump_fmt_ipv6(e->entries[i].peer.peer_ip, peer_ip);
#endif
	} else {
	  sprintf(peer_ip, "[N/A, unsupported AF]");
	}
	wandio_printf(state->outfile, "FROM: %s AS%u\n", peer_ip, e->entries[i].peer.peer_as);
	time_t time_temp = (time_t)((e->entries[i]).originated_time);
	bgpdump_time2str(gmtime(&time_temp),time_str);
	wandio_printf(state->outfile, "ORIGINATED: %s\n",time_str);
	if (e->entries[i].attr && e->entries[i].attr->len)
	  show_attr(bgpcorsaro, e->entries[i].attr);
      }
    } else if (state->dump_mode==1 || state->dump_mode==2) { // -m -M
      table_line_dump_v2_prefix(bgpcorsaro, &entry->body.mrtd_table_dump_v2_prefix,entry);
    }
    break;

  case BGPDUMP_TYPE_ZEBRA_BGP:

    switch(entry->subtype)
      {
      case BGPDUMP_SUBTYPE_ZEBRA_BGP_MESSAGE:
      case BGPDUMP_SUBTYPE_ZEBRA_BGP_MESSAGE_AS4:

	switch(entry->body.zebra_message.type)
	  {
	  case BGP_MSG_UPDATE:
	    if (state->dump_mode ==0)
	      {
		wandio_printf(state->outfile, "TYPE: BGP4MP/MESSAGE/Update\n");
		if (entry->body.zebra_message.source_as)
		  {
		    wandio_printf(state->outfile, "FROM:");
		    switch(entry->body.zebra_message.address_family)
		      {
#ifdef BGPDUMP_HAVE_IPV6
		      case AFI_IP6:

			bgpdump_fmt_ipv6(entry->body.zebra_message.source_ip,prefix);
			wandio_printf(state->outfile, " %s ",prefix);
			break;
#endif
		      case AFI_IP:
		      default:
			if (entry->body.zebra_message.source_ip.v4_addr.s_addr != 0x00000000L)
			  wandio_printf(state->outfile, " %s ",inet_ntoa(entry->body.zebra_message.source_ip.v4_addr));
			else
			  wandio_printf(state->outfile, " N/A ");
		      }
		    wandio_printf(state->outfile, "AS%u\n",entry->body.zebra_message.source_as);
		  }
		if (entry->body.zebra_message.destination_as)
		  {
		    wandio_printf(state->outfile, "TO:");
		    switch(entry->body.zebra_message.address_family)
		      {
#ifdef BGPDUMP_HAVE_IPV6
		      case AFI_IP6:

			bgpdump_fmt_ipv6(entry->body.zebra_message.destination_ip,prefix);
			wandio_printf(state->outfile, " %s ",prefix);
			break;
#endif
		      case AFI_IP:
		      default:
			if (entry->body.zebra_message.destination_ip.v4_addr.s_addr != 0x00000000L)
			  wandio_printf(state->outfile, " %s ",inet_ntoa(entry->body.zebra_message.destination_ip.v4_addr));
			else
			  wandio_printf(state->outfile, " N/A ");
		      }
		    wandio_printf(state->outfile, "AS%u\n",entry->body.zebra_message.destination_as);
		  }
		if (entry->attr && entry->attr->len)
		  show_attr(bgpcorsaro, entry->attr);
		if (entry->body.zebra_message.cut_bytes)
		  {
		    u_int16_t cutted,idx;
		    u_int8_t buf[128];

		    wandio_printf(state->outfile, "   INCOMPLETE PACKET: %d bytes cutted\n",entry->body.zebra_message.cut_bytes);
		    wandio_printf(state->outfile, "   INCOMPLETE PART: ");
		    if (entry->body.zebra_message.incomplete.orig_len)
		      {
			cutted=entry->body.zebra_message.incomplete.prefix.len/8+1;
			buf[0]=entry->body.zebra_message.incomplete.orig_len;
			memcpy(buf+1,&entry->body.zebra_message.incomplete.prefix.address,cutted-1);

			for (idx=0;idx<cutted;idx++)
			  {
			    if (buf[idx]<0x10)
			      wandio_printf(state->outfile, "0%x ",buf[idx]);
			    else
			      wandio_printf(state->outfile, "%x ",buf[idx]);
			  }
		      }
		    wandio_printf(state->outfile, "\n");
		  }
		if(! entry->attr)
		  return;
		if ((entry->body.zebra_message.withdraw_count) || (entry->attr->flag & ATTR_FLAG_BIT(BGP_ATTR_MP_UNREACH_NLRI)))
		  {
#ifdef BGPDUMP_HAVE_IPV6
		    if ((entry->body.zebra_message.withdraw_count)||(entry->attr->mp_info->withdraw[AFI_IP][SAFI_UNICAST] && entry->attr->mp_info->withdraw[AFI_IP][SAFI_UNICAST]->prefix_count) || (entry->attr->mp_info->withdraw[AFI_IP][SAFI_MULTICAST] && entry->attr->mp_info->withdraw[AFI_IP][SAFI_MULTICAST]->prefix_count) || (entry->attr->mp_info->withdraw[AFI_IP][SAFI_UNICAST_MULTICAST] && entry->attr->mp_info->withdraw[AFI_IP][SAFI_UNICAST_MULTICAST]->prefix_count) ||(entry->attr->mp_info->withdraw[AFI_IP6][SAFI_UNICAST] && entry->attr->mp_info->withdraw[AFI_IP6][SAFI_UNICAST]->prefix_count) || (entry->attr->mp_info->withdraw[AFI_IP6][SAFI_MULTICAST] && entry->attr->mp_info->withdraw[AFI_IP6][SAFI_MULTICAST]->prefix_count) || (entry->attr->mp_info->withdraw[AFI_IP6][SAFI_UNICAST_MULTICAST] && entry->attr->mp_info->withdraw[AFI_IP6][SAFI_UNICAST_MULTICAST]->prefix_count) )

#else
		      if ((entry->body.zebra_message.withdraw_count)||(entry->attr->mp_info->withdraw[AFI_IP][SAFI_UNICAST] && entry->attr->mp_info->withdraw[AFI_IP][SAFI_UNICAST]->prefix_count) || (entry->attr->mp_info->withdraw[AFI_IP][SAFI_MULTICAST] && entry->attr->mp_info->withdraw[AFI_IP][SAFI_MULTICAST]->prefix_count) || (entry->attr->mp_info->withdraw[AFI_IP][SAFI_UNICAST_MULTICAST] && entry->attr->mp_info->withdraw[AFI_IP][SAFI_UNICAST_MULTICAST]->prefix_count))

#endif
			wandio_printf(state->outfile, "WITHDRAW\n");
		    if (entry->body.zebra_message.withdraw_count)
		      show_prefixes(bgpcorsaro, entry->body.zebra_message.withdraw_count,entry->body.zebra_message.withdraw);

		    if (entry->attr->mp_info->withdraw[AFI_IP][SAFI_UNICAST] && entry->attr->mp_info->withdraw[AFI_IP][SAFI_UNICAST]->prefix_count)
		      show_prefixes(bgpcorsaro, entry->attr->mp_info->withdraw[AFI_IP][SAFI_UNICAST]->prefix_count,entry->attr->mp_info->withdraw[AFI_IP][SAFI_UNICAST]->nlri);

		    if (entry->attr->mp_info->withdraw[AFI_IP][SAFI_MULTICAST] && entry->attr->mp_info->withdraw[AFI_IP][SAFI_MULTICAST]->prefix_count)
		      show_prefixes(bgpcorsaro, entry->attr->mp_info->withdraw[AFI_IP][SAFI_MULTICAST]->prefix_count,entry->attr->mp_info->withdraw[AFI_IP][SAFI_MULTICAST]->nlri);

		    if (entry->attr->mp_info->withdraw[AFI_IP][SAFI_UNICAST_MULTICAST] && entry->attr->mp_info->withdraw[AFI_IP][SAFI_UNICAST_MULTICAST]->prefix_count)
		      show_prefixes(bgpcorsaro, entry->attr->mp_info->withdraw[AFI_IP][SAFI_UNICAST_MULTICAST]->prefix_count,entry->attr->mp_info->withdraw[AFI_IP][SAFI_UNICAST_MULTICAST]->nlri);

#ifdef BGPDUMP_HAVE_IPV6
		    if (entry->attr->mp_info->withdraw[AFI_IP6][SAFI_UNICAST] && entry->attr->mp_info->withdraw[AFI_IP6][SAFI_UNICAST]->prefix_count)
		      show_prefixes6(bgpcorsaro, entry->attr->mp_info->withdraw[AFI_IP6][SAFI_UNICAST]->prefix_count,entry->attr->mp_info->withdraw[AFI_IP6][SAFI_UNICAST]->nlri);

		    if (entry->attr->mp_info->withdraw[AFI_IP6][SAFI_MULTICAST] && entry->attr->mp_info->withdraw[AFI_IP6][SAFI_MULTICAST]->prefix_count)
		      show_prefixes6(bgpcorsaro, entry->attr->mp_info->withdraw[AFI_IP6][SAFI_MULTICAST]->prefix_count,entry->attr->mp_info->withdraw[AFI_IP6][SAFI_MULTICAST]->nlri);

		    if (entry->attr->mp_info->withdraw[AFI_IP6][SAFI_UNICAST_MULTICAST] && entry->attr->mp_info->withdraw[AFI_IP6][SAFI_UNICAST_MULTICAST]->prefix_count)
		      show_prefixes6(bgpcorsaro, entry->attr->mp_info->withdraw[AFI_IP6][SAFI_UNICAST_MULTICAST]->prefix_count,entry->attr->mp_info->withdraw[AFI_IP6][SAFI_UNICAST_MULTICAST]->nlri);
#endif
		  }
		if ( (entry->body.zebra_message.announce_count) || (entry->attr->flag & ATTR_FLAG_BIT(BGP_ATTR_MP_REACH_NLRI)))
		  {
		    wandio_printf(state->outfile, "ANNOUNCE\n");
		    if (entry->body.zebra_message.announce_count)
		      show_prefixes(bgpcorsaro, entry->body.zebra_message.announce_count,entry->body.zebra_message.announce);

		    if (entry->attr->mp_info->announce[AFI_IP][SAFI_UNICAST] && entry->attr->mp_info->announce[AFI_IP][SAFI_UNICAST]->prefix_count)
		      show_prefixes(bgpcorsaro, entry->attr->mp_info->announce[AFI_IP][SAFI_UNICAST]->prefix_count,entry->attr->mp_info->announce[AFI_IP][SAFI_UNICAST]->nlri);

		    if (entry->attr->mp_info->announce[AFI_IP][SAFI_MULTICAST] && entry->attr->mp_info->announce[AFI_IP][SAFI_MULTICAST]->prefix_count)
		      show_prefixes(bgpcorsaro, entry->attr->mp_info->announce[AFI_IP][SAFI_MULTICAST]->prefix_count,entry->attr->mp_info->announce[AFI_IP][SAFI_MULTICAST]->nlri);

		    if (entry->attr->mp_info->announce[AFI_IP][SAFI_UNICAST_MULTICAST] && entry->attr->mp_info->announce[AFI_IP][SAFI_UNICAST_MULTICAST]->prefix_count)
		      show_prefixes(bgpcorsaro, entry->attr->mp_info->announce[AFI_IP][SAFI_UNICAST_MULTICAST]->prefix_count,entry->attr->mp_info->announce[AFI_IP][SAFI_UNICAST_MULTICAST]->nlri);

#ifdef BGPDUMP_HAVE_IPV6
		    if (entry->attr->mp_info->announce[AFI_IP6][SAFI_UNICAST] && entry->attr->mp_info->announce[AFI_IP6][SAFI_UNICAST]->prefix_count)
		      show_prefixes6(bgpcorsaro, entry->attr->mp_info->announce[AFI_IP6][SAFI_UNICAST]->prefix_count,entry->attr->mp_info->announce[AFI_IP6][SAFI_UNICAST]->nlri);

		    if (entry->attr->mp_info->announce[AFI_IP6][SAFI_MULTICAST] && entry->attr->mp_info->announce[AFI_IP6][SAFI_MULTICAST]->prefix_count)
		      show_prefixes6(bgpcorsaro, entry->attr->mp_info->announce[AFI_IP6][SAFI_MULTICAST]->prefix_count,entry->attr->mp_info->announce[AFI_IP6][SAFI_MULTICAST]->nlri);

		    if (entry->attr->mp_info->announce[AFI_IP6][SAFI_UNICAST_MULTICAST] && entry->attr->mp_info->announce[AFI_IP6][SAFI_UNICAST_MULTICAST]->prefix_count)
		      show_prefixes6(bgpcorsaro, entry->attr->mp_info->announce[AFI_IP6][SAFI_UNICAST_MULTICAST]->prefix_count,entry->attr->mp_info->announce[AFI_IP6][SAFI_UNICAST_MULTICAST]->nlri);
#endif
		  }
	      }
	    else if (state->dump_mode == 1  || state->dump_mode == 2) //-m -M
	      {
		if ((entry->body.zebra_message.withdraw_count) || (entry->attr->flag & ATTR_FLAG_BIT(BGP_ATTR_MP_UNREACH_NLRI)))
		  {

		    table_line_withdraw(bgpcorsaro, entry->body.zebra_message.withdraw,entry->body.zebra_message.withdraw_count,entry,time_str);

		    if (entry->attr->mp_info->withdraw[AFI_IP][SAFI_UNICAST] && entry->attr->mp_info->withdraw[AFI_IP][SAFI_UNICAST]->prefix_count)
		      table_line_withdraw(bgpcorsaro, entry->attr->mp_info->withdraw[AFI_IP][SAFI_UNICAST]->nlri,entry->attr->mp_info->withdraw[AFI_IP][SAFI_UNICAST]->prefix_count,entry,time_str);

		    if (entry->attr->mp_info->withdraw[AFI_IP][SAFI_MULTICAST] && entry->attr->mp_info->withdraw[AFI_IP][SAFI_MULTICAST]->prefix_count)
		      table_line_withdraw(bgpcorsaro, entry->attr->mp_info->withdraw[AFI_IP][SAFI_MULTICAST]->nlri,entry->attr->mp_info->withdraw[AFI_IP][SAFI_MULTICAST]->prefix_count,entry,time_str);

		    if (entry->attr->mp_info->withdraw[AFI_IP][SAFI_UNICAST_MULTICAST] && entry->attr->mp_info->withdraw[AFI_IP][SAFI_UNICAST_MULTICAST]->prefix_count)
		      table_line_withdraw(bgpcorsaro, entry->attr->mp_info->withdraw[AFI_IP][SAFI_UNICAST_MULTICAST]->nlri,entry->attr->mp_info->withdraw[AFI_IP][SAFI_UNICAST_MULTICAST]->prefix_count,entry,time_str);

#ifdef BGPDUMP_HAVE_IPV6
		    if (entry->attr->mp_info->withdraw[AFI_IP6][SAFI_UNICAST] && entry->attr->mp_info->withdraw[AFI_IP6][SAFI_UNICAST]->prefix_count)
		      table_line_withdraw6(bgpcorsaro, entry->attr->mp_info->withdraw[AFI_IP6][SAFI_UNICAST]->nlri,entry->attr->mp_info->withdraw[AFI_IP6][SAFI_UNICAST]->prefix_count,entry,time_str);

		    if (entry->attr->mp_info->withdraw[AFI_IP6][SAFI_MULTICAST] && entry->attr->mp_info->withdraw[AFI_IP6][SAFI_MULTICAST]->prefix_count)
		      table_line_withdraw6(bgpcorsaro, entry->attr->mp_info->withdraw[AFI_IP6][SAFI_MULTICAST]->nlri,entry->attr->mp_info->withdraw[AFI_IP6][SAFI_MULTICAST]->prefix_count,entry,time_str);

		    if (entry->attr->mp_info->withdraw[AFI_IP6][SAFI_UNICAST_MULTICAST] && entry->attr->mp_info->withdraw[AFI_IP6][SAFI_UNICAST_MULTICAST]->prefix_count)
		      table_line_withdraw6(bgpcorsaro, entry->attr->mp_info->withdraw[AFI_IP6][SAFI_UNICAST_MULTICAST]->nlri,entry->attr->mp_info->withdraw[AFI_IP6][SAFI_UNICAST_MULTICAST]->prefix_count,entry,time_str);
#endif

		  }
		if ( (entry->body.zebra_message.announce_count) || (entry->attr->flag & ATTR_FLAG_BIT(BGP_ATTR_MP_REACH_NLRI)))
		  {
		    table_line_announce(bgpcorsaro, entry->body.zebra_message.announce,entry->body.zebra_message.announce_count,entry,time_str);
		    if (entry->attr->mp_info->announce[AFI_IP][SAFI_UNICAST] && entry->attr->mp_info->announce[AFI_IP][SAFI_UNICAST]->prefix_count)
		      table_line_announce_1(bgpcorsaro, entry->attr->mp_info->announce[AFI_IP][SAFI_UNICAST],entry->attr->mp_info->announce[AFI_IP][SAFI_UNICAST]->prefix_count,entry,time_str);
		    if (entry->attr->mp_info->announce[AFI_IP][SAFI_MULTICAST] && entry->attr->mp_info->announce[AFI_IP][SAFI_MULTICAST]->prefix_count)
		      table_line_announce_1(bgpcorsaro, entry->attr->mp_info->announce[AFI_IP][SAFI_MULTICAST],entry->attr->mp_info->announce[AFI_IP][SAFI_MULTICAST]->prefix_count,entry,time_str);
		    if (entry->attr->mp_info->announce[AFI_IP][SAFI_UNICAST_MULTICAST] && entry->attr->mp_info->announce[AFI_IP][SAFI_UNICAST_MULTICAST]->prefix_count)
		      table_line_announce_1(bgpcorsaro, entry->attr->mp_info->announce[AFI_IP][SAFI_UNICAST_MULTICAST],entry->attr->mp_info->announce[AFI_IP][SAFI_UNICAST_MULTICAST]->prefix_count,entry,time_str);
#ifdef BGPDUMP_HAVE_IPV6
		    if (entry->attr->mp_info->announce[AFI_IP6][SAFI_UNICAST] && entry->attr->mp_info->announce[AFI_IP6][SAFI_UNICAST]->prefix_count)
		      table_line_announce6(bgpcorsaro, entry->attr->mp_info->announce[AFI_IP6][SAFI_UNICAST],entry->attr->mp_info->announce[AFI_IP6][SAFI_UNICAST]->prefix_count,entry,time_str);
		    if (entry->attr->mp_info->announce[AFI_IP6][SAFI_MULTICAST] && entry->attr->mp_info->announce[AFI_IP6][SAFI_MULTICAST]->prefix_count)
		      table_line_announce6(bgpcorsaro, entry->attr->mp_info->announce[AFI_IP6][SAFI_MULTICAST],entry->attr->mp_info->announce[AFI_IP6][SAFI_MULTICAST]->prefix_count,entry,time_str);
		    if (entry->attr->mp_info->announce[AFI_IP6][SAFI_UNICAST_MULTICAST] && entry->attr->mp_info->announce[AFI_IP6][SAFI_UNICAST_MULTICAST]->prefix_count)
		      table_line_announce6(bgpcorsaro, entry->attr->mp_info->announce[AFI_IP6][SAFI_UNICAST_MULTICAST],entry->attr->mp_info->announce[AFI_IP6][SAFI_UNICAST_MULTICAST]->prefix_count,entry,time_str);
#endif

		  }
	      }
	    break;

	  case BGP_MSG_OPEN:
	    if (state->dump_mode != 0)
	      break;
	    wandio_printf(state->outfile, "TYPE: BGP4MP/MESSAGE/Open\n");
	    if (entry->body.zebra_message.source_as)
	      {
		wandio_printf(state->outfile, "FROM:");
		switch(entry->body.zebra_message.address_family)
		  {
#ifdef BGPDUMP_HAVE_IPV6
		  case AFI_IP6:

		    bgpdump_fmt_ipv6(entry->body.zebra_message.source_ip,prefix);
		    wandio_printf(state->outfile, " %s ",prefix);
		    break;
#endif
		  case AFI_IP:
		  default:
		    if (entry->body.zebra_message.source_ip.v4_addr.s_addr != 0x00000000L)
		      wandio_printf(state->outfile, " %s ",inet_ntoa(entry->body.zebra_message.source_ip.v4_addr));
		    else
		      wandio_printf(state->outfile, " N/A ");
		  }
		wandio_printf(state->outfile, "AS%u\n",entry->body.zebra_message.source_as);
	      }
	    if (entry->body.zebra_message.destination_as)
	      {
		wandio_printf(state->outfile, "TO:");
		switch(entry->body.zebra_message.address_family)
		  {
#ifdef BGPDUMP_HAVE_IPV6
		  case AFI_IP6:

		    bgpdump_fmt_ipv6(entry->body.zebra_message.destination_ip,prefix);
		    wandio_printf(state->outfile, " %s ",prefix);
		    break;
#endif
		  case AFI_IP:
		  default:
		    if (entry->body.zebra_message.destination_ip.v4_addr.s_addr != 0x00000000L)
		      wandio_printf(state->outfile, " %s ",inet_ntoa(entry->body.zebra_message.destination_ip.v4_addr));
		    else
		      wandio_printf(state->outfile, " N/A ");
		  }
		wandio_printf(state->outfile, "AS%u\n",entry->body.zebra_message.destination_as);
	      }

	    wandio_printf(state->outfile, "VERSION: %d\n",entry->body.zebra_message.version);
	    wandio_printf(state->outfile, "AS: %u\n",entry->body.zebra_message.my_as);
	    wandio_printf(state->outfile, "HOLD_TIME: %d\n",entry->body.zebra_message.hold_time);
	    wandio_printf(state->outfile, "ID: %s\n",inet_ntoa(entry->body.zebra_message.bgp_id));
	    wandio_printf(state->outfile, "OPT_PARM_LEN: %d\n",entry->body.zebra_message.opt_len);
	    break;

	  case BGP_MSG_NOTIFY:
	    if (state->dump_mode != 0)
	      break;
	    wandio_printf(state->outfile, "TYPE: BGP4MP/MESSAGE/Notify\n");
	    if (entry->body.zebra_message.source_as)
	      {
		wandio_printf(state->outfile, "FROM:");
		switch(entry->body.zebra_message.address_family)
		  {
#ifdef BGPDUMP_HAVE_IPV6
		  case AFI_IP6:

		    bgpdump_fmt_ipv6(entry->body.zebra_message.source_ip,prefix);
		    wandio_printf(state->outfile, " %s ",prefix);
		    break;
#endif
		  case AFI_IP:
		  default:
		    if (entry->body.zebra_message.source_ip.v4_addr.s_addr != 0x00000000L)
		      wandio_printf(state->outfile, " %s ",inet_ntoa(entry->body.zebra_message.source_ip.v4_addr));
		    else
		      wandio_printf(state->outfile, " N/A ");
		  }
		wandio_printf(state->outfile, "AS%u\n",entry->body.zebra_message.source_as);
	      }
	    if (entry->body.zebra_message.destination_as)
	      {
		wandio_printf(state->outfile, "TO:");
		switch(entry->body.zebra_message.address_family)
		  {
#ifdef BGPDUMP_HAVE_IPV6
		  case AFI_IP6:

		    bgpdump_fmt_ipv6(entry->body.zebra_message.destination_ip,prefix);
		    wandio_printf(state->outfile, " %s ",prefix);
		    break;
#endif
		  case AFI_IP:
		  default:
		    if (entry->body.zebra_message.destination_ip.v4_addr.s_addr != 0x00000000L)
		      wandio_printf(state->outfile, " %s ",inet_ntoa(entry->body.zebra_message.destination_ip.v4_addr));
		    else
		      wandio_printf(state->outfile, " N/A ");
		  }
		wandio_printf(state->outfile, "AS%u\n",entry->body.zebra_message.destination_as);
	      }

	    switch (entry->body.zebra_message.error_code)
	      {
	      case 	1:
		wandio_printf(state->outfile, "    ERROR CODE  : 1 (Message Header Error)\n");
		switch(entry->body.zebra_message.sub_error_code)
		  {
		  case	1:
		    wandio_printf(state->outfile, "    SUB ERROR   : 1 (Connection Not Synchronized)\n");
		    break;

		  case	2:
		    wandio_printf(state->outfile, "    SUB ERROR   : 2 (Bad Message Length)\n");
		    break;

		  case	3:
		    wandio_printf(state->outfile, "    SUB ERROR   : 3 (Bad Message Type)\n");
		    break;

		  default:
		    wandio_printf(state->outfile, "    SUB ERROR   : %d\n",entry->body.zebra_message.sub_error_code);
		    break;
		  }
		break;
	      case	2:
		wandio_printf(state->outfile, "    ERROR CODE  : 2 (OPEN Message Error)\n");
		switch(entry->body.zebra_message.sub_error_code)
		  {
		  case	1:
		    wandio_printf(state->outfile, "    SUB ERROR   : 1 (Unsupported Version Number)\n");
		    break;

		  case	2:
		    wandio_printf(state->outfile, "    SUB ERROR   : 2 (Bad Peer AS)\n");
		    break;

		  case	3:
		    wandio_printf(state->outfile, "    SUB ERROR   : 3 (Bad BGP Identifier)\n");
		    break;

		  case	4:
		    wandio_printf(state->outfile, "    SUB ERROR   : 4 (Unsupported Optional Parameter)\n");
		    break;

		  case	5:
		    wandio_printf(state->outfile, "    SUB ERROR   : 5 (Authentication Failure)\n");
		    break;

		  case	6:
		    wandio_printf(state->outfile, "    SUB ERROR   : 6 (Unacceptable Hold Time)\n");
		    break;

		  default:
		    wandio_printf(state->outfile, "    SUB ERROR   : %d\n",entry->body.zebra_message.sub_error_code);
		    break;
		  }
		break;
	      case	3:
		wandio_printf(state->outfile, "    ERROR CODE  : 3 (UPDATE Message Error)\n");
		switch(entry->body.zebra_message.sub_error_code)
		  {
		  case	1:
		    wandio_printf(state->outfile, "    SUB ERROR   : 1 (Malformed Attribute List)\n");
		    break;

		  case	2:
		    wandio_printf(state->outfile, "    SUB ERROR   : 2 (Unrecognized Well-known Attribute)\n");
		    break;

		  case	3:
		    wandio_printf(state->outfile, "    SUB ERROR   : 3 (Missing Well-known Attribute)\n");
		    break;

		  case	4:
		    wandio_printf(state->outfile, "    SUB ERROR   : 4 (Attribute Flags Error)\n");
		    break;

		  case	5:
		    wandio_printf(state->outfile, "    SUB ERROR   : 5 (Attribute Length Error)\n");
		    break;

		  case	6:
		    wandio_printf(state->outfile, "    SUB ERROR   : 6 (Invalid ORIGIN Attribute)\n");
		    break;

		  case	7:
		    wandio_printf(state->outfile, "    SUB ERROR   : 7 (AS Routing Loop)\n");
		    break;

		  case	8:
		    wandio_printf(state->outfile, "    SUB ERROR   : 8 (Invalid NEXT-HOP Attribute)\n");
		    break;

		  case	9:
		    wandio_printf(state->outfile, "    SUB ERROR   : 9 (Optional Attribute Error)\n");
		    break;

		  case	10:
		    wandio_printf(state->outfile, "    SUB ERROR   : 10 (Invalid Network Field)\n");
		    break;

		  case	11:
		    wandio_printf(state->outfile, "    SUB ERROR   : 11 (Malformed AS-PATH)\n");
		    break;

		  default:
		    wandio_printf(state->outfile, "    SUB ERROR   : %d\n",entry->body.zebra_message.sub_error_code);
		    break;
		  }
		break;
	      case	4:
		wandio_printf(state->outfile, "    ERROR CODE  : 4 (Hold Timer Expired)\n");
		break;
	      case	5:
		wandio_printf(state->outfile, "    ERROR CODE  : 5 (Finite State Machine Error)\n");
		break;
	      case	6:
		wandio_printf(state->outfile, "    ERROR CODE  : 6 (Cease)\n");
		break;
	      default:
		wandio_printf(state->outfile, "    ERROR CODE  : %d\n",entry->body.zebra_message.error_code);
		break;

	      }
	    break;

	  case BGP_MSG_KEEPALIVE:
	    if ( state->dump_mode != 0)
	      break;

	    wandio_printf(state->outfile, "TYPE: BGP4MP/MESSAGE/Keepalive\n");
	    if (entry->body.zebra_message.source_as)
	      {
		wandio_printf(state->outfile, "FROM:");
		switch(entry->body.zebra_message.address_family)
		  {
#ifdef BGPDUMP_HAVE_IPV6
		  case AFI_IP6:

		    bgpdump_fmt_ipv6(entry->body.zebra_message.source_ip,prefix);
		    wandio_printf(state->outfile, " %s ",prefix);
		    break;
#endif
		  case AFI_IP:
		  default:
		    if (entry->body.zebra_message.source_ip.v4_addr.s_addr != 0x00000000L)
		      wandio_printf(state->outfile, " %s ",inet_ntoa(entry->body.zebra_message.source_ip.v4_addr));
		    else
		      wandio_printf(state->outfile, " N/A ");
		  }
		wandio_printf(state->outfile, "AS%u\n",entry->body.zebra_message.source_as);
	      }
	    if (entry->body.zebra_message.destination_as)
	      {
		wandio_printf(state->outfile, "TO:");
		switch(entry->body.zebra_message.address_family)
		  {
#ifdef BGPDUMP_HAVE_IPV6
		  case AFI_IP6:

		    bgpdump_fmt_ipv6(entry->body.zebra_message.destination_ip,prefix);
		    wandio_printf(state->outfile, " %s ",prefix);
		    break;
#endif
		  case AFI_IP:
		  default:
		    if (entry->body.zebra_message.destination_ip.v4_addr.s_addr != 0x00000000L)
		      wandio_printf(state->outfile, " %s ",inet_ntoa(entry->body.zebra_message.destination_ip.v4_addr));
		    else
		      wandio_printf(state->outfile, " N/A ");
		  }
		wandio_printf(state->outfile, "AS%u\n",entry->body.zebra_message.destination_as);
	      }


	    break;
	  }
	break;

      case BGPDUMP_SUBTYPE_ZEBRA_BGP_STATE_CHANGE:
      case BGPDUMP_SUBTYPE_ZEBRA_BGP_STATE_CHANGE_AS4:
	if (state->dump_mode==0)
	  {
	    wandio_printf(state->outfile, "TYPE: BGP4MP/STATE_CHANGE\n");

	    wandio_printf(state->outfile, "PEER:");
	    switch(entry->body.zebra_state_change.address_family)
	      {
#ifdef BGPDUMP_HAVE_IPV6
	      case AFI_IP6:

		bgpdump_fmt_ipv6(entry->body.zebra_state_change.source_ip,prefix);
		wandio_printf(state->outfile, " %s ",prefix);
		break;
#endif
	      case AFI_IP:
	      default:
		if (entry->body.zebra_state_change.source_ip.v4_addr.s_addr != 0x00000000L)
		  wandio_printf(state->outfile, " %s ",inet_ntoa(entry->body.zebra_message.source_ip.v4_addr));
		else
		  wandio_printf(state->outfile, " N/A ");
	      }
	    //if (entry->body.zebra_message.source_ip.s_addr != 0x00000000L)
	    //	wandio_printf(state->outfile, " %s ",inet_ntoa(entry->body.zebra_message.source_ip));
	    //else
	    //	wandio_printf(state->outfile, " N/A ");
	    wandio_printf(state->outfile, "AS%u\n",entry->body.zebra_state_change.source_as);

	    wandio_printf(state->outfile, "STATE: %s/%s\n",bgp_state_name[entry->body.zebra_state_change.old_state],bgp_state_name[entry->body.zebra_state_change.new_state]);
	  }
	else if (state->dump_mode==1 || state->dump_mode==2 ) //-m -M
	  {
	    switch(entry->body.zebra_state_change.address_family)
	      {
#ifdef BGPDUMP_HAVE_IPV6
	      case AFI_IP6:

		bgpdump_fmt_ipv6(entry->body.zebra_state_change.source_ip,prefix);
		if (state->dump_mode == 1)
		  wandio_printf(state->outfile, "BGP4MP|%ld|STATE|%s|%u|%d|%d\n",
			 entry->time,
			 prefix,
			 entry->body.zebra_state_change.source_as,
			 entry->body.zebra_state_change.old_state,
			 entry->body.zebra_state_change.new_state);
		else
		  wandio_printf(state->outfile, "BGP4MP|%s|STATE|%s|%u|%d|%d\n",
			 time_str,
			 prefix,
			 entry->body.zebra_state_change.source_as,
			 entry->body.zebra_state_change.old_state,
			 entry->body.zebra_state_change.new_state);
		break;
#endif
	      case AFI_IP:
	      default:
		if (state->dump_mode == 1)
		  wandio_printf(state->outfile, "BGP4MP|%ld|STATE|%s|%u|%d|%d\n",
			 entry->time,
			 inet_ntoa(entry->body.zebra_state_change.source_ip.v4_addr),
			 entry->body.zebra_state_change.source_as,
			 entry->body.zebra_state_change.old_state,
			 entry->body.zebra_state_change.new_state);
		else
		  wandio_printf(state->outfile, "BGP4MP|%s|STATE|%s|%u|%d|%d\n",
			 time_str,
			 inet_ntoa(entry->body.zebra_state_change.source_ip.v4_addr),
			 entry->body.zebra_state_change.source_as,
			 entry->body.zebra_state_change.old_state,
			 entry->body.zebra_state_change.new_state);
		break;

	      }
	  }
	break;

      }
    break;
  }
  if (state->dump_mode==0)
    wandio_printf(state->outfile, "\n");
}

/* == PUBLIC PLUGIN FUNCS BELOW HERE == */

/** Implements the alloc function of the plugin API */
bgpcorsaro_plugin_t *bgpcorsaro_dump_alloc(bgpcorsaro_t *bgpcorsaro)
{
  return &bgpcorsaro_dump_plugin;
}

/** Implements the init_output function of the plugin API */
int bgpcorsaro_dump_init_output(bgpcorsaro_t *bgpcorsaro)
{
  struct bgpcorsaro_dump_state_t *state;
  bgpcorsaro_plugin_t *plugin = PLUGIN(bgpcorsaro);
  assert(plugin != NULL);

  if((state = malloc_zero(sizeof(struct bgpcorsaro_dump_state_t))) == NULL)
    {
      bgpcorsaro_log(__func__, bgpcorsaro,
		     "could not malloc bgpcorsaro_dump_state_t");
      goto err;
    }
  bgpcorsaro_plugin_register_state(bgpcorsaro->plugin_manager, plugin, state);

  /* parse the arguments */
  if(parse_args(bgpcorsaro) != 0)
    {
      return -1;
    }

  /* defer opening the output file until we start the first interval */

  return 0;

 err:
  bgpcorsaro_dump_close_output(bgpcorsaro);
  return -1;
}

/** Implements the close_output function of the plugin API */
int bgpcorsaro_dump_close_output(bgpcorsaro_t *bgpcorsaro)
{
  int i;
  struct bgpcorsaro_dump_state_t *state = STATE(bgpcorsaro);

  if(state != NULL)
    {
      /* close all the outfile pointers */
      for(i = 0; i < OUTFILE_POINTERS; i++)
	{
	  if(state->outfile_p[i] != NULL)
	    {
	      wandio_wdestroy(state->outfile_p[i]);
	      state->outfile_p[i] = NULL;
	    }
	}
      state->outfile = NULL;
      bgpcorsaro_plugin_free_state(bgpcorsaro->plugin_manager, PLUGIN(bgpcorsaro));
    }
  return 0;
}

/** Implements the start_interval function of the plugin API */
int bgpcorsaro_dump_start_interval(bgpcorsaro_t *bgpcorsaro,
				   bgpcorsaro_interval_t *int_start)
{
  struct bgpcorsaro_dump_state_t *state = STATE(bgpcorsaro);

  if(state->outfile == NULL)
    {
      if((
	  state->outfile_p[state->outfile_n] =
	  bgpcorsaro_io_prepare_file(bgpcorsaro,
				     PLUGIN(bgpcorsaro)->name,
				     int_start)) == NULL)
	{
	  bgpcorsaro_log(__func__, bgpcorsaro, "could not open %s output file",
			 PLUGIN(bgpcorsaro)->name);
	  return -1;
	}
      state->outfile = state->
	outfile_p[state->outfile_n];
    }

  bgpcorsaro_io_write_interval_start(bgpcorsaro, state->outfile, int_start);

  return 0;
}

/** Implements the end_interval function of the plugin API */
int bgpcorsaro_dump_end_interval(bgpcorsaro_t *bgpcorsaro,
				 bgpcorsaro_interval_t *int_end)
{
  struct bgpcorsaro_dump_state_t *state = STATE(bgpcorsaro);

  bgpcorsaro_io_write_interval_end(bgpcorsaro, state->outfile, int_end);

  /* if we are rotating, now is when we should do it */
  if(bgpcorsaro_is_rotate_interval(bgpcorsaro))
    {
      /* leave the current file to finish draining buffers */
      assert(state->outfile != NULL);

      /* move on to the next output pointer */
      state->outfile_n = (state->outfile_n+1) %
	OUTFILE_POINTERS;

      if(state->outfile_p[state->outfile_n] != NULL)
	{
	  /* we're gonna have to wait for this to close */
	  wandio_wdestroy(state->outfile_p[state->outfile_n]);
	  state->outfile_p[state->outfile_n] =  NULL;
	}

      state->outfile = NULL;
    }
  return 0;
}

/** Implements the process_record function of the plugin API */
int bgpcorsaro_dump_process_record(bgpcorsaro_t *bgpcorsaro,
				   bgpcorsaro_record_t *record)
{
  BGPDUMP_ENTRY *bd_entry = BS_REC(record)->bd_entry;

  if(BS_REC(record)->status != VALID_RECORD)
    {
      return 0;
    }

  /* no point carrying on if a previous plugin has already decided we should
     ignore this record */
  if((record->state.flags & BGPCORSARO_RECORD_STATE_FLAG_IGNORE) != 0)
    {
      return 0;
    }

  process(bgpcorsaro, bd_entry);

  return 0;
}
