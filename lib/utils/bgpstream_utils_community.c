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

#include "bgpstream_utils_community_int.h"
#include "config.h"
#include "khash.h"
#include "utils.h"
#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>

#define COMMUNITY_MAX_STR_LEN 16

/** Set of community values */
struct bgpstream_community_set {

  /** Array of community values */
  bgpstream_community_t *communities;

  /** Number of communities in the set */
  int communities_cnt;

  /** Number of communities allocated in the set */
  int communities_alloc_cnt;

  /** Communities hash (OR between
   *  all communities in the set) */
  uint32_t communities_hash;
};

/* ========== PUBLIC FUNCTIONS ========== */

int bgpstream_community_snprintf(char *buf, size_t len,
                                 bgpstream_community_t *comm)
{
  return snprintf(buf, len, "%" PRIu16 ":%" PRIu16, comm->asn, comm->value);
}

int bgpstream_str2community(const char *buf, bgpstream_community_t *comm)
{
  if (buf == NULL || comm == NULL) {
    return -1;
  }
  uint8_t mask = 0;
  char com_copy[COMMUNITY_MAX_STR_LEN];
  strncpy(com_copy, buf, COMMUNITY_MAX_STR_LEN);

  unsigned long int r;
  int ret;
  char *endptr = NULL;
  char *found = strchr(com_copy, ':');

  if (found == NULL) {
    return -1;
  }
  *found = '\0';
  errno = 0;
  comm->asn = 0;
  if (com_copy[0] != '*') {
    mask = mask | BGPSTREAM_COMMUNITY_FILTER_ASN;
    r = strtoul(com_copy, &endptr, 10);
    ret = errno;
    if (!(endptr != NULL && *endptr == '\0') || ret != 0) {
      return -1;
    }
    comm->asn = (uint32_t)r;
  }

  endptr = NULL;
  comm->value = 0;
  if (*(found + 1) != '*') {
    mask = mask | BGPSTREAM_COMMUNITY_FILTER_VALUE;
    r = strtoul(found + 1, &endptr, 10);
    ret = errno;
    if (!(endptr != NULL && *endptr == '\0') || ret != 0) {
      return -1;
    }
    comm->value = (uint32_t)r;
  }
  return (int)mask;
}

bgpstream_community_t *bgpstream_community_dup(bgpstream_community_t *src)
{
  bgpstream_community_t *dst = NULL;

  if ((dst = malloc(sizeof(bgpstream_community_t))) == NULL) {
    return NULL;
  }

  memcpy(dst, src, sizeof(bgpstream_community_t));

  return dst;
}

void bgpstream_community_destroy(bgpstream_community_t *comm)
{
  /* no dynamic memory */
  free(comm);
}

#if UINT_MAX == 0xffffffffu
unsigned int
#elif ULONG_MAX == 0xffffffffu
unsigned long
#endif
bgpstream_community_hash(bgpstream_community_t *comm)
{
  return comm->asn | comm->value;
}

#if UINT_MAX == 0xffffffffu
unsigned int
#elif ULONG_MAX == 0xffffffffu
unsigned long
#endif
bgpstream_community_hash_value(bgpstream_community_t comm)
{
  return bgpstream_community_hash(&comm);
}

int bgpstream_community_equal(bgpstream_community_t *comm1,
                              bgpstream_community_t *comm2)
{
  return (comm1->asn == comm2->asn) && (comm1->value == comm2->value);
}

int bgpstream_community_equal_value(bgpstream_community_t comm1,
                                    bgpstream_community_t comm2)
{
  return bgpstream_community_equal(&comm1, &comm2);
}

/* SET FUNCTIONS */

#define ADD_CHAR(chr)                                                          \
  do {                                                                         \
    if ((len - written) > 0) {                                                 \
      *bufp = chr;                                                             \
      bufp++;                                                                  \
    }                                                                          \
    written++;                                                                 \
  } while (0)

int bgpstream_community_set_snprintf(char *buf, size_t len,
                                     bgpstream_community_set_t *set)
{
  size_t written = 0;
  int i;
  int need_sep = 0;
  char *bufp = buf;

  for (i = 0; i < bgpstream_community_set_size(set); i++) {
    if (need_sep != 0) {
      ADD_CHAR(' ');
    }
    need_sep = 1;
    written += bgpstream_community_snprintf(
      bufp, (len - written), bgpstream_community_set_get(set, i));
    bufp = buf + written;
  }
  *bufp = '\0';

  if (written > len) {
    buf[len - 1] = '\0';
  }
  return written;
}

bgpstream_community_set_t *bgpstream_community_set_create()
{
  bgpstream_community_set_t *set = NULL;

  if ((set = malloc_zero(sizeof(bgpstream_community_set_t))) == NULL) {
    return NULL;
  }

  return set;
}

void bgpstream_community_set_clear(bgpstream_community_set_t *set)
{
  set->communities_cnt = 0;
  set->communities_hash = 0;
}

void bgpstream_community_set_destroy(bgpstream_community_set_t *set)
{
  /* alloc cnt is < 0 if owned externally */
  if (set->communities_alloc_cnt > 0) {
    free(set->communities);
  }
  set->communities = NULL;
  set->communities_cnt = 0;
  set->communities_alloc_cnt = 0;
  set->communities_hash = 0;

  free(set);
}

