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
#include <errno.h>
#include <sys/ioctl.h> // for TIOCGWINSZ
#ifdef WITH_RPKI
#include "utils/bgpstream_utils_rpki.h"
#endif
#include "bgpstream.h"
#include "utils.h"
#include "getopt.h"

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
  "#\n"

enum rpki_options {
  RPKI_OPTION_SSH = 500,
  RPKI_OPTION_COLLECTORS = 501,
  RPKI_OPTION_LIVE = 502,
  RPKI_OPTION_UNIFIED = 503,
  RPKI_OPTION_DEFAULT = 504
};

struct bs_options_t {
  struct option option;
  const char *usage;
  const char *expl;
};

static struct bs_options_t bs_opts[] =
{
  {{"data-interface", required_argument, 0, 'd'},
   "<interface>",
   "use the given data interface to find available data. "
   "Available values are:"},
  {{"filter", required_argument, 0, 'f'},
   "<filterstring>",
   "filter records and elements using the rules "
   "described in the given filter string"},
  {{"interval", required_argument, 0, 'I'},
   "<num> <unit>",
   "process records that were received the last <num> <unit>s of time, where "
   "<unit> is one of 's', 'm', 'h', 'd' (seconds, minutes, hours, days)." },
  {{"data-interface-option", required_argument, 0, 'o'},
   "<option-name>=<option-value>*",
   "set an option for the current data "
   "interface. Use '-o?' to get a list of available options for the "
   "current data interface (as selected with -d). "
   "Each option can only be set once."},
  {{"project", required_argument, 0, 'p'},
   "<project>",
   "process records from only the given project (routeviews, ris)*"},
  {{"collector", required_argument, 0, 'c'},
   "<collector>",
   "process records from only the given collector*"},
  {{"router", required_argument, 0, 'R'},
   "<router>",
   "process records from only the given router*"},
  {{"record-type", required_argument, 0, 't'},
   "<type>",
   "process records with only the given type (ribs, updates)*"},
  {{"resource-type", required_argument, 0, 'T'},
   "<resource-type>",
   "process records from only the given resource type (stream, batch)*"},
  {{"time-window", required_argument, 0, 'w'},
   "<start>[,<end>]",
   "process records within the given time window.  <start> and <end> may be in "
   "'Y-m-d [H:M[:S]]' format (in UTC) or in unix epoch time.  Omitting <end> "
   "enables live mode."},
  {{"rib-period", required_argument, 0, 'P'},
   "<period>",
   "process a rib files every <period> seconds (bgp time)"},
  {{"peer-asn", required_argument, 0, 'j'},
   "<peer ASN>",
   "return elems received by a given peer ASN*"},
  {{"origin-asn", required_argument, 0, 'a'},
   "<origin ASN>",
   "return elems originated by a given origin ASN*"},
  {{"prefix", required_argument, 0, 'k'},
   "<prefix>",
   "return elems associated with a given prefix*"},
  {{"community", required_argument, 0, 'y'},
   "<community>",
   "return elems with the specified community* "
   "(format: asn:value. the '*' metacharacter is recognized)"},
  {{"aspath", required_argument, 0, 'A'},
   "<regex>",
   "return elems that match the aspath regex*"},
  {{"count", required_argument, 0, 'n'},
   "<rec-cnt>",
   "process at most <rec-cnt> records"},
  {{"live", no_argument, 0, 'l'},
   "",
   "enable live mode (make blocking requests for BGP records); "
   "allows bgpstream to be used to process data in real-time"},
  {{"output-elems", no_argument, 0, 'e'},
   "",
   "print info "
   "for each element of a BGP record (default)"},
  {{"output-bgpdump", no_argument, 0, 'm'},
   "",
   "print info "
   "for each BGP record in bgpdump -m format"},
  {{"output-records", no_argument, 0, 'r'},
   "",
   "print info "
   "for each BGP record (used mostly for debugging BGPStream)"},
  {{"output-headers", no_argument, 0, 'i'},
   "",
   "print format information before output"},
  {{"version", no_argument, 0, 'v'},
   "",
   "print the version of bgpreader"},
#ifdef WITH_RPKI
  {{"rpki", no_argument, 0, RPKI_OPTION_DEFAULT},
   "",
   "validate the BGP "
   "records with historical RPKI dumps (default collector)"},
  {{"rpki-live", no_argument, 0, RPKI_OPTION_LIVE},
   "",
   "validate the BGP "
   " records with the current RPKI dump (default collector)"},
  {{"rpki-collectors", required_argument, 0, RPKI_OPTION_COLLECTORS},
   "<((*|project):(*|(collector(,collectors)*))(;)?)*>",
   "specify the "
   "collectors used for (historical or live) RPKI validation "},
  {{"rpki-unified", no_argument, 0, RPKI_OPTION_UNIFIED},
   "",
   "whether the RPKI validation for different collectors is unified"},
  {{"rpki-ssh", required_argument, 0, RPKI_OPTION_SSH},
   "<user,hostkey,private key>",
   "enable SSH encryption for the live connection to the RTR server"},
#endif
  {{"help", no_argument, 0, 'h'}, "", "print this help menu"},
  {{0, 0, 0, 0}, "", "" }
};

