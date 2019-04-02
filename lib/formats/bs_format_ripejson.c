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
 */

#include "bs_format_ripejson.h"
#include "bgpstream_format_interface.h"
#include "bgpstream_record_int.h"
#include "bgpstream_log.h"
#include "bgpstream_parsebgp_common.h"
#include "utils.h"
#include "jsmn_utils.h"
#include "libjsmn/jsmn.h"
#include <assert.h>
#include <stdio.h>

#define STATE ((state_t *)(format->state))
#define RDATA ((rec_data_t *)(record->__int->data))

typedef enum {
  RIPE_JSON_MSG_TYPE_OPEN = 1,
  RIPE_JSON_MSG_TYPE_UPDATE = 2,
  RIPE_JSON_MSG_TYPE_NOTIFICATION = 3,
  RIPE_JSON_MSG_TYPE_KEEPALIVE = 4,
  RIPE_JSON_MSG_TYPE_STATUS = 5,
} bs_format_ripejson_msg_type_t;

typedef struct json_field {
  char *ptr;  // field pointer
  size_t len; // length of the field
} json_field_t;

// json fields that does not contain in the raw message bytes
typedef struct json_field_ptrs {
  /* common fields */
  json_field_t timestamp; // timestamp of the message
  json_field_t peer;      // peer IP
  json_field_t peer_asn;  // peer ASN
  json_field_t raw;       // raw bytes of the bgp message
  json_field_t host;      // collector name (e.g. rrc21)
  json_field_t type;      // message type

  /* state message fields */
  json_field_t state;  // new state: connected, down
} json_field_ptrs_t;

typedef struct rec_data {
  // reusable elem instance
  bgpstream_elem_t *elem;

  // have we extracted all the possible elems out of the current message?
  int end_of_elems;

  // index of the NEXT rib entry to read from a TDv2 message
  int next_re;

  // state for UPDATE elem extraction
  bgpstream_parsebgp_upd_state_t upd_state;

  // reusable parser message structure
  parsebgp_msg_t *msg;

  // message type: OPEN, UDPATE, STATUS, NOTIFY
  bs_format_ripejson_msg_type_t msg_type;

  // message direction: special type for OPEN message, 0 - sent, or 1 - received
  int open_msg_direction;

  // status state: 0 - down, 1 - connected
  int status_msg_state;

} rec_data_t;

typedef struct state {
  // options
  parsebgp_opts_t opts;

  // json bgp message string buffer
  char *json_string_buffer;

  // json bgp message string buffer length
  int json_string_buffer_len;

  // json bgp message bytes buffer
  uint8_t json_bytes_buffer[4096];

  // json bgp message field buffer
  uint8_t field_buffer[100];

  // json bgp message fields
  json_field_ptrs_t json_fields;

} state_t;

/* ======================================================== */
/* ======================================================== */
/* ==================== JSON UTILITIES ==================== */
/* ======================================================== */
/* ======================================================== */

#define FIELDPTR(field) STATE->json_fields.field.ptr
#define FIELDLEN(field) STATE->json_fields.field.len

#define STRTOUL(field, dest)                                                   \
  do {                                                                         \
    char tmp = FIELDPTR(field)[FIELDLEN(field)];                               \
    FIELDPTR(field)[FIELDLEN(field)] = '\0';                                   \
    dest = strtoul((char *)FIELDPTR(field), NULL, 10);                         \
    FIELDPTR(field)[FIELDLEN(field)] = tmp;                                    \
  } while (0)

// convert char array to bytes
// by @alistair
static int hexstr_to_bytes(uint8_t *buf, const char *hexstr, size_t hexstr_len)
{
  size_t i;
  char c;
  for (i = 0; i < hexstr_len; i++) {
    c = hexstr[i];
    // sanity check on input characters
    if (c < '0' || (c > '9' && c < 'A') || (c > 'F' && c < 'a') || c > 'f') {
      return -1;
    }
    if (c >= 'a') {
      c -= ('a' - '9' - 1);
    } else if (c >= 'A') {
      c -= ('A' - '9' - 1);
    }
    c -= '0';

    if ((i & 0x1) == 0) {
      // high-order
      *buf = c << 4;
    } else {
      // low-order
      *(buf++) |= c;
    }
  }
  return 0;
}

