/*
 * Copyright (C) 2015 The Regents of the University of California.
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

#include "bsdi_broker.h"
#include "bgpstream_log.h"
#include "config.h"
#include "utils.h"
#include "jsmn_utils.h"
#include "libjsmn/jsmn.h"
#include <assert.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <wandio.h>

#if WITH_KAFKA
#include "transports/bs_transport_kafka.h"
#endif

#define STATE (BSDI_GET_STATE(di, broker))
#define TIF filter_mgr->time_interval

/* ---------- START CLASS DEFINITION ---------- */

/* define the internal option ID values */
enum {
  OPTION_BROKER_URL,
  OPTION_PARAM,
  OPTION_CACHE_DIR,
#if WITH_KAFKA
  OPTION_KAFKA_GROUP,
  OPTION_KAFKA_OFFSET,
#endif
};

/* define the options this data interface accepts */
static bgpstream_data_interface_option_t options[] = {
  /* Broker URL */
  {
    BGPSTREAM_DATA_INTERFACE_BROKER,                          // interface ID
    OPTION_BROKER_URL,                                        // internal ID
    "url",                                                    // name
    "Broker URL (default: " STR(BGPSTREAM_DI_BROKER_URL) ")", // description
  },
  /* Broker Param */
  {
    BGPSTREAM_DATA_INTERFACE_BROKER,    // interface ID
    OPTION_PARAM,                       // internal ID
    "param",                            // name
    "Additional Broker GET parameter*", // description
  },
  /* Broker Cache */
  {
    BGPSTREAM_DATA_INTERFACE_BROKER,             // interface ID
    OPTION_CACHE_DIR,                            // internal ID
    "cache-dir",                                 // name
    "Enable local cache at provided directory.", // description
  },
#if WITH_KAFKA
  /* Kafka group */
  {
    BGPSTREAM_DATA_INTERFACE_BROKER,             // interface ID
    OPTION_KAFKA_GROUP,                          // internal ID
    "kafka-group",                               // name
    "Override kafka group setting (for kafka-based resources).", // description
  },
  /* Kafka offset */
  {
    BGPSTREAM_DATA_INTERFACE_BROKER,             // interface ID
    OPTION_KAFKA_OFFSET,                         // internal ID
    "kafka-offset",                              // name
    "Override kafka offset (default: " STR(BGPSTREAM_TRANSPORT_KAFKA_DEFAULT_OFFSET) ").", // description
  },
#endif
};

/* create the class structure for this data interface */
BSDI_CREATE_CLASS(
  broker, BGPSTREAM_DATA_INTERFACE_BROKER,
  "Retrieve metadata information from the BGPStream Broker service", options)

/* ---------- END CLASS DEFINITION ---------- */

/* The maximum number of parameters we let users set (just to simplify memory
   management */
#define MAX_PARAMS 100

/* The length of the URL buffer (we can't build broker query URLs longer than
   this) */
#define URL_BUFLEN 4096

#define BROKER_VERSION 2

typedef struct bsdi_broker_state {

  /* user-provided options: */

  // Base URL of the Broker web service
  char *broker_url;

  // User-provided parameters
  char *params[MAX_PARAMS];

  // Number of parameters in the params array
  int params_cnt;

  // User-specified location for cache: NULL means cache disabled
  char *cache_dir;

#if WITH_KAFKA
  // Kafka group name
  char *kafka_group;

  // Kafka default offset
  char *kafka_offset;
#endif

  /* internal state: */

  // working space to build query urls
  char query_url_buf[URL_BUFLEN];

  size_t query_url_remaining;

  // pointer to the end of the common query url (for appending last ts info)
  char *query_url_end;

  // have any parameters been added to the url?
  int first_param;

  // time of the last response we got from the broker
  uint32_t last_response_time;

  // the max (file_time + duration) that we have seen
  uint32_t current_window_end;

} bsdi_broker_state_t;

// the max time we will wait between retries to the broker
#define MAX_WAIT_TIME 900

enum {
  ERR_FATAL = -1,
  ERR_RETRY = -2,
};

