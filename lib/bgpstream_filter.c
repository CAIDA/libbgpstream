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

#include "bgpstream_filter.h"
#include "bgpstream_log.h"
#include "utils.h"
#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>

/* allocate memory for a new bgpstream filter */
bgpstream_filter_mgr_t *bgpstream_filter_mgr_create()
{
  bgpstream_log(BGPSTREAM_LOG_VFINE, "\tBSF_MGR: create start");
  bgpstream_filter_mgr_t *bs_filter_mgr =
    (bgpstream_filter_mgr_t *)malloc_zero(sizeof(bgpstream_filter_mgr_t));
  if (bs_filter_mgr == NULL) {
    return NULL; // can't allocate memory
  }
  bgpstream_log(BGPSTREAM_LOG_VFINE, "\tBSF_MGR: create end");
  return bs_filter_mgr;
}

// Create *setp if needed, and insert value into *setp.
// Returns 1 for success, 0 for failure.
static int bsf_id_set_insert(bgpstream_id_set_t **setp, uint32_t value)
{
  if (*setp == NULL && (*setp = bgpstream_id_set_create()) == NULL) {
    bgpstream_log(BGPSTREAM_LOG_VFINE,
                  "\tBSF_MGR:: add_filter malloc failed");
    bgpstream_log(BGPSTREAM_LOG_ERR, "can't allocate memory");
    return 0;
  }
  return bgpstream_id_set_insert(*setp, value) >= 0;
}

// Create *setp if needed, and insert value into *setp.
// Returns 1 for success, 0 for failure.
static int bsf_str_set_insert(bgpstream_str_set_t **setp, const char *value)
{
  if (*setp == NULL && (*setp = bgpstream_str_set_create()) == NULL) {
    bgpstream_log(BGPSTREAM_LOG_VFINE,
                  "\tBSF_MGR:: add_filter malloc failed");
    bgpstream_log(BGPSTREAM_LOG_ERR, "can't allocate memory");
    return 0;
  }
  return bgpstream_str_set_insert(*setp, value) >= 0;
}

