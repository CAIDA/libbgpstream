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

#ifndef __CORSARO_INT_H
#define __CORSARO_INT_H

#include "config.h"

#include "libtrace.h"

#ifdef WITH_PLUGIN_IPMETA
#include <libipmeta.h>
#endif

#include "corsaro.h"

#include "corsaro_file.h"
#include "corsaro_plugin.h"
#include "corsaro_tag.h"

/** @file
 *
 * @brief Header file dealing with the internal corsaro functions
 *
 * @author Alistair King
 *
 */

/* GCC optimizations */
/** @todo either make use of those that libtrace defines, or copy the way that
    libtrace does this*/
#if __GNUC__ >= 3
#  ifndef DEPRECATED
#    define DEPRECATED __attribute__((deprecated))
#  endif
#  ifndef SIMPLE_FUNCTION
#    define SIMPLE_FUNCTION __attribute__((pure))
#  endif
#  ifndef UNUSED
#    define UNUSED __attribute__((unused))
#  endif
#  ifndef PACKED
#    define PACKED __attribute__((packed))
#  endif
#  ifndef PRINTF
#    define PRINTF(formatpos,argpos) __attribute__((format(printf,formatpos,argpos)))
#  endif
#else
#  ifndef DEPRECATED
#    define DEPRECATED
#  endif
#  ifndef SIMPLE_FUNCTION
#    define SIMPLE_FUNCTION
#  endif
#  ifndef UNUSED
#    define UNUSED
#  endif
#  ifndef PACKED
#    define PACKED
#  endif
#  ifndef PRINTF
#    define PRINTF(formatpos,argpos)
#  endif
#endif

/**
 * @name Corsaro data structures
 *
 * These data structures are used when reading corsaro files with libcorsaro
 *
 * @{ */

/** Enum of overall corsaro magic numbers */
typedef enum corsaro_magic
  { 
    /** Overall corsaro magic number - "EDGR" */
    CORSARO_MAGIC          = 0x45444752,
    /** corsaro header magic number - "HEAD" */
    CORSARO_MAGIC_HEADER   = 0x48454144,
    /** corsaro interval magic number - "INTR" */
    CORSARO_MAGIC_INTERVAL = 0x494E5452,
    /* corsaro data block magic number - "DATA" */
    CORSARO_MAGIC_DATA     = 0x44415441,
    /* corsaro trailer magic number - "FOOT" */
    CORSARO_MAGIC_TRAILER  = 0x464F4F54
  } corsaro_magic_t;

/** Structure representing a corsaro file header
 * 
 * Values are all in HOST byte order
 */
struct corsaro_header
{
  /** The global corsaro magic number */
  uint32_t corsaro_magic;
  /** The header magic number */
  uint32_t magic;
  /** The corsaro major version number */
  uint8_t version_major;
  /** The corsaro minor version number */
  uint8_t version_minor;
  /** The local time that corsaro was started at */
  uint32_t local_init_time;
  /** The interval length (seconds) */
  uint16_t interval_length;
  /** The length of the (optional) trace uri string */
  uint16_t traceuri_len;
  /** A pointer to the traceuri string */
  uint8_t *traceuri;
  /** The number of plugins used */
  uint16_t plugin_cnt;
  /** A pointer to the list of plugin magic numbers used */
  uint32_t *plugin_magics;
} PACKED;

/** Structure representing a corsaro file trailer
 * 
 * Values are all in HOST byte order
 */
struct corsaro_trailer
{
  /** The global corsaro magic number */
  uint32_t corsaro_magic;
  /** The trailer magic number */
  uint32_t magic;
  /** The total number of packets that corsaro processed */
  uint64_t packet_cnt;
  /** The number of packets libtrace reports as accepted */
  uint64_t accepted_cnt;
  /** The number of packets libtrace reports as dropped */
  uint64_t dropped_cnt;
  /** The trace time of the first packet (seconds) */
  uint32_t first_packet_time;
  /** The trace time of the last packet (seconds) */
  uint32_t last_packet_time;
  /** The time that corsaro completed processing the trace */
  uint32_t local_final_time;
  /** The number of (wall) seconds that corsaro took to process the trace */
  uint32_t runtime;
} PACKED;

