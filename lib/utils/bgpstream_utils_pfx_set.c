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
 */

#include <assert.h>
#include <stdio.h>

#include "khash.h"
#include "utils.h"

#include "bgpstream_utils_pfx_set.h"

/* ipv4 specific set */

KHASH_INIT(bgpstream_ipv4_pfx_set /* name */,
           bgpstream_ipv4_pfx_t /* khkey_t */, char /* khval_t */,
           0 /* kh_is_map */,
           bgpstream_ipv4_pfx_hash_val /*__hash_func */,
           bgpstream_ipv4_pfx_equal_val /* __hash_equal */)

struct bgpstream_ipv4_pfx_set {
  khash_t(bgpstream_ipv4_pfx_set) *hash;
};

static inline int v4hash_insert(khash_t(bgpstream_ipv4_pfx_set) *hash,
                                bgpstream_ipv4_pfx_t *pfx);
static inline int v4hash_merge(khash_t(bgpstream_ipv4_pfx_set) *dst_hash,
                               khash_t(bgpstream_ipv4_pfx_set) *src_hash);

/* ipv6 specific set */

KHASH_INIT(bgpstream_ipv6_pfx_set /* name */,
           bgpstream_ipv6_pfx_t /* khkey_t */, char /* khval_t */,
           0 /* kh_is_map */,
           bgpstream_ipv6_pfx_hash_val /*__hash_func */,
           bgpstream_ipv6_pfx_equal_val /* __hash_equal */)

struct bgpstream_ipv6_pfx_set {
  khash_t(bgpstream_ipv6_pfx_set) *hash;
};

static inline int v6hash_insert(khash_t(bgpstream_ipv6_pfx_set) *hash,
                                bgpstream_ipv6_pfx_t *pfx);
static inline int v6hash_merge(khash_t(bgpstream_ipv6_pfx_set) *dst_hash,
                               khash_t(bgpstream_ipv6_pfx_set) *src_hash);

/** set of unique IP prefixes
 *  We store v4 and v6 in separate hashes, because it would be unsafe for the
 *  khash functions in _pfx_set_insert() and _pfx_set_exists() to dereference
 *  pfx if it points to a ipv4_pfx.
 *  This also has the advantage of using less memory for the v4 hash.
 */
struct bgpstream_pfx_set {
  khash_t(bgpstream_ipv4_pfx_set) * v4hash;
  khash_t(bgpstream_ipv6_pfx_set) * v6hash;
};

/* STORAGE */

bgpstream_pfx_set_t *bgpstream_pfx_set_create()
{
  bgpstream_pfx_set_t *set;

  if ((set = malloc(sizeof(bgpstream_pfx_set_t))) == NULL) {
    return NULL;
  }

  if ((set->v4hash = kh_init(bgpstream_ipv4_pfx_set)) == NULL ||
      (set->v6hash = kh_init(bgpstream_ipv6_pfx_set)) == NULL) {
    bgpstream_pfx_set_destroy(set);
    return NULL;
  }
  return set;
}

int bgpstream_pfx_set_insert(bgpstream_pfx_set_t *set,
                             bgpstream_pfx_t *pfx)
{
  int khret;
  khiter_t k;

  if (pfx->address.version == BGPSTREAM_ADDR_VERSION_IPV4) {
    return v4hash_insert(set->v4hash, &pfx->bs_ipv4);
  } else {
    return v6hash_insert(set->v6hash, &pfx->bs_ipv6);
  }
}

int bgpstream_pfx_set_exists(bgpstream_pfx_set_t *set,
                             bgpstream_pfx_t *pfx)
{
  if (pfx->address.version == BGPSTREAM_ADDR_VERSION_IPV4) {
    return kh_get(bgpstream_ipv4_pfx_set, set->v4hash, pfx->bs_ipv4) !=
      kh_end(set->v4hash);
  } else {
    return kh_get(bgpstream_ipv6_pfx_set, set->v6hash, pfx->bs_ipv6) !=
      kh_end(set->v6hash);
  }
}

int bgpstream_pfx_set_size(bgpstream_pfx_set_t *set)
{
  return kh_size(set->v4hash) + kh_size(set->v6hash);
}

int bgpstream_pfx_set_version_size(bgpstream_pfx_set_t *set,
                                   bgpstream_addr_version_t v)
{
  switch (v) {
  case BGPSTREAM_ADDR_VERSION_IPV4:
    return kh_size(set->v4hash);
  case BGPSTREAM_ADDR_VERSION_IPV6:
    return kh_size(set->v6hash);
  default:
    return -1;
  }
}

int bgpstream_pfx_set_merge(bgpstream_pfx_set_t *dst_set,
                            bgpstream_pfx_set_t *src_set)
{
  if (v4hash_merge(dst_set->v4hash, src_set->v4hash) < 0 ||
      v6hash_merge(dst_set->v6hash, src_set->v6hash) < 0) {
    return -1;
  }
  return 0;
}

void bgpstream_pfx_set_destroy(bgpstream_pfx_set_t *set)
{
  if (set->v4hash) kh_destroy(bgpstream_ipv4_pfx_set, set->v4hash);
  if (set->v6hash) kh_destroy(bgpstream_ipv6_pfx_set, set->v6hash);
  free(set);
}

void bgpstream_pfx_set_clear(bgpstream_pfx_set_t *set)
{
  kh_clear(bgpstream_ipv4_pfx_set, set->v4hash);
  kh_clear(bgpstream_ipv6_pfx_set, set->v6hash);
}

/* IPv4 */