// convert bgp message hex string to char (byte) array, with added header marker
// by @alistairking
static ssize_t hexstr_to_bgpmsg(uint8_t *buf, size_t buflen, const char *hexstr,
                                size_t hexstr_len)
{
  // 2 characters per octet, and BGP messages cannot be more than 4096 bytes
  if ((hexstr_len & 0x1) != 0 || hexstr_len > (4096 * 2)) {
    return -1;
  }
  uint16_t msg_len = (hexstr_len / 2) + 2 + 1;
  if (msg_len > buflen) {
    return -1;
  }
  // parse the hex string, one nybble at a time
  if (hexstr_to_bytes(buf, hexstr, hexstr_len) < 0) {
    return -1;
  }
  return msg_len;
}

/* ====================================================================== */
/* ====================================================================== */
/* ==================== PRIVATE FUNCTIONS BELOW HERE ==================== */
/* ====================================================================== */
/* ====================================================================== */

// process common header fields for all message types
// the fields are stored in the json_fields struct, include:
// - host
// - peer
// - peer_asn
// - timestamp
static int process_common_fields(bgpstream_format_t *format,
                                 bgpstream_record_t *record)
{
  // populate collector name
  memcpy(record->collector_name, FIELDPTR(host), FIELDLEN(host));
  record->collector_name[FIELDLEN(host)] = '\0';

  // populate peer asn
  STRTOUL(peer_asn, RDATA->elem->peer_asn);

  // populate peer ip
  char tmp;
  tmp = FIELDPTR(peer)[FIELDLEN(peer)];
  FIELDPTR(peer)[FIELDLEN(peer)] = '\0';
  if (bgpstream_str2addr((char *)FIELDPTR(peer), &RDATA->elem->peer_ip) ==
      NULL) {
    bgpstream_log(BGPSTREAM_LOG_ERR, "error parsing address");
    return -1;
  }
  FIELDPTR(peer)[FIELDLEN(peer)] = tmp;

  // populate time-stamp
  strntotime(FIELDPTR(timestamp), FIELDLEN(timestamp), &record->time_sec,
             &record->time_usec);
  return 0;
}

static bgpstream_format_status_t
process_bgp_message(bgpstream_format_t *format, bgpstream_record_t *record)
{
  // convert body to bytes
  ssize_t rc = hexstr_to_bgpmsg(
    STATE->json_bytes_buffer, sizeof(STATE->json_bytes_buffer),
    (char *)FIELDPTR(raw), FIELDLEN(raw));
  if (rc < 0) {
    return BGPSTREAM_FORMAT_CORRUPTED_MSG;
  }

  // decode bytes of bgp message
  parsebgp_error_t err;
  size_t dec_len = (size_t)rc;
  if ((err = parsebgp_decode(STATE->opts, PARSEBGP_MSG_TYPE_BGP, RDATA->msg,
                             STATE->json_bytes_buffer, &dec_len)) !=
      PARSEBGP_OK) {
    bgpstream_log(BGPSTREAM_LOG_ERR, "Failed to parse message (%s)",
                  parsebgp_strerror(err));
    parsebgp_clear_msg(RDATA->msg);
    return BGPSTREAM_FORMAT_CORRUPTED_MSG;
  }

  // extract other fields from the message: peer, peer_asn, host, timestamp
  if (process_common_fields(format, record) < 0) {
    return BGPSTREAM_FORMAT_CORRUPTED_MSG;
  }

  return BGPSTREAM_FORMAT_OK;
}

static bgpstream_format_status_t
process_status_message(bgpstream_format_t *format, bgpstream_record_t *record)
{

  // process direction
  if (FIELDLEN(state) == 4 &&
      bcmp("down", FIELDPTR(state), FIELDLEN(state)) == 0) {
    // down message
    RDATA->status_msg_state = 0;
  } else if (FIELDLEN(state) == 9 &&
             bcmp("connected", FIELDPTR(state), FIELDLEN(state)) == 0) {
    // connected message
    RDATA->status_msg_state = 1;
  } else {
    // unknown
    RDATA->status_msg_state = -1;
  }

  if (process_common_fields(format, record) < 0) {
    return BGPSTREAM_FORMAT_CORRUPTED_MSG;
  }

  return BGPSTREAM_FORMAT_OK;
}

static bgpstream_format_status_t
process_unsupported_message(bgpstream_format_t *format,
                            bgpstream_record_t *record)
{
  bgpstream_log(BGPSTREAM_LOG_WARN, "unsupported ris-stream message: %s",
                STATE->json_string_buffer);
  record->status = BGPSTREAM_RECORD_STATUS_UNSUPPORTED_RECORD;
  record->collector_name[0] = '\0';
  return BGPSTREAM_FORMAT_UNSUPPORTED_MSG;
}

