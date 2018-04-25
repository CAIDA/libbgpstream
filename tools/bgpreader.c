/*
 * Copyright (C) 2014 The Regents of the University of California.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Authors:
 *   Chiara Orsini
 *   Alistair King
 *   Shane Alcock <salcock@waikato.ac.nz>
 */

#include "config.h"
#include <assert.h>
#include <ctype.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <unistd.h>
#include "bgpstream.h"
#include "getopt.h"
#include "utils.h"

#define PROJECT_CMD_CNT 10
#define TYPE_CMD_CNT 10
#define COLLECTOR_CMD_CNT 100
#define PREFIX_CMD_CNT 1000
#define COMMUNITY_CMD_CNT 1000
#define PEERASN_CMD_CNT 1000
#define WINDOW_CMD_CNT 1024
#define OPTION_CMD_CNT 1024
#define BGPSTREAM_RECORD_OUTPUT_FORMAT                                         \
  "# Record format:\n"                                                         \
  "# "                                                                         \
  "<type>|<dump-pos>|<rec-ts-sec>.<rec-ts-usec>|<project>|<collector>|<"       \
  "router>|<router-ip>|<status>|<dump-time>\n"                                 \
  "#\n"                                                                        \
  "# <type>: R RIB, U Update\n"                                                \
  "# <dump-pos>:  B begin, M middle, E end\n"                                  \
  "# <status>:    V valid, E empty, F filtered, O outside interval,\n"         \
  "#              R corrupted record, S corrupted source\n"                    \
  "#\n"
#define BGPSTREAM_ELEM_OUTPUT_FORMAT                                           \
  "# Elem format:\n"                                                           \
  "# "                                                                         \
  "<rec-type>|<elem-type>|<rec-ts-sec>.<rec-ts-usec>|<project>|<collector>|<"  \
  "router>|<router-ip>|<peer-ASN>|<peer-IP>|<prefix>|<next-hop-IP>|<AS-path>|" \
  "<origin-AS>|<communities>|<old-state>|<new-state>\n"                        \
  "#\n"                                                                        \
  "# <rec-type>: R RIB, U Update\n"                                            \
  "# <elem-type>: R RIB, A announcement, W withdrawal, S state message\n"      \
  "#\n"                                                                        \

#define LONG_OPTIONS                                                           \
  (struct option [])                                                           \
  {                                                                            \
    {"filter",                required_argument, 0, 'f'},                      \
    {"interval",              required_argument, 0, 'I'},                      \
    {"data-interface",        required_argument, 0, 'd'},                      \
    {"data-interface-option", required_argument, 0, 'o'},                      \
    {"project",               required_argument, 0, 'p'},                      \
    {"collector",             required_argument, 0, 'c'},                      \
    {"record-type",           required_argument, 0, 't'},                      \
    {"time-window",           required_argument, 0, 'w'},                      \
    {"peer-asn",              required_argument, 0, 'j'},                      \
    {"prefix",                required_argument, 0, 'k'},                      \
    {"community",             required_argument, 0, 'y'},                      \
    {"rib-period",            required_argument, 0, 'P'},                      \
    {"count",                 required_argument, 0, 'n'},                      \
    {"live",                  no_argument,       0, 'l'},                      \
    {"output-records",        no_argument,       0, 'r'},                      \
    {"output-bgpdump",        no_argument,       0, 'm'},                      \
    {"output-elems",          no_argument,       0, 'e'},                      \
    {"output-headers",        no_argument,       0, 'i'},                      \
    {"version",               no_argument,       0, 'v'},                      \
    {"help",                  no_argument,       0, 'h'},                      \
    {"help",                  no_argument,       0, '?'},                      \
    { 0, 0, 0, 0 }                                                             \
  }

#define LONG_OPTIONS_CNT (ARR_CNT(LONG_OPTIONS) - 1)

static char short_options[LONG_OPTIONS_CNT * 2 + 1] = {0};

struct window {
  uint32_t start;
  uint32_t end;
};

static char buf[65536];

static bgpstream_t *bs;
static bgpstream_data_interface_id_t di_id_default = 0;
static bgpstream_data_interface_id_t di_id = 0;
static bgpstream_data_interface_info_t *di_info = NULL;

