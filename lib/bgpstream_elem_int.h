/*
 * libbgpstream
 *
 * Chiara Orsini, CAIDA, UC San Diego
 * chiara@caida.org
 *
 * Copyright (C) 2013 The Regents of the University of California.
 *
 * This file is part of libbgpstream.
 *
 * libbgpstream is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * libbgpstream is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with libbgpstream.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef __BGPSTREAM_ELEM_INT_H
#define __BGPSTREAM_ELEM_INT_H

#include <bgpstream_elem.h>
#include <bgpstream_utils.h>

/** @file
 *
 * @brief Header file that exposes the protected interface of a bgpstream elem.
 *
 * @author Chiara Orsini
 *
 */

/**
 * @name Protected API Functions
 *
 * @{ */

/** Create a new BGP Stream Elem instance
 *
 * @return a pointer to an Elem instance if successful, NULL otherwise
 */
bgpstream_elem_t *bgpstream_elem_create();

/** Destroy the given BGP Stream Elem instance
 *
 * @param elem        pointer to a BGP Stream Elem instance to destroy
 */
void bgpstream_elem_destroy(bgpstream_elem_t *elem);

/** Clear the given BGP Stream Elem instance
 *
 * @param elem        pointer to a BGP Stream Elem instance to clear
 */
void bgpstream_elem_clear(bgpstream_elem_t *elem);

/** @} */

#endif /* __BGPSTREAM_ELEM_INT_H */
