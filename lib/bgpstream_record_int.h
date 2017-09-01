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

#ifndef __BGPSTREAM_RECORD_INT_H
#define __BGPSTREAM_RECORD_INT_H

#include "bgpstream_elem.h"
#include "bgpstream_format.h"
#include "bgpstream_record.h"
#include "bgpstream_utils.h"

/** @file
 *
 * @brief Header file that exposes the private interface of a bgpstream record.
 *
 * @author Alistair King
 *
 */

/** @} */

struct bgpstream_record_format_data {

  /** Pointer to the format module that created this data */
  bgpstream_format_t *format;

  /** Private data-structure (optionally) populated by the format module */
  void *data;

};

/**
 * @name Private API Functions
 *
 * @{ */

/** Create a new BGP Stream Record instance for passing to
 * bgpstream_get_next_record.
 *
 * @return a pointer to a Record instance if successful, NULL otherwise
 *
 * A Record may be reused for successive calls to bgpstream_get_next_record if
 * records are processed independently of each other
 */
bgpstream_record_t *bgpstream_record_create(bgpstream_format_t *format);

/** Destroy the given BGP Stream Record instance
 *
 * @param record        pointer to a BGP Stream Record instance to destroy
 */
void bgpstream_record_destroy(bgpstream_record_t *record);

/** Clear the given BGP Stream Record instance
 *
 * @param record        pointer to a BGP Stream Record instance to clear
 *
 * @note the record passed to bgpstream_get_next_record is automatically
 * cleaned.
 */
void bgpstream_record_clear(bgpstream_record_t *record);

/** @} */

#endif /* __BGPSTREAM_RECORD_INT_H */
