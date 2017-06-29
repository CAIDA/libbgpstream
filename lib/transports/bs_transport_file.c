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
#include "bs_transport_file.h"
#include "wandio.h"

int bs_transport_file_create(bgpstream_transport_t *transport,
                             bgpstream_resource_t *res)
{
  io_t *fh = NULL;

  BS_TRANSPORT_SET_METHODS(file, transport);

  if ((fh = wandio_create(res->uri)) == NULL) {
    bgpstream_log(BGPSTREAM_LOG_ERR, "Could not open %s for reading", res->uri);
    return -1;
  }

  transport->state = fh;

  return 0;
}

int64_t bs_transport_file_read(bgpstream_transport_t *transport,
                               uint8_t *buffer, int64_t len)
{
  return wandio_read((io_t*)transport->state, buffer, len);
}

void bs_transport_file_destroy(bgpstream_transport_t *transport)
{
  if (transport->state != NULL) {
    wandio_destroy((io_t*)transport->state);
    transport->state = NULL;
  }
}
