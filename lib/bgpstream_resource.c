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
                          bgpstream_record_dump_type_t record_type)
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

const char * bgpstream_resource_get_hash(bgpstream_resource_t *res)
{
  // name format: PROJECT.COLLECTOR.TYPE.INIT-TIME.DURATION
  char* hash;

  char* project = res->project;
  char* collector = res->collector;
  char* record_type; // 0: update; 1: rib
  char* initial_time; // zero error
  char* duration;// 0: forever

  if(res->record_type == 0){
    record_type = "updates";
  } else if (res->record_type == 1){
    record_type = "rib";
  } else {
    return NULL;
  }

  if((duration = (char*) malloc (sizeof(char) * 16) )==NULL){
    return NULL;
  }
  if((initial_time = (char*) malloc (sizeof(char) * 16) )==NULL){
    return NULL;
  }

  snprintf(duration, 16, "%d", res->duration);
  snprintf(initial_time, 16, "%d", res->initial_time);

  if((hash = (char *) malloc( sizeof( char ) *
                                        (
                                         strlen(project) +
                                         strlen(collector) +
                                         strlen(record_type) +
                                         strlen(initial_time) +
                                         strlen(duration) +
                                         10
                                         )
                                        )) == NULL) {
    return NULL;
  }

  strcat(hash, project);
  strcat(hash, ".");
  strcat(hash, collector);
  strcat(hash, ".");
  strcat(hash, record_type);
  strcat(hash, ".");
  strcat(hash, initial_time);
  strcat(hash, ".");
  strcat(hash, duration);

  return hash;
}