static bgpstream_format_status_t
process_corrupted_message(bgpstream_format_t *format,
                          bgpstream_record_t *record)
{
  bgpstream_log(BGPSTREAM_LOG_WARN, "corrupted ris-stream message: %s",
                STATE->json_string_buffer);
  record->status = BGPSTREAM_RECORD_STATUS_CORRUPTED_RECORD;
  record->collector_name[0] = '\0';
  return BGPSTREAM_FORMAT_CORRUPTED_MSG;
}

#define NEXT_TOK t++

#define PARSEFIELD(field)                                                      \
  if (jsmn_streq(STATE->json_string_buffer, t, STR(field)) == 1) {             \
    NEXT_TOK;                                                                  \
    STATE->json_fields.field.ptr = STATE->json_string_buffer + t->start;\
    STATE->json_fields.field.len = t->end - t->start;                  \
    NEXT_TOK;                                                                  \
  }

static int process_data(bgpstream_format_t *format,
                        jsmntok_t *root_tok)
{
  int i;
  jsmntok_t *t = root_tok + 1;

  for (i = 0; i < root_tok->size; i++) {
    // all keys must be strings
    if (t->type != JSMN_STRING) {
      bgpstream_log(BGPSTREAM_LOG_ERR, "Encountered non-string key: '%.*s'",
                    t->end - t->start, STATE->json_string_buffer + t->start);
      return -1;
    }
    PARSEFIELD(raw) //
    else PARSEFIELD(timestamp)  //
      else PARSEFIELD(host)     //
      else PARSEFIELD(peer_asn) //
      else PARSEFIELD(peer)     //
      else PARSEFIELD(type)     //
      else PARSEFIELD(state)    //
      else
    {
      NEXT_TOK; // move past the key
      t = jsmn_skip(t); // skip the value
    }
  }

  return 0;
}

static bgpstream_format_status_t
bs_format_process_json_fields(bgpstream_format_t *format,
                              bgpstream_record_t *record)
{
  int i, r;
  bgpstream_format_status_t rc;
  jsmn_parser p;

  jsmntok_t *t, *root_tok;
  size_t tokcount = 128;

  // prepare parser
  jsmn_init(&p);

  // allocate some tokens to start
  if ((root_tok = malloc(sizeof(jsmntok_t) * tokcount)) == NULL) {
    bgpstream_log(BGPSTREAM_LOG_ERR, "Could not malloc initial tokens");
    goto corrupted;
  }

again:
  if ((r = jsmn_parse(&p, STATE->json_string_buffer,
                      STATE->json_string_buffer_len, root_tok, tokcount)) < 0) {
    if (r == JSMN_ERROR_NOMEM) {
      tokcount *= 2;
      if ((root_tok = realloc(root_tok, sizeof(jsmntok_t) * tokcount)) == NULL) {
        bgpstream_log(BGPSTREAM_LOG_ERR, "Could not realloc tokens");
        goto corrupted;
      }
      goto again;
    }
    if (r == JSMN_ERROR_INVAL) {
      bgpstream_log(BGPSTREAM_LOG_ERR, "Invalid character in JSON string");
      goto corrupted;
    }
    bgpstream_log(BGPSTREAM_LOG_ERR, "JSON parser returned %d", r);
    goto corrupted;
  }

  /* Assume the top-level element is an object */
  if (p.toknext < 1 || root_tok->type != JSMN_OBJECT) {
    bgpstream_log(BGPSTREAM_LOG_ERR, "JSON top-level not object");
    goto corrupted;
  }
  t = root_tok + 1;

  /* iterate over the children of the root object */
  for (i = 0; i < root_tok->size; i++) {
    // all keys must be strings
    if (t->type != JSMN_STRING) {
      bgpstream_log(BGPSTREAM_LOG_ERR, "Encountered non-string key: '%.*s'",
                    t->end - t->start, STATE->json_string_buffer + t->start);
      goto corrupted;
    }

    if (jsmn_streq(STATE->json_string_buffer, t, "data") == 1) {
      NEXT_TOK;
      jsmn_type_assert(t, JSMN_OBJECT);
      // handle data
      if ((rc = process_data(format, t)) != 0) {
        goto corrupted;
      }
      break; // we have all we need, so no need to keep parsing
    } else {
      // skip any other top-level keys (e.g., "type")
      NEXT_TOK;
      // and their value
      t = jsmn_skip(t);
    }
  }

  if (FIELDLEN(type) == 0) {
    bgpstream_log(BGPSTREAM_LOG_ERR, "Missing message type");
    goto corrupted;
  }

  // process each type of message separately
  // the types of messages include
  //   - UPDATE
  //   - OPEN
  //   - NOTIFICATION
  //   - KEEPALIVE
  //   - RIS_PEER_STATE
  switch (*FIELDPTR(type)) {
  case 'U':
    RDATA->msg_type = RIPE_JSON_MSG_TYPE_UPDATE;
    rc = process_bgp_message(format, record);
    break;
  case 'O':
    RDATA->msg_type = RIPE_JSON_MSG_TYPE_OPEN;
    rc = process_bgp_message(format, record);
    break;
  case 'N':
    RDATA->msg_type = RIPE_JSON_MSG_TYPE_NOTIFICATION;
    rc = process_bgp_message(format, record);
    break;
  case 'K':
    RDATA->msg_type = RIPE_JSON_MSG_TYPE_KEEPALIVE;
    rc = process_bgp_message(format, record);
    break;
  case 'R':
    RDATA->msg_type = RIPE_JSON_MSG_TYPE_STATUS;
    rc = process_status_message(format, record);
    break;
  default:
    rc = BGPSTREAM_FORMAT_UNSUPPORTED_MSG;
    break;
  }

  switch (rc) {
  case BGPSTREAM_FORMAT_OK:
    goto ok;
  case BGPSTREAM_FORMAT_CORRUPTED_MSG:
    goto corrupted;
  case BGPSTREAM_FORMAT_UNSUPPORTED_MSG:
    goto unsupported;
  default:
    // other status codes should not appear
    assert(0);
    goto corrupted;
    break;
  }

ok:
  free(root_tok);
  return BGPSTREAM_FORMAT_OK;

err:
corrupted:
  free(root_tok);
  return process_corrupted_message(format, record);

unsupported:
  free(root_tok);
  return process_unsupported_message(format, record);
}

