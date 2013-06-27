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

#ifndef __CORSARO_H
#define __CORSARO_H

#include "config.h"

#include "libtrace.h"
#include "wandio.h"

/** @file
 *
 * @brief Header file which exports the public libcorsaro API
 *
 * @author Alistair King
 *
 */

/**
 * @name Opaque Data Structures
 *
 * @{ */

/** Opaque struct holding corsaro output state */
typedef struct corsaro corsaro_t;
/** Opaque struct holding corsaro input state */
typedef struct corsaro_in corsaro_in_t;
/** Opaque corsaro packet wrapper */
typedef struct corsaro_packet corsaro_packet_t;
/** Struct holding corsaro packet state */
typedef struct corsaro_packet_state corsaro_packet_state_t;
/** Opaque struct holding a corsaro record read from file */
typedef struct corsaro_in_record corsaro_in_record_t;
/** Opaque struct representing a corsaro file header */
typedef struct corsaro_header corsaro_header_t;
/** Opaque struct representing a corsaro file trailer */
typedef struct corsaro_trailer corsaro_trailer_t;
/** Opaque struct representing the start or end of an interval */
typedef struct corsaro_interval corsaro_interval_t;
/** Opaque struct representing the start of end of a plugin data block */
typedef struct corsaro_plugin_data corsaro_plugin_data_t;

#ifdef WITH_PLUGIN_SIXT
/** Structure representing a flowtuple record (see corsaro_flowtuple.h) */
typedef struct corsaro_flowtuple corsaro_flowtuple_t;
/** Structure representing a flowtuple class start record (see
    corsaro_flowtuple.h) */
typedef struct corsaro_flowtuple_class_start corsaro_flowtuple_class_start_t;
/** Structure representing a flowtuple class end record (see
    corsaro_flowtuple.h) */
typedef struct corsaro_flowtuple_class_end corsaro_flowtuple_class_end_t;
#endif

/** @} */

/**
 * @name Enumerations
 *
 * @{ */

/** Corsaro input record types 
 *
 * Use these types to request a specific record, or to cast a returned 
 * record, from corsaro_in_read_record
 *
 * You should be able to cast this by removing the 'type' and adding '_t'.
 * For example, CORSARO_IN_RECORD_TYPE_IO_HEADER becomes 
 * corsaro_in_record_io_header_t
 *
 * Additionally, the field immediately following 'TYPE' indicates the module
 * which is responsible for reading and writing these records.
 * It is probably safe to look in corsaro_<module>.[ch] to find them.
 */
typedef enum corsaro_in_record_type
  {
    /** The null type used for wildcard matching */
    CORSARO_IN_RECORD_TYPE_NULL                  = 0,

    /** Internal type for directing read requests */
    CORSARO_IN_RECORD_TYPE_INTERNAL_REDIRECT    = 1,

    /** The overall corsaro header (currently only in global) */
    CORSARO_IN_RECORD_TYPE_IO_HEADER             = 2,

    /** The overall corsaro trailer (currently only in global) */
    CORSARO_IN_RECORD_TYPE_IO_TRAILER            = 3,
    
    /** The start of an interval */
    CORSARO_IN_RECORD_TYPE_IO_INTERVAL_START     = 4,

    /** The end of an interval */
    CORSARO_IN_RECORD_TYPE_IO_INTERVAL_END       = 5,

    /** The start of a plugin data section */
    CORSARO_IN_RECORD_TYPE_IO_PLUGIN_START       = 6,
    
    /** The end of a plugin data section */
    CORSARO_IN_RECORD_TYPE_IO_PLUGIN_END         = 7,

    /* plugin specific records */

    /* corsaro_flowtuple has 20-29 */

    /** The corsaro_flowtuple flowtuple classification type start record */
    CORSARO_IN_RECORD_TYPE_FLOWTUPLE_CLASS_START  = 20,

    /** The corsaro_flowtuple flowtuple classification type end record */
    CORSARO_IN_RECORD_TYPE_FLOWTUPLE_CLASS_END    = 21,

    /** The corsaro_flowtuple flowtuple record */
    CORSARO_IN_RECORD_TYPE_FLOWTUPLE_FLOWTUPLE     = 22,

    /* corsaro_dos has 30-39 */
    
    /** The corsaro_dos global header record */
    CORSARO_IN_RECORD_TYPE_DOS_GLOBAL_HEADER     = 30,

    /** The corsaro_dos header record */
    CORSARO_IN_RECORD_TYPE_DOS_HEADER            = 31,

    /** The corsaro_dos attack vector record */
    CORSARO_IN_RECORD_TYPE_DOS_ATTACK_VECTOR     = 32,

  } corsaro_in_record_type_t;