int bgpstream_filter_mgr_filter_add(bgpstream_filter_mgr_t *this,
                                    bgpstream_filter_type_t filter_type,
                                    const char *filter_value)
{
  unsigned long ul;
  char *endp;
  bgpstream_log(BGPSTREAM_LOG_VFINE, "\tBSF_MGR:: add_filter start");
  if (this == NULL) {
    return 1; // nothing to customize
  }

  switch (filter_type) {
  case BGPSTREAM_FILTER_TYPE_ELEM_PEER_ASN:
    errno = 0;
    ul = strtoul(filter_value, &endp, 10);
    if (errno || ul > UINT32_MAX || *endp) {
      bgpstream_log(BGPSTREAM_LOG_ERR, "invalid peer asn '%s'", filter_value);
      return 0;
    }
    return bsf_id_set_insert(&this->peer_asns, (uint32_t)ul);

  case BGPSTREAM_FILTER_TYPE_ELEM_ORIGIN_ASN:
    errno = 0;
    ul = strtoul(filter_value, &endp, 10);
    if (errno || ul > UINT32_MAX || *endp) {
      bgpstream_log(BGPSTREAM_LOG_ERR, "invalid origin asn '%s'", filter_value);
      return 0;
    }
    return bsf_id_set_insert(&this->origin_asns, (uint32_t)ul);

  case BGPSTREAM_FILTER_TYPE_ELEM_TYPE:
    if (strcmp(filter_value, "ribs") == 0) {
      this->elemtype_mask |= (BGPSTREAM_FILTER_ELEM_TYPE_RIB);
    } else if (strcmp(filter_value, "announcements") == 0) {
      this->elemtype_mask |= (BGPSTREAM_FILTER_ELEM_TYPE_ANNOUNCEMENT);
    } else if (strcmp(filter_value, "withdrawals") == 0) {
      this->elemtype_mask |= (BGPSTREAM_FILTER_ELEM_TYPE_WITHDRAWAL);
    } else if (strcmp(filter_value, "peerstates") == 0) {
      this->elemtype_mask |= (BGPSTREAM_FILTER_ELEM_TYPE_PEERSTATE);
    } else {
      bgpstream_log(BGPSTREAM_LOG_ERR,
                    "unknown element type '%s'", filter_value);
      return 0;
    }
    return 1;

  case BGPSTREAM_FILTER_TYPE_ELEM_ASPATH: {
    regex_t *re;
    uint8_t negate = 0;
    if (*filter_value == '!') {
      negate = 1;
      filter_value++;
    }
    if (!(re = malloc(sizeof(regex_t)))) {
      bgpstream_log(BGPSTREAM_LOG_ERR, "can't allocate memory");
      return 0;
    }
    // Cisco AS path regular expression
    // https://www.cisco.com/c/en/us/td/docs/routers/crs/software/crs_r4-2/getting_started/configuration/guide/gs42crs/gs42aexp.html
    // These characters are the same as in POSIX extended:  \|()[].^$*+?
    // These have no special meaning (unlike POSIX extended):  {}
    // We also support backreferences, which aren't described in any official
    // documentation I can find, but are in unofficial descriptions.
    // Cisco adds "_" which is equivalent to POSIX extended "(^|$|[ {},_])"
    // We convert the Cisco regex to a POSIX extended regex.
    char posix_re[256];
    const char *c_ptr = filter_value;
    char *p_ptr = posix_re;
    int c_parens = 0;
    int p_parens = 0;
    int c2p_parens[10] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    while (*c_ptr) {
      if (p_ptr - posix_re > sizeof(posix_re) - 15) {
        bgpstream_log(BGPSTREAM_LOG_ERR, "regex too long");
        return 0;
      }
      if (*c_ptr == '\\' && isdigit(c_ptr[1])) {
        // backref may need to be adjusted if we've added extra parens
        *(p_ptr++) = *(c_ptr++);
        int n = *(c_ptr++) - '0';
        if (n > 9 || n > c_parens || c2p_parens[n] > 9) {
          bgpstream_log(BGPSTREAM_LOG_ERR, "bad backreference in regex");
          return 0;
        }
        *(p_ptr++) = (char)c2p_parens[n] + '0';
      } else if (*c_ptr == '\\' && c_ptr[1]) {
        *(p_ptr++) = *(c_ptr++);
        *(p_ptr++) = *(c_ptr++);
      } else if (*c_ptr == '_') {
        strcpy(p_ptr, "(^|$|[ {},_])");
        p_ptr += strlen(p_ptr);
        c_ptr++;
        p_parens++; // we added parens to posix_re that weren't in cisco_re
      } else if (strchr("{}", *c_ptr)) {
        *(p_ptr++) = '\\';
        *(p_ptr++) = *(c_ptr++);
      } else if (*c_ptr == '(') {
        c2p_parens[++c_parens] = ++p_parens;
        *(p_ptr++) = *(c_ptr++);
      } else {
        *(p_ptr++) = *(c_ptr++);
      }
    }
    *(p_ptr++) = '\0';
    bgpstream_log(BGPSTREAM_LOG_FINE,
        "convert cisco regex \"%s\" to posix \"%s\"", filter_value, posix_re);
    int rc = regcomp(re, posix_re, REG_EXTENDED | REG_NOSUB);
    if (rc != 0) {
      char errbuf[1024];
      regerror(rc, re, errbuf, sizeof(errbuf));
      bgpstream_log(BGPSTREAM_LOG_ERR, "regex error: %s", errbuf);
      return 0;
    }
    if (++this->aspath_expr_cnt > this->aspath_expr_alloc_cnt) {
      void *tmp = realloc(this->aspath_exprs,
          this->aspath_expr_cnt * sizeof(*this->aspath_exprs));
      if (!tmp) {
        bgpstream_log(BGPSTREAM_LOG_ERR, "can't allocate memory");
        regfree(re);
        return 0;
      }
      this->aspath_exprs = tmp;
      this->aspath_expr_alloc_cnt = this->aspath_expr_cnt;
    }
    this->aspath_exprs[this->aspath_expr_cnt-1].re = re;
    this->aspath_exprs[this->aspath_expr_cnt-1].negate = negate;
    return 1;
  }


  case BGPSTREAM_FILTER_TYPE_ELEM_PREFIX:
  case BGPSTREAM_FILTER_TYPE_ELEM_PREFIX_MORE:
  case BGPSTREAM_FILTER_TYPE_ELEM_PREFIX_LESS:
  case BGPSTREAM_FILTER_TYPE_ELEM_PREFIX_EXACT:
  case BGPSTREAM_FILTER_TYPE_ELEM_PREFIX_ANY: {
    bgpstream_pfx_t pfx;
    uint8_t matchtype;

    if (this->prefixes == NULL) {
      if ((this->prefixes = bgpstream_patricia_tree_create(NULL)) ==
          NULL) {
        bgpstream_log(BGPSTREAM_LOG_VFINE,
                      "\tBSF_MGR:: add_filter malloc failed");
        bgpstream_log(BGPSTREAM_LOG_ERR, "can't allocate memory");
        return 0;
      }
    }
    if (!bgpstream_str2pfx(filter_value, &pfx)) {
      bgpstream_log(BGPSTREAM_LOG_ERR, "invalid prefix '%s'", filter_value);
      return 0;
    }
    if (filter_type == BGPSTREAM_FILTER_TYPE_ELEM_PREFIX_MORE ||
        filter_type == BGPSTREAM_FILTER_TYPE_ELEM_PREFIX) {
      matchtype = BGPSTREAM_PREFIX_MATCH_MORE;
    } else if (filter_type == BGPSTREAM_FILTER_TYPE_ELEM_PREFIX_LESS) {
      matchtype = BGPSTREAM_PREFIX_MATCH_LESS;
    } else if (filter_type == BGPSTREAM_FILTER_TYPE_ELEM_PREFIX_EXACT) {
      matchtype = BGPSTREAM_PREFIX_MATCH_EXACT;
    } else {
      matchtype = BGPSTREAM_PREFIX_MATCH_ANY;
    }

    pfx.allowed_matches = matchtype;
    if (bgpstream_patricia_tree_insert(this->prefixes, &pfx) == NULL) {
      bgpstream_log(BGPSTREAM_LOG_VFINE,
                    "\tBSF_MGR:: add_filter malloc failed");
      bgpstream_log(BGPSTREAM_LOG_ERR, "can't add prefix");
      return 0;
    }
    return 1;
  }
  case BGPSTREAM_FILTER_TYPE_ELEM_COMMUNITY: {
    int mask = 0;
    khiter_t k;
    int khret;

    bgpstream_community_t comm;
    if (this->communities == NULL) {
      if ((this->communities = kh_init(bgpstream_community_filter)) ==
          NULL) {
        bgpstream_log(BGPSTREAM_LOG_VFINE,
                      "\tBSF_MGR:: add_filter malloc failed");
        bgpstream_log(BGPSTREAM_LOG_ERR, "can't allocate memory");
        return 0;
      }
    }
    if ((mask = bgpstream_str2community(filter_value, &comm)) < 0) {
      bgpstream_log(BGPSTREAM_LOG_ERR, "invalid community '%s'",
          filter_value);
      return 0;
    }

    if ((k = kh_get(bgpstream_community_filter, this->communities,
                    comm)) == kh_end(this->communities)) {
      k = kh_put(bgpstream_community_filter, this->communities, comm,
                 &khret);
      kh_value(this->communities, k) = (uint8_t)mask;
    }

    /* we use the AND because the less restrictive filter wins over the more
     * restrictive:
     * e.g. 10:0, 10:* is equivalent to 10:*
     */
    kh_value(this->communities, k) =
      kh_value(this->communities, k) & mask;
    /* DEBUG: fprintf(stderr, "%s - %d\n",
     *                filter_value, kh_value(this->communities, k) );
     */
    return 1;
  }

  case BGPSTREAM_FILTER_TYPE_ELEM_IP_VERSION:
    if (strcmp(filter_value, "4") == 0) {
      this->ipversion = BGPSTREAM_ADDR_VERSION_IPV4;
    } else if (strcmp(filter_value, "6") == 0) {
      this->ipversion = BGPSTREAM_ADDR_VERSION_IPV6;
    } else {
      bgpstream_log(BGPSTREAM_LOG_ERR, "Unknown IP version '%s'",
          filter_value);
      return 0;
    }
    return 1;

  case BGPSTREAM_FILTER_TYPE_PROJECT:
    return bsf_str_set_insert(&this->projects, filter_value);

  case BGPSTREAM_FILTER_TYPE_COLLECTOR:
    return bsf_str_set_insert(&this->collectors, filter_value);

  case BGPSTREAM_FILTER_TYPE_ROUTER:
    return bsf_str_set_insert(&this->routers, filter_value);

  case BGPSTREAM_FILTER_TYPE_RECORD_TYPE:
    return bsf_str_set_insert(&this->bgp_types, filter_value);

  default:
    bgpstream_log(BGPSTREAM_LOG_ERR, "unknown filter %d", filter_type);
    return 0;
  }
}

