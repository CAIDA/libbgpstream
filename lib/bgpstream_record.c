/*
 * Copyright (C) 2014 The Regents of the University of California.
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
 *   Shane Alcock <salcock@waikato.ac.nz>
 */

#include "bgpstream_record.h"
#include "bgpstream_elem_int.h"
#include "bgpstream_format_interface.h" //< to access filter mgr
#include "bgpstream_int.h"
#include "bgpstream_log.h"
#include "utils.h"
#include <assert.h>
#include <inttypes.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>

bgpstream_record_t *bgpstream_record_create(bgpstream_format_t *format)
{
  bgpstream_record_t *record;

  if ((record = (bgpstream_record_t *)malloc_zero(
         sizeof(bgpstream_record_t))) == NULL ||
      (record->__int = malloc_zero(sizeof(bgpstream_record_internal_t))) ==
        NULL) {
    bgpstream_record_destroy(record);
    return NULL;
  }

  record->__int->format = format;
  bgpstream_format_init_data(record);

  return record;
}

void bgpstream_record_destroy(bgpstream_record_t *record)
{
  if (record == NULL) {
    return;
  }

  bgpstream_format_destroy_data(record);

  free(record->__int);
  free(record);
}

/* NOTE: this function deliberately does not reset many of the fields in a
   record, since in the v2 implementation of BGPStream records are specific to a
   reader and thus these fields can be reused between reads. */
void bgpstream_record_clear(bgpstream_record_t *record)
{
  bgpstream_format_clear_data(record);

  // reset the record timestamps
  record->time_sec = 0;
  record->time_usec = 0;
}

void bgpstream_record_print_mrt_data(bgpstream_record_t *const record)
{
  assert(0 && "TODO: fix this implementation");
  // bgpdump_print_entry(record->bd_entry);
}

static int bgpstream_elem_prefix_match(bgpstream_patricia_tree_t *prefixes,
                                       bgpstream_pfx_t *search)
{

  bgpstream_patricia_tree_result_set_t *res = NULL;
  bgpstream_patricia_node_t *it;
  int matched = 0;

  /* If this is an exact match, the allowable matches don't matter */
  if (bgpstream_patricia_tree_search_exact(prefixes, search)) {
    return 1;
  }

  bgpstream_patricia_node_t *n =
    bgpstream_patricia_tree_insert(prefixes, search);

  /* Check for less specific prefixes that have the "MORE" match flag */
  res = bgpstream_patricia_tree_result_set_create();
  bgpstream_patricia_tree_get_less_specifics(prefixes, n, res);

  while ((it = bgpstream_patricia_tree_result_set_next(res)) != NULL) {
    bgpstream_pfx_t *pfx = bgpstream_patricia_tree_get_pfx(it);

    if (pfx->allowed_matches == BGPSTREAM_PREFIX_MATCH_ANY ||
        pfx->allowed_matches == BGPSTREAM_PREFIX_MATCH_MORE) {
      matched = 1;
      goto endmatch;
    }
  }

  bgpstream_patricia_tree_get_more_specifics(prefixes, n, res);

  while ((it = bgpstream_patricia_tree_result_set_next(res)) != NULL) {
    bgpstream_pfx_t *pfx = bgpstream_patricia_tree_get_pfx(it);

    /* TODO maybe have a way of limiting the amount of bits we are allowed to
     * go back? or make it specifiable via the language? */
    if (pfx->allowed_matches == BGPSTREAM_PREFIX_MATCH_ANY ||
        pfx->allowed_matches == BGPSTREAM_PREFIX_MATCH_LESS) {
      matched = 1;
      goto endmatch;
    }
  }

endmatch:
  bgpstream_patricia_tree_result_set_destroy(&res);
  bgpstream_patricia_tree_remove_node(prefixes, n);
  return matched;
}

