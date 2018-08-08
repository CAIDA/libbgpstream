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

#define STATE ((state_t*)(format->state))

#define RDATA ((rec_data_t *)(record->__int->data))

typedef struct peer_index_entry {

  /** Peer ASN */
  uint32_t peer_asn;

  /** Peer IP */
  bgpstream_addr_storage_t peer_ip;

} peer_index_entry_t;

KHASH_INIT(td2_peer, int, peer_index_entry_t, 1, kh_int_hash_func,
           kh_int_hash_equal);

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

  // parsebgp decode wrapper state
  bgpstream_parsebgp_decode_state_t decoder;

  // state to store the "peer index table" when reading TABLE_DUMP_V2 records
  khash_t(td2_peer) *peer_table;

} state_t;


static int handle_bgp4mp_state_change(rec_data_t *rd,
                                      parsebgp_mrt_bgp4mp_t *bgp4mp)
{
  rd->elem->type = BGPSTREAM_ELEM_TYPE_PEERSTATE;
  rd->elem->old_state = bgp4mp->data.state_change.old_state;
  rd->elem->new_state = bgp4mp->data.state_change.new_state;
  rd->end_of_elems = 1;
  return 1;
}

static int handle_bgp4mp(rec_data_t *rd, parsebgp_mrt_msg_t *mrt)
{
  int rc = 0;
  parsebgp_mrt_bgp4mp_t *bgp4mp = mrt->types.bgp4mp;

  // no originated time information in BGP4MP
  rd->elem->orig_time_sec = 0;
  rd->elem->orig_time_usec = 0;

  COPY_IP(&rd->elem->peer_ip, bgp4mp->afi, bgp4mp->peer_ip, return 0);
  rd->elem->peer_asn = bgp4mp->peer_asn;
  // other elem fields are specific to the message

  switch (mrt->subtype) {
  case PARSEBGP_MRT_BGP4MP_STATE_CHANGE:
  case PARSEBGP_MRT_BGP4MP_STATE_CHANGE_AS4:
    rc = handle_bgp4mp_state_change(rd, bgp4mp);
    break;

  case PARSEBGP_MRT_BGP4MP_MESSAGE:
  case PARSEBGP_MRT_BGP4MP_MESSAGE_AS4:
  case PARSEBGP_MRT_BGP4MP_MESSAGE_LOCAL:
  case PARSEBGP_MRT_BGP4MP_MESSAGE_AS4_LOCAL:
    rc = bgpstream_parsebgp_process_update(&rd->upd_state, rd->elem,
                                           bgp4mp->data.bgp_msg);
    if (rc == 0) {
      rd->end_of_elems = 1;
    }
    break;

  default:
    bgpstream_log(BGPSTREAM_LOG_WARN,
                  "Skipping unknown BGP4MP record subtype %d", mrt->subtype);
    break;
  }

  return rc;
}

/* -------------------- RECORD FILTERING -------------------- */

static int is_wanted_time(uint32_t record_time,
                          bgpstream_filter_mgr_t *filter_mgr)
{
  bgpstream_interval_filter_t *tif;

  if (filter_mgr->time_intervals == NULL) {
    // no time filtering
    return 1;
  }

  tif = filter_mgr->time_intervals;

  while (tif != NULL) {
    if (record_time >= tif->begin_time &&
        (tif->end_time == BGPSTREAM_FOREVER || record_time <= tif->end_time)) {
      // matches a filter interval
      return 1;
    }
    tif = tif->next;
  }

  return 0;
}

static int handle_td2_peer_index(bgpstream_format_t *format,
                                 parsebgp_mrt_table_dump_v2_peer_index_t *pi)
{
  int i;
  khiter_t k;
  int khret;
  peer_index_entry_t *bs_pie;
  parsebgp_mrt_table_dump_v2_peer_entry_t *pie;

  // alloc the table hash
  if ((STATE->peer_table = kh_init(td2_peer)) == NULL) {
    return -1;
  }

  // add peers to the table
  for (i = 0; i < pi->peer_count; i++) {
    k = kh_put(td2_peer, STATE->peer_table, i, &khret);
    if (khret == -1) {
      return -1;
    }

    pie = &pi->peer_entries[i];
    bs_pie = &kh_val(STATE->peer_table, k);

    bs_pie->peer_asn = pie->asn;
    COPY_IP(&bs_pie->peer_ip, pie->ip_afi, pie->ip, return -1);
  }

  return 0;
}