/** Enum of supported file modes */
typedef enum corsaro_file_mode
  {
    /** ASCII IO mode */
    CORSARO_FILE_MODE_ASCII  = 0,
    /** Binary IO mode */
    CORSARO_FILE_MODE_BINARY = 1,
    /** Pseudo IO mode which allows trace files to be written */
    CORSARO_FILE_MODE_TRACE  = 2,
    /** Unknown IO mode */
    CORSARO_FILE_MODE_UNKNOWN = 3,

    /** Default IO mode */
    CORSARO_FILE_MODE_DEFAULT = CORSARO_FILE_MODE_UNKNOWN
  } corsaro_file_mode_t;

/** Supported compression types (must be kept in sync with wandio) */
typedef enum corsaro_file_compress
  {
    /** No compression */
    CORSARO_FILE_COMPRESS_NONE = WANDIO_COMPRESS_NONE,
    /** Zlib compression (gzip) */
    CORSARO_FILE_COMPRESS_ZLIB = WANDIO_COMPRESS_ZLIB,
    /** Bzip compression */
    CORSARO_FILE_COMPRESS_BZ2  = WANDIO_COMPRESS_BZ2,
    /** LZO Compression */
    CORSARO_FILE_COMPRESS_LZO  = WANDIO_COMPRESS_LZO,

    /** Default compression */
    CORSARO_FILE_COMPRESS_DEFAULT = CORSARO_FILE_COMPRESS_ZLIB
  } corsaro_file_compress_t;

/** Settings for interval alignment */
typedef enum corsaro_interval_align
  {
    CORSARO_INTERVAL_ALIGN_NO      = 0,
    CORSARO_INTERVAL_ALIGN_YES     = 1,
    CORSARO_INTERVAL_ALIGN_DEFAULT = CORSARO_INTERVAL_ALIGN_NO,
  } corsaro_interval_align_t;

/** @} */

/**
 * @name Corsaro output API functions
 *
 * These functions are used to generate corsaro output from libtrace packets
 * 
 * The basic process for using corsaro to generate output is:
 * -# init corsaro using corsaro_alloc_output
 * -# optionally call corsaro_set_interval to set the interval time
 * -# optionally call corsaro_set_traceuri to set the name of the trace
 * -# call corsaro_start_output to initialize the plugins (and create the files)
 * -# call corsaro_per_packet with each packet to be processed
 * -# call corsaro_finalize when all packets have been processed
 *
 * If an API function returns an error condition, it is your responsibility
 * to call corsaro_finalize to clean up any resources corsaro is using.
 * This is so you can decide if halting execution is really what you want to
 * do. For example, if a packet fails to process, you may decide to log it
 * and attempt to continue with the next packet. Beware that this could
 * get corsaro into an unstable state if the error arose from something like
 * malloc failing.
 *
 * @{ */

/** Allocate an corsaro object
 *
 * @param template     The string used to generate output files
 * @param mode         The file output mode
 * @return a pointer to an opaque corsaro structure, or NULL if an error occurs
 *
 * The template must contain a pattern to be replaced with the plugin
 * names (%P). The output modes that make sense to use are 
 * CORSARO_FILE_MODE_ASCII and CORSARO_FILE_MODE_BINARY. Using 
 * CORSARO_FILE_MODE_TRACE will result in an error as not all plugins are 
 * expected to be able to write to generic packets
 * 
 * The returned object can then be used to set options (corsaro_set_*) before
 * calling corsaro_start_output to write headers to the output files ready
 * to process packets.
 */
corsaro_t *corsaro_alloc_output(char *template, corsaro_file_mode_t mode);

/** Initialize an corsaro object that has already been allocated
 *
 * @param corsaro       The corsaro object to start
 * @return 0 if corsaro is started successfully, -1 if an error occurs
 *
 * It is only when this function is called that the plugins will parse their
 * arguments and initialize any state (open files etc).
 *
 * @warning Plugins may use getopt to parse their arguments, please be sure
 * that you are not using the getopt global variables (optarg, optind etc) when
 * calling this function.
 */
int corsaro_start_output(corsaro_t *corsaro);

/** Accessor function to enable/disable the alignment of the initial interval
 *
 * @param corsaro         The corsaro object to set the interval for
 * @param interval_align  Enable or disable the alignment of interval end times
 *
 * The end time of the first interval will be rounded down to the nearest 
 * integer multiple of the interval length. Interval rounding makes the most 
 * sense when the interval length is evenly divisible into 1 hour.
 * The default is no interval alignment.
 */
void corsaro_set_interval_alignment(corsaro_t *corsaro, 
				    corsaro_interval_align_t interval_align);

/** Accessor function to set the interval length
 *
 * @param corsaro      The corsaro object to set the interval for
 * @param interval     The interval (in seconds)
 *
 * If this function is not called, the default interval, CORSARO_INTERVAL_DEFAULT,
 * will be used.
 */
