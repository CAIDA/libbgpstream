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
 */

#ifndef __BGPSTREAM_PARSEBGP_COMMON_H
#define __BGPSTREAM_PARSEBGP_COMMON_H

#include "bgpstream_elem.h"
#include "bgpstream_format.h"
#include "parsebgp.h"

#define COPY_IP(dst, afi, src, do_unknown)                                     \
  do {                                                                         \
    switch (afi) {                                                             \
    case PARSEBGP_BGP_AFI_IPV4:                                                \
      (dst)->version = BGPSTREAM_ADDR_VERSION_IPV4;                            \
      memcpy(&(dst)->ipv4, src, 4);                                            \
      break;                                                                   \
                                                                               \
    case PARSEBGP_BGP_AFI_IPV6:                                                \
      (dst)->version = BGPSTREAM_ADDR_VERSION_IPV6;                            \
      memcpy(&(dst)->ipv6, src, 16);                                           \
      break;                                                                   \
                                                                               \
    default:                                                                   \
      do_unknown;                                                              \
    }                                                                          \
  } while (0)

// read in chunks of 1MB to minimize the number of partial parses we end up
// doing.  this is also the same length as the wandio thread buffer, so this
// might help reduce the time waiting for locks
#define BGPSTREAM_PARSEBGP_BUFLEN 1024 * 1024

/** Process the given path attributes and populate the given elem
 *
 * @param el            pointer to the elem to populate
 * @param attrs         array of parsebgp path attributes to process
 * @return 0 if processing was successful, -1 otherwise
 *
 * @note this does not process the NEXT_HOP attribute, nor the
 * MP_REACH/MP_UNREACH attributes
 */
int bgpstream_parsebgp_process_path_attrs(
  bgpstream_elem_t *el, parsebgp_bgp_update_path_attr_t *attrs);

/** Extract the appropriate NEXT-HOP information from the given attributes
 *
 * @param el            pointer to the elem to populate
 * @param attrs         array of parsebgp path attributes to process
 * @param is_mp_pfx     flag indicating if the current prefix is from MP_REACH
 * @return 0 if processing was successful, -1 otherwise
 *
 * Note: from my reading of RFC4760, it is theoretically possible for a single
 * UPDATE to carry reachability information for both v4 and another (v6) AFI, so
 * we use the is_mp_pfx flag to direct us to either the NEXT_HOP attr, or the
 * MP_REACH attr.
 */
int bgpstream_parsebgp_process_next_hop(bgpstream_elem_t *el,
                                        parsebgp_bgp_update_path_attr_t *attrs,
                                        int is_mp_pfx);

/** State used when extracting elems from an UPDATE message */
typedef struct bgpstream_parsebgp_upd_state {

  // has the BGP4MP state been prepared
  int ready;

  // how many native (IPv4) withdrawals still to yield
  int withdrawal_v4_cnt;
  int withdrawal_v4_idx;

  // how many MP_UNREACH (IPv6) withdrawals still to yield
  int withdrawal_v6_cnt;
  int withdrawal_v6_idx;

  // how many native (IPv4) announcements still to yield
  int announce_v4_cnt;
  int announce_v4_idx;

  // how many MP_REACH (IPv6) announcements still to yield
  int announce_v6_cnt;
  int announce_v6_idx;

  // have path attributes been processed
  int path_attr_done;

  // has the native next-hop been processed
  int next_hop_v4_done;

  // has the mp_reach next-hop been processed
  int next_hop_v6_done;

} bgpstream_parsebgp_upd_state_t;

/** Reset the given update state */
void bgpstream_parsebgp_upd_state_reset(
  bgpstream_parsebgp_upd_state_t *upd_state);

/** Process the given UPDATE message and extract a single elem from it
 *
 * @param upd_state     pointer to the generator state
 * @param elem          pointer to the elem to populate
 * @param bgp           pointer to a parsed BGP message
 * @return 1 if the elem was populated, 0 if there are no more elems, -1 if an
 * error occurred.
 */
int bgpstream_parsebgp_process_update(bgpstream_parsebgp_upd_state_t *upd_state,
                                      bgpstream_elem_t *elem,
                                      parsebgp_bgp_msg_t *bgp);

typedef struct bgpstream_parsebgp_decode_state {

  // outer message type to decode (MRT or BMP)
  parsebgp_msg_type_t msg_type;

  // options for libparsebgp
  parsebgp_opts_t parser_opts;

  // raw data buffer
  // TODO: once parsebgp supports reading using a read callback, just pass the
  // transport callback to the parser
  uint8_t buffer[BGPSTREAM_PARSEBGP_BUFLEN];

  // number of bytes left to read in the buffer
  size_t remain;

  // pointer into buffer
  uint8_t *ptr;

  // the total number of successful (filtered and not) reads
  uint64_t successful_read_cnt;

  // the number of non-filtered reads (i.e. "useful")
  uint64_t valid_read_cnt;

} bgpstream_parsebgp_decode_state_t;

typedef enum {

  /** Indicates the message should be filtered */
  BGPSTREAM_PARSEBGP_FILTER_OUT = 0,

  /** Indicates the message should be silently skipped */
  BGPSTREAM_PARSEBGP_SKIP = 1,

  /** Indicates the message should be kept and given to the user */
  BGPSTREAM_PARSEBGP_KEEP = 2,

  /** Indicates the message should be treated as EOS (since no more filters
     could possibly match) */
  BGPSTREAM_PARSEBGP_EOS = 3,

} bgpstream_parsebgp_check_filter_rc_t;

/** Once a message has been read by _populate_record, this callback gives the
 * caller a chance to check filters, and choose to skip the message
 *
 * @param format        pointer to the format that originally called
 *                      _populate_record
 * @param record        pointer to the record being populated
 * @param msg           pointer to the parsed message
 * @return 1 if the message should be kept, 0 if it should be skipped, -1 if an
 * error occurred.
 *
 * It is the responsibility of the callee to set the record timestamp fields.
 */
typedef bgpstream_parsebgp_check_filter_rc_t (
  bgpstream_parsebgp_check_filter_cb_t)(bgpstream_format_t *format,
                                        bgpstream_record_t *record,
                                        parsebgp_msg_t *msg);

/** Called before a message is passed to parsebgp to parse any non-standard
 * headers encapsulating a message
 *
 * @param format        pointer to the format that originally called
 *                      _populate_record
 * @param buf           pointer to the raw data buffer
 * @param len[out]      length of the data buffer, should updated with the
 *                      number of bytes read
 * @param record        pointer to the record being populated
 * @return 0 if successful, -1 otherwise
 */
typedef int (bgpstream_parsebgp_prep_buf_cb_t)(bgpstream_format_t *format,
                                               uint8_t *buf, size_t *len,
                                               bgpstream_record_t *record);

/** Use libparsebgp to decode a message */
bgpstream_format_status_t bgpstream_parsebgp_populate_record(
  bgpstream_parsebgp_decode_state_t *state, parsebgp_msg_t *msg,
  bgpstream_format_t *format, bgpstream_record_t *record,
  bgpstream_parsebgp_prep_buf_cb_t *prep_cb,
  bgpstream_parsebgp_check_filter_cb_t *filter_cb);

/** Set options specific to how we use libparsebgp in BGPStream */
void bgpstream_parsebgp_opts_init(parsebgp_opts_t *opts);

#endif /* __BGPSTREAM_PARSEBGP_COMMON_H */