static int elem_check_filters(bgpstream_record_t *record,
                              bgpstream_elem_t *elem)
{
  bgpstream_filter_mgr_t *filter_mgr = record->__int->format->filter_mgr;

  /* First up, check if this element is the right type */
  if (filter_mgr->elemtype_mask) {

    if (elem->type == BGPSTREAM_ELEM_TYPE_PEERSTATE &&
        !(filter_mgr->elemtype_mask & BGPSTREAM_FILTER_ELEM_TYPE_PEERSTATE)) {
      return 0;
    }

    if (elem->type == BGPSTREAM_ELEM_TYPE_RIB &&
        !(filter_mgr->elemtype_mask & BGPSTREAM_FILTER_ELEM_TYPE_RIB)) {
      return 0;
    }

    if (elem->type == BGPSTREAM_ELEM_TYPE_ANNOUNCEMENT &&
        !(filter_mgr->elemtype_mask &
          BGPSTREAM_FILTER_ELEM_TYPE_ANNOUNCEMENT)) {
      return 0;
    }

    if (elem->type == BGPSTREAM_ELEM_TYPE_WITHDRAWAL &&
        !(filter_mgr->elemtype_mask & BGPSTREAM_FILTER_ELEM_TYPE_WITHDRAWAL)) {
      return 0;
    }
  }

  /* Checking peer ASNs: if the filter is on and the peer asn is not in the
   * set, return 0 */
  if (filter_mgr->peer_asns &&
      bgpstream_id_set_exists(filter_mgr->peer_asns, elem->peer_asn) == 0) {
    return 0;
  }

  /* Checking origin ASN */
  if (filter_mgr->origin_asns) {
    if (elem->type == BGPSTREAM_ELEM_TYPE_WITHDRAWAL ||
        elem->type == BGPSTREAM_ELEM_TYPE_PEERSTATE) {
      return 0;
    }
    uint32_t origin_asn;

    if (bgpstream_as_path_get_origin_val(elem->as_path, &origin_asn) < 0) {
      return 0;
    }

    if (bgpstream_id_set_exists(filter_mgr->origin_asns, origin_asn) == 0) {
      return 0;
    }
  }

  if (filter_mgr->ipversion) {
    /* Determine address version for the element prefix */

    if (elem->type == BGPSTREAM_ELEM_TYPE_PEERSTATE) {
      return 0;
    }

    if (elem->prefix.address.version != filter_mgr->ipversion)
      return 0;
  }

  if (filter_mgr->prefixes) {
    if (elem->type == BGPSTREAM_ELEM_TYPE_PEERSTATE) {
      return 0;
    }
    if (bgpstream_elem_prefix_match(filter_mgr->prefixes, &elem->prefix) == 0)
      return 0;
  }

  /* Checking AS Path expressions */
  if (filter_mgr->aspath_exprs) {
    char aspath[65536];
    int pathlen;

    if (elem->type == BGPSTREAM_ELEM_TYPE_WITHDRAWAL ||
        elem->type == BGPSTREAM_ELEM_TYPE_PEERSTATE) {
      return 0;
    }

    pathlen = bgpstream_as_path_get_filterable(aspath, 65535, elem->as_path);

    if (pathlen >= sizeof(aspath)) {
      bgpstream_log(BGPSTREAM_LOG_WARN,
                    "AS Path is too long? Filter may not work well.");
    }

    for (int i = 0; i < filter_mgr->aspath_expr_cnt; i++) {
      int result = regexec(filter_mgr->aspath_exprs[i].re, aspath, 0, NULL, 0);
      // All aspath regexes must match
      if ((result == 0) != (filter_mgr->aspath_exprs[i].negate == 0)) {
        return 0;
      }
    }
  }

  /* Checking communities (unless it is a withdrawal message) */
  if (filter_mgr->communities) {
    int pass = 0;
    if (elem->type == BGPSTREAM_ELEM_TYPE_WITHDRAWAL ||
        elem->type == BGPSTREAM_ELEM_TYPE_PEERSTATE) {
      return 0;
    }

    bgpstream_community_t *c;
    khiter_t k;
    for (k = kh_begin(filter_mgr->communities);
         k != kh_end(filter_mgr->communities); ++k) {
      if (kh_exist(filter_mgr->communities, k)) {
        c = &(kh_key(filter_mgr->communities, k));
        if (bgpstream_community_set_match(
              elem->communities, c, kh_value(filter_mgr->communities, k))) {
          pass = 1;
          break;
        }
      }
    }
    if (pass == 0) {
      return 0;
    }
  }

  return 1;
}