int bgpstream_filter_mgr_rib_period_filter_add(
  bgpstream_filter_mgr_t *this, uint32_t period)
{
  bgpstream_log(BGPSTREAM_LOG_VFINE, "\tBSF_MGR:: add_filter start");
  assert(this != NULL);
  if (period != 0 && this->last_processed_ts == NULL) {
    if ((this->last_processed_ts = kh_init(collector_ts)) == NULL) {
      bgpstream_log(BGPSTREAM_LOG_ERR,
                    "can't allocate memory for collectortype map");
      return 0;
    }
  }
  this->rib_period = period;
  bgpstream_log(BGPSTREAM_LOG_VFINE, "\tBSF_MGR:: add_filter end");
  return 1;
}

int bgpstream_filter_mgr_interval_filter_add(
  bgpstream_filter_mgr_t *this, uint32_t begin_time, uint32_t end_time)
{
  bgpstream_log(BGPSTREAM_LOG_VFINE, "\tBSF_MGR:: add_filter start");
  if (this == NULL) {
    return 1; // nothing to customize
  }
  // create a new filter structure
  bgpstream_interval_filter_t *f =
    (bgpstream_interval_filter_t *)malloc(sizeof(bgpstream_interval_filter_t));
  if (f == NULL) {
    bgpstream_log(BGPSTREAM_LOG_ERR, "can't allocate memory");
    return 0;
  }
  // copying filter values
  f->begin_time = begin_time;
  f->end_time = end_time;
  this->time_interval = f;

  bgpstream_log(BGPSTREAM_LOG_VFINE, "\tBSF_MGR:: add_filter stop");
  return 1;
}