#define APPEND_STR(str)                                                        \
  do {                                                                         \
    size_t len = strlen(str);                                                  \
    if (STATE->query_url_remaining < len + 1) {                                \
      goto err;                                                                \
    }                                                                          \
    strncat(STATE->query_url_buf, str, STATE->query_url_remaining - 1);        \
    STATE->query_url_remaining -= len;                                         \
  } while (0)

#define AMPORQ                                                                 \
  do {                                                                         \
    if (STATE->first_param) {                                                  \
      APPEND_STR("?");                                                         \
      STATE->first_param = 0;                                                  \
    } else {                                                                   \
      APPEND_STR("&");                                                         \
    }                                                                          \
  } while (0)

// NB: this ONLY replaces \<char> with <char>
static void unescape_char(char *url, char c)
{
  char *p = url;

  while (*p != '\0') {
    if (*p == '\\' && *(p + 1) == c) {
      // copy the remainder of the string backward (ugh)
      memmove(p, p + 1, strlen(p + 1) + 1);
    }
    p++;
  }
}

#define NEXT_TOK t++

static int process_json(bsdi_t *di, const char *js, jsmntok_t *root_tok,
                        size_t count)
{
  int i, j, k, l, m;
  jsmntok_t *t = root_tok + 1;

  int arr_len, obj_len, attr_len;

  int time_set = 0;

  // per-file info
  char *url = NULL;
  size_t url_len = 0;
  int url_set = 0;
  char collector[BGPSTREAM_UTILS_STR_NAME_LEN] = "";
  int collector_set = 0;
  char project[BGPSTREAM_UTILS_STR_NAME_LEN] = "";
  int project_set = 0;
  bgpstream_record_type_t type = 0;
  int type_set = 0;
  bgpstream_resource_transport_type_t transport_type = 0;
  int transport_type_set = 0;
  bgpstream_resource_format_type_t format_type = 0;
  int format_type_set = 0;
  unsigned long initial_time = 0;
  int initial_time_set = 0;
  unsigned long duration = 0;
  unsigned long version = 0;
  int duration_set = 0;
  char *kafka_topic = NULL;
  size_t topic_len = 0;

  // local cache related variables.
  bgpstream_resource_t *res = NULL;

  if (count == 0) {
    bgpstream_log(BGPSTREAM_LOG_ERR, "Empty JSON response from broker");
    goto retry;
  }

  if (root_tok->type != JSMN_OBJECT) {
    bgpstream_log(BGPSTREAM_LOG_ERR, "Root object is not JSON");
    bgpstream_log(BGPSTREAM_LOG_ERR, "JSON: %s", js);
    goto err;
  }

  // iterate over the children of the root object
  for (i = 0; i < root_tok->size; i++) {
    // all keys must be strings
    if (t->type != JSMN_STRING) {
      bgpstream_log(BGPSTREAM_LOG_ERR, "Encountered non-string key: '%.*s'",
                    t->end - t->start, js + t->start);
      goto err;
    }
    if (jsmn_streq(js, t, "version") == 1) {
      NEXT_TOK;
      jsmn_strtoul(&version, js, t);
      if(version != BROKER_VERSION){
        bgpstream_log(BGPSTREAM_LOG_ERR, "Broker version does not match: wanted=%d received=%.*s",
                      BROKER_VERSION, t->end - t->start, js + t->start);
        goto err;
      }
      NEXT_TOK;
    } else if (jsmn_streq(js, t, "time") == 1) {
      NEXT_TOK;
      jsmn_type_assert(t, JSMN_PRIMITIVE);
      unsigned long tmp = 0;
      jsmn_strtoul(&tmp, js, t);
      STATE->last_response_time = (uint32_t)tmp;
      time_set = 1;
      NEXT_TOK;
    } else if (jsmn_streq(js, t, "type") == 1) {
      NEXT_TOK;
      jsmn_str_assert(js, t, "data");
      NEXT_TOK;
    } else if (jsmn_streq(js, t, "error") == 1) {
      NEXT_TOK;
      if (jsmn_isnull(js, t) == 0) { // i.e. there is an error set
        bgpstream_log(BGPSTREAM_LOG_ERR, "Broker reported an error: %.*s",
                      t->end - t->start, js + t->start);
        goto err;
      }
      NEXT_TOK;
    } else if (jsmn_streq(js, t, "queryParameters") == 1) {
      NEXT_TOK;
      jsmn_type_assert(t, JSMN_OBJECT);
      // skip over this object
      t = jsmn_skip(t);
    } else if (jsmn_streq(js, t, "data") == 1) {
      NEXT_TOK;
      jsmn_type_assert(t, JSMN_OBJECT);

      int data_type_len;
      data_type_len = t->size;
      NEXT_TOK;
      for (l = 0; l < data_type_len; l++) {
        jsmn_str_assert(js, t, "resources");
        NEXT_TOK;
        jsmn_type_assert(t, JSMN_ARRAY);
        arr_len = t->size; // number of dump files
        NEXT_TOK;          // first elem in array
        for (j = 0; j < arr_len; j++) {
          jsmn_type_assert(t, JSMN_OBJECT);
          obj_len = t->size;
          NEXT_TOK;

          url_set = 0;
          project_set = 0;
          collector_set = 0;
          type_set = 0;
          initial_time_set = 0;
          duration_set = 0;

          for (k = 0; k < obj_len; k++) {
            if (jsmn_streq(js, t, "url") == 1) {
              NEXT_TOK;
              jsmn_type_assert(t, JSMN_STRING);
              if (url_len < (t->end - t->start + 1)) {
                url_len = t->end - t->start + 1;
                if ((url = realloc(url, url_len)) == NULL) {
                  bgpstream_log(BGPSTREAM_LOG_ERR,
                                "Could not realloc URL string");
                  goto err;
                }
              }
              jsmn_strcpy(url, t, js);
              unescape_char(url,'/');
              url_set = 1;
              NEXT_TOK;
            } else if (jsmn_streq(js, t, "project") == 1) {
              NEXT_TOK;
              jsmn_type_assert(t, JSMN_STRING);
              jsmn_strcpy(project, t, js);
              project_set = 1;
              NEXT_TOK;
            } else if (jsmn_streq(js, t, "collector") == 1) {
              NEXT_TOK;
              jsmn_type_assert(t, JSMN_STRING);
              jsmn_strcpy(collector, t, js);
              collector_set = 1;
              NEXT_TOK;
            } else if (jsmn_streq(js, t, "type") == 1) {
              NEXT_TOK;
              jsmn_type_assert(t, JSMN_STRING);
              if (jsmn_streq(js, t, "ribs") == 1) {
                type = BGPSTREAM_RIB;
              } else if (jsmn_streq(js, t, "updates") == 1) {
                type = BGPSTREAM_UPDATE;
              } else {
                bgpstream_log(BGPSTREAM_LOG_ERR, "Invalid type '%.*s'",
                              t->end - t->start, js + t->start);
                goto err;
              }
              type_set = 1;
              NEXT_TOK;
            } else if (jsmn_streq(js, t, "initialTime") == 1) {
              NEXT_TOK;
              jsmn_type_assert(t, JSMN_PRIMITIVE);
              jsmn_strtoul(&initial_time, js, t);
              initial_time_set = 1;
              NEXT_TOK;
            } else if (jsmn_streq(js, t, "duration") == 1) {
              NEXT_TOK;
              jsmn_type_assert(t, JSMN_PRIMITIVE);
              jsmn_strtoul(&duration, js, t);
              duration_set = 1;
              NEXT_TOK;
            } else if (jsmn_streq(js, t, "transport") == 1) {
              NEXT_TOK;
              jsmn_type_assert(t, JSMN_STRING);
              if (jsmn_streq(js, t, "file") == 1) {
                transport_type = BGPSTREAM_RESOURCE_TRANSPORT_FILE;
              } else if (jsmn_streq(js, t, "http") == 1) {
                transport_type = BGPSTREAM_RESOURCE_TRANSPORT_HTTP;
              } else if (jsmn_streq(js, t, "kafka") == 1) {
                transport_type = BGPSTREAM_RESOURCE_TRANSPORT_KAFKA;
              } else {
                bgpstream_log(BGPSTREAM_LOG_ERR, "Invalid transport type '%.*s'",
                              t->end - t->start, js + t->start);
                goto err;
              }
              transport_type_set = 1;
              NEXT_TOK;
            } else if (jsmn_streq(js, t, "format") == 1) {
              NEXT_TOK;
              jsmn_type_assert(t, JSMN_STRING);
              if (jsmn_streq(js, t, "mrt") == 1) {
                format_type = BGPSTREAM_RESOURCE_FORMAT_MRT;
              } else if (jsmn_streq(js, t, "ris-live") == 1) {
                format_type = BGPSTREAM_RESOURCE_FORMAT_RISLIVE;
              } else if (jsmn_streq(js, t, "bmp") == 1) {
                format_type = BGPSTREAM_RESOURCE_FORMAT_BMP;
              } else {
                bgpstream_log(BGPSTREAM_LOG_ERR, "Invalid format type '%.*s'",
                              t->end - t->start, js + t->start);
                goto err;
              }
              format_type_set = 1;
              NEXT_TOK;
            } else if (jsmn_streq(js, t, "attr") == 1) {
              NEXT_TOK;
              attr_len = t->size;
              NEXT_TOK;
              for (m = 0; m < attr_len; m++) {
                if (jsmn_streq(js, t, "kafka-topics") == 1) {
                  NEXT_TOK;
                  jsmn_type_assert(t, JSMN_STRING);
                  if (topic_len < (t->end - t->start + 1)) {
                    topic_len = t->end - t->start + 1;
                    if ((kafka_topic = realloc(kafka_topic, topic_len)) == NULL) {
                      bgpstream_log(BGPSTREAM_LOG_ERR,
                                    "Could not realloc kafka topic string");
                      goto err;
                    }
                  }
                  jsmn_strcpy(kafka_topic, t, js);
                  unescape_char(kafka_topic, '\\');
                  url_set = 1;
                  NEXT_TOK;
                } else {
                  bgpstream_log(BGPSTREAM_LOG_ERR, "Unknown field '%.*s'",
                                t->end - t->start, js + t->start);
                  goto err;
                }
              }
            } else {
              bgpstream_log(BGPSTREAM_LOG_ERR, "Unknown field '%.*s'",
                            t->end - t->start, js + t->start);
              goto err;
            }
          }

#ifdef BROKER_DEBUG
          bgpstream_log(BGPSTREAM_LOG_INFO, "----------");
          bgpstream_log(BGPSTREAM_LOG_INFO, "Transport Type: %d", transport_type);
          bgpstream_log(BGPSTREAM_LOG_INFO, "Format Type: %d", format_type);
          bgpstream_log(BGPSTREAM_LOG_INFO, "URL: %s", url);
          bgpstream_log(BGPSTREAM_LOG_INFO, "Project: %s", project);
          bgpstream_log(BGPSTREAM_LOG_INFO, "Collector: %s", collector);
          bgpstream_log(BGPSTREAM_LOG_INFO, "Type: %d", type);
          bgpstream_log(BGPSTREAM_LOG_INFO, "InitialTime: %lu", initial_time);
          bgpstream_log(BGPSTREAM_LOG_INFO, "Duration: %lu", duration);
#ifdef WITH_KAFKA
          if (transport_type == BGPSTREAM_RESOURCE_TRANSPORT_KAFKA && kafka_topic != NULL){
            bgpstream_log(BGPSTREAM_LOG_INFO, "Kafka topic: %s", kafka_topic);
          }
#endif
#endif
          if (url_set == 0 || project_set == 0 || collector_set == 0 ||
              type_set == 0 || initial_time_set == 0 || duration_set == 0 ||
              format_type_set == 0 || transport_type_set == 0) {
            bgpstream_log(BGPSTREAM_LOG_ERR, "Invalid resource record");
            goto retry;
          }


#ifndef WITH_KAFKA
          // we are built without kafka support, so ignore kafka resources
          if (transport_type == BGPSTREAM_RESOURCE_TRANSPORT_KAFKA) {
            bgpstream_log(
              BGPSTREAM_LOG_WARN,
              "Skipping unsuported kafka-based resource (rebuild libbgpstream "
              "with kafka support to handle this resource)");
            continue;
          }
#endif

          // do we need to update our current_window_end?
          if (transport_type == BGPSTREAM_RESOURCE_TRANSPORT_FILE){

            if(initial_time + duration > STATE->current_window_end) {
              STATE->current_window_end = (initial_time + duration);
            }

            if(STATE->cache_dir != NULL){
              transport_type = BGPSTREAM_RESOURCE_TRANSPORT_CACHE;
            }
          }

          if (bgpstream_resource_mgr_push(BSDI_GET_RES_MGR(di), transport_type,
                                          format_type, url,
                                          initial_time, duration, project,
                                          collector, type, &res) < 0) {

            bgpstream_log(BGPSTREAM_LOG_ERR, "Unable to push resource");
            goto err;
          }

#if WITH_KAFKA
          // handle kafka-specific configuration
          if (transport_type == BGPSTREAM_RESOURCE_TRANSPORT_KAFKA) {
            if (kafka_topic == NULL) {
              bgpstream_log(BGPSTREAM_LOG_ERR,
                            "Missing bmp kafka topic from the broker");
              goto err;
            }
            if (bgpstream_resource_set_attr(
                  res, BGPSTREAM_RESOURCE_ATTR_KAFKA_TOPICS, kafka_topic) !=
                0) {
              bgpstream_log(BGPSTREAM_LOG_ERR,
                            "Unable to set kafka topic to %s", kafka_topic);
              goto err;
            }

            if (STATE->kafka_group != NULL &&
                bgpstream_resource_set_attr(
                  res, BGPSTREAM_RESOURCE_ATTR_KAFKA_CONSUMER_GROUP,
                  STATE->kafka_group) != 0) {
              bgpstream_log(BGPSTREAM_LOG_ERR,
                            "Unable to set kafka group to %s", STATE->kafka_group);
              goto err;
            }

            if (STATE->kafka_offset != NULL &&
                bgpstream_resource_set_attr(
                  res, BGPSTREAM_RESOURCE_ATTR_KAFKA_INIT_OFFSET,
                  STATE->kafka_offset) != 0) {
              bgpstream_log(BGPSTREAM_LOG_ERR,
                            "Unable to set kafka offset to %s",
                            STATE->kafka_offset);
              goto err;
            }
          }
#endif

          // set cache attribute to resource
          if (transport_type == BGPSTREAM_RESOURCE_TRANSPORT_CACHE &&
              bgpstream_resource_set_attr(res,
                                          BGPSTREAM_RESOURCE_ATTR_CACHE_DIR_PATH,
                                          STATE->cache_dir) != 0) {
            return -1;
          }
        }
      }
    }
  }
  if (time_set == 0) {
    goto err;
  }

  free(url);
  free(kafka_topic);
  return 0;

retry:
  free(url);
  return ERR_RETRY;

err:
  bgpstream_log(BGPSTREAM_LOG_ERR,
                "Invalid JSON response received from broker");
  free(url);
  return ERR_RETRY;
}

