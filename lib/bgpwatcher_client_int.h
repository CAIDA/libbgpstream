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

#define hash_row(row) (bl_pfx_storage_hash_func(row.prefix))

#define hash_eq_row(a, b) (bl_pfx_storage_hash_equal(a.prefix, b.prefix))

KHASH_INIT(pfx_peers, bgpwatcher_pfx_row_t, char, 0, hash_row, hash_eq_row)

struct bgpwatcher_client_pfx_table {

  /** Indicates that a table_start message should not be sent on the next
      pfx_add */
  int started;

  /** Table information (partially used) */
  bgpwatcher_pfx_table_t info;

  /** Count of peers added so far */
  int peers_added;

  /** Hash table of prefixes being added */
  kh_pfx_peers_t *pfx_peers;

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