/** Structure representing the start or end of an interval 
 *
 * The start time represents the first second which this interval covers.
 * I.e. start.time <= pkt.time for all pkt in the interval
 * The end time represents the last second which this interval covers.
 * I.e. end.time >= pkt.time for all pkt in the interval
 *
 * If looking at the start and end interval records for a given interval,
 * the interval duration will be: 
 * @code end.time - start.time + 1 @endcode
 * The +1 includes the final second in the time.
 *
 * If corsaro is shutdown at any time other than an interval boundary, the
 * end.time value will be the seconds component of the arrival time of the
 * last observed packet.
 * 
 * Values are all in HOST byte order
 */
struct corsaro_interval
{
  /** The global corsaro magic number */
  uint32_t corsaro_magic;
  /** The interval magic number */
  uint32_t magic;
  /** The interval number (starts at 0) */
  uint16_t         number;
  /** The time this interval started/ended */
  uint32_t         time;
} PACKED;

/** Structure representing the start or end of a plugin data block
 *
 * Values are all in HOST byte order
 */
struct corsaro_plugin_data
{
  /** The global corsaro magic number */
  uint32_t corsaro_magic;
  /** The plugin data magic number */
  uint32_t magic;
  /** The plugin magic */
  uint32_t plugin_magic;
} PACKED;

/** @} */

/** The interval after which we will end an interval */
#define CORSARO_INTERVAL_DEFAULT 60

/** Corsaro state for a packet
 *
 * This is passed, along with the packet, to each plugin.
 * Plugins can add data to it, or check for data from earlier plugins.
 */
struct corsaro_packet_state
{
  /** Features of the packet that have been identified by earlier plugins */
  uint8_t flags;

  /** Tag state */
  corsaro_tag_state_t tags;

#ifdef WITH_PLUGIN_IPMETA
  /** Set of libipmeta records based on lookups performed by the corsaro_ipmeta
      plugin. Other plugins should use the corsaro_ipmeta_get_record() function
      to retrieve records from this array */
  /** @todo consider making this bi-directional, one for src, one for dst */
  ipmeta_record_t *ipmeta_records[IPMETA_PROVIDER_MAX];

  /** Record that corresponds to the default ipmeta provider */
  ipmeta_record_t *ipmeta_record_default;
#endif
};

/** The possible packet state flags */
enum
  {
    /** The packet is classified as backscatter */
    CORSARO_PACKET_STATE_FLAG_BACKSCATTER    = 0x01,

    /** The packet should be ignored by filter-aware plugins */
    CORSARO_PACKET_STATE_FLAG_IGNORE         = 0x02,
  };

/** A lightweight wrapper around a libtrace packet */
struct corsaro_packet
{
  /** The corsaro state associated with this packet */
  corsaro_packet_state_t  state;

  /** A pointer to the underlying libtrace packet */
  libtrace_packet_t    *ltpacket;
};

/** Convenience macro to get to the libtrace packet inside an corsaro packet */
#define LT_PKT(corsaro_packet)   (corsaro_packet->ltpacket) 

/** Corsaro output state */
struct corsaro
{
  /** The local wall time that corsaro was started at */
  struct timeval init_time;

  /** The libtrace trace pointer for the trace that we are being fed */
  libtrace_t *trace;

  /** The uri that was used to open the trace file */
  char *uridata;

  /** The name of the monitor that corsaro is running on */
  char *monitorname;

  /** The template used to create corsaro output files */
  char *template;

  /** The default output mode for new files */
  corsaro_file_mode_t output_mode;

  /** The compression type (based on the file name) */
  corsaro_file_compress_t compress;

