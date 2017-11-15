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
#include "bs_transport_kafka.h"
#include "utils.h"
#include <assert.h>
#include <librdkafka/rdkafka.h>
#include <string.h>
#include <stdlib.h>

#define STATE ((state_t*)(transport->state))

#define DEFAULT_OFFSET "latest"

#define POLL_TIMEOUT_MSEC 0

typedef struct state {

  // convenience local copies of attrs
  char *topic;
  char *group;
  char *offset;

  // rdkafka instance
  rd_kafka_t *rk;

  // topics
  rd_kafka_topic_partition_list_t *topics;

  // is the client connected?
  int connected;

  // has a fatal error occured?
  int fatal_error;

} state_t;

static int parse_attrs(bgpstream_transport_t *transport)
{
  char buf[1024];
  uint64_t ts;

  // Topic Name (required)
  if (bgpstream_resource_get_attr(
        transport->res, BGPSTREAM_RESOURCE_ATTR_KAFKA_TOPICS) == NULL) {
    bgpstream_log(BGPSTREAM_LOG_ERR,
                  "Kafka transport requires KAFKA_TOPIC attribute to be set");
    return -1;
  } else {
    if ((STATE->topic = strdup(bgpstream_resource_get_attr(
           transport->res, BGPSTREAM_RESOURCE_ATTR_KAFKA_TOPICS))) == NULL) {
      return -1;
    }
  }

  // Group (optional, time+random if not present)
  if (bgpstream_resource_get_attr(
        transport->res, BGPSTREAM_RESOURCE_ATTR_KAFKA_CONSUMER_GROUP) == NULL) {
    // generate a "random" group ID
    ts = epoch_msec();
    srand(ts);
    snprintf(buf, sizeof(buf), "bgpstream-%"PRIx64"-%x", ts, rand());
    if ((STATE->group = strdup(buf)) == NULL) {
      return -1;
    }
  } else {
    if ((STATE->group = strdup(bgpstream_resource_get_attr(
           transport->res, BGPSTREAM_RESOURCE_ATTR_KAFKA_CONSUMER_GROUP))) ==
        NULL) {
      return -1;
    }
  }

  // Offset (optional, default to "latest")
  if (bgpstream_resource_get_attr(
        transport->res, BGPSTREAM_RESOURCE_ATTR_KAFKA_INIT_OFFSET) == NULL) {
    // set to "latest"
    if ((STATE->offset = strdup(DEFAULT_OFFSET)) == NULL) {
      return -1;
    }
  } else {
    // use whatever they provided
    if ((STATE->offset = strdup(bgpstream_resource_get_attr(
           transport->res, BGPSTREAM_RESOURCE_ATTR_KAFKA_INIT_OFFSET))) ==
        NULL) {
      return -1;
    }
  }

  bgpstream_log(
    BGPSTREAM_LOG_FINE,
    "Kafka transport: brokers: '%s', topic: '%s', group: '%s', offset: %s",
    transport->res->uri, STATE->topic, STATE->group, STATE->offset);
  return 0;
}

static void kafka_error_callback(rd_kafka_t *rk, int err, const char *reason,
                                 void *opaque)
{
  bgpstream_transport_t *transport = (bgpstream_transport_t *)opaque;

  switch (err) {
  // fatal errors:
  case RD_KAFKA_RESP_ERR__BAD_COMPRESSION:
  case RD_KAFKA_RESP_ERR__RESOLVE:
    STATE->fatal_error = 1;
  // fall through

  // recoverable? errors:
  case RD_KAFKA_RESP_ERR__DESTROY:
  case RD_KAFKA_RESP_ERR__FAIL:
  case RD_KAFKA_RESP_ERR__TRANSPORT:
  case RD_KAFKA_RESP_ERR__ALL_BROKERS_DOWN:
    STATE->connected = 0;
    break;
  }

  bgpstream_log(BGPSTREAM_LOG_ERR, "ERROR: %s (%d): %s\n",
                rd_kafka_err2str(err), err, reason);

  // TODO: handle other errors
}

static int init_kafka_config(bgpstream_transport_t *transport,
                             rd_kafka_conf_t *conf)
{
  char errstr[512];

  // Set the opaque pointer that will be passed to callbacks
  rd_kafka_conf_set_opaque(conf, transport);

  // Set our error handler
  rd_kafka_conf_set_error_cb(conf, kafka_error_callback);

  // Configure the initial offset
  if (rd_kafka_conf_set(conf, "auto.offset.reset", STATE->offset, errstr,
                        sizeof(errstr)) != RD_KAFKA_CONF_OK) {
    bgpstream_log(BGPSTREAM_LOG_ERR, "Config Error: %s", errstr);
    return -1;
  }

  // Set our group ID
  if (rd_kafka_conf_set(conf, "group.id", STATE->group, errstr,
                        sizeof(errstr)) != RD_KAFKA_CONF_OK) {
    bgpstream_log(BGPSTREAM_LOG_ERR, "Config Error: %s", errstr);
    return -1;
  }

  // Disable logging of connection close/idle timeouts caused by Kafka 0.9.x
  //   See https://github.com/edenhill/librdkafka/issues/437 for more details.
  // TODO: change this when librdkafka has better handling of idle disconnects
  if (rd_kafka_conf_set(conf, "log.connection.close", "false", errstr,
                        sizeof(errstr)) != RD_KAFKA_CONF_OK) {
    bgpstream_log(BGPSTREAM_LOG_ERR, "Config Error: %s", errstr);
    return -1;
  }

  return 0;
}

