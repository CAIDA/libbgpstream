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

#include "bgpwatcher_common.h"
#include <bgpstream_utils_pfx.h>

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

  /** A view for the server to process */
  BGPWATCHER_MSG_TYPE_VIEW   = 4,

  /** Server is sending a response to a client */
  BGPWATCHER_MSG_TYPE_REPLY     = 5,

  /** Highest message number in use */
  BGPWATCHER_MSG_TYPE_MAX      = BGPWATCHER_MSG_TYPE_REPLY,

} bgpwatcher_msg_type_t;

#define bgpwatcher_msg_type_size_t sizeof(uint8_t)


/** @} */

/* ========== MESSAGE TYPES ========== */

/** Receives one message from the given socket and decodes as a message type
 *
 * @param src          socket to receive on
 * @param flags        flags passed directed to zmq_recv (e.g. ZMQ_DONTWAIT)
 * @return the type of the message, or BGPWATCHER_MSG_TYPE_UNKNOWN
 */
bgpwatcher_msg_type_t bgpwatcher_recv_type(void *src, int flags);


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