/* =============================================================== */
/* =============================================================== */
/* ==================== PUBLIC API BELOW HERE ==================== */
/* =============================================================== */
/* =============================================================== */

int bs_format_ripejson_create(bgpstream_format_t *format,
                              bgpstream_resource_t *res)
{
  BS_FORMAT_SET_METHODS(ripejson, format);

  if ((format->state = malloc_zero(sizeof(state_t))) == NULL) {
    return -1;
  }

  if ((STATE->json_string_buffer = malloc(BGPSTREAM_PARSEBGP_BUFLEN)) == NULL) {
    return -1;
  }

  parsebgp_opts_init(&STATE->opts);
  bgpstream_parsebgp_opts_init(&STATE->opts);
  STATE->opts.bgp.marker_omitted = 0;
  STATE->opts.bgp.asn_4_byte = 1;

  return 0;
}

bgpstream_format_status_t
bs_format_ripejson_populate_record(bgpstream_format_t *format,
                                   bgpstream_record_t *record)
{
  int rc;

  STATE->json_string_buffer_len = bgpstream_transport_readline(
    format->transport, STATE->json_string_buffer, BGPSTREAM_PARSEBGP_BUFLEN);

  assert(STATE->json_string_buffer_len < BGPSTREAM_PARSEBGP_BUFLEN);

  if (STATE->json_string_buffer_len < 0) {
    record->status = BGPSTREAM_RECORD_STATUS_CORRUPTED_RECORD;
    record->collector_name[0] = '\0';
    return BGPSTREAM_FORMAT_CORRUPTED_DUMP;
  } else if (STATE->json_string_buffer_len == 0) {
    return BGPSTREAM_FORMAT_END_OF_DUMP;
  }

  if ((rc = bs_format_process_json_fields(format, record)) != 0) {
    return rc;
  }

  return BGPSTREAM_FORMAT_OK;
}

