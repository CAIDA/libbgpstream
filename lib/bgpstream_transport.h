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

#ifndef __BGPSTREAM_TRANSPORT_H
#define __BGPSTREAM_TRANSPORT_H

#include "bgpstream_resource.h"


/** Generic interface to specific data transport modules */
typedef struct bgpstream_transport bgpstream_transport_t;


/** Create a transport handler for the given resource
 *
 * @param res           pointer to a resource
 * @return pointer to a transport module instance if successful, NULL otherwise
 */
bgpstream_transport_t *
bgpstream_transport_create(bgpstream_resource_t *res);

/** Read from the given transport handler
 *
 * @param transport     pointer to a transport handler to read from
 * @return the number of bytes read if successful, -1 otherwise
 */
int64_t bgpstream_transport_read(bgpstream_transport_t *transport,
                                 void *buffer, int64_t len);

/** Read one line from the given transport handler
 *
 * @param transport     pointer to a transport handler to read from
 * @return the number of bytes read if successful, -1 otherwise
 */
int64_t bgpstream_transport_readline(bgpstream_transport_t *transport,
                                 void *buffer, int64_t len);

/** Shutdown and destroy the given transport handler
 *
 * @param transport     pointer to a transport handler to destroy
 */
void bgpstream_transport_destroy(bgpstream_transport_t *transport);

#endif /* __BGPSTREAM_TRANSPORT_H */
