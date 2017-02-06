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

#ifndef __BGPSTREAM_TRANSPORT_H
#define __BGPSTREAM_TRANSPORT_H

/** Types of transport supported */
typedef enum {

  /** Data is in a file (either local or via HTTP) */
  BGPSTREAM_TRANSPORT_FILE,

  /** Data is served from a Kafka queue */
  //  BGPSTREAM_TRANSPORT_KAFKA,

  /** Data is streamed via websockets */
  //  BGPSTREAM_TRANSPORT_WEBSOCKET,

} bgpstream_transport_type_t;

#endif /* __BGPSTREAM_TRANSPORT_H */