static void data_if_usage()
{
  bgpstream_data_interface_id_t *ids = NULL;
  int id_cnt = 0;
  int i;

  bgpstream_data_interface_info_t *info = NULL;

  id_cnt = bgpstream_get_data_interfaces(bs, &ids);

  for (i = 0; i < id_cnt; i++) {
    info = bgpstream_get_data_interface_info(bs, ids[i]);

    if (info != NULL) {
      fprintf(stderr, "       %-16s%s%s\n", info->name, info->description,
              (ids[i] == di_id_default) ? " (default)" : "");
    }
  }
}

static void dump_if_options()
{
  assert(di_id != _BGPSTREAM_DATA_INTERFACE_INVALID);

  bgpstream_data_interface_option_t *options;
  int opt_cnt = 0;
  int i;

  opt_cnt = bgpstream_get_data_interface_options(bs, di_id, &options);

  fprintf(stderr, "Data interface options for '%s':\n", di_info->name);
  if (opt_cnt == 0) {
    fprintf(stderr, "   [NONE]\n");
  } else {
    for (i = 0; i < opt_cnt; i++) {
      fprintf(stderr, "   %-15s%s\n", options[i].name, options[i].description);
    }
  }
  fprintf(stderr, "\n");
}

static void usage()
{
  fprintf(
    stderr,
    "usage: bgpreader -w <start>[,<end>] [<options>]\n"
    "Available options are:\n"
    "   -d <interface> use the given data interface to find available data\n"
    "                  available data interfaces are:\n");
  data_if_usage();
  fprintf(
    stderr,
    "   -o <option-name=option-value>*\n"
    "                  set an option for the current data interface.\n"
    "                  use '-o ?' to get a list of available options for the "
    "current\n"
    "                  data interface. (data interface can be selected using "
    "-d)\n"
    "   -p <project>   process records from only the given project "
    "(routeviews, ris)*\n"
    "   -c <collector> process records from only the given collector*\n"
    "   -t <type>      process records with only the given type (ribs, "
    "updates)*\n"
    "   -f <filterstring>   filter records and elements using the rules \n"
    "                       described in the given filter string\n"
    "   -I <interval>       process records that were received recently, where "
    "the\n"
    "                       interval describes how far back in time to go. The "
    "\n"
    "                       interval should be expressed as '<num> <unit>', "
    "where\n"
    "                       where <unit> can be one of 's', 'm', 'h', 'd' "
    "(seconds,\n"
    "                       minutes, hours, days).\n"
    "   -w <start>[,<end>]\n"
    "                  process records within the given time window\n"
    "                    specified in Unix epoch time\n"
    "                    (omitting the end parameter enables live mode)*\n"
    "   -P <period>    process a rib files every <period> seconds (bgp "
    "time)\n"
    "   -j <peer ASN>  return valid elems originated by a specific peer "
    "ASN*\n"
    "   -k <prefix>    return valid elems associated with a specific "
    "prefix*\n"
    "   -y <community> return valid elems with the specified community* \n"
    "                  (format: asn:value, the '*' metacharacter is "
    "recognized)\n"
    "   -l             enable live mode (make blocking requests for BGP "
    "records)\n"
    "                  allows bgpstream to be used to process data in "
    "real-time\n"
    "\n"
    "   -e             print info for each element of a valid BGP record "
    "(default)\n"
    "   -m             print info for each BGP valid record in bgpdump -m "
    "format\n"
    "   -r             print info for each BGP record (used mostly for "
    "debugging BGPStream)\n"
    "   -i             print format information before output\n"
    "\n"
    "   -n <rec-cnt>   process at most <rec-cnt> records\n"
    "   -h             print this help menu\n"
    "* denotes an option that can be given multiple times\n");
}

// print / utility functions

static int print_record(bgpstream_record_t *record);
static int print_elem(bgpstream_record_t *record, bgpstream_elem_t *elem);

