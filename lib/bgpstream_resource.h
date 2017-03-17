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

#ifndef __BGPSTREAM_RESOURCE_H
#define __BGPSTREAM_RESOURCE_H

#include <stdint.h>
#include "bgpstream_record.h"
#include "bgpstream_transport.h"
#include "bgpstream_format.h"

/** Set of possible resource attribute types */
typedef enum bgpstream_resource_attr_type {

  /** INTERNAL: The total number of attribute types in use */
  _BGPSTREAM_RESOURCE_ATTR_CNT,

} bgpstream_resource_attr_type_t;

/** Structure describing a resource that BGPStream can obtain BGP data
 * from. Could be describing a file, an OpenBMP kafka cluster, a RIPE WebSockets
 * API, etc. It also conveys information about the underlying encapsulation
 * (e.g. MRT) and encoding (e.g. binary) format of the data. */
typedef struct bgpstream_resource {

  /** The Transport Protocol to use for this resource
   *  (e.g. kafka, file, websockets)
   */
  bgpstream_transport_type_t transport_type;

  /** The encapsulation/encoding format of this resource
   * (e.g. MRT/Binary, BMP/Binary, JSON/Binary, JSON/RIPE-ASCII etc.)
   */
  bgpstream_format_type_t format_type;

  /** Protocol/format specific info
   * (e.g. filename, kafka brokers, etc.)
   */
  char *uri;

  /** Time of first record offered by the resource. A value of -1 indicates that
      the resource contains no historical data. */
  uint32_t initial_time;

  /** Time duration of data offered by the resource. A value of -1 indicates
      that the resource contains a variable (probably live) length of data */
  uint32_t duration;

  /** The name of the collection project */
  char *project;

  /** The name of the collector */
  char *collector;

  /** The type of records provided by the resource */
  bgpstream_record_dump_type_t record_type;

#if 0
  /** Extra attributes provided by the data interface that can be used by the
   * transport or format layers (they are optional as some may be provided by
   * the transport or format layers)
   *
   * (e.g., project, collector, type, nominal dump time)
   */
  struct attr attrs[_BGPSTREAM_RESOURCE_ATTR_CNT];
#endif

} bgpstream_resource_t;

/** Create a new resource metadata object */
bgpstream_resource_t *
bgpstream_resource_create(bgpstream_transport_type_t transport_type,
                          bgpstream_format_type_t format_type,
                          const char *uri,
                          uint32_t initial_time,
                          uint32_t duration,
                          const char *project, const char *collector,
                          bgpstream_record_dump_type_t record_type);

/** Destroy the given resource metadata object */
void bgpstream_resource_destroy(bgpstream_resource_t *resource);

/** Open the given resource and ready for reading */
int bgpstream_resource_open(bgpstream_resource_t *resource);

#if 0
/** Helper function for setting an attribute for a resource object
 *
 * @param resource      pointer to the resource object
 * @param type          type of the attribute to set
 * @param value         borrowed pointer to a string attribute value
 * @return 0 if the attribute was added successfully, -1 otherwise
 */
int bgpstream_resource_set_attr(bgpstream_resource_t *resource,
                                bgpstream_resource_attr_type_t type,
                                const char *value);

/** Get the value for the given attribute type
 *
 * @param resource      pointer to the resource object
 * @param type          type of the attribute to get the value for
 * @return borrowed pointer to the attribute value, NULL if the attribute is
 * unset
 */
const char *
bgpstream_resource_get_attr(bgpstream_resource_t *resource,
                            bgpstream_resource_attr_type_t type);
#endif

#endif /* __BGPSTREAM_RESOURCE_H */
