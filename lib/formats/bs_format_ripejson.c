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
#include "jsmn_utils.h"
#include <assert.h>
#include <stdio.h>

#define STATE ((state_t*)(format->state))
#define RDATA ((rec_data_t *)(record->__int->data))

typedef enum {
  RIPE_JSON_MSG_TYPE_ANNOUNCE = 1,
  RIPE_JSON_MSG_TYPE_WITHDRAW = 2,
  RIPE_JSON_MSG_TYPE_STATUS   = 3,
  RIPE_JSON_MSG_TYPE_OPEN     = 4,
  RIPE_JSON_MSG_TYPE_NOTIFY   = 5,
} bs_format_ripejson_msg_type_t;

typedef struct json_field{
  char* ptr;  // field pointer
  size_t len; // length of the field
} json_field_t;

// json fields that does not contain in the raw message bytes
typedef struct json_field_ptrs {
  /* common fields */
  json_field_t body;       // raw bytes of the bgp message
  json_field_t timestamp;  // timestamp of the message
  json_field_t host;       // collector name (e.g. rrc21)
  json_field_t id;         // message ID
  json_field_t peer_asn;   // peer ASN
  json_field_t peer;       // peer IP
  json_field_t type;       // message type

  /* open message fields */
  json_field_t asn;        // AS number for connected router
  json_field_t hold_time;  // up time
  json_field_t router_id;  // router IP address
  json_field_t direction;  // sent/receive

  /* state message fields */
  json_field_t state;      // new state: connected, down
  json_field_t reason;     // reason of the state change
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
  uint8_t json_string_buffer[BGPSTREAM_PARSEBGP_BUFLEN];

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

#define STRTOUL(field, dest)                        \
  do {                                              \
    char tmp=FIELDPTR(field)[FIELDLEN(field)];      \
    FIELDPTR(field)[FIELDLEN(field)]='\0';          \
    dest=strtoul((char*)FIELDPTR(field), NULL, 10); \
    FIELDPTR(field)[FIELDLEN(field)]=tmp;           \
  } while(0)
#define STRTOD(field, dest)                         \
  do {                                              \
    char tmp=FIELDPTR(field)[FIELDLEN(field)];      \
    FIELDPTR(field)[FIELDLEN(field)]='\0';          \
    dest=strtod((char*)FIELDPTR(field), NULL    );  \
    FIELDPTR(field)[FIELDLEN(field)]=tmp;           \
  } while(0)

// convert char array to bytes
// by @alistair
static int hexstr_to_bytes(uint8_t *buf, const char* hexstr, size_t hexstr_len){
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
  uint8_t *bufp = buf;

  if (msg_len > buflen) {
    return -1;
  }

  // populate the message header (but don't include the marker)
  memcpy(buf, &tmp, sizeof(tmp));
  buf[2] = msg_type;

  // parse the hex string, one nybble at a time
  bufp = buf + 3;
  if(hexstr_to_bytes(bufp, hexstr, hexstr_len)<0){
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
static int process_common_fields(bgpstream_format_t *format, bgpstream_record_t *record){

  double time_double;
  // populate collector name
  memcpy(record->collector_name , FIELDPTR(host), FIELDLEN(host));
  record->collector_name[FIELDLEN(host)] = '\0';

  // populate peer asn
  STRTOUL(peer_asn, RDATA->elem->peer_asn);

  // populate peer ip
  char tmp;
  tmp = FIELDPTR(peer)[FIELDLEN(peer)];
  FIELDPTR(peer)[FIELDLEN(peer)] = '\0';
  if(bgpstream_str2addr((char *)FIELDPTR(peer), &RDATA->elem->peer_ip)==NULL){
    fprintf(stderr, "ERROR: error parsing address\n");
    return -1;
  }
  FIELDPTR(peer)[FIELDLEN(peer)] = tmp;

  // populate time-stamp
  STRTOD(timestamp, time_double);
  record->time_sec = (uint32_t) time_double;
  record->time_usec = (uint32_t)((time_double - (uint32_t) time_double) * 1000000);

  return 0;
}

static bgpstream_format_status_t process_update_message(bgpstream_format_t *format, bgpstream_record_t *record){

  // convert body to bytes
  ssize_t rc =  hexstr_to_bgpmsg(STATE->json_bytes_buffer,
                                 sizeof(STATE->json_bytes_buffer),
                                 (char*) FIELDPTR(body),
                                 FIELDLEN(body),
                                 PARSEBGP_BGP_TYPE_UPDATE);
  if(rc<0){
    return BGPSTREAM_FORMAT_CORRUPTED_MSG;
  }

  parsebgp_error_t err;
  size_t dec_len =  (size_t) rc;

  if ((err = parsebgp_decode(STATE->opts, PARSEBGP_MSG_TYPE_BGP, RDATA->msg,
                             STATE->json_bytes_buffer, &dec_len)) != PARSEBGP_OK) {
    fprintf(stderr, "ERROR: Failed to parse message (%s)\n", parsebgp_strerror(err));
    parsebgp_clear_msg(RDATA->msg);
    return BGPSTREAM_FORMAT_CORRUPTED_MSG;
  }

  if(process_common_fields(format, record)<0){
    return BGPSTREAM_FORMAT_CORRUPTED_MSG;
  }

  return BGPSTREAM_FORMAT_OK;
}

static bgpstream_format_status_t process_status_message(bgpstream_format_t *format, bgpstream_record_t *record){

  // process direction
  if(FIELDLEN(state) == 4 && bcmp("down", FIELDPTR(state), FIELDLEN(state)) == 0){
    // down message
    RDATA->status_msg_state = 0;
  } else if(FIELDLEN(state) == 9 && bcmp("connected", FIELDPTR(state), FIELDLEN(state)) == 0){
    // connected message
    RDATA->status_msg_state = 1;
  } else {
    // unknown
    RDATA->status_msg_state = -1;
  }

  if(process_common_fields(format, record)<0){
    return BGPSTREAM_FORMAT_CORRUPTED_MSG;
  }

  return BGPSTREAM_FORMAT_OK;
}

static bgpstream_format_status_t process_open_message(bgpstream_format_t *format, bgpstream_record_t *record){

  int json_str_len = FIELDLEN(body);
  char* json_str_ptr = FIELDPTR(body);
  parsebgp_error_t err;
  uint8_t* ptr = STATE->json_bytes_buffer;
  int msg_len = 0;

  // fields
  uint32_t asn4;
  uint16_t hold_time;
  uint32_t router_id;
  uint16_t total_length;
 size_t dec_len;
  bgpstream_addr_storage_t addr;

  // the first two bytes will be filled with the message length later
  int loc = 2;

  ptr[loc] = PARSEBGP_BGP_TYPE_OPEN;
  loc ++;

  /* add missing open message headers */

  // version
  ptr[loc] = 4;
  loc ++;

  // my autonomous system
  STRTOUL(asn, asn4);
  // if the ASN is 4 byte, use ASN23456 as placeholder
  uint16_t asn = htons( (uint16_t) (asn4 > UINT16_MAX ?  23456: asn4) );
  memcpy(&ptr[loc], &asn, sizeof(uint16_t));
  loc += 2;

  // hold time
  STRTOUL(hold_time, hold_time);
  memcpy(&ptr[loc], &hold_time, sizeof(uint16_t));
  loc += 2;

  // bgp identifier
  memcpy(STATE->field_buffer, FIELDPTR(router_id), FIELDLEN(router_id));
  STATE->field_buffer[FIELDLEN(router_id)] = '\0';
  if(bgpstream_str2addr((char*)STATE->field_buffer, &addr)==NULL){
    // if the field is not an IP address, it is then an 4 byte integer
    STRTOUL(router_id, router_id);
    router_id = htonl(router_id);
    memcpy(&ptr[loc], &router_id, sizeof(uint32_t));
  } else {
    memcpy(&ptr[loc], &addr.ipv4, sizeof(uint32_t));
  }
  loc += 4;

  // opt parm len
  if(json_str_ptr[0] == '0' && json_str_ptr[1] == '2'){
    // if body starts with correct parameter type "0x02"
    // no missing param type
    msg_len = json_str_len/2;
    ptr[loc] = (uint8_t) ( msg_len ); // adding
    loc ++;
  } else {
    // if misses param type
    msg_len = json_str_len/2 + 1 ;
    ptr[loc] = (uint8_t) ( msg_len ); // adding
    ptr[loc+1] = 2;
    loc += 2;
  }

  if(hexstr_to_bytes(&ptr[loc], (char*)FIELDPTR(body) , FIELDLEN(body))<0){
    return BGPSTREAM_FORMAT_CORRUPTED_MSG ;
  }

  // set msg length
  total_length = htons(msg_len+10+2+1);
  memcpy(ptr, &total_length, sizeof(total_length));

  dec_len = (size_t) total_length;
  if ((err = parsebgp_decode(STATE->opts, PARSEBGP_MSG_TYPE_BGP, RDATA->msg,
                             STATE->json_bytes_buffer, &dec_len)) != PARSEBGP_OK) {
    fprintf(stderr, "ERROR: Failed to parse message (%d:%s)\n", err, parsebgp_strerror(err));
    parsebgp_clear_msg(RDATA->msg);
    return BGPSTREAM_FORMAT_CORRUPTED_MSG ;
  }

  // process direction
  if( FIELDLEN(direction) == 4 && bcmp("sent", FIELDPTR(direction), FIELDLEN(direction)) == 0){
    // equals
    RDATA->open_msg_direction = 0;
  } else {
    RDATA->open_msg_direction = 1;
  }

  if(process_common_fields(format, record)<0){
    return BGPSTREAM_FORMAT_CORRUPTED_MSG;
  }

  return BGPSTREAM_FORMAT_OK;
}

static bgpstream_format_status_t process_unsupported_message(bgpstream_format_t *format, bgpstream_record_t *record){
  fprintf(stderr, "WARN: unsupported ris-stream message: %s\n", STATE->json_string_buffer);
  record -> status = BGPSTREAM_RECORD_STATUS_UNSUPPORTED_RECORD;
  record -> collector_name [0] = '\0';
  return BGPSTREAM_FORMAT_UNSUPPORTED_MSG;
}

static bgpstream_format_status_t process_corrupted_message(bgpstream_format_t *format, bgpstream_record_t *record){
  fprintf(stderr, "WARN: corrupted ris-stream message: %s\n",STATE->json_string_buffer);
  record -> status = BGPSTREAM_RECORD_STATUS_CORRUPTED_RECORD;
  record -> collector_name [0] = '\0';
  return BGPSTREAM_FORMAT_CORRUPTED_MSG;
}

#define PARSEFIELD(field)                                               \
  if (jsmn_streq(json_string, &t[i], STR(field)) == 1) {                \
    STATE->json_fields.field = (json_field_t) {value_ptr, value_size};  \
    i++;                                                                \
  }

static bgpstream_format_status_t bs_format_process_json_fields(bgpstream_format_t *format, bgpstream_record_t *record){
  int i;
  int r;
  bgpstream_format_status_t rc;
  jsmn_parser p;
  char* key_ptr, *value_ptr, *next_ptr;
  size_t key_size, value_size;

  jsmntok_t *t; /* We expect no more than 128 tokens */
  size_t tokcount = 128;
  char* json_string = (char*)&STATE->json_string_buffer;

  // prepare parser
  jsmn_init(&p);

  // allocate some tokens to start
  if ((t = malloc(sizeof(jsmntok_t) * tokcount)) == NULL) {
    fprintf(stderr, "ERROR: Could not malloc initial tokens\n");
    goto corrupted;
  }

again:
  if ((r = jsmn_parse(&p, json_string, strlen(json_string), t, tokcount)) < 0) {
    if (r == JSMN_ERROR_NOMEM) {
      tokcount *= 2;
      if ((t = realloc(t, sizeof(jsmntok_t) * tokcount)) == NULL) {
        fprintf(stderr, "ERROR: Could not realloc tokens\n");
        goto corrupted;
      }
      goto again;
    }
    if (r == JSMN_ERROR_INVAL) {
      fprintf(stderr, "ERROR: Invalid character in JSON string\n");
      goto corrupted;
    }
    fprintf(stderr, "ERROR: JSON parser returned %d\n", r);
    goto corrupted;
  }

  /* Assume the top-level element is an object */
  if (r < 1 || t[0].type != JSMN_OBJECT) {
    fprintf(stderr, "ERROR: JSON top-level not object\n");
    goto corrupted;
  }

  /* Loop over all fields of the json string buffer */
  for (i = 1; i < r; i++) {
    key_ptr = (char*)json_string + t[i].start;
    key_size = t[i].end-t[i].start;
    value_ptr = (char*)json_string + t[i+1].start;
    value_size = t[i+1].end-t[i+1].start;

    /* Assume the top-level element is an object */
    if (t[i].type != JSMN_STRING) {
      fprintf(stderr, "ERROR: JSON key not string\n");
      goto corrupted;
    }

    PARSEFIELD(body)
    else PARSEFIELD(timestamp)
    else PARSEFIELD(host)
    else PARSEFIELD(id)
    else PARSEFIELD(peer_asn)
    else PARSEFIELD(peer)
    else PARSEFIELD(type)
    else PARSEFIELD(asn)
    else PARSEFIELD(hold_time)
    else PARSEFIELD(router_id)
    else PARSEFIELD(direction)
    else PARSEFIELD(state)
    else PARSEFIELD(reason)
    else {
          // skipping all
          next_ptr = value_ptr + value_size;
          while((char*)json_string + t[i+1].start < next_ptr){
            i++;
          }
        }
  }

  // process each type of message separately
  switch(*STATE->json_fields.type.ptr){
  case 'A':
    RDATA->msg_type = RIPE_JSON_MSG_TYPE_ANNOUNCE;
    rc = process_update_message(format, record);
    break;
  case 'W':
    RDATA->msg_type = RIPE_JSON_MSG_TYPE_WITHDRAW;
    rc = process_update_message(format, record);
    break;
  case 'S':
    RDATA->msg_type = RIPE_JSON_MSG_TYPE_STATUS;
    rc = process_status_message(format, record);
    break;
  case 'O':
    RDATA->msg_type = RIPE_JSON_MSG_TYPE_OPEN;
    rc = process_open_message(format, record);
    break;
  case 'N':
    RDATA->msg_type = RIPE_JSON_MSG_TYPE_NOTIFY;
    rc = BGPSTREAM_FORMAT_UNSUPPORTED_MSG;
    break;
  default:
    rc = BGPSTREAM_FORMAT_UNSUPPORTED_MSG;
    break;
  }

  switch(rc){
  case BGPSTREAM_FORMAT_OK:
    goto ok;
  case BGPSTREAM_FORMAT_CORRUPTED_MSG:
    goto corrupted;
  case BGPSTREAM_FORMAT_UNSUPPORTED_MSG:
    goto unsupported;
  // all status codes below should not happen
  case BGPSTREAM_FORMAT_FILTERED_DUMP:
  case BGPSTREAM_FORMAT_EMPTY_DUMP:
  case BGPSTREAM_FORMAT_CANT_OPEN_DUMP:
  case BGPSTREAM_FORMAT_CORRUPTED_DUMP:
  case BGPSTREAM_FORMAT_END_OF_DUMP:
  case BGPSTREAM_FORMAT_OUTSIDE_TIME_INTERVAL:
  case BGPSTREAM_FORMAT_READ_ERROR:
  case BGPSTREAM_FORMAT_UNKNOWN_ERROR:
    goto corrupted;
    break;
  }

 ok:
  return BGPSTREAM_FORMAT_OK;

 corrupted:
  return process_corrupted_message(format, record);
 unsupported:
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
  int newread, rc;

  newread = bgpstream_transport_readline(format->transport, &STATE->json_string_buffer, BGPSTREAM_PARSEBGP_BUFLEN);

  assert(newread < BGPSTREAM_PARSEBGP_BUFLEN);

  if (newread <0 ) {
    record -> status = BGPSTREAM_RECORD_STATUS_CORRUPTED_RECORD;
    record -> collector_name [0] = '\0';
    return BGPSTREAM_FORMAT_CORRUPTED_DUMP;
  } else if ( newread == 0 ){
    return BGPSTREAM_FORMAT_END_OF_DUMP;
  }

  if( ( rc = bs_format_process_json_fields(format, record) ) != 0){
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

  switch(RDATA->msg_type){
  case RIPE_JSON_MSG_TYPE_ANNOUNCE:
  case RIPE_JSON_MSG_TYPE_WITHDRAW:
    rc = bgpstream_parsebgp_process_update(&RDATA->upd_state, RDATA->elem,
                                               RDATA->msg->types.bgp);
    if (rc <= 0) {
      return rc;
    }
    break;
  case RIPE_JSON_MSG_TYPE_STATUS:
    RDATA->elem->type = BGPSTREAM_ELEM_TYPE_PEERSTATE;
    if(RDATA->status_msg_state == 0){
      // "down" state
      RDATA->elem->old_state = BGPSTREAM_ELEM_PEERSTATE_UNKNOWN;
      RDATA->elem->new_state = BGPSTREAM_ELEM_PEERSTATE_IDLE;
    } else if (RDATA->status_msg_state == 1  ){
      // "connected" state
      RDATA->elem->old_state = BGPSTREAM_ELEM_PEERSTATE_UNKNOWN;
      RDATA->elem->new_state = BGPSTREAM_ELEM_PEERSTATE_ESTABLISHED;
    } else {
      RDATA->elem->old_state = BGPSTREAM_ELEM_PEERSTATE_UNKNOWN;
      RDATA->elem->new_state = BGPSTREAM_ELEM_PEERSTATE_UNKNOWN;
      fprintf(stderr, "WARN: unsupported status type, %d\n", RDATA->status_msg_state);
    }
    RDATA->end_of_elems = 1;
    rc = 1;
    break;
  case RIPE_JSON_MSG_TYPE_OPEN:
    RDATA->elem->type = BGPSTREAM_ELEM_TYPE_PEERSTATE;
    if(RDATA->open_msg_direction == 0){
      // "sent" OPEN
      RDATA->elem->old_state = BGPSTREAM_ELEM_PEERSTATE_UNKNOWN;
      RDATA->elem->new_state = BGPSTREAM_ELEM_PEERSTATE_OPENSENT;
    } else {
      // "received" OPEN
      RDATA->elem->old_state = BGPSTREAM_ELEM_PEERSTATE_UNKNOWN;
      RDATA->elem->new_state = BGPSTREAM_ELEM_PEERSTATE_OPENCONFIRM;
    }
    RDATA->end_of_elems = 1;
    rc = 1;
    break;
  case RIPE_JSON_MSG_TYPE_NOTIFY:
    break;
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
