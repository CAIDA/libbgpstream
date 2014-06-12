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

#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "libtrace.h"

#include "corsaro.h"
#include "corsaro_log.h"
#include "corsaro_io.h"

#include "corsaro_flowtuple.h"

/** @file
 *
 * @brief Code which uses libcorsaro to convert an corsaro output file to ascii
 *
 * @author Alistair King
 *
 * @todo extend to allow to write out to binary again
 * @todo respect the tuple classes for reaggregation (currently classes are
 * discarded).
 * @todo add a BPF-like filter
 *
 */

/** Initialize the hash functions and datatypes */
KHASH_SET_INIT_INT64(64xx)

/*KHASH_INIT(sixt, corsaro_flowtuple_t*, khint32_t, 1,*/
KHASH_INIT(sixt_map, corsaro_flowtuple_t*, kh_64xx_t*, 1,
	   corsaro_flowtuple_hash_func, corsaro_flowtuple_hash_equal);

/** Hash to use when we are aggregating on packets */
KHASH_INIT(sixt_int, corsaro_flowtuple_t*, uint64_t, 1,
	   corsaro_flowtuple_hash_func, corsaro_flowtuple_hash_equal);

/** A map of aggregated flowtuple records */
static kh_sixt_map_t *sixt_f = NULL;
/** A hash of aggregated flowtuple records */
static kh_sixt_int_t *sixt_v = NULL;

/** The corsaro_in instance to read from */
static corsaro_in_t *corsaro = NULL;
/** The record object to read into */
static corsaro_in_record_t *record = NULL;

/** The amount of time to wait until we dump the hash */
static int interval = 0;

/** Set of FlowTuple fields that can be used for aggregation */
typedef enum field_index {
  /** The Source IP address field of the FlowTuple */
  SRC_IP    = 0,
  /** The Destination IP address field of the FlowTuple */
  DST_IP    = 1,
  /** The Source Port field of the FlowTuple */
  SRC_PORT  = 2,
  /** The Destination Port field of the FlowTuple */
  DST_PORT  = 3,
  /** The Protocol field of the FlowTuple */
  PROTO     = 4,
  /** The TTL field of the FlowTuple */
  TTL       = 5,
  /** The TCP Flags field of the FlowTuple */
  TCP_FLAGS = 6,
  /** The IP Length field of the FlowTuple */
  IP_LEN    = 7,
  /** The Packet Count field of the FlowTuple */
  VALUE     = 8,

  /** The number of possible FlowTuple fields */
  FIELD_CNT = 9,
} field_index_t;

/** Value if field is enabled */
#define FIELD_ENABLED 1

/** Array of strings corresponding to FlowTuple fields */
static char *field_names[] = {
  "src_ip",
  "dst_ip",
  "src_port",
  "dst_port",
  "protocol",
  "ttl",
  "tcp_flags",
  "ip_len",
  "packet_cnt",
};

/** Set if reading from a legacy FlowTuple file */
static int legacy = 0;

/** An array of enabled fields for aggregation */
static field_index_t fields[FIELD_CNT];

/** The field to use as the value in aggregation */
static int value_field = -1;

/** The number of flowtuple records we have processed */
static uint64_t flowtuple_cnt = 0;

/** the END time of the interval that we last dumped data */
static corsaro_interval_t last_dump_end = {
  CORSARO_MAGIC,
  CORSARO_MAGIC_INTERVAL,
  0,
  0
};

/** The time that we need to dump the next interval at */
static int next_interval = 0;
/** The time that the last interval ended */
static corsaro_interval_t last_interval_end = {
  CORSARO_MAGIC,
  CORSARO_MAGIC_INTERVAL,
  0,
  0
};

/** Cleanup and free state */
static void clean()
{
  if(record != NULL)
    {
      corsaro_in_free_record(record);
      record = NULL;
    }

  if(corsaro != NULL)
    {
      corsaro_finalize_input(corsaro);
      corsaro = NULL;
    }
}

/** Initialize a corsaro_in instance for the given input file name */
static int init_corsaro(char *corsarouri)
{
  /* get an corsaro_in object */
  if((corsaro = corsaro_alloc_input(corsarouri)) == NULL)
    {
      fprintf(stderr, "could not alloc corsaro_in\n");
      clean();
      return -1;
    }

  /* get a record */
  if ((record = corsaro_in_alloc_record(corsaro)) == NULL) {
    fprintf(stderr, "could not alloc record\n");
    clean();
    return -1;
  }

  /* start corsaro */
  if(corsaro_start_input(corsaro) != 0)
    {
      fprintf(stderr, "could not start corsaro\n");
      clean();
      return -1;
    }

  return 0;
}