#define OPTIONS_CNT (ARR_CNT(bs_opts) - 1)

static struct option long_options[OPTIONS_CNT + 1];
static char short_options[OPTIONS_CNT * 2 + 1];

static char buf[65536];

static bgpstream_t *bs;
static bgpstream_data_interface_id_t di_id_default = 0;
static bgpstream_data_interface_id_t di_id = 0;
static bgpstream_data_interface_info_t *di_info = NULL;

#define longopt_width  (16)
#define opt_width      (5 + longopt_width)
#define optarg_col     (opt_width + 2)
#define optarg_width   (15)
#define expl_col       (optarg_col + optarg_width + 2)

static int columns(int fd)
{
  static int c = -1;
#ifdef TIOCGWINSZ
  if (c < 0) {
    struct winsize w;
    if (ioctl(fd, TIOCGWINSZ, &w) == 0)
      c = w.ws_col;
  }
#endif
  return c > 0 ? c : 80;
}

// Print a string to stderr with wrapping.
// str       string to print
// startcol  assume cursor is initially at this column
// indent    amount to indent wrapped lines
// returns   final cursor column
static int wrap(const char *str, int startcol, int indent)
{
  int cols = columns(STDERR_FILENO);
  if (cols < startcol) cols = 80;
  if (cols < startcol) cols = INT_MAX;
  while (strlen(str) > cols - startcol) {
    const char *p = str + (cols - startcol);
    while (!isspace(*p) && p > str) p--; // find last space in line
    if (!isspace(*p)) { // couldn't find a space
      p = str + (cols - startcol); // wrap at last possible char
    }
    fprintf(stderr, "%-.*s\n%*s", (int)(p - str), str, indent, "");
    for (str = p; isspace(*str); str++); // skip leading spaces on next line
    startcol = indent;
  }
  return startcol + fprintf(stderr, "%s", str);
}

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
      fprintf(stderr, "%-*s%-*s  ", optarg_col, "", optarg_width, info->name);
      int col = wrap(info->description, expl_col, expl_col);
      if (ids[i] == di_id_default)
        wrap(" (default)", col, expl_col);
      fprintf(stderr, "\n");
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
      fprintf(stderr, "   %-*s", 15, options[i].name);
      wrap(options[i].description, 18, 18);
      fprintf(stderr, "\n");
    }
  }
  fprintf(stderr, "\n");
}

static void usage()
{
  fprintf(stderr, "Usage: bgpreader [<options>]\n"
                  "Available options are:\n");
  for (unsigned k = 0; k < OPTIONS_CNT; k++) {
    // short option
    if (isgraph(bs_opts[k].option.val)) {
      fprintf(stderr, " -%c, ", bs_opts[k].option.val);
    } else {
      fprintf(stderr, "     ");
    }

    // long option
    if (fprintf(stderr, "--%-*s  ", longopt_width - 2, bs_opts[k].option.name) > longopt_width + 2)
      fprintf(stderr, "\n%*s", optarg_col, "");

    // optarg
    if (fprintf(stderr, "%-*s  ", optarg_width, bs_opts[k].usage) > optarg_width + 2)
      fprintf(stderr, "\n%*s", expl_col, "");

    // explanatory text
    wrap(bs_opts[k].expl, expl_col, expl_col);
    fprintf(stderr, "\n");
    if (bs_opts[k].option.val == 'd') {
      data_if_usage();
    }
  }
  fprintf(stderr, "* denotes an option that can be given multiple times\n");
}

