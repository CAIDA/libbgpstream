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


/** @} */

/* ========== UTILITIES ========== */


/* ========== MESSAGE TYPES ========== */

/** Receives one message from the given socket and decodes as a message type
 *
 * @param src          socket to receive on
 * @return the type of the message, or BGPWATCHER_MSG_TYPE_UNKNOWN
 */
bgpwatcher_msg_type_t bgpwatcher_recv_type(void *src);

/** Receives one message from the given socket and decodes as data message type
 *
 * @param src           socket to receive on
 * @return the type of the message, or BGPWATCHER_REQ_MSG_TYPE_UNKNOWN if an
 *         error occurred
 */
bgpwatcher_data_msg_type_t bgpwatcher_recv_data_type(void *src);



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
int bgpwatcher_pfx_row_send(void *dest, bgpwatcher_pfx_row_t *row,
                            int peer_cnt);

/** Deserialize a prefix message into provided memory
 *
 * @param      src           pointer to socket to receive on
 * @param[out] row_out       pointer to a prefix row structure to fill
 * @param      peer_cnt      number of peer info records expected
 * @return 0 if the information was deserialized successfully, -1 otherwise
 */
int bgpwatcher_pfx_row_recv(void *src, bgpwatcher_pfx_row_t *row_out,
                            int peer_cnt);

/** Dump the given prefix row information to stdout
 *
 * @param table      pointer to a prefix table (for peer info)
 * @param row        pointer to a prefix row
 */
void bgpwatcher_pfx_row_dump(bgpwatcher_pfx_table_t *table,
                             bgpwatcher_pfx_row_t *row);

#endif
