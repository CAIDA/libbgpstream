/*
 * Copyright (C) 2017 The Regents of the University of California.
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
 *   Mingwei Zhang
 */

#ifndef __BGPSTREAM_RESOURCE_H
#define __BGPSTREAM_RESOURCE_H

#include "bgpstream_record.h"
#include <stdint.h>

/** Types of transport supported */
typedef enum {

  /** Data is in a file (either local or via HTTP) */
  BGPSTREAM_RESOURCE_TRANSPORT_FILE = 0,

  /** Data is served from a Kafka queue */
  BGPSTREAM_RESOURCE_TRANSPORT_KAFKA = 1,

  /** Data is streamed via websockets */
  //  BGPSTREAM_RESOURCE_TRANSPORT_WEBSOCKET = 2,

  /** Data is locally cached */
  BGPSTREAM_RESOURCE_TRANSPORT_CACHE = 2,

  /** Data is streamed via http */
  BGPSTREAM_RESOURCE_TRANSPORT_HTTP = 3,

} bgpstream_resource_transport_type_t;

/** Encapsulation/encoding formats supported */
typedef enum bgpstream_resource_format_type {

  /** Native BGP data encapsulated in MRT */
  BGPSTREAM_RESOURCE_FORMAT_MRT = 0,

  /** Native BGP data encapsulated in BMP */
  BGPSTREAM_RESOURCE_FORMAT_BMP = 1,

  /** RIPE-format data encapsulated in JSON */
  BGPSTREAM_RESOURCE_FORMAT_RISLIVE = 2,

} bgpstream_resource_format_type_t;

/** Set of possible resource attribute types */
typedef enum bgpstream_resource_attr_type {

  /* BGPSTREAM_RESOURCE_TRANSPORT_KAFKA options */

  /** The topics to consume raw BMP data from (comma-separated) */
  BGPSTREAM_RESOURCE_ATTR_KAFKA_TOPICS = 0,

  /** The consumer group to use (for load balancing). If unset, defaults to a
      random group name */
  BGPSTREAM_RESOURCE_ATTR_KAFKA_CONSUMER_GROUP = 1,

  /** The initial offset to read from within the topic ("earliest", "latest") */
  BGPSTREAM_RESOURCE_ATTR_KAFKA_INIT_OFFSET = 2,

  /** The path toward a local cache */
  BGPSTREAM_RESOURCE_ATTR_CACHE_DIR_PATH = 3,

  /** Kafka message timestamp to begin from */
  BGPSTREAM_RESOURCE_ATTR_KAFKA_TIMESTAMP_FROM = 4,

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
  bgpstream_resource_transport_type_t transport_type;

  /** The encapsulation/encoding format of this resource
   * (e.g. MRT/Binary, BMP/Binary, JSON/Binary, JSON/RIPE-ASCII etc.)
   */
  bgpstream_resource_format_type_t format_type;

  /** Protocol/format specific info
   * (e.g. filename, kafka brokers, etc.)
   */
  char *url;

  /** Time of first record offered by the resource. A value of 0 indicates that
      the initial time is unknown. */
  uint32_t initial_time;

  /** Time duration of data offered by the resource. A value of
      BGPSTREAM_FOREVER indicates that the resource is a "stream" and should
      continue to be polled even after EOS is returned. */
  uint32_t duration;

  /** The name of the collection project */
  char *project;

  /** The name of the collector */
  char *collector;

  /** The type of records provided by the resource */
  bgpstream_record_type_t record_type;

  /** Extra attributes provided by the data interface that can be used by the
   * transport or format layers (they are optional as some may be provided by
   * the transport or format layers)
   *
   * (e.g., project, collector, type, nominal dump time)
   */
  struct attr *attrs[_BGPSTREAM_RESOURCE_ATTR_CNT];

} bgpstream_resource_t;

/** Create a new resource metadata object */
bgpstream_resource_t *bgpstream_resource_create(
  bgpstream_resource_transport_type_t transport_type,
  bgpstream_resource_format_type_t format_type, const char *url,
  uint32_t initial_time, uint32_t duration, const char *project,
  const char *collector, bgpstream_record_type_t record_type);

/** Destroy the given resource metadata object */
void bgpstream_resource_destroy(bgpstream_resource_t *resource);

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
const char *bgpstream_resource_get_attr(bgpstream_resource_t *resource,
                                        bgpstream_resource_attr_type_t type);

/** Get a unique hash of the resource
 *
 * @param buf           pointer to the buffer that stores the hash value
 * @param buf_len       buffer size
 * @param resource      pointer to the resource object
 * @return pointer to a unique hash of this resource
 */
int bgpstream_resource_hash_snprintf(char *buf, size_t buf_len,
                                     const bgpstream_resource_t *resource);

#endif /* __BGPSTREAM_RESOURCE_H */