bgpstream_ipv4_pfx_set_t *bgpstream_ipv4_pfx_set_create()
{
  bgpstream_ipv4_pfx_set_t *set;

  if ((set = (bgpstream_ipv4_pfx_set_t *)malloc(
         sizeof(bgpstream_ipv4_pfx_set_t))) == NULL) {
    return NULL;
  }

  if ((set->hash = kh_init(bgpstream_ipv4_pfx_set)) == NULL) {
    bgpstream_ipv4_pfx_set_destroy(set);
    return NULL;
  }

  return set;
}

static inline int v4hash_insert(khash_t(bgpstream_ipv4_pfx_set) *hash,
                                bgpstream_ipv4_pfx_t *pfx)
{
  int khret;
  if (kh_get(bgpstream_ipv4_pfx_set, hash, *pfx) != kh_end(hash))
    return 0;
  kh_put(bgpstream_ipv4_pfx_set, hash, *pfx, &khret);
  return (khret < 0) ? -1 : 1;
}

int bgpstream_ipv4_pfx_set_insert(bgpstream_ipv4_pfx_set_t *set,
                                  bgpstream_ipv4_pfx_t *pfx)
{
  return v4hash_insert(set->hash, pfx);
}

int bgpstream_ipv4_pfx_set_exists(bgpstream_ipv4_pfx_set_t *set,
                                  bgpstream_ipv4_pfx_t *pfx)
{
  return kh_get(bgpstream_ipv4_pfx_set, set->hash, *pfx) != kh_end(set->hash);
}

int bgpstream_ipv4_pfx_set_size(bgpstream_ipv4_pfx_set_t *set)
{
  return kh_size(set->hash);
}

static inline int v4hash_merge(khash_t(bgpstream_ipv4_pfx_set) *dst_hash,
                               khash_t(bgpstream_ipv4_pfx_set) *src_hash)
{
  khiter_t k;
  for (k = kh_begin(src_hash); k != kh_end(src_hash); ++k) {
    if (kh_exist(src_hash, k)) {
      if (v4hash_insert(dst_hash, &(kh_key(src_hash, k))) < 0) {
        return -1;
      }
    }
  }
  return 0;
}

int bgpstream_ipv4_pfx_set_merge(bgpstream_ipv4_pfx_set_t *dst_set,
                                 bgpstream_ipv4_pfx_set_t *src_set)
{
  return v4hash_merge(dst_set->hash, src_set->hash);
}

void bgpstream_ipv4_pfx_set_destroy(bgpstream_ipv4_pfx_set_t *set)
{
  kh_destroy(bgpstream_ipv4_pfx_set, set->hash);
  free(set);
}

void bgpstream_ipv4_pfx_set_clear(bgpstream_ipv4_pfx_set_t *set)
{
  kh_clear(bgpstream_ipv4_pfx_set, set->hash);
}

/* IPv6 */

bgpstream_ipv6_pfx_set_t *bgpstream_ipv6_pfx_set_create()
{
  bgpstream_ipv6_pfx_set_t *set;

  if ((set = (bgpstream_ipv6_pfx_set_t *)malloc(
         sizeof(bgpstream_ipv6_pfx_set_t))) == NULL) {
    return NULL;
  }

  if ((set->hash = kh_init(bgpstream_ipv6_pfx_set)) == NULL) {
    bgpstream_ipv6_pfx_set_destroy(set);
    return NULL;
  }

  return set;
}

static inline int v6hash_insert(khash_t(bgpstream_ipv6_pfx_set) *hash,
                                bgpstream_ipv6_pfx_t *pfx)
{
  int khret;
  if (kh_get(bgpstream_ipv6_pfx_set, hash, *pfx) != kh_end(hash))
    return 0;
  kh_put(bgpstream_ipv6_pfx_set, hash, *pfx, &khret);
  return (khret < 0) ? -1 : 1;
}

int bgpstream_ipv6_pfx_set_insert(bgpstream_ipv6_pfx_set_t *set,
                                  bgpstream_ipv6_pfx_t *pfx)
{
  return v6hash_insert(set->hash, pfx);
}

int bgpstream_ipv6_pfx_set_exists(bgpstream_ipv6_pfx_set_t *set,
                                  bgpstream_ipv6_pfx_t *pfx)
{
  return kh_get(bgpstream_ipv6_pfx_set, set->hash, *pfx) != kh_end(set->hash);
}

int bgpstream_ipv6_pfx_set_size(bgpstream_ipv6_pfx_set_t *set)
{
  return kh_size(set->hash);
}

static inline int v6hash_merge(khash_t(bgpstream_ipv6_pfx_set) *dst_hash,
                               khash_t(bgpstream_ipv6_pfx_set) *src_hash)
{
  khiter_t k;
  for (k = kh_begin(src_hash); k != kh_end(src_hash); ++k) {
    if (kh_exist(src_hash, k)) {
      if (v6hash_insert(dst_hash, &(kh_key(src_hash, k))) < 0) {
        return -1;
      }
    }
  }
  return 0;
}

int bgpstream_ipv6_pfx_set_merge(bgpstream_ipv6_pfx_set_t *dst_set,
                                 bgpstream_ipv6_pfx_set_t *src_set)
{
  return v6hash_merge(dst_set->hash, src_set->hash);
}

void bgpstream_ipv6_pfx_set_destroy(bgpstream_ipv6_pfx_set_t *set)
{
  kh_destroy(bgpstream_ipv6_pfx_set, set->hash);
  free(set);
}

void bgpstream_ipv6_pfx_set_clear(bgpstream_ipv6_pfx_set_t *set)
{
  kh_clear(bgpstream_ipv6_pfx_set, set->hash);
}