static int read_json(bsdi_t *di, io_t *jsonfile)
{
  jsmn_parser p;
  jsmntok_t *tok = NULL;
  size_t tokcount = 128;

  int ret;
  char *js = NULL;
  size_t jslen = 0;
#define BUFSIZE 1024
  char buf[BUFSIZE];

  // prepare parser
  jsmn_init(&p);

  // allocate some tokens to start
  if ((tok = malloc(sizeof(jsmntok_t) * tokcount)) == NULL) {
    bgpstream_log(BGPSTREAM_LOG_ERR, "Could not malloc initial tokens");
    goto err;
  }

  // slurp the whole file into a buffer
  while (1) {
    /* do a read */
    ret = wandio_read(jsonfile, buf, BUFSIZE);
    if (ret < 0) {
      bgpstream_log(BGPSTREAM_LOG_ERR, "Reading from broker failed");
      goto err;
    }
    if (ret == 0) {
      // we're done
      break;
    }
    if ((js = realloc(js, jslen + ret + 1)) == NULL) {
      bgpstream_log(BGPSTREAM_LOG_ERR, "Could not realloc json string");
      goto err;
    }
    strncpy(js + jslen, buf, ret);
    jslen += ret;
  }
  js[jslen] = '\0';

again:
  if ((ret = jsmn_parse(&p, js, jslen, tok, tokcount)) < 0) {
    if (ret == JSMN_ERROR_NOMEM) {
      tokcount *= 2;
      if ((tok = realloc(tok, sizeof(jsmntok_t) * tokcount)) == NULL) {
        bgpstream_log(BGPSTREAM_LOG_ERR, "Could not realloc tokens");
        goto err;
      }
      goto again;
    }
    if (ret == JSMN_ERROR_INVAL) {
      bgpstream_log(BGPSTREAM_LOG_ERR, "Invalid character in JSON string");
      goto err;
    }
    bgpstream_log(BGPSTREAM_LOG_ERR, "JSON parser returned %d", ret);
    goto err;
  }
  ret = process_json(di, js, tok, p.toknext);

  free(js);
  free(tok);
  if (ret == ERR_FATAL) {
    bgpstream_log(BGPSTREAM_LOG_ERR,
                  "Received fatal error from process_json");
  }
  return ret;

err:
  free(js);
  free(tok);
  bgpstream_log(BGPSTREAM_LOG_ERR, "%s: Returning fatal error code",
                __func__);
  return ERR_FATAL;
}