int bgpstream_record_get_next_elem(bgpstream_record_t *record,
                                   bgpstream_elem_t **elemp)
{
  int rc;
  bgpstream_elem_t *elem = NULL;
  *elemp = NULL;

  if (record == NULL ||
      record->status != BGPSTREAM_RECORD_STATUS_VALID_RECORD ||
      record->__int->format == NULL) {
    return 0; // treat as end-of-elems
  }

  while (elem == NULL) {
    if ((rc = bgpstream_format_get_next_elem(record->__int->format, record,
                                             &elem)) <= 0) {
      // either error or end-of-elems
      return rc;
    }

    // TODO: push elem filtering down into the formats
    if (elem_check_filters(record, elem) == 0) {
      elem = NULL;
    }
  }

  *elemp = elem;
  return 1;
}

int bgpstream_record_type_snprintf(char *buf, size_t len,
                                   bgpstream_record_type_t type)
{
  /* ensure we have enough bytes to write our single character */
  if (len == 0) {
    return -1;
  } else if (len == 1) {
    buf[0] = '\0';
    return -1;
  }
  switch (type) {
  case BGPSTREAM_RIB:
    buf[0] = 'R';
    break;
  case BGPSTREAM_UPDATE:
    buf[0] = 'U';
    break;
  default:
    buf[0] = '\0';
    break;
  }
  buf[1] = '\0';
  return 1;
}

int bgpstream_record_dump_pos_snprintf(char *buf, size_t len,
                                       bgpstream_dump_position_t dump_pos)
{
  /* ensure we have enough bytes to write our single character */
  if (len == 0) {
    return -1;
  } else if (len == 1) {
    buf[0] = '\0';
    return -1;
  }

  switch (dump_pos) {
  case BGPSTREAM_DUMP_START:
    buf[0] = 'B';
    break;
  case BGPSTREAM_DUMP_MIDDLE:
    buf[0] = 'M';
    break;
  case BGPSTREAM_DUMP_END:
    buf[0] = 'E';
    break;
  default:
    buf[0] = '\0';
    break;
  }
  buf[1] = '\0';
  return 1;
}

int bgpstream_record_status_snprintf(char *buf, size_t len,
                                     bgpstream_record_status_t status)
{
  /* ensure we have enough bytes to write our single character */
  if (len == 0) {
    return -1;
  } else if (len == 1) {
    buf[0] = '\0';
    return -1;
  }

  switch (status) {
  case BGPSTREAM_RECORD_STATUS_VALID_RECORD:
    buf[0] = 'V';
    break;
  case BGPSTREAM_RECORD_STATUS_FILTERED_SOURCE:
    buf[0] = 'F';
    break;
  case BGPSTREAM_RECORD_STATUS_EMPTY_SOURCE:
    buf[0] = 'E';
    break;
  case BGPSTREAM_RECORD_STATUS_OUTSIDE_TIME_INTERVAL:
    buf[0] = 'O';
    break;
  case BGPSTREAM_RECORD_STATUS_CORRUPTED_SOURCE:
    buf[0] = 'S';
    break;
  case BGPSTREAM_RECORD_STATUS_CORRUPTED_RECORD:
    buf[0] = 'R';
    break;
  case BGPSTREAM_RECORD_STATUS_UNSUPPORTED_RECORD:
    buf[0] = 'U';
    break;
  }
  buf[1] = '\0';
  return 1;
}

#define B_REMAIN (len - written)
#define B_FULL (written >= len)
#define ADD_PIPE                                                               \
  do {                                                                         \
    if (B_REMAIN > 1) {                                                        \
      *buf_p = '|';                                                            \
      buf_p++;                                                                 \
      *buf_p = '\0';                                                           \
      written++;                                                               \
    } else {                                                                   \
      return NULL;                                                             \
    }                                                                          \
  } while (0)

