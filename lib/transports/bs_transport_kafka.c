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

#include "bgpstream_transport_interface.h"
#include "bgpstream_log.h"
#include "bs_transport_kafka.h"

int bs_transport_kafka_create(bgpstream_transport_t *transport,
                              bgpstream_resource_t *res)
{
  BS_TRANSPORT_SET_METHODS(kafka, transport);

  return 0;
}

int64_t bs_transport_kafka_read(bgpstream_transport_t *transport,
                                uint8_t *buffer, int64_t len)
{
  return -1;
}

void bs_transport_kafka_destroy(bgpstream_transport_t *transport)
{
  return;
}