/** Either add the given flowtuple to the hash, or add the value to the map */
static int add_inc_map(void *h, corsaro_flowtuple_t *t, uint32_t value)
{
  kh_sixt_map_t *hash = (kh_sixt_map_t *)h;
  int khret;
  khiter_t khiter;
  corsaro_flowtuple_t *new_6t = NULL;
  kh_64xx_t *val_map = NULL;

  assert(hash != NULL);

  /* this function must not be used to aggregate for packet counts */
  assert(value_field != VALUE);

  /* check if this is in the hash already */
  if((khiter = kh_get(sixt_map, hash, t)) == kh_end(hash))
    {
      /* create a new tuple struct */
      if((new_6t = malloc(sizeof(corsaro_flowtuple_t))) == NULL)
	{
	  corsaro_log_file(__func__, NULL, "malloc failed");
	  return -1;
	}

      /* fill it */
      memcpy(new_6t, t, sizeof(corsaro_flowtuple_t));

      /* add it to the hash */
      khiter = kh_put(sixt_map, hash, new_6t, &khret);

      /* create a new map for this key */
      val_map = kh_init(64xx);

      /* add this value to the map */
      kh_put(64xx, val_map, value, &khret);

      /* set the value in the hash to the map */
      kh_value(hash, khiter) = val_map;
    }
  else
    {
      /* simply add this value to the map */
      kh_put(64xx, kh_value(hash, khiter), value, &khret);
    }
  return 0;
}

/** Either add the given flowtuple to the hash, or increment the current count */
int add_inc_hash(kh_sixt_int_t *hash, corsaro_flowtuple_t *t, uint32_t increment)
{
  int khret;
  khiter_t khiter;
  corsaro_flowtuple_t *new_6t = NULL;

  assert(hash != NULL);

  /* check if this is in the hash already */
  if((khiter = kh_get(sixt_int, hash, t)) == kh_end(hash))
    {
      /* create a new tuple struct */
      if((new_6t = malloc(sizeof(corsaro_flowtuple_t))) == NULL)
	{
	  corsaro_log_file(__func__, NULL, "malloc failed");
	  return -1;
	}

      /* fill it */
      memcpy(new_6t, t, sizeof(corsaro_flowtuple_t));

      /* add it to the hash */
      khiter = kh_put(sixt_int, hash, new_6t, &khret);
      /* set the count to one */
      kh_value(hash, khiter) = increment;
      /*new_6t->packet_cnt = htonl(increment);*/
    }
  else
    {
      /* will this cause a wrap? */
      assert((UINT64_MAX - kh_value(hash, khiter)) > increment);
      /* simply increment the existing one */
      kh_value(hash, khiter)+=increment;
      /*new_6t = kh_key(hash, khiter);*/
      /*new_6t->packet_cnt = htonl(ntohl(new_6t->packet_cnt)+increment);*/
    }
  return 0;
}

/** Print a flowtuple with a 64 bit value field */
static void flowtuple_print_64(corsaro_flowtuple_t *flowtuple, uint64_t value)
{
  char ip_a[16];
  char ip_b[16];
  uint32_t tmp;

  assert(flowtuple != NULL);

  tmp = flowtuple->src_ip;
  inet_ntop(AF_INET,&tmp, &ip_a[0], 16);
  tmp = CORSARO_FLOWTUPLE_SIXT_TO_IP(flowtuple);
  inet_ntop(AF_INET, &tmp, &ip_b[0], 16);

  fprintf(stdout, "%s|%s"
	 "|%"PRIu16"|%"PRIu16
	 "|%"PRIu8"|%"PRIu8"|0x%02"PRIx8
	 "|%"PRIu16
	 ",%"PRIu64"\n",
	 ip_a, ip_b,
	 ntohs(flowtuple->src_port),
	 ntohs(flowtuple->dst_port),
	 flowtuple->protocol,
	 flowtuple->ttl,
	 flowtuple->tcp_flags,
	 ntohs(flowtuple->ip_len),
	 value);
}

