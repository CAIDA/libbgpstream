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

#ifndef __BGPWATCHER_INT_H
#define __BGPWATCHER_INT_H

#include <stdint.h>

#include <bgpwatcher.h>
#include <bgpwatcher_server.h>
#include "bgpstore_lib.h"

/** @file
 *
 * @brief Header file that exposes the private interface of bgpwatcher.
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

typedef struct bgpwatcher {

  /** Our server instance */
  bgpwatcher_server_t *server;

  /** Error status */
  bgpwatcher_err_t err;

  /** bgp data time series store */
  bgpstore_t *bgp_store;
  
} bgpwatcher_t;

/** @} */

/**
 * @name Public Enums
 *
 * @{ */


/** @} */

#endif
