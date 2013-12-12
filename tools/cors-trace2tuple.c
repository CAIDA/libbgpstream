/*
 * corsaro
 *
 * Alistair King, CAIDA, UC San Diego
 * corsaro-info@caida.org
 *
 * Copyright (C) 2012 The Regents of the University of California.
 *
 * This file is part of corsaro.
 *
 * Code in this file is adapted from tracestats.c from libtrace:
 *   tracestats.c 1730 2011-09-20 02:23:33Z salcock
 *
 * corsaro is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * corsaro is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with corsaro.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

/*
 * This program takes a series of traces and bpf filters and dumps a record
 * for each packet which is perhaps a cross between the coral t2 format and
 * the default tcpdump ascii output.
 * That is, it dumps:
 * <timestamp> <src_ip> <dst_ip> <src_port> <dst_port> <protocol> <ip_len>
 *
 * The original tracestats output has been retained, but diverted to stderr
 * so at the end of dumping each packet, stats about the file are dumped
 * to stderr.
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/tcp.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <getopt.h>
#include <inttypes.h>
#include <signal.h>

#include "libtrace.h"
#include "lt_inttypes.h"

struct libtrace_t *trace;

static volatile int done=0;

static void cleanup_signal(int signal)
{
  (void)signal;
  done=1;
  trace_interrupt();
}

struct filter_t {
  char *expr;
  struct libtrace_filter_t *filter;
  uint64_t count;
  uint64_t bytes;
} *filters = NULL;
int filter_count=0;
uint64_t totcount;
uint64_t totbytes;

/* Process a trace, counting packets that match filter(s) */
static void run_trace(char *uri)
{
  struct libtrace_packet_t *packet = trace_create_packet();
  int i;
  uint64_t count = 0;
  uint64_t bytes = 0;
  uint64_t packets;

  fprintf(stderr,"%s:\n",uri);

  trace = trace_create(uri);

  if (trace_is_err(trace)) {
    trace_perror(trace,"Failed to create trace");
    return;
  }

  if (trace_start(trace)==-1) {
    trace_perror(trace,"Failed to start trace");
    return;
  }


  for (;;) {
    int psize;
    int wlen;
    int match = 0;
    if ((psize = trace_read_packet(trace, packet)) <1) {
      break;
    }

    if (done)
      break;
    wlen = trace_get_wire_length(packet);

    for(i=0;i<filter_count;++i) {
      if (filters[i].filter == NULL)
	continue;
      if(trace_apply_filter(filters[i].filter,packet) > 0) {
	++filters[i].count;
	filters[i].bytes+=wlen;
	match = 1;
      }
      if (trace_is_err(trace)) {
	trace_perror(trace, "trace_apply_filter");
	fprintf(stderr, "Removing filter from filterlist\n");
	filters[i].filter = NULL;
      }
    }

    if(match == 1 || filter_count == 0)
      {
	libtrace_ip_t *ip_hdr = trace_get_ip(packet);
	if(ip_hdr != NULL)
	  {
	    char src_ip[INET_ADDRSTRLEN];
	    char dst_ip[INET_ADDRSTRLEN];

	    inet_ntop(AF_INET, &ip_hdr->ip_src, &src_ip[0], INET_ADDRSTRLEN);
	    inet_ntop(AF_INET, &ip_hdr->ip_dst, &dst_ip[0], INET_ADDRSTRLEN);

	    printf("%f\t%s\t%s\t%"PRIu16"\t%"PRIu16"\t"
		   "%"PRIu8"\t%"PRIu16"\t%"PRIu16"\n",
		   trace_get_seconds(packet), src_ip, dst_ip,
		   trace_get_source_port(packet),
		   trace_get_destination_port(packet),
		   ip_hdr->ip_p,
		   ntohs(ip_hdr->ip_id),
		   ntohs(ip_hdr->ip_len)
		   );
	  }
      }

    ++count;
    bytes+=wlen;
  }

  fprintf(stderr, "%-30s\t%12s\t%12s\t%7s\n","filter","count","bytes","%");
  for(i=0;i<filter_count;++i) {
    fprintf(stderr, "%30s:\t%12"PRIu64"\t%12"PRIu64"\t%7.03f\n",
	    filters[i].expr,filters[i].count,
	    filters[i].bytes,filters[i].count*100.0/count);
    filters[i].bytes=0;
    filters[i].count=0;
  }
  packets=trace_get_received_packets(trace);
  if (packets!=UINT64_MAX)
    fprintf(stderr,"%30s:\t%12" PRIu64"\n",
	    "Input packets", packets);
  packets=trace_get_filtered_packets(trace);
  if (packets!=UINT64_MAX)
    fprintf(stderr,"%30s:\t%12" PRIu64"\n",
	    "Filtered packets", packets);
  packets=trace_get_dropped_packets(trace);
  if (packets!=UINT64_MAX)
    fprintf(stderr,"%30s:\t%12" PRIu64"\n",
	    "Dropped packets",packets);
  packets=trace_get_accepted_packets(trace);
  if (packets!=UINT64_MAX)
    fprintf(stderr,"%30s:\t%12" PRIu64 "\n",
	    "Accepted Packets",packets);
  fprintf(stderr, "%30s:\t%12"PRIu64"\t%12" PRIu64 "\n","Total",count,bytes);
  totcount+=count;
  totbytes+=bytes;

  if (trace_is_err(trace))
    trace_perror(trace,"%s",uri);

  trace_destroy(trace);
}

static void usage(char *argv0)
{
  fprintf(stderr,"Usage: %s [-H|--libtrace-help] [--filter|-f bpf ]... libtraceuri...\n",argv0);
}

int main(int argc, char *argv[]) {

  int i;
  struct sigaction sigact;

  while(1) {
    int option_index;
    struct option long_options[] = {
      { "filter",	   1, 0, 'f' },
      { "libtrace-help", 0, 0, 'H' },
      { NULL, 	   0, 0, 0   },
    };

    int c=getopt_long(argc, argv, "f:H",
		      long_options, &option_index);

    if (c==-1)
      break;

    switch (c) {
    case 'f':
      ++filter_count;
      filters=realloc(filters,filter_count*sizeof(struct filter_t));
      filters[filter_count-1].expr=strdup(optarg);
      filters[filter_count-1].filter=trace_create_filter(optarg);
      filters[filter_count-1].count=0;
      filters[filter_count-1].bytes=0;
      break;
    case 'H':
      trace_help();
      exit(1);
      break;
    default:
      fprintf(stderr,"Unknown option: %c\n",c);
      usage(argv[0]);
      return 1;
    }
  }

  if(optind > argc - 1)
    {
      usage(argv[0]);
      exit(-1);
    }

  sigact.sa_handler = cleanup_signal;
  sigemptyset(&sigact.sa_mask);
  sigact.sa_flags = SA_RESTART;

  sigaction(SIGINT, &sigact, NULL);
  sigaction(SIGTERM, &sigact, NULL);

  for(i=optind;i<argc;++i) {
    run_trace(argv[i]);
  }
  if (optind+1<argc) {
    fprintf(stderr, "Grand total:\n");
    fprintf(stderr,
	    "%30s:\t%12"PRIu64"\t%12" PRIu64 "\n",
	    "Total",totcount,totbytes);
  }


  return 0;
}
