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

#include "bsdi_betabmp.h"
#include "bgpstream_log.h"
#include "config.h"
#include "utils.h"
#include <assert.h>
#include <inttypes.h>
#include <string.h>
#include <wandio.h>

#define STATE (BSDI_GET_STATE(di, betabmp))

#define DEFAULT_BROKERS "bmp.bgpstream.caida.org"
#define DEFAULT_TOPIC "^openbmp\\.router--.+\\.peer-as--.+\\.bmp_raw"
#define DEFAULT_OFFSET "latest"
#define DEFAULT_PROJECT "caida"

// allowed offset types
static char *offset_strs[] = {
  "earliest",     // start from the beginning of the topic
  "latest",       // start from the end of the topic
};

/* ---------- START CLASS DEFINITION ---------- */

/* define the internal option ID values */
enum {
  OPTION_BROKERS,        // stored in res->uri
  OPTION_TOPIC,          // stored in kafka_topic res attribute
  OPTION_CONSUMER_GROUP, // allow multiple BGPStream instances to load-balance
  OPTION_OFFSET,         // begin, end, committed
};

/* define the options this data interface accepts */
static bgpstream_data_interface_option_t options[] = {
  /* Kafka Broker list */
  {
    BGPSTREAM_DATA_INTERFACE_BETABMP, // interface ID
    OPTION_BROKERS,                 // internal ID
    "brokers",                      // name
    "comma-separated list of kafka brokers (default: " DEFAULT_BROKERS ")",
  },
  /* Kafka Topic */
  {
    BGPSTREAM_DATA_INTERFACE_BETABMP, // interface ID
    OPTION_TOPIC,                   // internal ID
    "topic",                        // name
    "topic to consume from (default: " DEFAULT_TOPIC ")",
  },
  /* Kafka Consumer Group */
  {
    BGPSTREAM_DATA_INTERFACE_BETABMP, // interface ID
    OPTION_CONSUMER_GROUP,          // internal ID
    "group",                        // name
    "consumer group name (default: random)",
  },
  /* Initial offset */
  {
    BGPSTREAM_DATA_INTERFACE_KAFKA, // interface ID
    OPTION_OFFSET,             // internal ID
    "offset",                       // name
    "initial offset (earliest/latest) (default: " DEFAULT_OFFSET ")",
  },
};

/* create the class structure for this data interface */
BSDI_CREATE_CLASS_FULL(
  betabmp,
  "beta-bmp-stream",
  BGPSTREAM_DATA_INTERFACE_BETABMP,
  "Read updates in real-time from the public BGPStream BMP feed (BETA)",
  options
  );

/* ---------- END CLASS DEFINITION ---------- */


typedef struct bsdi_betabmp_state {
  /* user-provided options: */

  // Comma-separated list of Kafka brokers
  char *brokers;

  // Topic to consume from
  char *topic_name;

  // Consumer group
  char *group;

  // Offset
  char *offset;

  // we only ever yield one resource
  int done;

} bsdi_betabmp_state_t;

/* ========== PUBLIC METHODS BELOW HERE ========== */

int bsdi_betabmp_init(bsdi_t *di)
{
  bsdi_betabmp_state_t *state;

  if ((state = malloc_zero(sizeof(bsdi_betabmp_state_t))) == NULL) {
    goto err;
  }
  BSDI_SET_STATE(di, state);

  /* set default state */
  state->brokers = strdup(DEFAULT_BROKERS);
  state->topic_name = strdup(DEFAULT_TOPIC);

  return 0;
err:
  bsdi_betabmp_destroy(di);
  return -1;
}

int bsdi_betabmp_start(bsdi_t *di)
{
  // our defaults are sufficient to run
  return 0;
}

int bsdi_betabmp_set_option(bsdi_t *di,
                           const bgpstream_data_interface_option_t *option_type,
                           const char *option_value)
{
  int i;
  int found = 0;

  switch (option_type->id) {
  case OPTION_BROKERS:
    free(STATE->brokers);
    if ((STATE->brokers = strdup(option_value)) == NULL) {
      return -1;
    }
    break;

  case OPTION_TOPIC:
    free(STATE->topic_name);
    if ((STATE->topic_name = strdup(option_value)) == NULL) {
      return -1;
    }
    break;

  case OPTION_CONSUMER_GROUP:
    free(STATE->group);
    if ((STATE->group = strdup(option_value)) == NULL) {
      return -1;
    }
    break;

  case OPTION_OFFSET:
    for (i = 0; i < ARR_CNT(offset_strs); i++) {
      if (strcmp(option_value, offset_strs[i]) == 0) {
        found = 1;
        break;
      }
    }
    if (!found) {
      fprintf(stderr,
              "ERROR: Unknown offset type '%s'. Allowed options are: "
              "earliest/latest\n",
              option_value);
      return -1;
    }
    free(STATE->offset);
    if ((STATE->offset = strdup(option_value)) == NULL) {
      return -1;
    }
    break;

  default:
    return -1;
  }

  return 0;
}

void bsdi_betabmp_destroy(bsdi_t *di)
{
  if (di == NULL || STATE == NULL) {
    return;
  }

  free(STATE->brokers);
  STATE->brokers = NULL;

  free(STATE->topic_name);
  STATE->topic_name = NULL;

  free(STATE->group);
  STATE->group = NULL;

  free(STATE->offset);
  STATE->offset = NULL;

  free(STATE);
  BSDI_SET_STATE(di, NULL);
}

int bsdi_betabmp_update_resources(bsdi_t *di)
{
  int rc;
  bgpstream_resource_t *res = NULL;

  // we only ever yield one resource
  if (STATE->done != 0) {
    return 0;
  }
  STATE->done = 1;

  // TODO: if router or peer filters are set, then replace the topic with one
  // that we construct to filter for the given routers/peers. as usual, if
  // router AND peer is set, it means this peer from this router

  // we treat kafka as having data from <recent> to <forever>
  if ((rc = bgpstream_resource_mgr_push(
         BSDI_GET_RES_MGR(di), BGPSTREAM_RESOURCE_TRANSPORT_KAFKA,
         BGPSTREAM_RESOURCE_FORMAT_BMP, STATE->brokers,
         0, // indicate we don't know how much historical data there is
         BGPSTREAM_FOREVER, // indicate that the resource is a "stream"
         DEFAULT_PROJECT, // fix our project to "caida"
         "", // leave collector unset since we'll get it from openbmp hdrs
         BGPSTREAM_UPDATE, //
         &res)) <= 0) {
    return rc;
  }
  assert(res != NULL);

  if (bgpstream_resource_set_attr(res, BGPSTREAM_RESOURCE_ATTR_KAFKA_TOPIC,
                                  STATE->topic_name) != 0) {
    return -1;
  }

  if (STATE->group != NULL &&
      bgpstream_resource_set_attr(
        res, BGPSTREAM_RESOURCE_ATTR_KAFKA_CONSUMER_GROUP, STATE->group) != 0) {
    return -1;
  }

  if (STATE->offset != NULL &&
      bgpstream_resource_set_attr(
        res, BGPSTREAM_RESOURCE_ATTR_KAFKA_INIT_OFFSET, STATE->offset) != 0) {
    return -1;
  }

  return 0;
}
