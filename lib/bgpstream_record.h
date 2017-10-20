/*
 * This file is part of bgpstream
 *
 * CAIDA, UC San Diego
 * bgpstream-info@caida.org
 *
 * Copyright (C) 2012 The Regents of the University of California.
 * Authors: Alistair King, Chiara Orsini
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __BGPSTREAM_RECORD_H
#define __BGPSTREAM_RECORD_H

#include "bgpstream_elem.h"
#include "bgpstream_utils.h"

/** @file
 *
 * @brief Header file that exposes the public interface of a bgpstream record.
 *
 * @author Chiara Orsini
 *
 */

/**
 * @name Public Opaque Data Structures
 *
 * @{ */

/** Opaque structure used internally by BGPStream to store raw data obtained
    from the underlying data resource. */
typedef struct bgpstream_record_internal bgpstream_record_internal_t;

/** @} */

/**
 * @name Public Constants
 *
 * @{ */

/** @} */

/**
 * @name Public Enums
 *
 * @{ */

/** The type of the record */
typedef enum {

  /** The record contains data for a BGP Update message */
  BGPSTREAM_UPDATE = 0,

  /** The record contains data for a BGP RIB message */
  BGPSTREAM_RIB = 1,

  /** INTERNAL: The number of record types in use */
  _BGPSTREAM_RECORD_TYPE_CNT = 2,

} bgpstream_record_type_t;

/** The position of this record in the dump */
typedef enum {

  /** This is the first record of the dump */
  BGPSTREAM_DUMP_START = 0,

  /** This is a record in the middle of the dump. i.e. not the first or the last
      record of the dump */
  BGPSTREAM_DUMP_MIDDLE = 1,

  /** This is the last record of the dump */
  BGPSTREAM_DUMP_END = 2,

} bgpstream_dump_position_t;

/** Status of the record */
typedef enum {

  /** The record is valid and may be used */
  BGPSTREAM_RECORD_STATUS_VALID_RECORD = 0,

  /** Source is not empty, but no valid record was found */
  BGPSTREAM_RECORD_STATUS_FILTERED_SOURCE = 1,

  /** Source has no entries */
  BGPSTREAM_RECORD_STATUS_EMPTY_SOURCE = 2,

  /** Record read with timestamp "above" all interval filters. */
  BGPSTREAM_RECORD_STATUS_OUTSIDE_TIME_INTERVAL = 3,

  /* Error in opening or reading from dump */
  BGPSTREAM_RECORD_STATUS_CORRUPTED_SOURCE = 4,

  /* Dump corrupted at some point */
  BGPSTREAM_RECORD_STATUS_CORRUPTED_RECORD = 5,

} bgpstream_record_status_t;

/** @} */

/**
 * @name Public Data Structures
 *
 * @{ */

/** Record attributes */
typedef struct bgpstream_record_attributes {

  /** Project name
   *
   * When set, this is a BGPStream-specific string that identifies the
   * adminstrative organization that operates the BGP collector that collected
   * this information. That is, "projects" operate "collectors", which collect
   * data from "routers", who have "peers".
   */
  char project_name[BGPSTREAM_UTILS_STR_NAME_LEN];

  /** Collector name
   *
   * This is a BGPStream-specific string that identifies the "collector", within
   * the "project" that collected this BGP data. When using the broker data
   * interface, the collector name is provided by the broker, and usually maps
   * as closely as possible to the collector names used by the given
   * project. When processing data directly from an OpenBMP kafka stream, the
   * collector name is the "Admin ID" configured on the collector, which is set
   * by the collection operator (the "project"). When using other data
   * interfaces, this collector name is normally set manually.
   */
  char collector_name[BGPSTREAM_UTILS_STR_NAME_LEN];

  /** Router name
   *
   * This field is only used when processing data obtained from an OpenBMP kafka
   * stream (e.g., bmp.bgpstream.caida.org). It is a name set by the collection
   * "project", and is unique for a given collector. If unused or unknown, it
   * will be set to the empty string.
   */
  char router_name[BGPSTREAM_UTILS_STR_NAME_LEN];

  /** Router IP
   *
   * This is the IP address that the (BMP) router used when connecting to the
   * collector. If the version is set to zero, then it is not in use (as is the
   * case when not processing data from an OpenBMP kafka stream.
   *
   * It is possible (but unlikely) that the Router IP to be set, but the router
   * name is empty. In this case the router IP should be used to identify the
   * router.
   */
  bgpstream_addr_storage_t router_ip;

  /** Type
   *
   * Indicates if this record comes from a RIB dump, or from a BGP Update
   * message.
   */
  bgpstream_record_type_t type;

  /** Time that the BGP data was "aggregated". E.g. the start time of an MRT
      dump file. If the dump start time was unknown (e.g., the record came from
      a streaming source), this will be set to 0. */
  uint32_t dump_time_sec;

  /** Collection time (seconds component)
   *
   * The time the record was received/generated by the collector. For RIB
   * records, this is the time the RIB entry was dumped by the collector,
   * whereas for UPDATE records, this is the time the collector received the
   * message. It is possible for this field to be zero, e.g., when reading BMP
   * data directly from a raw BMP dump (i.e., not from an OpenBMP-based
   * collector) as BMP does not explicitly include collection time
   * information. This may cause problems with any time interval filters in
   * place.
   */
  uint32_t time_sec;

  /** Collection time (microseconds component)
   *
   * Care must be taken when using this field. First, if `time_sec` is zero,
   * then this field should be ignored. Second, if this field is zero, then it
   * should be assumed that the timestamp is accurate only to second
   * granularity. Third, even some collection formats provide microsecond
   * granularity timing information, most collectors are running on commodity
   * hardware with clocks that have unknown accuracy.
   */
  uint32_t time_usec;

} bgpstream_record_attributes_t;