static int update_query_url(bsdi_t *di)
{
  bgpstream_filter_mgr_t *filter_mgr = BSDI_GET_FILTER_MGR(di);

  if (STATE->broker_url == NULL) {
    goto err;
  }

  // reset the query url buffer
  STATE->first_param = 1;
  STATE->query_url_remaining = URL_BUFLEN;
  STATE->query_url_buf[0] = '\0';

  // http://bgpstream.caida.org/broker (e.g.)
  APPEND_STR(STATE->broker_url);

  // http://bgpstream.caida.org/broker/data?
  APPEND_STR("/data");

  // projects, collectors, bgp_types, res_types, and time_interval are
  // used as filters only if they are provided by the user

  // projects
  char *f;
  if (filter_mgr->projects != NULL) {
    bgpstream_str_set_rewind(filter_mgr->projects);
    while ((f = bgpstream_str_set_next(filter_mgr->projects)) != NULL) {
      AMPORQ;
      APPEND_STR("projects[]=");
      APPEND_STR(f);
    }
  }
  // collectors
  if (filter_mgr->collectors != NULL) {
    bgpstream_str_set_rewind(filter_mgr->collectors);
    while ((f = bgpstream_str_set_next(filter_mgr->collectors)) != NULL) {
      AMPORQ;
      APPEND_STR("collectors[]=");
      APPEND_STR(f);
    }
  }
  // routers
  if (filter_mgr->routers != NULL) {
    bgpstream_str_set_rewind(filter_mgr->routers);
    while ((f = bgpstream_str_set_next(filter_mgr->routers)) != NULL) {
      AMPORQ;
      APPEND_STR("routers[]=");
      APPEND_STR(f);
    }
  }
  // peer asns
  uint32_t *p;
  char p_buf[sizeof(STR(UINT32_MAX))+1];
  if (filter_mgr->peer_asns != NULL) {
    bgpstream_id_set_rewind(filter_mgr->peer_asns);
    while ((p = bgpstream_id_set_next(filter_mgr->peer_asns)) != NULL) {
      if (snprintf(p_buf, sizeof(p_buf), "%"PRIu32, *p) >= sizeof(p_buf)) {
        goto err;
      }
      AMPORQ;
      APPEND_STR("peer_asns[]=");
      APPEND_STR(p_buf);
    }
  }
  // bgp_types
  if (filter_mgr->bgp_types != NULL) {
    bgpstream_str_set_rewind(filter_mgr->bgp_types);
    while ((f = bgpstream_str_set_next(filter_mgr->bgp_types)) != NULL) {
      AMPORQ;
      APPEND_STR("types[]=");
      APPEND_STR(f);
    }
  }
  // res_types
  if (filter_mgr->res_types != NULL) {
    bgpstream_str_set_rewind(filter_mgr->res_types);
    while ((f = bgpstream_str_set_next(filter_mgr->res_types)) != NULL) {
      AMPORQ;
      APPEND_STR("resourceTypes[]=");
      APPEND_STR(f);
    }
  }

  // user-provided params
  int i;
  for (i = 0; i < STATE->params_cnt; i++) {
    assert(STATE->params[i] != NULL);
    AMPORQ;
    APPEND_STR(STATE->params[i]);
  }

// time_interval
#define BUFLEN 20
  char int_buf[BUFLEN];
  if (TIF != NULL) {

    AMPORQ;
    APPEND_STR("intervals[]=");

    // BEGIN TIME
    if (snprintf(int_buf, BUFLEN, "%" PRIu32, TIF->begin_time) >= BUFLEN) {
      goto err;
    }
    APPEND_STR(int_buf);
    APPEND_STR(",");

    // END TIME
    if (snprintf(int_buf, BUFLEN, "%" PRIu32, TIF->end_time) >= BUFLEN) {
      goto err;
    }
    APPEND_STR(int_buf);
  }

  // grab pointer to the end of the current string to simplify modifying the
  // query later
  STATE->query_url_end = STATE->query_url_buf + strlen(STATE->query_url_buf);
  assert((*STATE->query_url_end) == '\0');

  return 0;

err:
  return -1;
}

