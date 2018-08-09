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
#include "libjsmn/jsmn.h"
#include "utils.h"
#include <assert.h>
#include <stdio.h>

#define STATE ((state_t*)(format->state))

#define RDATA ((rec_data_t *)(record->__int->data))

#define RIPE_JSON_MSG_TYPE_UPDATE 0

// might not very useful
typedef enum {

  RIPE_JSON_MSG_TYPE_ANNOUNCE = 1,

  RIPE_JSON_MSG_TYPE_WITHDRAW = 2,

  RIPE_JSON_MSG_TYPE_STATUS   = 3,

  RIPE_JSON_MSG_TYPE_OPEN     = 4,

  RIPE_JSON_MSG_TYPE_NOTIFY   = 5,

} bs_format_ripejson_msg_type_t;

typedef struct json_field{

  unsigned char* ptr;

  size_t len;

} json_field_t;

typedef struct json_field_ptrs {

  json_field_t body;

  json_field_t timestamp;

  json_field_t host;

  json_field_t id;

  json_field_t peer_asn;

  json_field_t type;

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
  int message_type;

} rec_data_t;

typedef struct state {
  // options
  parsebgp_opts_t opts;

  // json string buffer
  uint8_t json_string_buffer[BGPSTREAM_PARSEBGP_BUFLEN];

  // json bgp message bytes buffer
  uint8_t json_bytes_buffer[4096];

  bs_format_ripejson_msg_type_t msg_type;

  json_field_ptrs_t json_fields;

} state_t;


/* ==================== RECORD FILTERING ==================== */

/* ======================================================== */
/* ======================================================== */
/* ==================== JSON UTILITIES ==================== */
/* ======================================================== */
/* ======================================================== */

static int jsoneq(const char *json, jsmntok_t *tok, const char *s) {
  if (tok->type == JSMN_STRING && (int) strlen(s) == tok->end - tok->start &&
      bcmp(json + tok->start, s, tok->end - tok->start) == 0) {
    return 0;
  }
  return -1;
}

// convert bgp message hex string to char (byte) array, with added header marker
// by @alistairking
static ssize_t hexstr_to_bgpmsg(uint8_t *buf, size_t buflen,
                                const char* hexstr, size_t hexstr_len,
                                uint8_t msg_type)
{
  // 2 characters per octet, and BGP messages cannot be more than 4096 bytes
  if ((hexstr_len & 0x1) != 0 || hexstr_len > (4096*2)){
    return -1;
  }
  uint16_t msg_len = (hexstr_len / 2) + 2 + 1;
  uint16_t tmp = htons(msg_len);
  size_t i;
  char c;
  uint8_t *bufp = buf;

  if (msg_len > buflen) {
    return -1;
  }

  // populate the message header (but don't include the marker)
  memcpy(buf, &tmp, sizeof(tmp));
  buf[2] = msg_type;

  // parse the hex string, one nybble at a time
  bufp = buf + 3;
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
      *bufp = c << 4;
    } else {
      // low-order
      *(bufp++) |= c;
    }
  }

  return msg_len;
}

/* ====================================================================== */
/* ====================================================================== */
/* ==================== PRIVATE FUNCTIONS BELOW HERE ==================== */
/* ====================================================================== */
/* ====================================================================== */

int process_update_message(bgpstream_format_t *format, bgpstream_record_t *record){

  ssize_t dec_len =  hexstr_to_bgpmsg(STATE->json_bytes_buffer,
                                      4096,
                                      STATE->json_fields.body.ptr,
                                      STATE->json_fields.body.len,
                                      2);
  size_t err;

  RDATA->msg->type = PARSEBGP_MSG_TYPE_BGP; // TODO: necessary?

  if ((err = parsebgp_decode(STATE->opts, RDATA->msg->type, RDATA->msg,
                             STATE->json_bytes_buffer, &dec_len)) != PARSEBGP_OK) {
    fprintf(stderr, "ERROR: Failed to parse message (%d:%s)\n", err, parsebgp_strerror(err));
    parsebgp_clear_msg(RDATA->msg);
  }

  memcpy(record -> collector_name , STATE->json_fields.host.ptr, STATE->json_fields.host.len);
  RDATA->elem->peer_asn = (uint32_t) strtol(STATE->json_fields.peer_asn.ptr, NULL, 10);
  return 0;
}