int main(int argc, char *argv[])
{

  int opt;
  int prevoptind;

  opterr = 0;

  // variables associated with options
  char *projects[PROJECT_CMD_CNT];
  int projects_cnt = 0;

  char *types[TYPE_CMD_CNT];
  int types_cnt = 0;

  char *collectors[COLLECTOR_CMD_CNT];
  int collectors_cnt = 0;

  char *peerasns[PEERASN_CMD_CNT];
  int peerasns_cnt = 0;

  char *prefixes[PREFIX_CMD_CNT];
  int prefixes_cnt = 0;

  char *communities[COMMUNITY_CMD_CNT];
  int communities_cnt = 0;

  struct window windows[WINDOW_CMD_CNT];
  char *endp;
  int windows_cnt = 0;

  char *interface_options[OPTION_CMD_CNT];
  int interface_options_cnt = 0;

  char *filterstring = NULL;
  char *intervalstring = NULL;

  int rib_period = 0;
  int live = 0;
  int output_info = 0;
  int record_output_on = 0;
  int record_bgpdump_output_on = 0;
  int elem_output_on = 0;

  int rec_limit = -1;

  bgpstream_data_interface_option_t *option;

  int i;

  /* required to be created before usage is called */
  bs = bgpstream_create();
  if (!bs) {
    fprintf(stderr, "ERROR: Could not create BGPStream instance\n");
    return -1;
  }
  di_id_default = di_id = bgpstream_get_data_interface_id(bs);
  di_info = bgpstream_get_data_interface_info(bs, di_id);
  assert(di_id != 0);

  /* allocate memory for bs_record */
  bgpstream_record_t *bs_record = NULL;

  /* build the short options */
  int k;
  for (k = 0; k < LONG_OPTIONS_CNT; k++) {
    size_t size = strlen(short_options);
    snprintf(short_options + size, sizeof(short_options) - size,
             LONG_OPTIONS[k].has_arg ? "%c:": "%c", LONG_OPTIONS[k].val);
  }

  while (prevoptind = optind,
         (opt = getopt_long(argc, argv, short_options, LONG_OPTIONS, 0)) >= 0) {
    if (optind == prevoptind + 2 && (optarg == NULL || *optarg == '-')) {
      opt = ':';
      --optind;
    }
    switch (opt) {
    case 'p':
      if (projects_cnt == PROJECT_CMD_CNT) {
        fprintf(stderr, "ERROR: A maximum of %d projects can be specified on "
                        "the command line\n",
                PROJECT_CMD_CNT);
        usage();
        goto err;
      }
      projects[projects_cnt++] = strdup(optarg);
      break;
    case 'c':
      if (collectors_cnt == COLLECTOR_CMD_CNT) {
        fprintf(stderr, "ERROR: A maximum of %d collectors can be specified on "
                        "the command line\n",
                COLLECTOR_CMD_CNT);
        usage();
        goto err;
      }
      collectors[collectors_cnt++] = strdup(optarg);
      break;
    case 't':
      if (types_cnt == TYPE_CMD_CNT) {
        fprintf(stderr, "ERROR: A maximum of %d types can be specified on "
                        "the command line\n",
                TYPE_CMD_CNT);
        usage();
        goto err;
      }
      types[types_cnt++] = strdup(optarg);
      break;
    case 'w':
      if (windows_cnt == WINDOW_CMD_CNT) {
        fprintf(stderr, "ERROR: A maximum of %d windows can be specified on "
                        "the command line\n",
                WINDOW_CMD_CNT);
        usage();
        goto err;
      }
      /* split the window into a start and end */
      if ((endp = strchr(optarg, ',')) == NULL) {
        windows[windows_cnt].end = BGPSTREAM_FOREVER;
      } else {
        *endp = '\0';
        endp++;
        windows[windows_cnt].end = atoi(endp);
      }
      windows[windows_cnt].start = atoi(optarg);
      windows_cnt++;
      break;
    case 'j':
      if (peerasns_cnt == PEERASN_CMD_CNT) {
        fprintf(stderr, "ERROR: A maximum of %d peer asns can be specified on "
                        "the command line\n",
                PEERASN_CMD_CNT);
        usage();
        goto err;
      }
      peerasns[peerasns_cnt++] = strdup(optarg);
      break;
    case 'k':
      if (prefixes_cnt == PREFIX_CMD_CNT) {
        fprintf(stderr, "ERROR: A maximum of %d peer asns can be specified on "
                        "the command line\n",
                PREFIX_CMD_CNT);
        usage();
        goto err;
      }
      prefixes[prefixes_cnt++] = strdup(optarg);
      break;
    case 'y':
      if (communities_cnt == COMMUNITY_CMD_CNT) {
        fprintf(stderr,
                "ERROR: A maximum of %d communities can be specified on "
                "the command line\n",
                PREFIX_CMD_CNT);
        usage();
        goto err;
      }
      communities[communities_cnt++] = strdup(optarg);
      break;
    case 'P':
      rib_period = atoi(optarg);
      break;
    case 'd':
      if ((di_id = bgpstream_get_data_interface_id_by_name(bs, optarg)) == 0) {
        fprintf(stderr, "ERROR: Invalid data interface name '%s'\n", optarg);
        usage();
        goto err;
      }
      di_info = bgpstream_get_data_interface_info(bs, di_id);
      break;
    case 'o':
      if (interface_options_cnt == OPTION_CMD_CNT) {
        fprintf(stderr,
                "ERROR: A maximum of %d interface options can be specified\n",
                OPTION_CMD_CNT);
        usage();
        goto err;
      }
      interface_options[interface_options_cnt++] = strdup(optarg);
      break;

    case 'n':
      rec_limit = atoi(optarg);
      fprintf(stderr, "INFO: Processing at most %d records\n", rec_limit);
      break;

    case 'l':
      live = 1;
      break;
    case 'r':
      record_output_on = 1;
      break;
    case 'm':
      record_bgpdump_output_on = 1;
      break;
    case 'e':
      elem_output_on = 1;
      break;
    case 'i':
      output_info = 1;
      break;
    case 'f':
      filterstring = optarg;
      break;
    case 'I':
      intervalstring = optarg;
      break;
    case ':':
      fprintf(stderr, "ERROR: Missing option argument for -%c\n", optopt);
      usage();
      goto err;
      break;
    case '?':
    case 'v':
      fprintf(stderr, "bgpreader version %d.%d.%d\n", BGPSTREAM_MAJOR_VERSION,
              BGPSTREAM_MID_VERSION, BGPSTREAM_MINOR_VERSION);
      usage();
      goto done;
      break;
    default:
      usage();
      goto err;
    }
  }

  for (i = 0; i < interface_options_cnt; i++) {
    if (*interface_options[i] == '?') {
      dump_if_options();
      usage();
      goto done;
    } else {
      /* actually set this option */
      if ((endp = strchr(interface_options[i], '=')) == NULL) {
        fprintf(stderr, "ERROR: Malformed data interface option (%s)\n",
                interface_options[i]);
        fprintf(stderr, "ERROR: Expecting <option-name>,<option-value>\n");
        usage();
        goto err;
      }
      *endp = '\0';
      endp++;
      if ((option = bgpstream_get_data_interface_option_by_name(
             bs, di_id, interface_options[i])) == NULL) {
        fprintf(stderr, "ERROR: Invalid option '%s' for data interface '%s'\n",
                interface_options[i], di_info->name);
        usage();
        goto err;
      }
      if (bgpstream_set_data_interface_option(bs, option, endp) != 0) {
        fprintf(stderr, "ERROR: Failed to set option '%s' for data interface '%s'\n",
                interface_options[i], di_info->name);
        usage();
        goto err;
      }
    }
    free(interface_options[i]);
    interface_options[i] = NULL;
  }
  interface_options_cnt = 0;

  if (windows_cnt == 0 && !intervalstring) {
    if (di_id == BGPSTREAM_DATA_INTERFACE_BROKER) {
      fprintf(stderr,
              "ERROR: At least one time window must be set when using the "
              "broker data interface\n");
      usage();
      goto err;
    } else {
      fprintf(stderr, "WARN: No time windows specified, defaulting to all "
                      "available data\n");
    }
  }

  /* if the user did not specify any output format
   * then the default one is per elem */
  if (record_output_on == 0 && elem_output_on == 0 &&
      record_bgpdump_output_on == 0) {
    elem_output_on = 1;
  }

  /* the program can now start */

  /* allocate memory for interface */

  /* Parse the filter string */
  if (filterstring) {
    bgpstream_parse_filter_string(bs, filterstring);
  }

  if (intervalstring) {
    bgpstream_add_recent_interval_filter(bs, intervalstring, live);
  }

  /* projects */
  for (i = 0; i < projects_cnt; i++) {
    bgpstream_add_filter(bs, BGPSTREAM_FILTER_TYPE_PROJECT, projects[i]);
    free(projects[i]);
  }

  /* collectors */
  for (i = 0; i < collectors_cnt; i++) {
    bgpstream_add_filter(bs, BGPSTREAM_FILTER_TYPE_COLLECTOR, collectors[i]);
    free(collectors[i]);
  }

  /* types */
  for (i = 0; i < types_cnt; i++) {
    bgpstream_add_filter(bs, BGPSTREAM_FILTER_TYPE_RECORD_TYPE, types[i]);
    free(types[i]);
  }

  /* windows */
  for (i = 0; i < windows_cnt; i++) {
    bgpstream_add_interval_filter(bs, windows[i].start, windows[i].end);
  }

  /* peer asns */
  for (i = 0; i < peerasns_cnt; i++) {
    bgpstream_add_filter(bs, BGPSTREAM_FILTER_TYPE_ELEM_PEER_ASN, peerasns[i]);
    free(peerasns[i]);
  }

  /* prefixes */
  for (i = 0; i < prefixes_cnt; i++) {
    bgpstream_add_filter(bs, BGPSTREAM_FILTER_TYPE_ELEM_PREFIX, prefixes[i]);
    free(prefixes[i]);
  }

  /* communities */
  for (i = 0; i < communities_cnt; i++) {
    bgpstream_add_filter(bs, BGPSTREAM_FILTER_TYPE_ELEM_COMMUNITY,
                         communities[i]);
    free(communities[i]);
  }

  /* frequencies */
  if (rib_period > 0) {
    bgpstream_add_rib_period_filter(bs, rib_period);
  }

  /* set data interface */
  bgpstream_set_data_interface(bs, di_id);

  /* live */
  if (live != 0) {
    bgpstream_set_live_mode(bs);
  }

  /* turn on interface */
  if (bgpstream_start(bs) < 0) {
    return -1;
  }

  if (output_info) {
    if (record_output_on) {
      printf(BGPSTREAM_RECORD_OUTPUT_FORMAT);
    }
    if (elem_output_on) {
      printf(BGPSTREAM_ELEM_OUTPUT_FORMAT);
    }
  }

  /* use the interface */
  int rrc = 0, erc = 0, rec_cnt = 0;
  bgpstream_elem_t *bs_elem;

  while ((rrc = bgpstream_get_next_record(bs, &bs_record)) > 0 &&
         (rec_limit < 0 || rec_cnt < rec_limit)) {
    rec_cnt++;

    if (record_output_on && print_record(bs_record) != 0) {
      goto err;
    }

    if (bs_record->status != BGPSTREAM_RECORD_STATUS_VALID_RECORD) {
      continue;
    }

    if (record_bgpdump_output_on) {
      bgpstream_record_print_mrt_data(bs_record);
    }

    if (elem_output_on) {
      /* check if the record is of type RIB, in case extract the ID */
      /* print the RIB start line */
      if (bs_record->type == BGPSTREAM_RIB &&
          bs_record->dump_pos == BGPSTREAM_DUMP_START &&
          print_record(bs_record) != 0) {
        goto err;
      }

      while ((erc = bgpstream_record_get_next_elem(bs_record, &bs_elem)) > 0) {
        if (print_elem(bs_record, bs_elem) != 0) {
          goto err;
        }
      }
      if (erc != 0) {
        fprintf(stderr, "ERROR: Failed to get elem from record\n");
        goto err;
      }
      /* check if end of RIB has been reached */
      if (bs_record->type == BGPSTREAM_RIB &&
          bs_record->dump_pos == BGPSTREAM_DUMP_END &&
          print_record(bs_record) != 0) {
        goto err;
      }
    }
  }
  if (rrc != 0) {
    fprintf(stderr, "ERROR: Failed to get record from stream\n");
    goto err;
  }

 done:
  /* deallocate memory for interface */
  bgpstream_destroy(bs);
  return 0;

err:
  bgpstream_destroy(bs);
  return -1;
}

/* print utility functions */

static int print_record(bgpstream_record_t *record)
{
  if (bgpstream_record_snprintf(buf, sizeof(buf), record) == NULL) {
    fprintf(stderr, "ERROR: Could not convert record to string\n");
    return -1;
  }

  printf("%s\n", buf);
  return 0;
}

static int print_elem(bgpstream_record_t *record, bgpstream_elem_t *elem)
{
  if (bgpstream_record_elem_snprintf(buf, sizeof(buf), record, elem) == NULL) {
    fprintf(stderr, "ERROR: Could not convert record/elem to string\n");
    return -1;
  }

  printf("%s\n", buf);
  return 0;
}