/** Dump a map of flowtuple records */
static void dump_hash_map(kh_sixt_map_t *hash)
{
  khiter_t k;
  corsaro_flowtuple_t *key;

  /* dump the hash */
  if(kh_size(hash) > 0)
    {
      for(k = kh_begin(hash); k != kh_end(hash); ++k)
	{
	  if(kh_exist(hash, k))
	    {
	      key = kh_key(hash, k);
	      /*key->packet_cnt = htonl(kh_val(hash,k));*/
	      flowtuple_print_64(key, kh_size(kh_val(hash, k)));
	      /* free the map while we still have a pointer to it */
	      kh_destroy(64xx, kh_val(hash, k));
	    }
	}
    }

  /* empty the hash */
  kh_free(sixt_map, hash, &corsaro_flowtuple_free);
  kh_clear(sixt_map, hash);
}

/** Dump a hash of flowtuple records */
static void dump_hash_int(kh_sixt_int_t *hash)
{
  khiter_t k;
  corsaro_flowtuple_t *key;

  /* dump the hash */
  if(kh_size(hash) > 0)
    {
      for(k = kh_begin(hash); k != kh_end(hash); ++k)
	{
	  if(kh_exist(hash, k))
	    {
	      key = kh_key(hash, k);
	      /*key->packet_cnt = htonl(kh_val(hash,k));*/
	      flowtuple_print_64(key, kh_val(hash,k));
	    }
	}
    }

  /* empty the hash */
  kh_free(sixt_int, hash, &corsaro_flowtuple_free);
  kh_clear(sixt_int, hash);
}

/** Dump the aggregated FlowTuple records */
static void dump_hash()
{
  assert(sixt_f || sixt_v);

  corsaro_io_print_interval_start(&last_dump_end);

  if(sixt_f != NULL)
    {
      dump_hash_map(sixt_f);
    }
  else
    {
      dump_hash_int(sixt_v);
    }

  corsaro_io_print_interval_end(&last_interval_end);

  /* move on to the next interval start */
  last_dump_end.number++;
  /* move on to the next interval end */
  last_interval_end.number++;
  /* translate from int_end to int_start */
  last_dump_end.time = last_interval_end.time+1;
}

/** Process a FlowTuple record */
static int process_flowtuple(corsaro_flowtuple_t *tuple)
{
  int i;

  int value;

  /* work out which field from the tuple we want to use as the value */
  switch(value_field)
    {
    case SRC_IP:
      value = ntohl(tuple->src_ip);
      break;
    case DST_IP:
      value = ntohl(CORSARO_FLOWTUPLE_SIXT_TO_IP(tuple));
      break;
    case SRC_PORT:
      value = ntohs(tuple->src_port);
      break;
    case DST_PORT:
      value = ntohs(tuple->dst_port);
      break;
    case PROTO:
      value = tuple->protocol;
      break;
    case TTL:
      value = tuple->ttl;
      break;
    case TCP_FLAGS:
      value = tuple->tcp_flags;
      break;
    case IP_LEN:
      value = ntohs(tuple->ip_len);
      break;
    case VALUE:
      value = ntohl(tuple->packet_cnt);
      break;
    default:
      fprintf(stderr, "ERROR: invalid value field number\n");
      clean();
      return -1;
    }

  /* zero out the fields we do not care about */
  for(i = 0; i < FIELD_CNT; i++)
    {
      if(fields[i] != FIELD_ENABLED)
	{
	  switch(i)
	    {
	    case SRC_IP:
	      tuple->src_ip = 0;
	      break;
	    case DST_IP:
	      CORSARO_FLOWTUPLE_IP_TO_SIXT(0, tuple);
	      break;
	    case SRC_PORT:
	      tuple->src_port = 0;
	      break;
	    case DST_PORT:
	      tuple->dst_port = 0;
	      break;
	    case PROTO:
	      tuple->protocol = 0;
	      break;
	    case TTL:
	      tuple->ttl = 0;
	      break;
	    case TCP_FLAGS:
	      tuple->tcp_flags = 0;
	      break;
	    case IP_LEN:
	      tuple->ip_len = 0;
	      break;
	    case VALUE:
	      tuple->packet_cnt = 0;
	      break;
	    default:
	      fprintf(stderr, "ERROR: invalid field number\n");
	      clean();
	      return -1;
	    }
	}
    }

  /* check if this stripped down flowtuple is already in the hash,
   if not, add it.*/
  if(value_field == VALUE)
    {
      if(add_inc_hash(sixt_v, tuple, value) != 0)
	{
	  fprintf(stderr, "couldn't increment flowtuple packet_cnt value\n");
	  return -1;
	}
    }
  else
    {
      if(add_inc_map(sixt_f, tuple, value) != 0)
	{
	  fprintf(stderr, "could not add value to map");
	  return -1;
	}
    }

  return 0;
}