int bgpstream_filter_mgr_validate(bgpstream_filter_mgr_t *filter_mgr)
{
  /* currently we only validate the interval */
  bgpstream_interval_filter_t *TIF = filter_mgr->time_interval;
  if (TIF != NULL && (TIF->end_time != BGPSTREAM_FOREVER &&
                      TIF->begin_time > TIF->end_time)) {
    /* invalid interval */
    bgpstream_log(BGPSTREAM_LOG_ERR,
                  "Interval %" PRIu32 ",%" PRIu32 " is invalid\n",
                  TIF->begin_time, TIF->end_time);
    return -1;
  }

  return 0;
}

/* destroy the memory allocated for bgpstream filter */
void bgpstream_filter_mgr_destroy(bgpstream_filter_mgr_t *this)
{
  bgpstream_log(BGPSTREAM_LOG_VFINE, "\tBSF_MGR:: destroy start");
  if (this == NULL) {
    return; // nothing to destroy
  }
  // destroying filters
  khiter_t k;
  // projects
  if (this->projects != NULL) {
    bgpstream_str_set_destroy(this->projects);
  }
  // collectors
  if (this->collectors != NULL) {
    bgpstream_str_set_destroy(this->collectors);
  }
  // routers
  if (this->routers != NULL) {
    bgpstream_str_set_destroy(this->routers);
  }
  // bgp_types
  if (this->bgp_types != NULL) {
    bgpstream_str_set_destroy(this->bgp_types);
  }
  // peer asns
  if (this->peer_asns != NULL) {
    bgpstream_id_set_destroy(this->peer_asns);
  }
  // origin asns
  if (this->origin_asns != NULL) {
    bgpstream_id_set_destroy(this->origin_asns);
  }
  // aspath expressions
  if (this->aspath_exprs != NULL) {
    for (int i = 0; i < this->aspath_expr_cnt; i++) {
      if (this->aspath_exprs[i].re) {
        regfree(this->aspath_exprs[i].re);
      }
    }
    free(this->aspath_exprs);
  }
  // prefixes
  if (this->prefixes != NULL) {
    bgpstream_patricia_tree_destroy(this->prefixes);
  }
  // communities
  if (this->communities != NULL) {
    kh_destroy(bgpstream_community_filter, this->communities);
  }
  // time_interval
  if (this->time_interval != NULL) {
    free(this->time_interval);
  }
  // rib/update frequency
  if (this->last_processed_ts != NULL) {
    for (k = kh_begin(this->last_processed_ts);
         k != kh_end(this->last_processed_ts); ++k) {
      if (kh_exist(this->last_processed_ts, k)) {
        free(kh_key(this->last_processed_ts, k));
      }
    }
    kh_destroy(collector_ts, this->last_processed_ts);
  }
  // free the mgr structure
  free(this);
  this = NULL;
  bgpstream_log(BGPSTREAM_LOG_VFINE, "\tBSF_MGR:: destroy end");
}
