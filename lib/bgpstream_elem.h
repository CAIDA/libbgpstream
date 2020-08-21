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
 */

#ifndef __BGPSTREAM_ELEM_H
#define __BGPSTREAM_ELEM_H

#include "bgpstream_utils.h"

/** @file
 *
 * @brief Header file that exposes the public interface of a bgpstream elem.
 *
 * @author Chiara Orsini
 *
 */

/**
 * @name Public Opaque Data Structures
 *
 * @{ */

/** @} */

/**
 * @name Public Enums
 *
 * @{ */

/** Peer state encodes the state of the peer:
 *  - 0 - the state of the peer is unknown
 *  - [1-6] - the state encoded is one of the six FSM
 *            states described in RFC1771
 *  - [7-8] - inactive state in which all routes are cleared,
 *            more infor in quagga documentation http://goo.gl/NS9mSv
 */
typedef enum {

  /** Peer state unknown */
  BGPSTREAM_ELEM_PEERSTATE_UNKNOWN = 0,

  /** Peer state idle */
  BGPSTREAM_ELEM_PEERSTATE_IDLE = 1,

  /** Peer state connect */
  BGPSTREAM_ELEM_PEERSTATE_CONNECT = 2,

  /** Peer state active */
  BGPSTREAM_ELEM_PEERSTATE_ACTIVE = 3,

  /** Peer state open-sent */
  BGPSTREAM_ELEM_PEERSTATE_OPENSENT = 4,

  /** Peer state open-confirm */
  BGPSTREAM_ELEM_PEERSTATE_OPENCONFIRM = 5,

  /** Peer state established */
  BGPSTREAM_ELEM_PEERSTATE_ESTABLISHED = 6,

  /** Peer state clearing */
  BGPSTREAM_ELEM_PEERSTATE_CLEARING = 7,

  /** Peer state clearing */
  BGPSTREAM_ELEM_PEERSTATE_DELETED = 8,

} bgpstream_elem_peerstate_t;

/**
 * BGP ORIGIN Path Attribute values
 */
typedef enum {

  /** IGP - Network Layer Reachability Information is interior to the
      originating AS */
  BGPSTREAM_ELEM_BGP_UPDATE_ORIGIN_IGP = 0,

  /** EGP - Network Layer Reachability Information learned via the EGP protocol
      [RFC904] */
  BGPSTREAM_ELEM_BGP_UPDATE_ORIGIN_EGP = 1,

  /** INCOMPLETE - Network Layer Reachability Information learned by some other
      means */
  BGPSTREAM_ELEM_BGP_UPDATE_ORIGIN_INCOMPLETE = 2,

} bgpstream_elem_origin_type_t;

/** Elem types */
typedef enum {

  /** Unknown */
  BGPSTREAM_ELEM_TYPE_UNKNOWN = 0,

  /** RIB Entry */
  BGPSTREAM_ELEM_TYPE_RIB = 1,

  /** Announcement */
  BGPSTREAM_ELEM_TYPE_ANNOUNCEMENT = 2,

  /** Withdrawal */
  BGPSTREAM_ELEM_TYPE_WITHDRAWAL = 3,

  /** Peer state change */
  BGPSTREAM_ELEM_TYPE_PEERSTATE = 4,

} bgpstream_elem_type_t;

typedef struct struct_bgpstream_annotations_t {

  /** RPKI active */
  int rpki_active;

  /** RPKI validation configuration */
  struct struct_rpki_config_t *cfg;

  /** Record timestamp */
  uint32_t timestamp;

} bgpstream_annotations_t;

/** Elem aggregator object */
typedef struct bgpstream_elem_aggregator {

  /** Boolean value to check if aggregator field is set */
  uint8_t has_aggregator;

  /** Aggregator ASN */
  uint32_t aggregator_asn;

  /** Aggregator IP */
  bgpstream_ip_addr_t aggregator_addr;

} bgpstream_elem_aggregator_t;

/** @} */

/**
 * @name Public Data Structures
 *
 * @{ */