static bgpstream_parsebgp_check_filter_rc_t
populate_filter_cb(bgpstream_format_t *format, bgpstream_record_t *record,
                   parsebgp_msg_t *msg)
{
  uint32_t ts_sec;
  assert(msg->type == PARSEBGP_MSG_TYPE_MRT);

  // if this is a peer index table message, we parse it now and move on (we
  // could also add a "filtered" flag to the peer_index_entry_t struct so that
  // when elem parsing happens it can quickly filter out unwanted peers
  // without having to check ASN or IP
  if (msg->types.mrt->type == PARSEBGP_MRT_TYPE_TABLE_DUMP_V2 &&
      msg->types.mrt->subtype ==
      PARSEBGP_MRT_TABLE_DUMP_V2_PEER_INDEX_TABLE) {
    if (handle_td2_peer_index(
          format, &msg->types.mrt->types.table_dump_v2->peer_index) != 0) {
      bgpstream_log(BGPSTREAM_LOG_ERR, "Failed to process Peer Index Table");
      return -1;
    }
    // indicate that we want this message SKIPPED
    return BGPSTREAM_PARSEBGP_SKIP;
  }

  // set record timestamps
  ts_sec = record->time_sec = msg->types.mrt->timestamp_sec;
  record->time_usec = msg->types.mrt->timestamp_usec;

  // ensure the router fields are unset
  record->router_name[0] = '\0';
  record->router_ip.version = 0;

  // check the filters

  // is this above all of our intervals?
  if (format->filter_mgr->time_intervals != NULL &&
      format->filter_mgr->time_intervals_max != BGPSTREAM_FOREVER &&
      ts_sec > format->filter_mgr->time_intervals_max) {
    // force EOS
    return BGPSTREAM_PARSEBGP_EOS;
  }

  if (is_wanted_time(ts_sec, format->filter_mgr) != 0) {
    // we want this entry
    return BGPSTREAM_PARSEBGP_KEEP;
  } else {
    return BGPSTREAM_PARSEBGP_FILTER_OUT;
  }
}

/* ==================== PUBLIC API BELOW HERE ==================== */

int bs_format_ripejson_create(bgpstream_format_t *format,
                         bgpstream_resource_t *res)
{
  BS_FORMAT_SET_METHODS(ripejson, format);
  parsebgp_opts_t *opts = NULL;

  if ((format->state = malloc_zero(sizeof(state_t))) == NULL) {
    return -1;
  }

  STATE->decoder.msg_type = PARSEBGP_MSG_TYPE_MRT;

  opts = &STATE->decoder.parser_opts;
  parsebgp_opts_init(opts);
  bgpstream_parsebgp_opts_init(opts);

  return 0;
}

bgpstream_format_status_t
bs_format_ripejson_populate_record(bgpstream_format_t *format,
                              bgpstream_record_t *record)
{

  // FIXME: record only need to populate timestamp and host
  printf("ripejson populate record!\n");
  record -> time_sec = 1;
  record -> time_usec = 100;

  strcpy(record -> project_name , "ripe-stream");
  strcpy(record -> collector_name , "rrrrr");

  unsigned char buffer[2048];

  int newread = bgpstream_transport_readline(format->transport, &buffer, BGPSTREAM_PARSEBGP_BUFLEN);

  if (newread <0) {
    printf("read error!\n");
    return newread;
  } else if ( newread == 0 ){
    printf("read finished!\n");
    return BGPSTREAM_FORMAT_END_OF_DUMP;
  }

  printf("newread = %d\nbuffer: %s\nstrlen(buffer): %d\n", newread, buffer, strlen(buffer));

  bs_format_extract_json_fields(buffer);

    // return bgpstream_parsebgp_populate_record(&STATE->decoder, RDATA->msg, format,
    //                                           record, NULL, populate_filter_cb);
  return BGPSTREAM_FORMAT_OK;
}

static int jsoneq(const char *json, jsmntok_t *tok, const char *s) {
  if (tok->type == JSMN_STRING && (int) strlen(s) == tok->end - tok->start &&
      strncmp(json + tok->start, s, tok->end - tok->start) == 0) {
    return 0;
  }
  return -1;
}

int bs_format_extract_json_fields(const char* JSON_STRING){
  int i;
  int r;
  jsmn_parser p;
  jsmntok_t t[128]; /* We expect no more than 128 tokens */

  jsmn_init(&p);
  r = jsmn_parse(&p, JSON_STRING, strlen(JSON_STRING), t, sizeof(t)/sizeof(t[0]));
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
    if (jsoneq(JSON_STRING, &t[i], "body") == 0) {
      /* We may use strndup() to fetch string value */
      printf("- body: %.*s\n", t[i+1].end-t[i+1].start,
          JSON_STRING + t[i+1].start);
      i++;
    } else if (jsoneq(JSON_STRING, &t[i], "timestamp") == 0) {
      /* We may additionally check if the value is either "true" or "false" */
      printf("- timestamp: %.*s\n", t[i+1].end-t[i+1].start,
          JSON_STRING + t[i+1].start);
      i++;
    } else if (jsoneq(JSON_STRING, &t[i], "host") == 0) {
      /* We may want to do strtol() here to get numeric value */
      printf("- host: %.*s\n", t[i+1].end-t[i+1].start,
          JSON_STRING + t[i+1].start);
      i++;
    } else if (jsoneq(JSON_STRING, &t[i], "id") == 0) {
      /* We may want to do strtol() here to get numeric value */
      printf("- id: %.*s\n", t[i+1].end-t[i+1].start,
          JSON_STRING + t[i+1].start);
      i++;
    } else {
      // printf("Unexpected key: %.*s\n", t[i].end-t[i].start,
      //     JSON_STRING + t[i].start);
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
  if (STATE->peer_table != NULL) {
    kh_destroy(td2_peer, STATE->peer_table);
    STATE->peer_table = NULL;
  }

  free(format->state);
  format->state = NULL;
}