int bs_format_ripejson_get_next_elem(bgpstream_format_t *format,
                                     bgpstream_record_t *record,
                                     bgpstream_elem_t **elem)
{
  int rc = 0;

  if (RDATA == NULL || RDATA->end_of_elems != 0) {
    // end-of-elems
    return 0;
  }

  switch (RDATA->msg_type) {
  case RIPE_JSON_MSG_TYPE_UPDATE:
    rc = bgpstream_parsebgp_process_update(&RDATA->upd_state, RDATA->elem,
                                           RDATA->msg->types.bgp);
    if (rc <= 0) {
      return rc;
    }
    break;
  case RIPE_JSON_MSG_TYPE_STATUS:
    RDATA->elem->type = BGPSTREAM_ELEM_TYPE_PEERSTATE;
    switch (RDATA->status_msg_state) {
    case 0:
      RDATA->elem->old_state = BGPSTREAM_ELEM_PEERSTATE_UNKNOWN;
      RDATA->elem->new_state = BGPSTREAM_ELEM_PEERSTATE_IDLE;
      break;
    case 1:
      RDATA->elem->old_state = BGPSTREAM_ELEM_PEERSTATE_UNKNOWN;
      RDATA->elem->new_state = BGPSTREAM_ELEM_PEERSTATE_ESTABLISHED;
      break;
    default:
      RDATA->elem->old_state = BGPSTREAM_ELEM_PEERSTATE_UNKNOWN;
      RDATA->elem->new_state = BGPSTREAM_ELEM_PEERSTATE_UNKNOWN;
      bgpstream_log(BGPSTREAM_LOG_WARN, "unsupported status type, %d",
                    RDATA->status_msg_state);
      break;
    }
    RDATA->end_of_elems = 1;
    rc = 1;
    break;
  case RIPE_JSON_MSG_TYPE_OPEN:
    RDATA->elem->type = BGPSTREAM_ELEM_TYPE_PEERSTATE;
    switch (RDATA->open_msg_direction) {
    case 0:
      // "sent" OPEN
      RDATA->elem->old_state = BGPSTREAM_ELEM_PEERSTATE_UNKNOWN;
      RDATA->elem->new_state = BGPSTREAM_ELEM_PEERSTATE_OPENSENT;
      break;
    case 1:
      // "received" OPEN
      RDATA->elem->old_state = BGPSTREAM_ELEM_PEERSTATE_UNKNOWN;
      RDATA->elem->new_state = BGPSTREAM_ELEM_PEERSTATE_OPENCONFIRM;
      break;
    default:
      RDATA->elem->old_state = BGPSTREAM_ELEM_PEERSTATE_UNKNOWN;
      RDATA->elem->new_state = BGPSTREAM_ELEM_PEERSTATE_UNKNOWN;
      bgpstream_log(BGPSTREAM_LOG_WARN,
                    "unsupported open message direction, %d",
                    RDATA->open_msg_direction);
    }
    RDATA->end_of_elems = 1;
    rc = 1;
    break;
  case RIPE_JSON_MSG_TYPE_NOTIFICATION:
  case RIPE_JSON_MSG_TYPE_KEEPALIVE:
  default:
    break;
  }

  // return a borrowed pointer to the elem we populated
  *elem = RDATA->elem;
  return rc;
}

int bs_format_ripejson_init_data(bgpstream_format_t *format, void **data)
{
  rec_data_t *rd;
  *data = NULL;

  if ((rd = malloc_zero(sizeof(rec_data_t))) == NULL) {
    return -1;
  }

  if ((rd->elem = bgpstream_elem_create()) == NULL) {
    return -1;
  }

  if ((rd->msg = parsebgp_create_msg()) == NULL) {
    return -1;
  }

  *data = rd;
  return 0;
}

void bs_format_ripejson_clear_data(bgpstream_format_t *format, void *data)
{
  rec_data_t *rd = (rec_data_t *)data;
  assert(rd != NULL);
  bgpstream_elem_clear(rd->elem);
  rd->end_of_elems = 0;
  rd->next_re = 0;
  bgpstream_parsebgp_upd_state_reset(&rd->upd_state);
  parsebgp_clear_msg(rd->msg);
}

void bs_format_ripejson_destroy_data(bgpstream_format_t *format, void *data)
{
  rec_data_t *rd = (rec_data_t *)data;
  if (rd == NULL) {
    return;
  }
  bgpstream_elem_destroy(rd->elem);
  rd->elem = NULL;
  parsebgp_destroy_msg(rd->msg);
  rd->msg = NULL;
  free(data);
}

void bs_format_ripejson_destroy(bgpstream_format_t *format)
{
  free(STATE->json_string_buffer);
  free(format->state);
  format->state = NULL;
}
