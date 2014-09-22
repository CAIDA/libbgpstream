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

#include <bgpwatcher_client.h>
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
 * @name Public Data Structures
 *
 * @{ */

typedef struct bgpwatcher_client {

  /** handle to communicate with our broker */
  zactor_t *broker;

  /** config that we have prepared for our broker */
  bgpwatcher_client_broker_t broker_state;

  /** Error status */
  bgpwatcher_err_t err;

  /** Next request sequence number to use */
  uint32_t sequence_num;

  /** @todo add lazy pirate re-tx stuff here */

  /** Indicates that the client has been signaled to shutdown */
  int shutdown;

} bgpwatcher_client_t;

/** @todo consider merging pfx and peer tables into a single table type */
typedef struct bgpwatcher_client_pfx_table {

  /** Client instance that owns this table */
  bgpwatcher_client_t *client;

  /** Time that this table was dumped */
  uint32_t time;

  /** @todo */

} bgpwatcher_client_pfx_table_t;

typedef struct bgpwatcher_client_peer_table {

  /** Client instance that owns this table */
  bgpwatcher_client_t *client;

  /** Time that this table was dumped */
  uint32_t time;

  /** @todo */

} bgpwatcher_client_peer_table_t;

/** @} */

/**
 * @name Public Enums
 *
 * @{ */

/** @} */

#endif