// print / utility functions

static int print_record(bgpstream_record_t *record);
static int print_elem(bgpstream_record_t *record, bgpstream_elem_t *elem);
static int print_elem_bgpdump(bgpstream_record_t *record,
                              bgpstream_elem_t *elem);

int main(int argc, char *argv[])
{

  int opt;
  int prevoptind;
  int error_cnt = 0; // for errors that don't prevent additional option parsing

  // variables associated with options
  const char *interface_options[1024];
  int interface_options_cnt = 0;

  char *endp;

#ifdef WITH_RPKI
  bgpstream_rpki_input_t *rpki_input = bgpstream_rpki_create_input();
#endif

  char *filterstring = NULL;
  char *intervalstring = NULL;
  uint32_t interval_start = 0;
  uint32_t interval_end = BGPSTREAM_FOREVER;
  int rib_period = 0;
  int live = 0;
  int output_info = 0;
  int record_output_on = 0;
  int record_bgpdump_output_on = 0;
  int elem_output_on = 0;
  int exitstatus = -1; // fail, until proven otherwise

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

  /* build the short and long options */
  unsigned k;
  size_t short_len = 0;
  for (k = 0; k < OPTIONS_CNT; k++) {
    if (isgraph(bs_opts[k].option.val)) {
      short_options[short_len++] = (char)bs_opts[k].option.val;
      if (bs_opts[k].option.has_arg)
        short_options[short_len++] = ':';
    }
    long_options[k] = bs_opts[k].option;
  }
  short_options[short_len++] = '\0';
  long_options[k] = bs_opts[k].option;

  opterr = 1;
  while (prevoptind = optind,
         (opt = getopt_long(argc, argv, short_options, long_options, 0)) >= 0) {
    if (optind == prevoptind + 2 && (optarg && *optarg == '-')) {
      for (k = 0; long_options[k].name; k++) {
        if (long_options[k].val == opt) break;
      }
      if (long_options[k].name) {
        fprintf(stderr, "ERROR: spaced argument for ");
        if (isgraph(opt)) fprintf(stderr, "-%c/", opt);
        fprintf(stderr, "--%s looks like an option (use ",
            long_options[k].name);
        if (isgraph(opt)) fprintf(stderr, "-%c'%s' or ", opt, optarg);
        fprintf(stderr, "--%s='%s' to force the argument)\n",
            long_options[k].name, optarg);
      }
      goto done;
    }
    switch (opt) {

#define PARSE_FILTER_OPTION(optletter, filter_type)           \
    case (optletter):                                         \
      if (!bgpstream_add_filter(bs, (filter_type), optarg))   \
        error_cnt++;

    // Filter options that don't depend on di_info can be parsed immediately
    PARSE_FILTER_OPTION('p', BGPSTREAM_FILTER_TYPE_PROJECT)
      break;
    PARSE_FILTER_OPTION('c', BGPSTREAM_FILTER_TYPE_COLLECTOR)
      break;
    PARSE_FILTER_OPTION('R', BGPSTREAM_FILTER_TYPE_ROUTER)
      break;
    PARSE_FILTER_OPTION('j', BGPSTREAM_FILTER_TYPE_ELEM_PEER_ASN)
      break;
    PARSE_FILTER_OPTION('a', BGPSTREAM_FILTER_TYPE_ELEM_ORIGIN_ASN)
      break;
    PARSE_FILTER_OPTION('k', BGPSTREAM_FILTER_TYPE_ELEM_PREFIX)
      break;
    PARSE_FILTER_OPTION('y', BGPSTREAM_FILTER_TYPE_ELEM_COMMUNITY)
      break;
    PARSE_FILTER_OPTION('A', BGPSTREAM_FILTER_TYPE_ELEM_ASPATH)
      break;
    PARSE_FILTER_OPTION('t', BGPSTREAM_FILTER_TYPE_RECORD_TYPE)
      break;
    PARSE_FILTER_OPTION('T', BGPSTREAM_FILTER_TYPE_RESOURCE_TYPE)
      break;

    case 'o':
      if (interface_options_cnt == ARR_CNT(interface_options)) {
        fprintf(stderr,
                "ERROR: A maximum of %lu interface_options (-o) can be "
                "specified on the command line\n",
                ARR_CNT(interface_options));
        goto done;
      }
      interface_options[interface_options_cnt++] = optarg;
      break;

    case 'w':
    {
      char *end;
      end = bgpstream_parse_time(optarg, &interval_start);
      char *label = "start";
      if (end) {
        if (*end == '\0') {
          interval_end = BGPSTREAM_FOREVER;
        } else if (*end == ',') {
          end = bgpstream_parse_time(end+1, &interval_end);
          label = "end";
        }
      }
      if (end == NULL || *end != '\0') {
        fprintf(stderr, "ERROR: bad %s time in '%s'\n", label, optarg);
        goto done;
      }
      break;
    }
    case 'P':
      rib_period = atoi(optarg);
      break;
    case 'd':
      if ((di_id = bgpstream_get_data_interface_id_by_name(bs, optarg)) == 0) {
        fprintf(stderr, "ERROR: Invalid data interface name '%s'\n", optarg);
        usage();
        goto done;
      }
      di_info = bgpstream_get_data_interface_info(bs, di_id);
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
    case 'v':
      fprintf(stderr, "bgpreader version %d.%d.%d\n", BGPSTREAM_MAJOR_VERSION,
              BGPSTREAM_MID_VERSION, BGPSTREAM_MINOR_VERSION);
      exitstatus = 0; // success
      goto done;
#ifdef WITH_RPKI
    case RPKI_OPTION_SSH:
      bgpstream_rpki_parse_ssh(optarg, rpki_input);
      break;
    case RPKI_OPTION_COLLECTORS:
      bgpstream_rpki_parse_collectors(optarg, rpki_input);
      break;
    case RPKI_OPTION_LIVE:
      bgpstream_rpki_parse_live(rpki_input);
      break;
    case RPKI_OPTION_UNIFIED:
      bgpstream_rpki_parse_unified(rpki_input);
      break;
    case RPKI_OPTION_DEFAULT:
      bgpstream_rpki_parse_default(rpki_input);
      break;
#endif
    case 'h':
      usage();
      exitstatus = 0; // success
      goto done;
    default:
      usage();
      goto done;
    }
  }

  // note: di_info must be initialized before processing interface_options
  for (i = 0; i < interface_options_cnt; i++) {
    if (strcmp(interface_options[i], "?") == 0) {
      dump_if_options();
      exitstatus = 0; // success
      goto done;
    } else {
      /* actually set this option */
      if ((endp = strchr(interface_options[i], '=')) == NULL) {
        fprintf(stderr, "ERROR: Malformed data interface option (%s)\n",
                interface_options[i]);
        fprintf(stderr, "ERROR: Expecting <option-name>=<option-value>\n");
        error_cnt++;
      } else {
        *endp = '\0';
        endp++;
        if ((option = bgpstream_get_data_interface_option_by_name(
               bs, di_id, interface_options[i])) == NULL) {
          fprintf(stderr, "ERROR: Invalid option '%s' for data interface '%s'\n",
                  interface_options[i], di_info->name);
          dump_if_options();
          error_cnt++;
        } else if (bgpstream_set_data_interface_option(bs, option, endp) != 0) {
          fprintf(stderr,
                  "ERROR: Failed to set option '%s' for data interface '%s'\n",
                  interface_options[i], di_info->name);
          error_cnt++;
        }
      }
    }
  }

  // Cannot output in both bgpstream elem and bgpdump format
  if (elem_output_on && record_bgpdump_output_on) {
    fprintf(stderr, "ERROR: Cannot output in both bgpstream elem (-e) and "
                    "bgpdump format (-m).\n");
    error_cnt++;
  }

  // if the user did not specify any output format, default to per elem
  if (!record_output_on && !elem_output_on && !record_bgpdump_output_on) {
    elem_output_on = 1;
  }

  // Parse the filter string
  if (filterstring) {
    if (!bgpstream_parse_filter_string(bs, filterstring)) {
      error_cnt++;
    }
  }

  if (intervalstring) {
    if (!bgpstream_add_recent_interval_filter(bs, intervalstring, live))
      error_cnt++;
  }

  // windows
  if (interval_start != 0) {
    if (!bgpstream_add_interval_filter(bs, interval_start, interval_end))
      error_cnt++;
  }

  /* frequencies */
  if (rib_period > 0) {
    if (!bgpstream_add_rib_period_filter(bs, rib_period))
      error_cnt++;
  }

  if (error_cnt > 0)
    goto done;

  // if the user didn't specify any arguments, or gave extra args,
  // then give them the help output
  if (argc == 1 || argc != optind) {
    usage();
    goto done;
  }

  if (interval_start == 0 && !intervalstring) {
    if (di_id == BGPSTREAM_DATA_INTERFACE_BROKER) {
      fprintf(stderr, "WARN: No time window specified, defaulting to live mode\n");
      interval_start = epoch_sec();
      if (!bgpstream_add_interval_filter(bs, interval_start, interval_end)){
        fprintf(stderr, "ERROR: Could not set interval between %d and %d\n",
                interval_start, interval_end);
        goto done;
      }
    } else {
      fprintf(stderr, "WARN: No time window specified, defaulting to all "
                      "available data\n");
    }
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

#ifdef WITH_RPKI
  rpki_cfg_t *cfg = NULL;
  if (rpki_input != NULL && rpki_input->rpki_active) {
    if (!bgpstream_rpki_parse_interval(rpki_input, interval_start, interval_end)) {
      fprintf(stderr, "ERROR: Could not parse time window for RPKI\n");
      goto done;
    }
    cfg = bgpstream_rpki_set_cfg(rpki_input);
  }
#endif

  while ((rec_limit < 0 || rec_cnt < rec_limit) &&
         (rrc = bgpstream_get_next_record(bs, &bs_record)) > 0) {
    rec_cnt++;

    if (bs_record->status != BGPSTREAM_RECORD_STATUS_VALID_RECORD) {
      continue;
    }

    if (record_output_on && print_record(bs_record) != 0) {
      goto done;
    }

    /* check if the record is of type RIB, in case extract the ID */
    /* print the RIB start line */
    if (bs_record->type == BGPSTREAM_RIB &&
        bs_record->dump_pos == BGPSTREAM_DUMP_START &&
        print_record(bs_record) != 0) {
      goto done;
    }

    if (record_bgpdump_output_on || elem_output_on) {
      while ((erc = bgpstream_record_get_next_elem(bs_record, &bs_elem)) > 0) {
#ifdef WITH_RPKI
        if (rpki_input != NULL && rpki_input->rpki_active) {
          bs_elem->annotations.cfg = cfg;
          bs_elem->annotations.rpki_active = rpki_input->rpki_active;
          bs_elem->annotations.timestamp = bs_record->time_sec;
        }
#endif
        // print record following bgpdump format
        if (record_bgpdump_output_on &&
            print_elem_bgpdump(bs_record, bs_elem) != 0) {
          goto done;
        } else if (elem_output_on && print_elem(bs_record, bs_elem) != 0) {
          goto done;
        }
      }

      if (erc != 0) {
        fprintf(stderr, "ERROR: Failed to get elem from record\n");
        goto done;
      }

      /* check if end of RIB has been reached */
      if (bs_record->type == BGPSTREAM_RIB &&
          bs_record->dump_pos == BGPSTREAM_DUMP_END &&
          print_record(bs_record) != 0) {
        goto done;
      }
    }
  }

  if (rrc < 0) {
    fprintf(stderr, "ERROR: Failed to get record from stream\n");
  } else {
    exitstatus = 0; // success
  }

done:
#ifdef WITH_RPKI
  if (rpki_input != NULL && rpki_input->rpki_active) {
    bgpstream_rpki_destroy_cfg(cfg);
    bgpstream_rpki_destroy_input(rpki_input);
  }
#endif

  /* deallocate memory for interface */
  bgpstream_destroy(bs);
  return exitstatus;
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

static int print_elem_bgpdump(bgpstream_record_t *record,
                              bgpstream_elem_t *elem)
{
  if (bgpstream_record_elem_bgpdump_snprintf(buf, sizeof(buf), record, elem) ==
      NULL) {
    fprintf(stderr, "ERROR: Could not convert record/elem to string\n");
    return -1;
  }

  printf("%s\n", buf);
  return 0;
}
