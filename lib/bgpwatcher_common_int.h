/*
 * This file is part of bgpwatcher
 *
 * Copyright (C) 2015 The Regents of the University of California.
 * Authors: Alistair King, Chiara Orsini
 *
 * All rights reserved.
 *
 * This code has been developed by CAIDA at UC San Diego.
 * For more information, contact bgpstream-info@caida.org
 *
 * This source code is proprietary to the CAIDA group at UC San Diego and may
 * not be redistributed, published or disclosed without prior permission from
 * CAIDA.
 *
 * Report any bugs, questions or comments to bgpstream-info@caida.org
 *
 */

#ifndef __BGPWATCHER_COMMON_INT_H
#define __BGPWATCHER_COMMON_INT_H

#include <czmq.h>
#include <stdint.h>
#include <sys/socket.h>

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

#define BW_PFX_ROW_BUFFER_LEN 17 + (BGPWATCHER_PEER_MAX_CNT*5)

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

  /** Highest table number in use */
  BGPWATCHER_TABLE_TYPE_MAX = BGPWATCHER_TABLE_TYPE_PREFIX,

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

#if 0
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

  /** Highest message number in use */
  BGPWATCHER_DATA_MSG_TYPE_MAX      = BGPWATCHER_DATA_MSG_TYPE_PREFIX_RECORD,
} bgpwatcher_data_msg_type_t;

#define bgpwatcher_data_msg_type_size_t sizeof(uint8_t)
#endif


/** @} */

/* ========== UTILITIES ========== */

/** Send the given IP over the given socket */
int bw_send_ip(void *dest, bgpstream_ip_addr_t *ip, int flags);

/** Receive an IP address on the given socket */
int bw_recv_ip(void *src, bgpstream_addr_storage_t *ip);

/** Serialize the given IP into the given byte array */
int bw_serialize_ip(uint8_t *buf, size_t len, bgpstream_ip_addr_t *ip);

/** Deserialize an IP from given byte array */
int bw_deserialize_ip(uint8_t *buf, size_t len, bgpstream_addr_storage_t *ip);

/* ========== MESSAGE TYPES ========== */

/** Receives one message from the given socket and decodes as a message type
 *
 * @param src          socket to receive on
 * @param flags        flags passed directed to zmq_recv (e.g. ZMQ_DONTWAIT)
 * @return the type of the message, or BGPWATCHER_MSG_TYPE_UNKNOWN
 */
bgpwatcher_msg_type_t bgpwatcher_recv_type(void *src, int flags);

#if 0
/** Receives one message from the given socket and decodes as data message type
 *
 * @param src           socket to receive on
 * @return the type of the message, or BGPWATCHER_REQ_MSG_TYPE_UNKNOWN if an
 *         error occurred
 */
bgpwatcher_data_msg_type_t bgpwatcher_recv_data_type(void *src);
#endif



/* ========== PREFIX TABLES ========== */

/** Transmit a pfx table begin message from the given pfx table structure
 *
 * @param dest            pointer to socket to send to
 * @param table           pointer to an initialized prefix table
 * @return 0 if the table was sent successfully, -1 otherwise
 */
int bgpwatcher_pfx_table_begin_send(void *dest, bgpwatcher_pfx_table_t *table);

/** Transmit a pfx table end message from the given pfx table structure
 *
 * @param dest            pointer to socket to send to
 * @param table           pointer to an initialized prefix table
 * @return 0 if the table was sent successfully, -1 otherwise
 *
 * This message can be used for both the table_begin and table_end events
 */
int bgpwatcher_pfx_table_end_send(void *dest, bgpwatcher_pfx_table_t *table);

/** Receive a prefix table begin message into provided memory
 *
 * @param      src           pointer to socket to receive on
 * @param[out] table         pointer to a prefix table structure to fill
 * @return 0 if the information was deserialized successfully, -1 otherwise
 */
int bgpwatcher_pfx_table_begin_recv(void *src, bgpwatcher_pfx_table_t *table);

/** Receive a prefix table end message and check it matches the given table
 *
 * @param      src           pointer to socket to receive on
 * @param[out] table         pointer to a prefix table structure to end
 * @return 0 if the information was deserialized successfully, -1 otherwise
 */
int bgpwatcher_pfx_table_end_recv(void *src, bgpwatcher_pfx_table_t *table);

/** Dump the given prefix row information to stdout
 *
 * @param prefix        pointer to a prefix table structure
 */
void bgpwatcher_pfx_table_dump(bgpwatcher_pfx_table_t *table);



/* ========== PREFIX ROWS ========== */

/** Send msgs from the given pfx information on the given socket
 *
 * @param dest            socket to send the prefix to
 * @param row             pointer to the prefix row to send
 * @param peer_cnt        number of peers in the row info array
 * @return 0 if the record was sent successfully, -1 otherwise
 */
int bgpwatcher_pfx_row_send(void *dest,
                            bl_pfx_storage_t *pfx,
                            bgpwatcher_pfx_peer_info_t *peer_infos,
                            int peer_cnt);

/** Deserialize a prefix message into provided memory
 *
 * @param      src           pointer to socket to receive on
 * @param[out] row_out       pointer to a prefix row structure to fill
 * @param      peer_cnt      number of peer info records expected
 * @return 0 if the information was deserialized successfully, -1 otherwise
 */
int bgpwatcher_pfx_row_recv(void *src,
                            bl_pfx_storage_t *pfx,
                            bgpwatcher_pfx_peer_info_t *peer_infos,
                            int peer_cnt);

/** Dump the given prefix row information to stdout
 *
 * @param table      pointer to a prefix table (for peer info)
 * @param row        pointer to a prefix row
 */
void bgpwatcher_pfx_row_dump(bgpwatcher_pfx_table_t *table,
                             bgpstream_pfx_storage_t *pfx,
                             bgpwatcher_pfx_peer_info_t *peer_infos);

/* ========== INTERESTS/VIEWS ========== */

/** Given a set of interests that are satisfied by a view, find the most
 *  specific and return the publication string
 *
 * @param interests     set of interests
 * @return most-specific publication string that satisfies the interests
 */
const char *bgpwatcher_consumer_interest_pub(int interests);

/** Given a set of interests, find the least specific return the subscription
 * string
 *
 * @param interests     set of interests
 * @return least-specific subscription string that satisfies the interests
 */
const char *bgpwatcher_consumer_interest_sub(int interests);

/** Receive an interest publication prefix and convert to an interests set
 *
 * @param src           socket to receive on
 * @return set of interest flags if successful, 0 otherwise
 */
uint8_t bgpwatcher_consumer_interest_recv(void *src);

#endif