void corsaro_set_interval(corsaro_t *corsaro, unsigned int interval);

/** Accessor function to set the rotation frequency of output files
 *
 * @param corsaro      The corsaro object to set the interval for
 * @param intervals    The number of intervals after which the output files
 *                     will be rotated
 *
 * If this is set to > 0, all output files will be rotated at the end of
 * n intervals. The default is 0 (no rotation).
 */
void corsaro_set_output_rotation(corsaro_t *corsaro, 
				 int intervals);

/** Accessor function to set the rotation frequency of meta output files
 *
 * @param corsaro      The corsaro object to set the interval for
 * @param intervals    The number of intervals after which the output files
 *                     will be rotated
 *
 * If this is set to > 0, corsaro meta output files (global and log) will be
 * rotated at the end of n intervals. The default is to follow the output 
 * rotation interval specified by corsaro_set_output_rotation. 
 */
void corsaro_set_meta_output_rotation(corsaro_t *corsaro, 
				      int intervals);

/** Convenience function to determine if the output files should be rotated
 *
 * @param corsaro     The corsaro object to check the rotation status of
 * @return 1 if output files should be rotated at the end of the current
 *         interval, 0 if not
 */
int corsaro_is_rotate_interval(corsaro_t *corsaro);
			       

/** Accessor function to set the trace pointer
 *
 * @param corsaro      The corsaro object to set the trace uri for
 * @param trace        A libtrace trace pointer for the current trace
 * @return 0 if the uri was successfully set, -1 if an error occurs
 *
 * The trace pointer is used by corsaro to report trace statistics such as
 * dropped and accepted packet counts. This is not required.
 */
int corsaro_set_trace(corsaro_t *corsaro, libtrace_t *trace);

/** Accessor function to set the trace uri string
 *
 * @param corsaro        The corsaro object to set the trace uri for
 * @param traceuri     The string to set as the trace uri
 * @return 0 if the uri was successfully set, -1 if an error occurs
 *
 * The trace uri is *not* used internally by corsaro, this can be any
 * user-defined string which is stored in the corsaro header in output files.
 * If it is not set, no uri is written to the output.
 */
int corsaro_set_traceuri(corsaro_t *corsaro, char *traceuri);

/** Attempt to enable a plugin using the given plugin name
 *
 * @param corsaro        The corsaro object to enable the plugin for
 * @param plugin_name    The string name of the plugin to enable
 * @param plugin_args    The string of arguments to pass to the plugin
 * @return 0 if the plugin was successfully enabled, -1 if an error occurs
 *
 * Until this function is called successfully, all compiled plugins are
 * considered enabled. Once it has been called, only the plugins explicitly
 * enabled using this function will be used
 */
int corsaro_enable_plugin(corsaro_t *corsaro, const char *plugin_name,
			  const char *plugin_args);

/** Return an array of the names of plugins which are compiled into corsaro
 *
 * @param[out] plugin_names   A pointer to an array of plugin names
 * @return the number of strings in the array, -1 if an error occurs
 *
 * Note that corsaro_free_plugin_names must be called to free the returned array
 */
int corsaro_get_plugin_names(char ***plugin_names);

/** Free the array of plugin names returned by corsaro_get_plugin_names
 *
 * @param plugin_names  The array of plugin names
 * @param plugin_cnt    The number of names in the array
 */
void corsaro_free_plugin_names(char **plugin_names, int plugin_cnt);

/** Accessor function to get the number of accepted packets in this interval
 * 
 * @param corsaro       The corsaro object to retrieve the packet count for
 * @return the number of packets libtrace reports as accepted in the current
 *         interval, or UINT64_MAX if this value is unavailable.
 *
 * This function requires that a pointer to the trace has been provided to
 * corsaro by way of the corsaro_set_trace function.
 */
uint64_t corsaro_get_accepted_packets(corsaro_t *corsaro);

/** Accessor function to get the number of dropped packets in this interval
 * 
 * @param corsaro       The corsaro object to retrieve the packet count for
 * @return the number of packets libtrace reports as dropped in the current
 *         interval, or UINT64_MAX if this value is unavailable.
 *
 * This function requires that a pointer to the trace has been provided to
 * corsaro by way of the corsaro_set_trace function.
 */
uint64_t corsaro_get_dropped_packets(corsaro_t *corsaro);

/** Accessor function to get the trace uri string
 *
 * @param corsaro        The corsaro object to set the trace uri for
 * @return a pointer to the traceuri string, or NULL if it is not set
 *
 */
const char *corsaro_get_traceuri(corsaro_t *corsaro);

