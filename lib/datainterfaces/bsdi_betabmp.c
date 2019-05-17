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
#define DEFAULT_OFFSET "latest"
#define DEFAULT_PROJECT "caida"

#define TOPIC_PATTERN "^openbmp\\.router--%s\\.peer-as--%s\\.bmp_raw"
#define ALL_ROUTERS ".+"
#define ALL_PEERS ".+"

// allowed offset types
static const char *offset_strs[] = {
  "earliest", // start from the beginning of the topic
  "latest",   // start from the end of the topic
};

/* ---------- START CLASS DEFINITION ---------- */

/* define the internal option ID values */
enum {
  OPTION_BROKERS,        // stored in res->uri
  OPTION_CONSUMER_GROUP, // allow multiple BGPStream instances to load-balance
  OPTION_OFFSET,         // earliest, latest
};

/* define the options this data interface accepts */
static bgpstream_data_interface_option_t options[] = {
  /* Kafka Broker list */
  {
    BGPSTREAM_DATA_INTERFACE_BETABMP, // interface ID
    OPTION_BROKERS,                   // internal ID
    "brokers",                        // name
    "comma-separated list of kafka brokers (default: " DEFAULT_BROKERS ")",
  },
  /* Kafka Consumer Group */
  {
    BGPSTREAM_DATA_INTERFACE_BETABMP, // interface ID
    OPTION_CONSUMER_GROUP,            // internal ID
    "group",                          // name
    "consumer group name (default: random)",
  },
  /* Initial offset */
  {
    BGPSTREAM_DATA_INTERFACE_BETABMP, // interface ID
    OPTION_OFFSET,                    // internal ID
    "offset",                         // name
    "initial offset (earliest/latest) (default: " DEFAULT_OFFSET ")",
  },
};

/* create the class structure for this data interface */
BSDI_CREATE_CLASS_FULL(
  betabmp, "beta-bmp-stream", BGPSTREAM_DATA_INTERFACE_BETABMP,
  "Read updates in real-time from the public BGPStream BMP feed (BETA)",
  options)

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

/* ========== PRIVATE METHODS BELOW HERE ========== */

static int append_topic(char **list, const char *router, uint32_t *peer_asn)
{
  char buf[256]; // temp buffer for this topic
  char as_buf[12];
  const char *peer_str = ALL_PEERS;
  int new_len = 0;
  int need_comma = (*list != NULL);

  if (router == NULL) {
    router = ALL_ROUTERS;
  }

  if (peer_asn != NULL) {
    // build the string representation of the peer AS
    if (snprintf(as_buf, sizeof(as_buf), "%" PRIu32, *peer_asn) >=
        sizeof(as_buf)) {
      return -1;
    }
    peer_str = as_buf;
  }

  // build the string for this topic
  if (snprintf(buf, sizeof(buf), TOPIC_PATTERN, router, peer_str) >=
      sizeof(buf)) {
    return -1;
  }

  // and append it to the list
  if (need_comma) {
    new_len = strlen(*list) + need_comma;
  }
  new_len += strlen(buf) + 1;

  if ((*list = realloc(*list, new_len)) == NULL) {
    return -1;
  }

  if (need_comma) {
    strcat(*list, ",");
  } else {
    *list[0] = '\0';
  }

  strcat(*list, buf);

  return new_len;
}

static int build_topic_list_peers(bsdi_t *di, char **list, const char *router)
{
  bgpstream_filter_mgr_t *filter_mgr = BSDI_GET_FILTER_MGR(di);
  uint32_t *peer_asn = NULL;

  if (filter_mgr->peer_asns == NULL) {
    return append_topic(list, router, NULL);
  }

  bgpstream_id_set_rewind(filter_mgr->peer_asns);
  while ((peer_asn = bgpstream_id_set_next(filter_mgr->peer_asns)) != NULL) {
    if (append_topic(list, router, peer_asn) < 0) {
      return -1;
    }
  }

  return 0;
}

static char *build_topic_list(bsdi_t *di)
{
  bgpstream_filter_mgr_t *filter_mgr = BSDI_GET_FILTER_MGR(di);
  char *topic_list = NULL;
  char *router = NULL;

  // we need r * p topics, one for each router/peer combination
  if (filter_mgr->routers == NULL) {
    if (build_topic_list_peers(di, &topic_list, ALL_ROUTERS) < 0) {
      goto err;
    }
  } else {
    bgpstream_str_set_rewind(filter_mgr->routers);
    while ((router = bgpstream_str_set_next(filter_mgr->routers)) != NULL) {
      if (build_topic_list_peers(di, &topic_list, router) < 0) {
        goto err;
      }
    }
  }

  return topic_list;

err:
  free(topic_list);
  return NULL;
}

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
  // can't build topic list now since filters aren't yet set

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

int bsdi_betabmp_set_option(
  bsdi_t *di, const bgpstream_data_interface_option_t *option_type,
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
      bgpstream_log(BGPSTREAM_LOG_ERR,
                    "Unknown offset type '%s'. Allowed options are: "
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

  if ((STATE->topic_name = build_topic_list(di)) == NULL) {
    bgpstream_log(BGPSTREAM_LOG_ERR,
                  "Could not build topic list. Check filters.");
    return -1;
  }

  // we treat kafka as having data from <recent> to <forever>
  if ((rc = bgpstream_resource_mgr_push(
         BSDI_GET_RES_MGR(di), BGPSTREAM_RESOURCE_TRANSPORT_KAFKA,
         BGPSTREAM_RESOURCE_FORMAT_BMP, STATE->brokers,
         0, // indicate we don't know how much historical data there is
         BGPSTREAM_FOREVER, // indicate that the resource is a "stream"
         DEFAULT_PROJECT,   // fix our project to "caida"
         "", // leave collector unset since we'll get it from openbmp hdrs
         BGPSTREAM_UPDATE, //
         &res)) <= 0) {
    return rc;
  }
  assert(res != NULL);

  if (bgpstream_resource_set_attr(res, BGPSTREAM_RESOURCE_ATTR_KAFKA_TOPICS,
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