int process_status_message(bgpstream_format_t *format, bgpstream_record_t *record){

}
int process_open_message(bgpstream_format_t *format, bgpstream_record_t *record){

}
int process_notify_message(bgpstream_format_t *format, bgpstream_record_t *record){

}


int bs_format_process_json_fields(bgpstream_format_t *format, bgpstream_record_t *record){
  int i;
  int r;
  jsmn_parser p;
  jsmntok_t t[128]; /* We expect no more than 128 tokens */

  uint8_t* json_string = &STATE->json_string_buffer;

  printf("announcement!");

  jsmn_init(&p);
  r = jsmn_parse(&p, json_string, strlen(json_string), t, sizeof(t)/sizeof(t[0]));
  if (r < 0) {
    printf("Failed to parse JSON: %d\n", r);
    return 1;
  }

  /* Assume the top-level element is an object */
  if (r < 1 || t[0].type != JSMN_OBJECT) {
    printf("Object expected\n%s\n", STATE->json_string_buffer);
    return 1;
  }

  /* Loop over all fields of the json string buffer */
  printf("announcement!");
  for (i = 1; i < r; i++) {
    unsigned char* value_ptr = json_string + t[i+1].start;
    size_t value_size = t[i+1].end-t[i+1].start;

    if (jsoneq(json_string, &t[i], "body") == 0) {
      STATE->json_fields.body = (json_field_t) {value_ptr, value_size};
      i++;
    } else if (jsoneq(json_string, &t[i], "timestamp") == 0) {
      // printf("- timestamp: %.*s\n", t[i+1].end-t[i+1].start, json_string + t[i+1].start);
      STATE->json_fields.timestamp = (json_field_t) {value_ptr, value_size};
      i++;
    } else if (jsoneq(json_string, &t[i], "host") == 0) {
      // printf("- host: %.*s\n", t[i+1].end-t[i+1].start, json_string + t[i+1].start);
      STATE->json_fields.host = (json_field_t) {value_ptr, value_size};
      i++;
    } else if (jsoneq(json_string, &t[i], "id") == 0) {
      // printf("- id: %.*s\n", t[i+1].end-t[i+1].start, json_string + t[i+1].start);
      STATE->json_fields.id = (json_field_t) {value_ptr, value_size};
      i++;
    } else if (jsoneq(json_string, &t[i], "peer_asn") == 0) {
      // printf("- peer_asn: %.*s\n", t[i+1].end-t[i+1].start, json_string + t[i+1].start);
      STATE->json_fields.peer_asn = (json_field_t) {value_ptr, value_size};
      i++;
    } else if (jsoneq(json_string, &t[i], "type") == 0) {
      // printf("- peer_asn: %.*s\n", t[i+1].end-t[i+1].start, json_string + t[i+1].start);
      STATE->json_fields.type = (json_field_t) {value_ptr, value_size};
      i++;
    } else {
      // printf("Unexpected key: %.*s\n", t[i].end-t[i].start,
      //     json_string + t[i].start);
    }
  }


  // process each type of message separately

  switch(*STATE->json_fields.type.ptr){
  case 65: // "A"
    STATE->msg_type = RIPE_JSON_MSG_TYPE_ANNOUNCE;
    fprintf(stderr, "announcement!\n");
    process_update_message(format, record);
    break;
  case 87: // "W"
    STATE->msg_type = RIPE_JSON_MSG_TYPE_WITHDRAW;
    break;
  case 83: // "S"
    STATE->msg_type = RIPE_JSON_MSG_TYPE_STATUS;
    break;
  case 79: // "O"
    STATE->msg_type = RIPE_JSON_MSG_TYPE_OPEN;
    break;
  case 78: // "N"
    STATE->msg_type = RIPE_JSON_MSG_TYPE_NOTIFY;
    break;
  default:
    break;
  }

  return EXIT_SUCCESS;
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

  parsebgp_opts_init(&STATE->opts);
  bgpstream_parsebgp_opts_init(&STATE->opts);
  STATE->opts.bgp.marker_omitted = 1;
  STATE->opts.bgp.asn_4_byte= 1;

  return 0;
}