/** Process a flowtuple file */
int process_flowtuple_file(char *file)
{
  off_t len = 0;
  corsaro_in_record_type_t type = CORSARO_IN_RECORD_TYPE_NULL;
  corsaro_interval_t *interval_record;
  corsaro_flowtuple_t *tuple;
    
  fprintf(stderr, "processing %s\n", file);
    
  /* this must be done before corsaro_init_output */
  if(init_corsaro(file) != 0)
    {
      fprintf(stderr, "failed to init corsaro\n");
      clean();
      return -1;
    }
    
  /* dirty hack to not -1 on the last interval in the previous file */
  if(last_interval_end.time > 0)
    {
      last_interval_end.time+=legacy;
    }
    
  while ((len = corsaro_in_read_record(corsaro, &type, record)) > 0) {
    /* we want to know the current time, so we will watch for interval start
       records */
    if(type == CORSARO_IN_RECORD_TYPE_IO_INTERVAL_START)
      {
	interval_record = (corsaro_interval_t *)
	  corsaro_in_get_record_data(record);
            
	if(interval_record->time <= last_dump_end.time)
	  {
	    fprintf(stderr, "ERROR: decrease in timestamp.\n"
		    "Are the input files sorted properly?\n");
	    clean();
	    return -1;
	  }
            
	if(flowtuple_cnt == 0)
	  {
	    last_dump_end.time = interval_record->time;
	    next_interval = interval_record->time + interval;
	  }
            
	/* an interval of 0 means dump at the source interval */
	if(last_interval_end.time > 0)
	  {
	    /* this was a non-end interval, if it is legacy, subtract
	       one from the last_interval_end time */
	    last_interval_end.time-=legacy;
	    if(interval == 0)
	      {
		dump_hash();
	      }
	    else if(interval > 0)
	      {
		while(interval_record->time >= next_interval)
		  {
		    dump_hash();
		    next_interval += interval;
		  }
	      }
	    /* else, if interval < 0, only dump at the end */
	  }
      }
    else if(type == CORSARO_IN_RECORD_TYPE_IO_INTERVAL_END)
      {
	interval_record = (corsaro_interval_t *)
	  corsaro_in_get_record_data(record);
            
	last_interval_end.time = interval_record->time;
            
      }
    else if(type == CORSARO_IN_RECORD_TYPE_FLOWTUPLE_FLOWTUPLE)
      {
	tuple = (corsaro_flowtuple_t *)corsaro_in_get_record_data(record);
	flowtuple_cnt++;
            
	process_flowtuple(tuple);
      }
        
    /* reset the type to NULL to indicate we don't care */
    type = CORSARO_IN_RECORD_TYPE_NULL;
  }
    
  if(len < 0)
    {
      fprintf(stderr, "corsaro_in_read_record failed to read record\n");
      clean();
      return -1;
    }
    
  clean();
    
  return 0;

}


/** Print usage information to stderr */
static void usage(const char *name)
{
  fprintf(stderr,
	  "usage: %s [-l] [-i interval] [-v value_field] [-f field]... [-F file_list] \n"
	  "          flowtuple_file [flowtuple_file]\n"
	  "       -l             treat the input files as containing legacy format data\n"
	  "       -i <interval>  new distribution interval in seconds. (default: 0)\n"
	  "                       a value of -1 aggregates to a single interval\n"
	  "                       a value of 0 uses the original interval\n"
	  "       -v <value>     field to use as aggregation value (default: packet_cnt)\n"
	  "       -f <field>     a tuple field to re-aggregate with\n"
	  "       -F <file_list> a file with the list flowtuple files\n"
	  "                       use '-' to read the list from standard input\n"
	  "\n"
	  "Supported field names are:\n"
	  " src_ip, dst_ip, src_port, dst_port, protocol, ttl, tcp_flags, \n"
	  " ip_len, packet_cnt\n",
	  name);
}