/** Accessor function to set the monitor name
 *
 * @param corsaro        The corsaro object to set the monitor name for
 * @param name           The string to set as the monitor name
 * @return 0 if the name was successfully set, -1 if an error occurs
 *
 * If it is not set, the value defined at compile time is used. This
 * is either the hostname of the machine it was compiled on, or a value
 * passed to configure using --with-monitorname
 */
int corsaro_set_monitorname(corsaro_t *corsaro, char *name);

/** Accessor function to get the monitor name string
 *
 * @param corsaro        The corsaro object to set the monitor name for
 * @return a pointer to the monitor name string
 *
 */
const char *corsaro_get_monitorname(corsaro_t *corsaro);

/** Perform corsaro processing on a given libtrace packet
 *
 * @param corsaro        The corsaro object used to process the packet
 * @param packet       The libtrace packet to process
 * @return 0 if the packet is successfully processed, -1 if an error occurs
 *
 * For each packet, corsaro will determine whether it falls within the current
 * interval, if not, it will write out data for the previous interval.
 * The packet is then handed to each plugin which processes it and updates
 * internal state.
 */
int corsaro_per_packet(corsaro_t *corsaro, libtrace_packet_t *packet);

/** Perform corsaro processing on a given corsaro record
 *
 * @param corsaro      The corsaro object used to process the packet
 * @param type         The type of the record
 * @param record       The record to process
 * @return 0 if the record is successfully processed, -1 if an error occurs
 *
 * For each record, corsaro will simply hand it to each plugin which can process
 * it and updates internal state.
 */
int corsaro_per_record(corsaro_t *corsaro, 
		       corsaro_in_record_type_t type,
		       corsaro_in_record_t *record);

/** Write the final interval and free resources allocated by corsaro
 *
 * @param corsaro        The corsaro object to finalize
 * @return 0 if corsaro finished properly, -1 if an error occurs.
 */
int corsaro_finalize_output(corsaro_t *corsaro);

/** @} */

/**
 * @name Corsaro input API functions
 *
 * These functions are used to process exising corsaro files.
 *
 * Similarly to using corsaro for output, the process for opening an input file
 * is:
 * -# call corsaro_alloc_input
 * -# call corsaro_start_input
 * -# call corsaro_in_read_record until all records have been read
 * -# for each record returned, cast to the appropriate type and carry out
 *    any processing required
 * -# call corsaro_in_finalize when all records have been processed
 *
 * @{ */

/** Allocate an corsaro object for reading an corsaro file
 *
 * @param corsarouri          The corsaro file uri to open
 * @return a pointer to an corsaro input structure, or NULL if an error occurs
 */
/*
 * === This comment is commented out... ===
 * The file uri can optionally contain a prefix which tells corsaro the type
 * of the file (ascii or binary) and the plugin which created it.
 * For example, binary:corsaro_flowtuple:/path/to/file.gz indicates that the 
 * file is written in binary format, by the corsaro_flowtuple plugin.
 *
 * If no prefix is given, corsaro will attempt to guess the type and plugin.
 */
corsaro_in_t *corsaro_alloc_input(const char *corsarouri);

/** Initialize an corsaro input object that has already been allocated
 *
 * @param corsaro       The corsaro input object to start
 * @return 0 if corsaro is started successfully, -1 if an error occurs
 */
int corsaro_start_input(corsaro_in_t *corsaro);

/** Allocate a reusable corsaro record object
 *
 * @param corsaro          The corsaro input object to associate with the record
 * @return a pointer to the record object, NULL if an error occurs
 */
corsaro_in_record_t *corsaro_in_alloc_record(corsaro_in_t *corsaro);

/** Free an corsaro record object
 *
 * @param record         The record object to free 
 */
void corsaro_in_free_record(corsaro_in_record_t *record);

/** Read the next corsaro record from the given corsaro input file
 *
 * @param corsaro               The corsaro input object to read from
 * @param[in,out] record_type  The type of the record to read
 * @param record                The generic corsaro input record pointer
 * @return 0 on EOF, -1 on error, number of bytes read when successful
 */
off_t corsaro_in_read_record(corsaro_in_t *corsaro, 
			     corsaro_in_record_type_t *record_type,
			     corsaro_in_record_t *record);

/** Get a pointer data in a record
 *
 * @param record              The corsaro record object to retrieve data from
 * @return a void pointer to the data, NULL if an error occurred
 */
void *corsaro_in_get_record_data(corsaro_in_record_t *record);

/** Close the input file and free resources allocated by corsaro
 *
 * @param corsaro        The corsaro input object to finalize
 * @return 0 if corsaro finished properly, -1 if an error occurs.
 */
int corsaro_finalize_input(corsaro_in_t *corsaro);

/** @} */
  
#endif /* __CORSARO_H */