#define SEEK_STR_END                                                           \
  do {                                                                         \
    while (*buf_p != '\0') {                                                   \
      written++;                                                               \
      buf_p++;                                                                 \
    }                                                                          \
  } while (0)

char *bgpstream_record_snprintf(char *buf, size_t len,
                                bgpstream_record_t *record)
{
  assert(record);

  size_t written = 0; /* < how many bytes we wanted to write */
  ssize_t c = 0;      /* < how many chars were written */
  char *buf_p = buf;

  /* Record type */
  if ((c = bgpstream_record_type_snprintf(buf_p, B_REMAIN, record->type)) < 0) {
    return NULL;
  }
  written += c;
  buf_p += c;
  ADD_PIPE;

  /* record position */
  if ((c = bgpstream_record_dump_pos_snprintf(buf_p, len - written,
                                              record->dump_pos)) < 0) {
    return NULL;
  }
  written += c;
  buf_p += c;
  ADD_PIPE;

  if (B_FULL)
    return NULL;

  /* Record timestamp, project, collector, router names */
  c = snprintf(buf_p, B_REMAIN, "%" PRIu32 ".%06" PRIu32 "|%s|%s|%s|",
               record->time_sec, record->time_usec, record->project_name,
               record->collector_name, record->router_name);
  written += c;
  buf_p += c;

  if (B_FULL)
    return NULL;

  /* Router IP */
  if (record->router_ip.version != 0) {
    if (bgpstream_addr_ntop(buf_p, B_REMAIN, &record->router_ip) == NULL) {
      bgpstream_log(BGPSTREAM_LOG_ERR, "Malformed Router IP address");
      return NULL;
    }
    SEEK_STR_END;
  }
  ADD_PIPE;

  /* record status */
  if ((c = bgpstream_record_status_snprintf(buf_p, len - written,
                                            record->status)) < 0) {
    return NULL;
  }
  written += c;
  buf_p += c;

  if (B_FULL)
    return NULL;

  /* dump time */
  c = snprintf(buf_p, B_REMAIN, "|%" PRIu32, record->dump_time_sec);
  written += c;
  buf_p += c;

  if (B_FULL)
    return NULL;

  return buf;
}

char *bgpstream_record_elem_snprintf(char *buf, size_t len,
                                     bgpstream_record_t *record,
                                     bgpstream_elem_t *elem)
{
  assert(record);
  assert(elem);

  size_t written = 0; /* < how many bytes we wanted to write */
  ssize_t c = 0;      /* < how many chars were written */
  char *buf_p = buf;

  /* Record type */
  if ((c = bgpstream_record_type_snprintf(buf_p, B_REMAIN, record->type)) < 0) {
    return NULL;
  }
  written += c;
  buf_p += c;
  ADD_PIPE;

  /* Elem type */
  if ((c = bgpstream_elem_type_snprintf(buf_p, B_REMAIN, elem->type)) < 0) {
    return NULL;
  }
  written += c;
  buf_p += c;
  ADD_PIPE;

  /* Record timestamp, project, collector, router names */
  c = snprintf(buf_p, B_REMAIN, "%" PRIu32 ".%06" PRIu32 "|%s|%s|%s|",
               record->time_sec, record->time_usec, record->project_name,
               record->collector_name, record->router_name);
  written += c;
  buf_p += c;

  if (B_FULL)
    return NULL;

  /* Router IP */
  if (record->router_ip.version != 0) {
    if (bgpstream_addr_ntop(buf_p, B_REMAIN, &record->router_ip) == NULL) {
      bgpstream_log(BGPSTREAM_LOG_ERR, "Malformed Router IP address");
      return NULL;
    }
    SEEK_STR_END;
  }
  ADD_PIPE;

  if (bgpstream_elem_custom_snprintf(buf_p, B_REMAIN, elem, 0) == NULL) {
    return NULL;
  }

  written += c;
  buf_p += c;

  if (B_FULL)
    return NULL;

  return buf;
}
