/*
 * This file is part of bgpstream
 *
 * CAIDA, UC San Diego
 * bgpstream-info@caida.org
 *
 * Copyright (C) 2012 The Regents of the University of California.
 * Authors: Alistair King, Chiara Orsini
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __BGPSTREAM_TRANSPORT_INTERFACE_H
#define __BGPSTREAM_TRANSPORT_INTERFACE_H

#include "config.h"
#include "bgpstream.h"
#include "bgpstream_transport.h" /*< for bs_transport_t */

/** @file
 *
 * @brief Header file that exposes the protected interface of the data transport
 * plugin API
 *
 * @author Alistair King
 *
 */

/** Convenience macro that defines all the function prototypes for the data
 * transport API */
#define BS_TRANSPORT_GENERATE_PROTOS(name)                                     \
  int bs_transport_##name##_create(bgpstream_transport_t *transport);          \
  int64_t bs_transport_##name##_read(bgpstream_transport_t *t,                 \
                                     uint8_t *buffer, int64_t len);            \
  void bs_transport_##name##_destroy(bgpstream_transport_t *t);

#define BS_TRANSPORT_SET_METHODS(classname, transport)                         \
  do {                                                                         \
    (transport)->read = bs_transport_##classname##_read;                       \
    (transport)->destroy = bs_transport_##classname##_destroy;                 \
  } while (0)

/** Structure which represents a data transport */
struct bgpstream_transport {

  /**
   * @name Transport method pointers
   *
   * @{ */

  /** Read bytes from this transport
   *
   * @param t           The data transport object to read from
   * @return the number of bytes read if successful, -1 otherwise
   */
  int64_t (*read)(struct bgpstream_transport *t, uint8_t *buffer, int64_t len);

  /** Shutdown and free this data transport
   *
   * @param transport   The data transport object to free
   *
   * @note transports should *only* free transport-specific state. All other
   * state will be free'd for them by the data transport manager.
   */
  void (*destroy)(struct bgpstream_transport *transport);

  /** }@ */

  /**
   * @name Data transport state fields
   *
   * These fields are only set if the transport is initialized
   * @note These fields should *not* be directly manipulated by
   * transports. Instead they should use accessor functions provided by the
   * transport manager.
   *
   * @{ */

  /** Pointer to the resource the transport is reading from */
  bgpstream_resource_t *res;

  /** An opaque pointer to transport-specific state if needed by the
      transport */
  void *state;
  
  /** }@ */
};

#endif /* __BGPSTREAM_TRANSPORT_INTERFACE_H */