  /** The compression level (ignored if not compressing) */
  int compress_level;

  /** The corsaro output file to write global output to */
  corsaro_file_t *global_file;

  /** Has the user asked us not to create a global output file? */
  int global_file_disabled;

  /** The file to write log output to */
  corsaro_file_t *logfile;

  /** Has the user asked us not to log to a file? */
  int logfile_disabled;

  /** A pointer to the wrapper packet passed to the plugins */
  corsaro_packet_t *packet;

  /** A pointer to the corsaro plugin manager state */
  /* this is what gets passed to any function relating to plugin management */
  corsaro_plugin_manager_t *plugin_manager;

  /** A pointer to the corsaro packet tag manager state */
  corsaro_tag_manager_t *tag_manager;

  /** The first interval end will be rounded down to the nearest integer
      multiple of the interval length if enabled */
  corsaro_interval_align_t interval_align;

  /** The number of seconds after which plugins will be asked to dump data */
  int interval;

  /** The output files will be rotated after n intervals if >0 */
  int output_rotate;

  /** The meta output files will be rotated after n intervals if >=0
   * a value of 0 indicates no rotation, <0 indicates the output_rotate
   * value should be used
   */
  int meta_output_rotate;

  /** State for the current interval */
  corsaro_interval_t interval_start;

  /** The time that this interval will be dumped at */
  uint32_t next_report;

  /** The time of the the first packet seen by corsaro */
  struct timeval first_ts;

  /** The time of the most recent packet seen by corsaro */
  struct timeval last_ts;

  /** Whether there are un-dumped packets in the current interval */
  int interval_end_needed;

  /** The total number of packets that have been processed */
  uint64_t packet_cnt;

  /** The total number of packets that have been accepted by libtrace 
      (before the current interval) */
  uint64_t accepted_pkts;

  /** The total number of packets that have been dropped by libtrace
      (before the current interval) **/
  uint64_t dropped_pkts;

  /** Has this corsaro object been started yet? */
  int started;

};

/** Corsaro input state */
struct corsaro_in
{
  /** The uri of the file to read data from */
  char *uridata;

  /** The corsaro input file to read data from */
  corsaro_file_in_t *file;

  /** The next expected record type when reading the corsaro_global file */
  corsaro_in_record_type_t expected_type;

  /** A pointer to the corsaro plugin manager state */
  /* this is what gets passed to any function relating to plugin management */
  corsaro_plugin_manager_t *plugin_manager;

  /** Pointer to the plugin to be used to read this file. NULL if global */
  corsaro_plugin_t *plugin;

  /** Has this corsaro_in object been started yet? */
  int started;

};

/** The initial buffer size in the record object */
#define CORSARO_IN_RECORD_DEFAULT_BUFFER_LEN     LIBTRACE_PACKET_BUFSIZE+1024

/** A reusable opaque structure for corsaro to read an input record into */
struct corsaro_in_record
{
  /** The corsaro input object the record is associated with */
  corsaro_in_t *corsaro;

  /** The buffer to read the record into */
  uint8_t *buffer;

  /** The length of the buffer */
  size_t buffer_len;

  /** The type of the record currently in the buffer */
  corsaro_in_record_type_t type;

};

#ifdef WITH_PLUGIN_TIMING
/* Helper macros for doing timing */

/** Start a timer with the given name */
#define TIMER_START(timer)			\
  struct timeval timer_start;			\
  do {						\
  gettimeofday_wrap(&timer_start);		\
  } while(0)

#define TIMER_END(timer)					\
  struct timeval timer_end, timer_diff;				\
  do {								\
    gettimeofday_wrap(&timer_end);				\
    timeval_subtract(&timer_diff, &timer_end, &timer_start);	\
  } while(0)

#define TIMER_VAL(timer)			\
  ((timer_diff.tv_sec*1000000) + timer_diff.tv_usec)
#endif

#endif /* __CORSARO_INT_H */