bgpstream_format_status_t
bs_format_ripejson_populate_record(bgpstream_format_t *format,
                              bgpstream_record_t *record)
{

  record -> time_sec = 1;
  record -> time_usec = 100;

  strcpy(record -> project_name , "ripe-stream");
  strcpy(record -> collector_name , "rrrrr");

  // ensure the router fields are unset
  record->router_name[0] = '\0';
  record->router_ip.version = 0;


  int newread = bgpstream_transport_readline(format->transport, &STATE->json_string_buffer, BGPSTREAM_PARSEBGP_BUFLEN);

  if (newread <0 || newread >= BGPSTREAM_PARSEBGP_BUFLEN) {
    printf("read error!\n");
    return newread;
  } else if ( newread == 0 ){
    printf("read finished!\n");
    return BGPSTREAM_FORMAT_END_OF_DUMP;
  }


  bs_format_process_json_fields(format, record);
  // COPY_IP(RDATA->elem->peer_ip, RDATA->msg->types.bgp->afi, RDATA->msg->types.bgp->peer_ip, return 0);
  RDATA->elem->peer_ip.version = BGPSTREAM_ADDR_VERSION_IPV4;


    // return bgpstream_parsebgp_populate_record(&STATE->decoder, RDATA->msg, format,
    //                                           record, NULL, populate_filter_cb);
  return BGPSTREAM_FORMAT_OK;
}

int bs_format_ripejson_get_next_elem(bgpstream_format_t *format,
                                bgpstream_record_t *record,
                                bgpstream_elem_t **elem)
{
  int rc;

  if (RDATA == NULL || RDATA->end_of_elems != 0) {
    // end-of-elems
    return 0;
  }


  // TODO: deal with other types of messages (OPEN, STATUS, NOTIFY)
  switch(STATE->msg_type){
  case RIPE_JSON_MSG_TYPE_ANNOUNCE:
  case RIPE_JSON_MSG_TYPE_WITHDRAW:
    rc = bgpstream_parsebgp_process_update(&RDATA->upd_state, RDATA->elem,
                                               RDATA->msg->types.bgp);
    if (rc <= 0) {
      if(rc<0){
        printf("get_next_elem failed\n");
      }
      return rc;
    }
    break;
  case RIPE_JSON_MSG_TYPE_STATUS:
  case RIPE_JSON_MSG_TYPE_OPEN:
  case RIPE_JSON_MSG_TYPE_NOTIFY:
    fprintf(stderr, "WARNING: unsupported message type\n");
    break;
  default:
    fprintf(stderr, "WARNING: unsupported message type\n");
    break;
  }

  // return a borrowed pointer to the elem we populated
  *elem = RDATA->elem;
  return 1;
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
  rec_data_t *rd = (rec_data_t*)data;
  assert(rd != NULL);
  bgpstream_elem_clear(rd->elem);
  rd->end_of_elems = 0;
  rd->next_re = 0;
  bgpstream_parsebgp_upd_state_reset(&rd->upd_state);
  parsebgp_clear_msg(rd->msg);
}

void bs_format_ripejson_destroy_data(bgpstream_format_t *format, void *data)
{
  rec_data_t *rd = (rec_data_t*)data;
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
  free(format->state);
  format->state = NULL;
}