static int init_topic(bgpstream_transport_t *transport)
{
  rd_kafka_resp_err_t err;
  int topics_cnt = 1;
  char *t, *tok;

  // sigh, first we need to count the topics
  t = STATE->topic;
  while (*t != '\0') {
    if (*(t++) == ',') {
      topics_cnt++;
    }
  }

  bgpstream_log(BGPSTREAM_LOG_FINE, "Subscribing to %d topics",
                topics_cnt);

  if ((STATE->topics = rd_kafka_topic_partition_list_new(topics_cnt)) == NULL) {
    return -1;
  }

  // and now go through and split the string
  t = STATE->topic;
  while ((tok = strsep(&t, ",")) != NULL) {
    bgpstream_log(BGPSTREAM_LOG_FINE, "Subscribing to %s", tok);
    rd_kafka_topic_partition_list_add(STATE->topics, tok, -1);
  }

  if ((err = rd_kafka_subscribe(STATE->rk, STATE->topics)) != 0) {
    bgpstream_log(BGPSTREAM_LOG_ERR, "Could not start topic consumer: %s",
                  rd_kafka_err2str(err));
    return -1;
  }

  return 0;
}

int bs_transport_kafka_create(bgpstream_transport_t *transport)
{
  rd_kafka_conf_t *conf;
  char errstr[512];

  BS_TRANSPORT_SET_METHODS(kafka, transport);

  if ((transport->state = malloc_zero(sizeof(state_t))) == NULL) {
    return -1;
  }

  if (parse_attrs(transport) != 0) {
    return -1;
  }

  // create Kafka config
  if ((conf = rd_kafka_conf_new()) == NULL ||
      init_kafka_config(transport, conf) != 0) {
    bgpstream_log(BGPSTREAM_LOG_ERR, "Could not create Kafka consumer config");
    return -1;
  }

  // create rd kafka instance
  if ((STATE->rk = rd_kafka_new(RD_KAFKA_CONSUMER, conf, errstr,
                                sizeof(errstr))) == NULL) {
    bgpstream_log(BGPSTREAM_LOG_ERR,
                  "Could not create Kafka consumer instance");
    return -1;
  }

  // add kafka brokers
  if (rd_kafka_brokers_add(STATE->rk, transport->res->uri) == 0) {
    bgpstream_log(BGPSTREAM_LOG_ERR, "Could not add Kafka brokers");
    return -1;
  }

  // set up topic subscription
  if (init_topic(transport) != 0) {
    bgpstream_log(BGPSTREAM_LOG_ERR, "Failed to create Kafka topic consumer");
    return -1;
  }

  STATE->connected = 1;

  // poll for connection errors
  rd_kafka_poll(STATE->rk, 5000);

  if (STATE->fatal_error != 0) {
    return -1;
  }

  // switch to consumer poll mode
  rd_kafka_poll_set_consumer(STATE->rk);

  bgpstream_log(BGPSTREAM_LOG_FINE, "Kafka connected!");
  return 0;
}

static int handle_err_msg(bgpstream_transport_t *transport,
                          rd_kafka_message_t *rk_msg)
{
  if (rk_msg->err == RD_KAFKA_RESP_ERR__PARTITION_EOF) {
    rd_kafka_message_destroy(rk_msg);
    return 0; // EOS
  }

  // TODO: handle some more errors
  bgpstream_log(BGPSTREAM_LOG_ERR, "Unhandled Kafka error: %s: %s",
                rd_kafka_err2str(rk_msg->err),
                rd_kafka_message_errstr(rk_msg));

  rd_kafka_message_destroy(rk_msg);
  return -1;
}

int64_t bs_transport_kafka_read(bgpstream_transport_t *transport,
                                uint8_t *buffer, int64_t len)
{
  rd_kafka_message_t *rk_msg;

  // see if there is a message waiting for us
  // POLL_TIMEOUT_MSEC is set very low (0) since the transport should be
  // non-blocking
  if ((rk_msg = rd_kafka_consumer_poll(STATE->rk, POLL_TIMEOUT_MSEC)) == NULL) {
    return 0;
  }
  if (rk_msg->err != 0) {
    return handle_err_msg(transport, rk_msg);
  }

  // is the message too long?
  // TODO: if this is really a problem (e.g., batches of MRT/BMP messages
  // produced into a single Kafka message, then we can use a local buffer and
  // split them up for the caller, but really the caller should have a large
  // enough buffer.. so for now:
  assert(rk_msg->len <= len);

  // copy the message into the provided buffer
  memcpy(buffer, rk_msg->payload, rk_msg->len);
  len = rk_msg->len;
  rd_kafka_message_destroy(rk_msg);

  return len;
}

void bs_transport_kafka_destroy(bgpstream_transport_t *transport)
{
  rd_kafka_resp_err_t err;

  if (transport->state == NULL) {
    return;
  }

  if (STATE->rk != NULL) {
    // TODO: consider committing offsets?

    // shut down consumer
    if ((err = rd_kafka_consumer_close(STATE->rk)) != 0) {
      bgpstream_log(BGPSTREAM_LOG_ERR, "Could not shut down consumer: %s",
                  rd_kafka_err2str(err));
    }

    // destroy topics list
    rd_kafka_topic_partition_list_destroy(STATE->topics);

    // shut down kafka
    rd_kafka_destroy(STATE->rk);
    STATE->rk = NULL;
  }

  free(STATE->topic);
  free(STATE->group);
  free(STATE->offset);

  free(transport->state);
  transport->state = NULL;
}
