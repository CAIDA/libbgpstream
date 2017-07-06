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

#ifndef __BGPSTREAM_FORMAT_INTERFACE_H
#define __BGPSTREAM_FORMAT_INTERFACE_H

#include "bgpstream.h"
#include "bgpstream_format.h" /*< for bgpstream_format_t */
#include "bgpstream_transport.h"
#include "config.h"

/** @file
 *
 * @brief Header file that exposes the protected interface of the data format
 * plugin API
 *
 * @author Alistair King
 *
 */

/** Convenience macro that defines all the function prototypes for the data
 * format API */
#define BS_FORMAT_GENERATE_PROTOS(name)                                        \
  int bs_format_##name##_create(bgpstream_format_t *format,                    \
                                bgpstream_resource_t *res);                    \
  int bs_format_##name##_get_next_record(bgpstream_format_t *format,           \
                                         bgpstream_record_t **record);         \
  void bs_format_##name##_destroy(bgpstream_format_t *format);

#define BS_FORMAT_SET_METHODS(classname, format)                               \
  do {                                                                         \
    (format)->get_next_record = bs_format_##classname##_get_next_record;       \
    (format)->destroy = bs_format_##classname##_destroy;                       \
  } while (0)

/** Structure which represents a data format */
struct bgpstream_format {

  /**
   * @name Format method pointers
   *
   * @{ */

  /**
   * Get the next record from this format instance
   *
   * @param format      pointer to the format object to read from
   * @param[out] record   set to point to a populated record instance
   * @return 1 if a record was read successfully, 0 if there is nothing more to
   * read, -1 if an error occurred.
   */
  int (*get_next_record)(struct bgpstream_format *format,
                         bgpstream_record_t **record);

  /** Destroy the given format module
   *
   * @param format        pointer to the format instance to destroy
   */
  void (*destroy)(struct bgpstream_format *format);

  /** }@ */

  /**
   * @name Data format state fields
   *
   * @{ */

  /** Pointer to the transport instance to read data from */
  bgpstream_transport_t *transport;

  /** An opaque pointer to format-specific state if needed */
  void *state;

  /** }@ */
};

#endif /* __BGPSTREAM_FORMAT_INTERFACE_H */