int bgpstream_community_set_copy(bgpstream_community_set_t *dst,
                                 bgpstream_community_set_t *src)
{
  if (dst->communities_alloc_cnt < src->communities_cnt) {
    if ((dst->communities =
           realloc(dst->communities, sizeof(bgpstream_community_t) *
                                       src->communities_cnt)) == NULL) {
      return -1;
    }
    dst->communities_alloc_cnt = src->communities_cnt;
  }

  memcpy(dst->communities, src->communities,
         sizeof(bgpstream_community_t) * src->communities_cnt);

  dst->communities_cnt = src->communities_cnt;
  dst->communities_hash = src->communities_hash;

  return 0;
}

bgpstream_community_t *
bgpstream_community_set_get(bgpstream_community_set_t *set, int i)
{
  return (i < set->communities_cnt) ? &set->communities[i] : NULL;
}

int bgpstream_community_set_size(bgpstream_community_set_t *set)
{
  return set->communities_cnt;
}

int bgpstream_community_set_insert(bgpstream_community_set_t *set,
                                   bgpstream_community_t *comm)
{
  if (set->communities_cnt == set->communities_alloc_cnt) {
    if ((set->communities = realloc(
           set->communities, sizeof(bgpstream_community_t) *
                               (set->communities_alloc_cnt + 1))) == NULL) {
      return -1;
    }
    set->communities_alloc_cnt++;
  }

  set->communities[set->communities_cnt] = *comm;
  set->communities_cnt++;
  set->communities_hash = set->communities_hash | *((uint32_t *)comm);
  return 0;
}

int bgpstream_community_set_populate_from_array(bgpstream_community_set_t *set,
                                                bgpstream_community_t *comms,
                                                int comms_cnt)
{
  bgpstream_community_set_t tmp;
  if (bgpstream_community_set_populate_from_array_zc(&tmp, comms, comms_cnt) !=
      0) {
    return -1;
  }
  return bgpstream_community_set_copy(set, &tmp);
}

int bgpstream_community_set_populate_from_array_zc(
  bgpstream_community_set_t *set, bgpstream_community_t *comms, int comms_cnt)
{
  set->communities_alloc_cnt = -1; /* signal that memory is not owned by us */
  set->communities = comms;
  set->communities_cnt = comms_cnt;
  set->communities_hash = 0;
  int i;
  for (i = 0; i < bgpstream_community_set_size(set); i++) {
    set->communities_hash =
      set->communities_hash | *((uint32_t *)&set->communities[i]);
  }
  return 0;
}

#if UINT_MAX == 0xffffffffu
unsigned int
#elif ULONG_MAX == 0xffffffffu
unsigned long
#endif
bgpstream_community_set_hash(bgpstream_community_set_t *set)
{
  int i;
  uint32_t h;
  int cnt = bgpstream_community_set_size(set);

  if (cnt == 0) {
    return 0;
  }

  h = bgpstream_community_hash(bgpstream_community_set_get(set, 0));

  for (i = 1; i < cnt; i++) {
    h = (h << 5) - h +
        bgpstream_community_hash(bgpstream_community_set_get(set, i));
  }
  return h;
}

int bgpstream_community_set_equal(bgpstream_community_set_t *set1,
                                  bgpstream_community_set_t *set2)
{
  return (set1->communities_hash == set2->communities_hash) &&
         (set1->communities_cnt == set2->communities_cnt) &&
         bcmp(set1->communities, set2->communities,
              sizeof(bgpstream_community_t) * set1->communities_cnt);
}

/* ========== PROTECTED FUNCTIONS ========== */

int bgpstream_community_set_populate(bgpstream_community_set_t *set,
                                     uint8_t *buf, size_t len)
{
  int cnt;
  int i;
  bgpstream_community_t *c = NULL;

  bgpstream_community_set_clear(set);

  if (buf == NULL || len == 0) {
    return 0;
  }

  cnt = len / sizeof(uint32_t);

  if (set->communities_alloc_cnt < cnt) {
    if ((set->communities = realloc(
           set->communities, sizeof(bgpstream_community_t) * cnt)) == NULL) {
      return -1;
    }
    set->communities_alloc_cnt = cnt;
  }

  for (i = 0; i < cnt; i++) {
    c = &set->communities[i];
    c->asn = ntohs(*(uint16_t *)buf);
    buf += sizeof(uint16_t);
    c->value = ntohs(*(uint16_t *)buf);
    buf += sizeof(uint16_t);
    set->communities_hash = set->communities_hash | *((uint32_t *)c);
  }

  set->communities_cnt = cnt;

  return 0;
}

int bgpstream_community_set_exists(bgpstream_community_set_t *set,
                                   bgpstream_community_t *com)
{
  return bgpstream_community_set_match(set, com,
                                       BGPSTREAM_COMMUNITY_FILTER_EXACT);
}

int bgpstream_community_set_match(bgpstream_community_set_t *set,
                                  bgpstream_community_t *com, uint8_t mask)
{
  bgpstream_community_t *hash = (bgpstream_community_t *)&set->communities_hash;

  /* first we verify if the hash is compatible */
  if ((!(mask & BGPSTREAM_COMMUNITY_FILTER_ASN) || hash->asn & com->asn) &&
      (!(mask & BGPSTREAM_COMMUNITY_FILTER_VALUE) ||
       hash->value & com->value)) {
    bgpstream_community_t *c;
    int i;
    int n = bgpstream_community_set_size(set);
    for (i = 0; i < n; i++) {
      c = bgpstream_community_set_get(set, i);
      /* checking if asn match is requested and compatible */
      if (mask & BGPSTREAM_COMMUNITY_FILTER_ASN) {
        if (c->asn != com->asn) {
          continue;
        }
      }
      /* checking if value match is requested and compatible */
      if (mask & BGPSTREAM_COMMUNITY_FILTER_VALUE) {
        if (c->value != com->value) {
          continue;
        }
      }

      return 1;
    }
  }
  return 0;
}
