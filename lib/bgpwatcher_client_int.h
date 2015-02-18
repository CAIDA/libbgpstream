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

#ifndef __BGPWATCHER_CLIENT_INT_H
#define __BGPWATCHER_CLIENT_INT_H

#include <czmq.h>
#include <stdint.h>

#include "bgpwatcher_client.h"
#include "bgpwatcher_client_broker.h"

/** @file
 *
 * @brief Header file that exposes the private interface of the bgpwatcher
 * client
 *
 * @author Alistair King
 *
 */

/**
 * @name Public Opaque Data Structures
 *
 * @{ */

/** @} */

/**
 * @name Protected Data Structures
 *
 * @{ */

KHASH_INIT(v4pfx_peers,
           bl_ipv4_pfx_t,
           bgpwatcher_pfx_peer_info_t*,
           1,
           bl_ipv4_pfx_hash_func,
           bl_ipv4_pfx_hash_equal)

KHASH_INIT(v6pfx_peers,
           bl_ipv6_pfx_t,
           bgpwatcher_pfx_peer_info_t*,
           1,
           bl_ipv6_pfx_hash_func,
           bl_ipv6_pfx_hash_equal)

struct bgpwatcher_client_pfx_table {

  /** Indicates that a table_start message should not be sent on the next
      pfx_add */
  int started;

  /** Table information (partially used) */
  bgpwatcher_pfx_table_t info;

  /** Count of peers added so far */
  int peers_added;

  /** Hash table of prefixes being added */
  kh_v4pfx_peers_t *v4pfx_peers;

  /** Hash table of prefixes being added */
  kh_v6pfx_peers_t *v6pfx_peers;

  /** The number of times this table has been reused */
  int reuse_cnt;

};

struct bgpwatcher_client {

  /** shared config that we have prepared for our broker(s) */
  bgpwatcher_client_broker_config_t broker_config;

  /** handle to communicate with our broker */
  zactor_t *broker;

  /** raw socket to the broker */
  void *broker_zocket;

  /** Error status */
  bgpwatcher_err_t err;

  /** Next request sequence number to use */
  seq_num_t seq_num;

  /** State for the current prefix table */
  bgpwatcher_client_pfx_table_t pfx_table;

  /** Indicates that the client has been signaled to shutdown */
  int shutdown;

};

/** @} */

/**
 * @name Public Enums
 *
 * @{ */

/** @} */

#endif
