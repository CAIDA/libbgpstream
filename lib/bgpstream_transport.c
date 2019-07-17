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

#include "bgpstream_transport.h"
#include "bgpstream_log.h"
#include "bgpstream_resource.h"
#include "utils.h"

// WITH_TRANSPORT_FILE
#include "bs_transport_cache.h"
#include "bs_transport_file.h"
#include "bs_transport_http.h"

#ifdef WITH_TRANSPORT_KAFKA
#include "bs_transport_kafka.h"
#endif

/** Convenience typedef for the transport create function type */
typedef int (*transport_create_func_t)(bgpstream_transport_t *transport);

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

  bs_transport_cache_create,

  bs_transport_http_create,
};

bgpstream_transport_t *bgpstream_transport_create(bgpstream_resource_t *res)
{
  bgpstream_transport_t *transport = NULL;

  // check that the transport type is valid
  // Note: the (int) cast is here to suppress a bogus warning from
  // -Wtautological-constant-out-of-range-compare in some versions of clang
  if ((int)res->transport_type >= ARR_CNT(create_functions)) {
    bgpstream_log(BGPSTREAM_LOG_ERR, "Invalid transport module for %s (ID: %d)",
                  res->uri, res->transport_type);
    goto err;
  }

  // check that the transport is enabled
  if (create_functions[res->transport_type] == NULL) {
    bgpstream_log(BGPSTREAM_LOG_ERR,
                  "Could not find transport module for %s (ID: %d)", res->uri,
                  res->transport_type);
    goto err;
  }

  // create the empty instance object
  if ((transport = malloc_zero(sizeof(bgpstream_transport_t))) == NULL) {
    goto err;
  }

  // store a pointer to the resource
  transport->res = res;

  if (create_functions[res->transport_type](transport) != 0) {
    bgpstream_log(BGPSTREAM_LOG_ERR, "Could not open resource (%s)", res->uri);
    goto err;
  }

  return transport;

err:
  free(transport);
  return NULL;
}

int64_t bgpstream_transport_read(bgpstream_transport_t *transport, void *buffer,
                                 int64_t len)
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

int64_t bgpstream_transport_readline(bgpstream_transport_t *transport,
                                     void *buffer, int64_t len)
{
  return transport->readline(transport, buffer, len);
}
