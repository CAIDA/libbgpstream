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
 *
 * Authors:
 *   Chiara Orsini
 *   Alistair King
 *   Mingwei Zhang
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
  int64_t bs_transport_##name##_readline(bgpstream_transport_t *t,             \
                                     uint8_t *buffer, int64_t len);            \
  void bs_transport_##name##_destroy(bgpstream_transport_t *t);

#define BS_TRANSPORT_SET_METHODS(classname, transport)                         \
  do {                                                                         \
    (transport)->read = bs_transport_##classname##_read;                       \
    (transport)->readline = bs_transport_##classname##_readline;               \
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
   * @param buffer      The byte buffer to read data to
   * @param len         The maximum number of bytes to read
   * @return the number of bytes read if successful, -1 otherwise
   */
  int64_t (*read)(struct bgpstream_transport *t, uint8_t *buffer, int64_t len);

  /** Read line from this transport
   *
   * @param t           The data transport object to read from
   * @param buffer      The byte buffer to read data to
   * @param len         The maximum number of bytes to read
   * @return the number of bytes read if successful, -1 otherwise
   */
  int64_t (*readline)(struct bgpstream_transport *t, uint8_t *buffer, int64_t len);

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
   * These fields are only set if the transport is initialized.
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
