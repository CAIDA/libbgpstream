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

#include "bsdi_kafka.h"
#include "bgpstream_log.h"
#include "config.h"
#include "utils.h"
#include <assert.h>
#include <inttypes.h>
#include <string.h>
#include <wandio.h>

#define STATE (BSDI_GET_STATE(di, kafka))

#define DEFAULT_TOPIC "^openbmp\\.router--.+\\.peer-as--.+\\.bmp_raw"
#define DEFAULT_OFFSET "latest"
#define DEFAULT_PROJECT ""
#define DEFAULT_COLLECTOR ""

// mapping from type name to resource format type
static char *type_strs[] = {
  "mrt", // BGPSTREAM_RESOURCE_FORMAT_MRT
  "bmp", // BGPSTREAM_RESOURCE_FORMAT_BMP
};

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
  OPTION_DATA_TYPE,      //
  OPTION_PROJECT,        //
  OPTION_COLLECTOR,      //
  // TODO: read time out (waiting for new data)
};

/* define the options this data interface accepts */
static bgpstream_data_interface_option_t options[] = {
  /* Kafka Broker list */
  {
    BGPSTREAM_DATA_INTERFACE_KAFKA, // interface ID
    OPTION_BROKERS,                 // internal ID
    "brokers",                      // name
    "list of kafka brokers (comma-separated)",
  },
  /* Kafka Topic */
  {
    BGPSTREAM_DATA_INTERFACE_KAFKA, // interface ID
    OPTION_TOPIC,                   // internal ID
    "topic",                        // name
    "topic to consume from (default: " DEFAULT_TOPIC ")",
  },
  /* Kafka Consumer Group */
  {
    BGPSTREAM_DATA_INTERFACE_KAFKA, // interface ID
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
  /* Data type */
  {
    BGPSTREAM_DATA_INTERFACE_SINGLEFILE, // interface ID
    OPTION_DATA_TYPE,                    // internal ID
    "data-type",                         // name
    "data type (mrt/bmp) (default: bmp)",
  },
  /* Project */
  {
    BGPSTREAM_DATA_INTERFACE_SINGLEFILE, // interface ID
    OPTION_PROJECT,                      // internal ID
    "project",                           // name
    "set project name (default: unset)",
  },
  /* Collector */
  {
    BGPSTREAM_DATA_INTERFACE_SINGLEFILE, // interface ID
    OPTION_COLLECTOR,                    // internal ID
    "collector",                         // name
    "set collector name (default: unset)",
  },
};

/* create the class structure for this data interface */
BSDI_CREATE_CLASS(
  kafka,
  BGPSTREAM_DATA_INTERFACE_KAFKA,
  "Read updates in real-time from an Apache Kafka topic",
  options
);

/* ---------- END CLASS DEFINITION ---------- */


typedef struct bsdi_kafka_state {
  /* user-provided options: */

  // Comma-separated list of Kafka brokers
  char *brokers;

  // Topic to consume from
  char *topic_name;

  // Consumer group
  char *group;

  // Offset
  char *offset;

  // explicitly set project name
  char *project;

  // explicitly set collector name
  char *collector;

  // Type of the data to be consumed
  bgpstream_resource_format_type_t data_type;

  // we only ever yield one resource
  int done;

} bsdi_kafka_state_t;

/* ========== PUBLIC METHODS BELOW HERE ========== */

int bsdi_kafka_init(bsdi_t *di)
{
  bsdi_kafka_state_t *state;

  if ((state = malloc_zero(sizeof(bsdi_kafka_state_t))) == NULL) {
    goto err;
  }
  BSDI_SET_STATE(di, state);

  /* set default state */
  state->topic_name = strdup(DEFAULT_TOPIC);
  state->data_type = BGPSTREAM_RESOURCE_FORMAT_BMP;
  state->project = strdup(DEFAULT_PROJECT);
  state->collector = strdup(DEFAULT_COLLECTOR);

  return 0;
err:
  bsdi_kafka_destroy(di);
  return -1;
}

int bsdi_kafka_start(bsdi_t *di)
{
  if (STATE->brokers != NULL) {
    return 0;
  } else {
    fprintf(
      stderr,
      "ERROR: The kafka data interface requires the 'brokers' option be set\n");
    return -1;
  }
}

int bsdi_kafka_set_option(bsdi_t *di,
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

  case OPTION_DATA_TYPE:
    for (i = 0; i < ARR_CNT(type_strs); i++) {
      if (strcmp(option_value, type_strs[i]) == 0) {
        STATE->data_type = i;
        found = 1;
        break;
      }
    }
    if (!found) {
      fprintf(stderr, "ERROR: Unknown data type '%s'\n", option_value);
      return -1;
    }
    break;

  case OPTION_PROJECT:
    free(STATE->project);
    if ((STATE->project = strdup(option_value)) == NULL) {
      return -1;
    }
    break;

  case OPTION_COLLECTOR:
    free(STATE->collector);
    if ((STATE->collector = strdup(option_value)) == NULL) {
      return -1;
    }
    break;

  default:
    return -1;
  }

  return 0;
}

void bsdi_kafka_destroy(bsdi_t *di)
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

  free(STATE->project);
  STATE->project = NULL;

  free(STATE->collector);
  STATE->collector = NULL;

  free(STATE);
  BSDI_SET_STATE(di, NULL);
}

int bsdi_kafka_update_resources(bsdi_t *di)
{
  int rc;
  bgpstream_resource_t *res = NULL;

  // we only ever yield one resource
  if (STATE->done != 0) {
    return 0;
  }
  STATE->done = 1;

  // we treat kafka as having data from <recent> to <forever>
  if ((rc = bgpstream_resource_mgr_push(
         BSDI_GET_RES_MGR(di), BGPSTREAM_RESOURCE_TRANSPORT_KAFKA,
         STATE->data_type, STATE->brokers,
         0, // indicate we don't know how much historical data there is
         BGPSTREAM_FOREVER, // indicate that the resource is a "stream"
         STATE->project, STATE->collector, BGPSTREAM_UPDATE, &res)) <= 0) {
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