/** A BGP Stream Elem object */
typedef struct bgpstream_elem {

  /** Type */
  bgpstream_elem_type_t type;

  /** Originated Time (seconds component).
   *
   * For RIB records, this is the time the prefix was heard (e.g.,
   * https://tools.ietf.org/html/rfc6396#section-4.3.4); for BMP messages, this
   * is the timestamp in the Peer Header (see
   * https://tools.ietf.org/html/rfc7854#section-4.2). Care must be taken when
   * using this value as it will often be zero (e.g., there is a bug in a Cisco
   * implementation of BMP that does not set the time in the peer header), and
   * even if it is set, its meaning is dependent on the data source.
   *
   * NOTE: This is NOT the same as the `time_sec` field in the record. It MUST
   * NOT be used in place of that field. If you are unsure about which time to
   * use, then you probably want the timestamp in the record structure.
   */
  uint32_t orig_time_sec;

  /** Originated Time (microseconds component) */
  uint32_t orig_time_usec;

  /** Peer IP address
   *
   * This is the IP address that the peer used to connect to the collector (or
   * router in the case of BMP).
   */
  bgpstream_ip_addr_t peer_ip;

  /** Peer AS number */
  uint32_t peer_asn;

  /* Type-dependent fields */

  /** IP prefix
   *
   * Available only for RIB, Announcement and Withdrawal elem types
   */
  bgpstream_pfx_t prefix;

  /** Next hop
   *
   * Available only for RIB and Announcement elem types
   */
  bgpstream_ip_addr_t nexthop;

  /** AS path
   *
   * Available only for RIB and Announcement elem types
   */
  bgpstream_as_path_t *as_path;

  /** Communities
   *
   * Available only for RIB and Announcement elem types
   */
  bgpstream_community_set_t *communities;

  /** Old peer state
   *
   * Available only for the Peer-state elem type
   */
  bgpstream_elem_peerstate_t old_state;

  /** New peer state
   *
   * Available only for the Peer-state elem type
   */
  bgpstream_elem_peerstate_t new_state;

  /** Annotations
   *
   * Annotations from other libraries
   */
  bgpstream_annotations_t annotations;

  /** ORIGIN as-path attribute
   * This attribute indicates where the update comes from:
   * internal network (IGP), external network (EGP), or other means
   * (INCOMPLETE).
   */
  bgpstream_elem_origin_type_t origin;

  /** Set if the origin field is valid */
  uint8_t has_origin;

  /** MED attribute */
  uint32_t med;

  /** Set if the med field is valid */
  uint8_t has_med;

  /** LOCAL_PREF attribute */
  uint32_t local_pref;

  /** Set if the local_pref field is valid */
  uint8_t has_local_pref;

  /** Atomic aggregate attribute */
  uint8_t atomic_aggregate;

  /** Atomic aggregate attribute */
  bgpstream_elem_aggregator_t aggregator;

} bgpstream_elem_t;

/** @} */

/**
 * @name Public API Functions
 *
 * @{ */

/** Create a new BGP Stream Elem instance
 *
 * @return a pointer to an Elem instance if successful, NULL otherwise
 */
bgpstream_elem_t *bgpstream_elem_create(void);

/** Destroy the given BGP Stream Elem instance
 *
 * @param elem        pointer to a BGP Stream Elem instance to destroy
 */
void bgpstream_elem_destroy(bgpstream_elem_t *elem);

/** Clear the given BGP Stream Elem instance
 *
 * @param elem        pointer to a BGP Stream Elem instance to clear
 */
void bgpstream_elem_clear(bgpstream_elem_t *elem);

/** Copy the given BGP Stream Elem to the given destination
 *
 * @param dst           pointer to an elem to copy into
 * @param src           pointer to an elem to copy from
 * @return pointer to dst if successful, NULL otherwise
 *
 * The `dst` elem must have been created using bgpstream_elem_create, or if
 * being re-used, cleared using bgpstream_elem_clear before calling this
 * function.
 */
bgpstream_elem_t *bgpstream_elem_copy(bgpstream_elem_t *dst,
                                      const bgpstream_elem_t *src);

/** Write the string representation of the elem type into the provided buffer
 *
 * @param buf           pointer to a char array
 * @param len           length of the char array
 * @param type          BGP Stream Elem type to convert to string
 * @return the number of characters that would have been written if len was
 * unlimited
 */
int bgpstream_elem_type_snprintf(char *buf, size_t len,
                                 bgpstream_elem_type_t type);

/** Write the string representation of the elem peerstate into the provided
 *  buffer
 *
 * @param buf           pointer to a char array
 * @param len           length of the char array
 * @param state         BGP Stream Elem peerstate to convert to string
 * @return the number of characters that would have been written if len was
 * unlimited
 */
int bgpstream_elem_peerstate_snprintf(char *buf, size_t len,
                                      bgpstream_elem_peerstate_t state);

/** Write the string representation of the elem into the provided buffer
 *
 * @param buf           pointer to a char array
 * @param len           length of the char array
 * @param elem          pointer to a BGP Stream Elem to convert to string
 * @return pointer to the start of the buffer if successful, NULL otherwise
 */
char *bgpstream_elem_snprintf(char *buf, size_t len,
                              const bgpstream_elem_t *elem);

/** @} */

#endif /* __BGPSTREAM_ELEM_H */
