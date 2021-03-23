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

#include "config.h"
#include "bs_transport_kafka.h"
#include "bgpstream_transport_interface.h"
#include "bgpstream_log.h"
#include "utils.h"
#include <assert.h>
#include <librdkafka/rdkafka.h>
#include <stdlib.h>
#include <string.h>

#define STATE ((state_t *)(transport->state))

#define POLL_TIMEOUT_MSEC 500

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
    snprintf(buf, sizeof(buf), "bgpstream-%" PRIx64 "-%x", ts, rand());
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
    if ((STATE->offset = strdup(BGPSTREAM_TRANSPORT_KAFKA_DEFAULT_OFFSET)) ==
        NULL) {
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
    transport->res->url, STATE->topic, STATE->group, STATE->offset);
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

  // we don't explicitly handle the error so just log it
  bgpstream_log(BGPSTREAM_LOG_ERR, "%s (%d): %s",
                rd_kafka_err2str(err), err, reason);
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

  // Enable SO_KEEPALIVE in case we're behind a NAT
  if (rd_kafka_conf_set(conf, "socket.keepalive.enable", "true", errstr,
                        sizeof(errstr)) != RD_KAFKA_CONF_OK) {
    bgpstream_log(BGPSTREAM_LOG_ERR, "Config Error: %s", errstr);
    return -1;
  }

  // Try to prevent slow consumers from getting batches that they can't download
  // within the 1 minute that rdkafka will wait.
  if (rd_kafka_conf_set(conf, "fetch.message.max.bytes", "131072", errstr,
                        sizeof(errstr)) != RD_KAFKA_CONF_OK) {
    bgpstream_log(BGPSTREAM_LOG_ERR, "Config Error: %s", errstr);
    return -1;
  }

  // Don't let the broker wait long before giving us data. We want realtime!
  if (rd_kafka_conf_set(conf, "fetch.wait.max.ms", "50", errstr,
                        sizeof(errstr)) != RD_KAFKA_CONF_OK) {
    bgpstream_log(BGPSTREAM_LOG_ERR, "Config Error: %s", errstr);
    return -1;
  }

  // We don't want to use range rebalance strategy since often our
  // topics only have one partition.
  // TODO: use an incremental strategy and allow group.instance.id to be set.
  if (rd_kafka_conf_set(conf, "partition.assignment.strategy", "roundrobin", errstr,
                        sizeof(errstr)) != RD_KAFKA_CONF_OK) {
    bgpstream_log(BGPSTREAM_LOG_ERR, "Config Error: %s", errstr);
    return -1;
  }

#ifdef DEBUG
  if (rd_kafka_conf_set(conf, "debug", "broker", errstr,
                        sizeof(errstr)) != RD_KAFKA_CONF_OK) {
    bgpstream_log(BGPSTREAM_LOG_ERR, "Config Error: %s", errstr);
    return -1;
  }
#endif

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

  bgpstream_log(BGPSTREAM_LOG_FINE, "Subscribing to %d topics", topics_cnt);

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
  if (rd_kafka_brokers_add(STATE->rk, transport->res->url) == 0) {
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
  int rc = -1;

  switch (rk_msg->err) {
  case RD_KAFKA_RESP_ERR__PARTITION_EOF:
    // treat this as EOS so we get re-queued in live mode
    rc = 0;
    break;

  default:
    bgpstream_log(BGPSTREAM_LOG_ERR, "Unhandled Kafka error: %s: %s",
                  rd_kafka_err2str(rk_msg->err), rd_kafka_message_errstr(rk_msg));
  }
  rd_kafka_message_destroy(rk_msg);
  return rc;
}

int64_t bs_transport_kafka_readline(bgpstream_transport_t *transport,
                                    uint8_t *buffer, int64_t len)
{
  int rc = bs_transport_kafka_read(transport, buffer, len - 1);

  if (rc <= 0) {
    return rc;
  }

  buffer[rc] = '\0';
  // NOTE: we assume there is only one line per kafka message
  assert(strchr((char *)buffer, '\n') == NULL);

  return rc;
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
  // if this is really a problem (e.g., batches of MRT/BMP messages
  // produced into a single Kafka message, then we can use a local
  // buffer and split them up for the caller, but really the caller
  // should have a large enough buffer.. so for now:
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