/* ========== PUBLIC METHODS BELOW HERE ========== */

int bsdi_broker_init(bsdi_t *di)
{
  bsdi_broker_state_t *state;

  if ((state = malloc_zero(sizeof(bsdi_broker_state_t))) == NULL) {
    goto err;
  }
  BSDI_SET_STATE(di, state);

  /* set default state */
  if ((state->broker_url = strdup(BGPSTREAM_DI_BROKER_URL)) == NULL) {
    goto err;
  }

  return 0;
err:
  bsdi_broker_destroy(di);
  return -1;
}

int bsdi_broker_start(bsdi_t *di)
{
  return update_query_url(di);
}

int bsdi_broker_set_option(bsdi_t *di,
                           const bgpstream_data_interface_option_t *option_type,
                           const char *option_value)
{
  switch (option_type->id) {
  case OPTION_BROKER_URL:
    // replaces our current URL
    if (STATE->broker_url != NULL) {
      free(STATE->broker_url);
      STATE->broker_url = NULL;
    }
    if ((STATE->broker_url = strdup(option_value)) == NULL) {
      return -1;
    }
    break;

  case OPTION_PARAM:
    // adds a parameter
    if (STATE->params_cnt == MAX_PARAMS) {
      bgpstream_log(BGPSTREAM_LOG_ERR,
                    "At most %d broker query parameters can be set",
                    MAX_PARAMS);
      return -1;
    }
    STATE->params[STATE->params_cnt++] = strdup(option_value);
    break;

  case OPTION_CACHE_DIR:
    // enable cache, no option_value needed
    if (access(option_value, F_OK) == -1) {
      bgpstream_log(BGPSTREAM_LOG_ERR, "Cannot access cache directory %s: %s.",
                    option_value, strerror(errno));
      STATE->cache_dir = NULL;
      return -1;
    } else {
      STATE->cache_dir = strdup(option_value);
    }
    break;

#if WITH_KAFKA
  case OPTION_KAFKA_GROUP:
    // replaces our current group
    if (STATE->kafka_group != NULL) {
      free(STATE->kafka_group);
      STATE->kafka_group = NULL;
    }
    if (strlen(option_value) &&
        (STATE->kafka_group = strdup(option_value)) == NULL) {
      return -1;
    }
    break;

  case OPTION_KAFKA_OFFSET:
    // replaces our current offset
    if (STATE->kafka_offset != NULL) {
      free(STATE->kafka_offset);
      STATE->kafka_offset = NULL;
    }
    if (!strlen(option_value) ||
        (STATE->kafka_offset = strdup(option_value)) == NULL) {
      return -1;
    }
    break;
#endif

  default:
    return -1;
  }

  return 0;
}