/** Record structure */
typedef struct struct_bgpstream_record_t {

  /** Collection of attributes pertaining to this record */
  bgpstream_record_attributes_t attrs;

  /** Status of this record */
  bgpstream_record_status_t status;

  /** Position of this record in the dump */
  bgpstream_dump_position_t dump_pos;

  /** INTERNAL BGPStream State. Do not use. */
  bgpstream_record_internal_t *__int;

} bgpstream_record_t;

/** @} */

/**
 * @name Public API Functions
 *
 * @{ */

/** Retrieve the next elem from the record
 *
 * @param record        pointer to the BGP Stream Record to retrieve the elem
 *                      from

 * @param[out] elem     set to point to a borrowed elem structure, or NULL if
 *                      there are no more elems
 * @return 1 if a valid elem was returned, 0 if there are no more elems, -1 if
 * an error occurred
 *
 * The returned pointer is guaranteed to be valid until the record is re-used in
 * a subsequent call to bgpstream_get_next_record, or is destroyed with
 * bgpstream_record_destroy
 */
int bgpstream_record_get_next_elem(bgpstream_record_t *record,
                                   bgpstream_elem_t **elem);

/** Dump the given record to stdout in bgpdump format
 *
 * @param record        pointer to a BGP Stream Record instance to dump
 *
 * See https://bitbucket.org/ripencc/bgpdump for more information about bgpdump
 */
void bgpstream_record_print_mrt_data(bgpstream_record_t *record);

/** Write the string representation of the record type into the provided buffer
 *
 * @param buf           pointer to a char array
 * @param len           length of the char array
 * @param type          record type to convert to string
 * @return the number of characters that would have been written if len was
 * unlimited
 */
int bgpstream_record_type_snprintf(char *buf, size_t len,
                                   bgpstream_record_type_t type);

/** Write the string representation of the record dump position into the
 * provided
 *  buffer
 *
 * @param buf           pointer to a char array
 * @param len           length of the char array
 * @param dump_pos      BGP Stream Record dump position to convert to string
 * @return the number of characters that would have been written if len was
 * unlimited
 */
int bgpstream_record_dump_pos_snprintf(char *buf, size_t len,
                                       bgpstream_dump_position_t dump_pos);

/** Write the string representation of the record status into the provided
 *  buffer
 *
 * @param buf           pointer to a char array
 * @param len           length of the char array
 * @param status        BGP Stream Record status to convert to string
 * @return the number of characters that would have been written if len was
 * unlimited
 */
int bgpstream_record_status_snprintf(char *buf, size_t len,
                                     bgpstream_record_status_t status);

/** Write the string representation of the record into the provided buffer
 *
 * @param buf           pointer to a char array
 * @param len           length of the char array
 * @param elem          pointer to a BGP Stream Record to convert to string
 * @return pointer to the start of the buffer if successful, NULL otherwise
 */
char *bgpstream_record_snprintf(char *buf, size_t len,
                                bgpstream_record_t *record);

/** Write the string representation of the record/elem into the provided buffer
 *
 * @param buf           pointer to a char array
 * @param len           length of the char array
 * @param elem          pointer to a BGP Stream Record to convert to string
 * @param elem          pointer to a BGP Stream Elem to convert to string
 * @return pointer to the start of the buffer if successful, NULL otherwise
 */
char *bgpstream_record_elem_snprintf(char *buf, size_t len,
                                     bgpstream_record_t *record,
                                     bgpstream_elem_t *elem);

/** @} */

#endif /* __BGPSTREAM_RECORD_H */
