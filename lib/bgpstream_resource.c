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

#include "config.h"
#include "utils.h"
#include "bgpstream_resource.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

/** A Type/Value structure for extra information about a specific resource */
typedef struct attr {

  /** The type of this attribute */
  bgpstream_resource_attr_type_t type;

  /** The value of this attribute */
  char *value;

} attr_t;

/* ========== PUBLIC FUNCTIONS BELOW HERE ========== */

bgpstream_resource_t *
bgpstream_resource_create(bgpstream_resource_transport_type_t transport_type,
                          bgpstream_resource_format_type_t format_type,
                          const char *uri,
                          uint32_t initial_time,
                          uint32_t duration,
                          const char *project, const char *collector,
                          bgpstream_record_type_t record_type)
{
  bgpstream_resource_t *res;

  if ((res = malloc_zero(sizeof(bgpstream_resource_t))) == NULL) {
    return NULL;
  }

  res->transport_type = transport_type;
  res->format_type = format_type;
  if ((res->uri = strdup(uri)) == NULL) {
    goto err;
  }
  res->initial_time = initial_time;
  res->duration = duration;
  if ((res->project = strdup(project)) == NULL) {
    goto err;
  }
  if ((res->collector = strdup(collector)) == NULL) {
    goto err;
  }
  res->record_type = record_type;

  return res;

 err:
  bgpstream_resource_destroy(res);
  return NULL;
}

void bgpstream_resource_destroy(bgpstream_resource_t *resource)
{
  int i;

  free(resource->uri);
  resource->uri = NULL;

  free(resource->project);
  resource->project = NULL;

  free(resource->collector);
  resource->collector = NULL;

  for (i = 0; i < _BGPSTREAM_RESOURCE_ATTR_CNT; i++) {
    if (resource->attrs[i] == NULL) {
      continue;
    }
    free(resource->attrs[i]->value);
    resource->attrs[i]->value = NULL;
    free(resource->attrs[i]);
    resource->attrs[i] = NULL;
  }

  free(resource);
}

int bgpstream_resource_set_attr(bgpstream_resource_t *resource,
                                bgpstream_resource_attr_type_t type,
                                const char *value)
{
  assert(type >= 0 && type < _BGPSTREAM_RESOURCE_ATTR_CNT);
  if (resource->attrs[type] == NULL &&
      (resource->attrs[type] = malloc_zero(sizeof(attr_t))) == NULL) {
    return -1;
  }
  resource->attrs[type]->type = type;
  free(resource->attrs[type]->value);
  if ((resource->attrs[type]->value = strdup(value)) == NULL) {
    return -1;
  }

  return 0;
}

const char *
bgpstream_resource_get_attr(bgpstream_resource_t *resource,
                            bgpstream_resource_attr_type_t type)
{
  assert(type >= 0 && type < _BGPSTREAM_RESOURCE_ATTR_CNT);
  if (resource->attrs[type] != NULL) {
    return resource->attrs[type]->value;
  } else {
    return NULL;
  }
}

int bgpstream_resource_hash_snprintf(char* buf, size_t buf_len, bgpstream_resource_t *res)
{
  return snprintf(buf, buf_len,
                  "%s.%s.%s.%"PRIu32".%"PRIu32,
                  res->project, res->collector,
                  res->record_type == BGPSTREAM_RIB ? "ribs" : "updates",
                  res->initial_time, res->duration);
}
