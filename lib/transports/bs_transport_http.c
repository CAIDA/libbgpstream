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
 */

#include "bs_transport_http.h"
#include "bgpstream_transport_interface.h"
#include "bgpstream_log.h"
#include "wandio.h"
#include "config.h"
#include <string.h>
#include <assert.h>

// https://developer.mozilla.org/en-US/docs/Web/HTTP/Headers/User-Agent
static char http_user_agent_hdr[] = "User-Agent: libbgpstream/"PACKAGE_VERSION;

int bs_transport_http_create(bgpstream_transport_t *transport)
{
  io_t *fh = NULL;
  char *http_hdr = http_user_agent_hdr;

  BS_TRANSPORT_SET_METHODS(http, transport);

  assert(strncmp(transport->res->uri, "http", 4) == 0);

  if ((fh = http_open_hdrs(transport->res->uri, &http_hdr, 1)) == NULL) {
    bgpstream_log(BGPSTREAM_LOG_ERR, "Could not open %s for reading",
                  transport->res->uri);
    return -1;
  }

  transport->state = fh;

  return 0;
}

int64_t bs_transport_http_read(bgpstream_transport_t *transport,
                               uint8_t *buffer, int64_t len)
{
  return wandio_read((io_t *)transport->state, buffer, len);
}

int64_t bs_transport_http_readline(bgpstream_transport_t *transport,
                                   uint8_t *buffer, int64_t len)
{
  return wandio_fgets((io_t *)transport->state, buffer, len, 1);
}

void bs_transport_http_destroy(bgpstream_transport_t *transport)
{
  if (transport->state != NULL) {
    wandio_destroy((io_t *)transport->state);
    transport->state = NULL;
  }
}
