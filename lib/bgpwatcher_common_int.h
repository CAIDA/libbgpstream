/*
 * bgpwatcher
 *
 * Alistair King, CAIDA, UC San Diego
 * corsaro-info@caida.org
 *
 * Copyright (C) 2012 The Regents of the University of California.
 *
 * This file is part of bgpwatcher.
 *
 * bgpwatcher is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * bgpwatcher is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with bgpwatcher.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef __BGPWATCHER_COMMON_INT_H
#define __BGPWATCHER_COMMON_INT_H

#include <czmq.h>
#include <stdint.h>
#include <sys/socket.h>

#include <bgpstream_elem.h>

#include <bgpwatcher_common.h>

/** @file
 *
 * @brief Header file that exposes the public structures used by both
 * bgpwatcher_client and bgpwatcher_server
 *
 * @author Alistair King
 *
 */

/**
 * @name Public Constants
 *
 * @{ */


#ifdef DEBUG_TIMING

#define TIMER_START(timer)			\
  struct timeval timer##_start;			\
  do {						\
  gettimeofday_wrap(&timer##_start);		\
  } while(0)

#define TIMER_END(timer)					\
  struct timeval timer##_end, timer##_diff;				\
  do {								\
    gettimeofday_wrap(&timer##_end);				\
    timeval_subtract(&timer##_diff, &timer##_end, &timer##_start);	\
  } while(0)

#define TIMER_VAL(timer)			\
  ((timer##_diff.tv_sec*1000000) + timer##_diff.tv_usec)
#else

#define TIMER_START(timer)
#define TIMER_END(timer)
#define TIMER_VAL(timer) (uint64_t)(0)

#endif

/* shared constants are in bgpwatcher_common.h */

/** @} */

/**
 * @name Public Opaque Data Structures
 *
 * @{ */

/** @} */

/**
 * @name Public Data Structures
 *
 * @{ */

/** Type of a sequence number */
typedef uint32_t seq_num_t;

/** @} */

/**
 * @name Public Enums
 *
 * @{ */

/** Table types */
typedef enum {

  /** Invalid table */
  BGPWATCHER_TABLE_TYPE_NONE = 0,

  /** Prefix table */
  BGPWATCHER_TABLE_TYPE_PREFIX = 1,

  /** Peer table */
  BGPWATCHER_TABLE_TYPE_PEER = 2,

  /** Highest table number in use */
  BGPWATCHER_TABLE_TYPE_MAX = BGPWATCHER_TABLE_TYPE_PEER,

} bgpwatcher_table_type_t;

#define bgpwatcher_table_type_size_t sizeof(uint8_t)

/** Enumeration of message types
 *
 * @note these will be cast to a uint8_t, so be sure that there are fewer than
 * 2^8 values
 */
typedef enum {
  /** Invalid message */
  BGPWATCHER_MSG_TYPE_UNKNOWN   = 0,

  /** Client is ready to send requests/Server is ready for requests */
  BGPWATCHER_MSG_TYPE_READY     = 1,

  /** Client is explicitly disconnecting (clean shutdown) */
  BGPWATCHER_MSG_TYPE_TERM      = 2,

  /** Server/Client is still alive */
  BGPWATCHER_MSG_TYPE_HEARTBEAT = 3,

  /** A request for the server to process */
  BGPWATCHER_MSG_TYPE_DATA   = 4,

  /** Server is sending a response to a client */
  BGPWATCHER_MSG_TYPE_REPLY     = 5,

  /** Highest message number in use */
  BGPWATCHER_MSG_TYPE_MAX      = BGPWATCHER_MSG_TYPE_REPLY,

} bgpwatcher_msg_type_t;

#define bgpwatcher_msg_type_size_t sizeof(uint8_t)

/** Enumeration of request message types
 *
 * @note these will be cast to a uint8_t, so be sure that there are fewer than
 * 2^8 values
 */
typedef enum {
  /** Invalid message */
  BGPWATCHER_DATA_MSG_TYPE_UNKNOWN   = 0,

  /** Client is beginning a new table */
  BGPWATCHER_DATA_MSG_TYPE_TABLE_BEGIN = 1,

  /* Client has completed a table */
  BGPWATCHER_DATA_MSG_TYPE_TABLE_END = 2,

  /** Client is sending a prefix record */
  BGPWATCHER_DATA_MSG_TYPE_PREFIX_RECORD  = 3,

  /** Client is sending a peer record */
  BGPWATCHER_DATA_MSG_TYPE_PEER_RECORD  = 4,

  /** Highest message number in use */
  BGPWATCHER_DATA_MSG_TYPE_MAX      = BGPWATCHER_DATA_MSG_TYPE_PEER_RECORD,
} bgpwatcher_data_msg_type_t;

#define bgpwatcher_data_msg_type_size_t sizeof(uint8_t)


/** @} */

/* ========== UTILITIES ========== */

/** @deprecated */
int bgpwatcher_msg_addip(zmsg_t *msg, bgpstream_ip_address_t *ip);

/** Extracts a BGP Stream IP address from the given message
 *
 * @param msg           pointer to an initialized zmsg
 * @param ip            pointer to an ip address to fill
 * @return 0 if the address was successfully extracted, -1 otherwise
 */
int bgpwatcher_msg_popip(zmsg_t *msg,
                         bgpstream_ip_address_t *ip);



/* ========== MESSAGE TYPES ========== */

/** Decodes the message type for the given frame
 *
 * @param frame         zframe object to inspect
 * @return the type of the message, or BGPWATCHER_MSG_TYPE_UNKNOWN if an error
 *         occurred
 */
bgpwatcher_msg_type_t bgpwatcher_msg_type_frame(zframe_t *frame);

/** Receives one message from the given socket and decodes as a message type
 *
 * @param src          socket to receive on
 * @return the type of the message, or BGPWATCHER_MSG_TYPE_UNKNOWN
 */
bgpwatcher_msg_type_t bgpwatcher_recv_type(void *src);

/** Decodes the message type for the given message
 *
 * @param msg           zmsg object to inspect
 * @param peek          if set, the msg type frame will be left on the msg
 * @return the type of the message, or BGPWATCHER_MSG_TYPE_UNKNOWN if an error
 *         occurred
 *
 * This function will pop the type frame from the beginning of the message
 */
bgpwatcher_msg_type_t bgpwatcher_msg_type(zmsg_t *msg, int peek);

/** Decodes the request type for the given message
 *
 * @param msg           zmsg object to inspect
 * @return the type of the message, or BGPWATCHER_REQ_MSG_TYPE_UNKNOWN if an
 *         error occurred
 *
 * This function will pop the type frame from the beginning of the message
 */
bgpwatcher_data_msg_type_t bgpwatcher_data_msg_type(zmsg_t *msg);



/* ========== PREFIX TABLES ========== */

/** Create a new 0mq msg from the given pfx table structure
 *
 * @param table           pointer to an initialized prefix table
 * @return pointer to a new zmsg if successful, NULL otherwise
 *
 * This message can be used for both the table_begin and table_end events
 */
zmsg_t *bgpwatcher_pfx_table_msg_create(bgpwatcher_pfx_table_t *table);

/** Deserialize a prefix table message into provided memory
 *
 * @param      msg           pointer to the message to deserialize
 * @param[out] table         pointer to a prefix table structure to fill
 * @return 0 if the information was deserialized successfully, -1 otherwise
 */
int bgpwatcher_pfx_table_msg_deserialize(zmsg_t *msg,
                                         bgpwatcher_pfx_table_t *table);

/** Dump the given prefix row information to stdout
 *
 * @param prefix        pointer to a prefix table structure
 */
void bgpwatcher_pfx_table_dump(bgpwatcher_pfx_table_t *table);



/* ========== PREFIX RECORDS ========== */

/** Send msgs from the given pfx information on the given socket
 *
 * @param dest            socket to send the prefix to
 * @param prefix          pointer to a bgpstream prefix
 * @param peer_ip         pointer to a bgpstream ip address of the peer
 * @param orig_asn        value of the origin ASN
 * @param sendmore        set to 1 if there is more to this message
 * @return 0 if the record was sent successfully, -1 otherwise
 */
int bgpwatcher_pfx_record_send(void *dest,
                               bgpstream_prefix_t *prefix,
                               uint32_t orig_asn,
                               int sendmore);

/** Deserialize a prefix message into provided memory
 *
 * @param      msg           pointer to the message to deserialize
 * @param[out] pfx_out       pointer to a prefix structure to fill
 * @param[out] orig_asn_out  pointer to memory to fill with orig_asn
 * @return 0 if the information was deserialized successfully, -1 otherwise
 */
int bgpwatcher_pfx_msg_deserialize(zmsg_t *msg,
                                   bgpstream_prefix_t *pfx_out,
                                   uint32_t *orig_asn_out);

/** Dump the given prefix row information to stdout
 *
 * @param prefix        pointer to a prefix structure
 * @param orig_asn      origin asn
 */
void bgpwatcher_pfx_record_dump(bgpstream_prefix_t *prefix,
                                uint32_t orig_asn);



/* ========== PEER TABLES ========== */

/** Create a new 0mq msg from the given peer table structure
 *
 * @param table           pointer to an initialized peer table
 * @return pointer to a new zmsg if successful, NULL otherwise
 *
 * This message can be used for both the table_begin and table_end events
 */
zmsg_t *bgpwatcher_peer_table_msg_create(bgpwatcher_peer_table_t *table);

/** Deserialize a peer table message into provided memory
 *
 * @param      msg           pointer to the message to deserialize
 * @param[out] table         pointer to a peer table structure to fill
 * @return 0 if the information was deserialized successfully, -1 otherwise
 */
int bgpwatcher_peer_table_msg_deserialize(zmsg_t *msg,
                                          bgpwatcher_peer_table_t *table);

/** Dump the given peer row information to stdout
 *
 * @param prefix        pointer to a peer table structure
 */
void bgpwatcher_peer_table_dump(bgpwatcher_peer_table_t *table);



/* ========== PEER RECORDS ========== */

/** Create a new 0mq msg from the given peer information
 *
 * @param peer_ip       pointer to the peer ip
 * @param status        status value
 * @return pointer to a new zmsg if successful, NULL otherwise
 */
zmsg_t *bgpwatcher_peer_msg_create(bgpstream_ip_address_t *peer_ip,
                                   uint8_t status);

/** Deserialize a peer message into provided memory
 *
 * @param      msg           pointer to the message to deserialize
 * @param[out] peer_ip_out   pointer to an ip structure to fill
 * @param[out] status_out    pointer to memory to fill with status code
 * @return 0 if the information was deserialized successfully, -1 otherwise
 */
int bgpwatcher_peer_msg_deserialize(zmsg_t *msg,
                                    bgpstream_ip_address_t *peer_ip_out,
                                    uint8_t *status_out);

/** Dump the given peer record information to stdout
 *
 * @param ip            pointer to the peer IP
 * @param status        peer status value
 */
void bgpwatcher_peer_record_dump(bgpstream_ip_address_t *peer_ip,
                                 uint8_t status);



#endif