void bsdi_broker_destroy(bsdi_t *di)
{
  if (di == NULL || STATE == NULL) {
    return;
  }

  free(STATE->broker_url);
  STATE->broker_url = NULL;

  int i;
  for (i = 0; i < STATE->params_cnt; i++) {
    free(STATE->params[i]);
    STATE->params[i] = NULL;
  }
  STATE->params_cnt = 0;

  free(STATE->cache_dir);
  STATE->cache_dir = NULL;

#if WITH_KAFKA
  free(STATE->kafka_group);
  STATE->kafka_group = NULL;

  free(STATE->kafka_offset);
  STATE->kafka_offset = NULL;
#endif

  free(STATE);
  BSDI_SET_STATE(di, NULL);
}

int bsdi_broker_update_resources(bsdi_t *di)
{

  // we need to set two parameters:
  //  - dataAddedSince ("time" from last response we got)
  //  - minInitialTime (max("initialTime"+"duration") of any file we've ever
  //  seen)

#define BUFLEN 20
  char buf[BUFLEN];

  io_t *jsonfile = NULL;

  int rc;
  int attempts = 0;
  int wait_time = 1;

  int success = 0;

  if (STATE->last_response_time > 0) {
    // need to add dataAddedSince
    if (snprintf(buf, BUFLEN, "%" PRIu32, STATE->last_response_time) >=
        BUFLEN) {
      bgpstream_log(BGPSTREAM_LOG_ERR,
                    "Could not build dataAddedSince param string");
      goto err;
    }
    AMPORQ;
    APPEND_STR("dataAddedSince=");
    APPEND_STR(buf);
  }
  if (STATE->current_window_end > 0) {
    // need to add minInitialTime
    if (snprintf(buf, BUFLEN, "%" PRIu32, STATE->current_window_end) >=
        BUFLEN) {
      bgpstream_log(BGPSTREAM_LOG_ERR,
                    "Could not build minInitialTime param string");
      goto err;
    }
    AMPORQ;
    APPEND_STR("minInitialTime=");
    APPEND_STR(buf);
  }

  do {
    if (attempts > 0) {
      bgpstream_log(BGPSTREAM_LOG_WARN,
                    "WARN: Broker request failed, waiting %ds before retry",
                    wait_time);
      sleep(wait_time);
      if (wait_time < MAX_WAIT_TIME) {
        wait_time *= 2;
      }
    }
    attempts++;

#ifdef BROKER_DEBUG
    bgpstream_log(BGPSTREAM_LOG_INFO, "Query URL: \"%s\"",
                  STATE->query_url_buf);
#endif

    if ((jsonfile = wandio_create(STATE->query_url_buf)) == NULL) {
      bgpstream_log(BGPSTREAM_LOG_ERR, "Could not open %s for reading",
                    STATE->query_url_buf);
      goto retry;
    }

    if ((rc = read_json(di, jsonfile)) == ERR_FATAL) {
      bgpstream_log(BGPSTREAM_LOG_ERR,
                    "Received fatal error code from read_json");
      goto err;
    } else if (rc == ERR_RETRY) {
      goto retry;
    } else {
      // success!
      success = 1;
    }

  retry:
    if (jsonfile != NULL) {
      wandio_destroy(jsonfile);
      jsonfile = NULL;
    }
  } while (success == 0);

  // reset the variable params
  *STATE->query_url_end = '\0';
  STATE->query_url_remaining = URL_BUFLEN - strlen(STATE->query_url_end);
  return 0;

err:
  bgpstream_log(BGPSTREAM_LOG_ERR, "Fatal error in broker data source");
  if (jsonfile != NULL) {
    wandio_destroy(jsonfile);
  }
  return -1;
}