/** Entry point for the cors-ft-aggregate tool */
int main(int argc, char *argv[])
{
  int opt;
  int i;

  int field_cnt = 0;

  /** A pointer to the file which contains the list of input files */
  FILE *flist = NULL;
  /** The file currently being processed by corsaro */
  char file[1024];

  int wanted_n_fields = 0;

  while((opt = getopt(argc, argv, "li:f:F:v:?")) >= 0)
    {
      switch(opt)
	{
	case 'l':
	  /* the user has indicated they're giving us old format data
	     which has the interval end at +1 than we currently use */
	  legacy = 1;
	  break;

	case 'i':
	  interval = atoi(optarg);
	  break;

	case 'f':
	  wanted_n_fields++;
	  /* figure out what field they have asked for and then set the
	     appropriate field in the bitmap */
	  for(i = 0; i < FIELD_CNT; i++)
	    {
	      if(strcmp(optarg, field_names[i]) == 0)
		{
		  fields[i] = FIELD_ENABLED;
		  field_cnt++;
		  break;
		}
	    }
	  break;

        case 'F':
	  /* check if a list of files has been already specified */
	  if (flist != NULL) {
	    fprintf(stderr,"a list of file has been already specified \n"
		    "this file is ignored: %s\n",optarg);
	    break;
	  }

	  /* check if the list of file is on stdin or in a file */
	  if(strcmp(optarg, "-") == 0)
            {
	      flist = stdin;
            }
	  else if((flist = fopen(optarg, "r")) == NULL)
            {
	      fprintf(stderr, "failed to open list of input files (%s)\n"
		      "NB: File List MUST be sorted\n", optarg);
	      return -1;
            }
	  break;

	case 'v':
	  if(value_field >= 0)
	    {
	      fprintf(stderr, "WARNING: Multiple value fields detected\n"
		      "Last specified will be used\n");
	    }
	  /* figure out what value they have asked for */
	  for(i = 0; i < FIELD_CNT; i++)
	    {
	      if(strcmp(optarg, field_names[i]) == 0)
		{
		  value_field = i;
		  break;
		}
	    }
	  break;

	case '?':
	  usage(argv[0]);
	  exit(0);
	  break;

	default:
	  usage(argv[0]);
	  exit(-1);
	}
    }

  /*
   * ak comments this 10/11/12 after talking with Tanja
   * it seems to make sense to allow *no* fields to be set so that users
   * can get 'overall' counts of field values
   * e.g. specifying no fields and using a 'src_ip' value will give the total
   * number of source ips in the interval
   */
  /*
  if(field_cnt < 1)
    {
      for(i = 0; i < FIELD_CNT; i++)
	{
	  fields[i] = FIELD_ENABLED;
	}
    }
  */

  if(wanted_n_fields != field_cnt)
    {
      fprintf(stderr, "Invalid field name\n");
      usage(argv[0]);
      exit(-1);
    }

  if(value_field < 0)
    {
      fprintf(stderr, "No value field specified. Defaulting to packet count\n");
      value_field = VALUE;
    }

  /* initialize the hash */
  if(value_field == VALUE)
    {
      sixt_v = kh_init(sixt_int);
    }
  else
    {
      sixt_f = kh_init(sixt_map);
    }

  /* check if the user specified a list file */
  if(flist !=  NULL)
    {
      /* flist point to a file of list of paths */
      while(fgets(file, sizeof(file), flist) != NULL)
	{
          /* chomp off the newline */
          file[strlen(file)-1] = '\0';
          /* process flowtuple file */
          if(process_flowtuple_file(file) == -1)
	    {
              return -1;
	    }
	}
        fclose(flist);
    }
  else /* user specified file directly on the command line */
    {
      if(optind >= argc)
	{
          /* no files has been specified */
          usage(argv[0]);
          exit(-1);
	}
      /* iterate over all the files on the command line */
      for (int i = optind; i < argc; i++)
	{
	  if(process_flowtuple_file(argv[i]) == -1)
	    {
	      return -1;
	    }
	}
    }

  /* dump again if the hash is not empty */
  if((sixt_f != NULL && kh_size(sixt_f) > 0)
     || (sixt_v != NULL && kh_size(sixt_v) > 0))
    {
      dump_hash();
    }

  if(sixt_f != NULL)
    {
      kh_free(sixt_map, sixt_f, &corsaro_flowtuple_free);
      kh_destroy(sixt_map, sixt_f);
      sixt_f = NULL;
    }

  if(sixt_v != NULL)
    {
      kh_free(sixt_int, sixt_v, &corsaro_flowtuple_free);
      kh_destroy(sixt_int, sixt_v);
      sixt_v = NULL;
    }

  return 0;
}
