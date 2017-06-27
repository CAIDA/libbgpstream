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

#include "bgpstream_resource.h"
#include "bgpstream_transport.h"
#include "bgpstream_log.h"
#include "utils.h"

// WITH_TRANSPORT_FILE
#include "bs_transport_file.h"

#ifdef WITH_TRANSPORT_KAFKA
#include "bs_transport_kafka.h"
#endif

/** Convenience typedef for the transport create function type */
typedef int (*transport_create_func_t)(bgpstream_transport_t *transport,
                                       bgpstream_resource_t *res);

/** Array of transport create functions.
 *
 * This MUST be kept in sync with the bgpstream_transport_type_t enum
 */
static const transport_create_func_t create_functions[] = {

  bs_transport_file_create,

#ifdef WITH_TRANSPORT_KAFKA
  bs_transport_kafka_create,
#else
  NULL,
#endif

};

bgpstream_transport_t *bgpstream_transport_create(bgpstream_resource_t *res)
{
  bgpstream_transport_t *transport = NULL;

  // check that the transport type is valid
  if (res->transport_type < 0 ||
      res->transport_type >= ARR_CNT(create_functions)) {
    bgpstream_log(BGPSTREAM_LOG_ERR,
                  "Invalid transport module for %s (ID: %d)",
                  res->uri, res->transport_type);
    goto err;
  }

  // check that the transport is enabled
  if (create_functions[res->transport_type] == NULL) {
    bgpstream_log(BGPSTREAM_LOG_ERR,
                  "Could not find transport module for %s (ID: %d)",
                  res->uri, res->transport_type);
    goto err;
  }

  // create the empty instance object
  if ((transport = malloc_zero(sizeof(bgpstream_transport_t))) == NULL) {
    goto err;
  }

  if (create_functions[res->transport_type](transport, res) != 0) {
    goto err;
  }

  return transport;

 err:
  free(transport);
  return NULL;
}

ssize_t bgpstream_transport_read(bgpstream_transport_t *transport,
                                 uint8_t *buffer, size_t len)
{
  return transport->read(transport, buffer, len);
}

void bgpstream_transport_destroy(bgpstream_transport_t *transport)
{
  if (transport == NULL) {
    return;
  }

  transport->destroy(transport);

  free(transport);
}
