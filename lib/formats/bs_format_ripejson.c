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

typedef struct peer_index_entry {

  /** Peer ASN */
  uint32_t peer_asn;

  /** Peer IP */
  bgpstream_addr_storage_t peer_ip;

} peer_index_entry_t;

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

} rec_data_t;

typedef struct state {
  // options
  parsebgp_opts_t opts;

  // read buffer
  uint8_t json_string_buffer[BGPSTREAM_PARSEBGP_BUFLEN];

  uint8_t json_bytes_buffer[4096];

} state_t;


/* ==================== RECORD FILTERING ==================== */

/* ==================== PUBLIC API BELOW HERE ==================== */

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

  printf("ripejson populate record!\n");
  record -> time_sec = 1;
  record -> time_usec = 100;

  strcpy(record -> project_name , "ripe-stream");
  strcpy(record -> collector_name , "rrrrr");


  int newread = bgpstream_transport_readline(format->transport, &STATE->json_string_buffer, BGPSTREAM_PARSEBGP_BUFLEN);

  if (newread <0 || newread >= BGPSTREAM_PARSEBGP_BUFLEN) {
    printf("read error!\n");
    return newread;
  } else if ( newread == 0 ){
    printf("read finished!\n");
    return BGPSTREAM_FORMAT_END_OF_DUMP;
  }

  printf("newread = %d\nbuffer: %s\nstrlen(buffer): %d\n", newread, STATE->json_string_buffer, strlen(STATE->json_string_buffer));

  bs_format_extract_json_fields(format, record);

    // return bgpstream_parsebgp_populate_record(&STATE->decoder, RDATA->msg, format,
    //                                           record, NULL, populate_filter_cb);
  return BGPSTREAM_FORMAT_OK;
}

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

int bs_format_extract_json_fields(bgpstream_format_t *format, bgpstream_record_t *record){
  int i;
  int r;
  jsmn_parser p;
  jsmntok_t t[128]; /* We expect no more than 128 tokens */

  uint8_t* json_string = &STATE->json_string_buffer;

  jsmn_init(&p);
  r = jsmn_parse(&p, json_string, strlen(json_string), t, sizeof(t)/sizeof(t[0]));
  if (r < 0) {
    printf("Failed to parse JSON: %d\n", r);
    return 1;
  }

  /* Assume the top-level element is an object */
  if (r < 1 || t[0].type != JSMN_OBJECT) {
    printf("Object expected\n");
    return 1;
  }

  /* Loop over all keys of the root object */
  for (i = 1; i < r; i++) {
    if (jsoneq(json_string, &t[i], "body") == 0) {
      /* We may use strndup() to fetch string value */
      printf("- body: %.*s\n", t[i+1].end-t[i+1].start,
          json_string + t[i+1].start);

      unsigned char* body_ptr = json_string + t[i+1].start;
      size_t body_size = t[i+1].end-t[i+1].start;

      ssize_t dec_len =  hexstr_to_bgpmsg(STATE->json_bytes_buffer, 4096, body_ptr, body_size, 2);
      size_t err;

      printf("before parsing, dec_len = %ld, body_size = %d\n", dec_len, body_size);

      if ((err = parsebgp_decode(STATE->opts, PARSEBGP_MSG_TYPE_BGP, RDATA->msg,
                                 STATE->json_bytes_buffer, &dec_len)) != PARSEBGP_OK) {
        fprintf(stderr, "ERROR: Failed to parse message (%d:%s)\n", err, parsebgp_strerror(err));
        parsebgp_clear_msg(RDATA->msg);
      }
      // FIXME: record with community fields would crash the `parsebgp_dump_msg`
      parsebgp_dump_msg(RDATA->msg);
      fprintf(stderr, "after parsing\n");

      i++;
    } else if (jsoneq(json_string, &t[i], "timestamp") == 0) {
      /* We may additionally check if the value is either "true" or "false" */
      printf("- timestamp: %.*s\n", t[i+1].end-t[i+1].start, json_string + t[i+1].start);
      i++;
    } else if (jsoneq(json_string, &t[i], "host") == 0) {
      /* We may want to do strtol() here to get numeric value */
      printf("- host: %.*s\n", t[i+1].end-t[i+1].start, json_string + t[i+1].start);
      i++;
    } else if (jsoneq(json_string, &t[i], "id") == 0) {
      /* We may want to do strtol() here to get numeric value */
      printf("- id: %.*s\n", t[i+1].end-t[i+1].start, json_string + t[i+1].start);
      i++;
    } else if (jsoneq(json_string, &t[i], "peer_asn") == 0) {
      /* We may want to do strtol() here to get numeric value */
      printf("- peer_asn: %.*s\n", t[i+1].end-t[i+1].start, json_string + t[i+1].start);
      i++;
    } else {
      // printf("Unexpected key: %.*s\n", t[i].end-t[i].start,
      //     json_string + t[i].start);
    }
  }
  return EXIT_SUCCESS;

}

int bs_format_ripejson_get_next_elem(bgpstream_format_t *format,
                                bgpstream_record_t *record,
                                bgpstream_elem_t **elem)
{
  parsebgp_mrt_msg_t *mrt;
  int rc;

  elem = NULL;

  // if (RDATA == NULL || RDATA->end_of_elems != 0) {
  //   // end-of-elems
  //   return 0;
  // }

  // mrt = RDATA->msg->types.mrt;
  // switch (mrt->type) {
  // case PARSEBGP_MRT_TYPE_BGP4MP:
  // case PARSEBGP_MRT_TYPE_BGP4MP_ET:
  //   rc = handle_bgp4mp(RDATA, mrt);
  //   break;

  // default:
  //   // a type we don't care about, so return end-of-elems
  //   bgpstream_log(BGPSTREAM_LOG_WARN, "Skipping unknown MRT record type %d",
  //                 mrt->type);
  //   rc = 0;
  //   break;
  // }

  // if (rc <= 0) {
  //   return rc;
  // }

  // // return a borrowed pointer to the elem we populated
  // *elem = RDATA->elem;
  return 0;
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
