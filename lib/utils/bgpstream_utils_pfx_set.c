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

/** set of unique IP prefixes
 *  this structure maintains a set of unique
 *  prefixes (ipv4 and ipv6 prefixes, both hashed
 *  using a int64 type)
 */
KHASH_INIT(bgpstream_pfx_storage_set /* name */,
           bgpstream_pfx_storage_t /* khkey_t */, char /* khval_t */,
           0 /* kh_is_set */, bgpstream_pfx_storage_hash_val /*__hash_func */,
           bgpstream_pfx_storage_equal_val /* __hash_equal */);

struct bgpstream_pfx_storage_set {
  khash_t(bgpstream_pfx_storage_set) * hash;
  uint64_t ipv4_size;
  uint64_t ipv6_size;
};

/* ipv4 specific set */

KHASH_INIT(bgpstream_ipv4_pfx_set /* name */,
           bgpstream_ipv4_pfx_t /* khkey_t */, char /* khval_t */,
           0 /* kh_is_set */,
           bgpstream_ipv4_pfx_storage_hash_val /*__hash_func */,
           bgpstream_ipv4_pfx_storage_equal_val /* __hash_equal */);

struct bgpstream_ipv4_pfx_set {
  khash_t(bgpstream_ipv4_pfx_set) * hash;
};

/* ipv6 specific set */

KHASH_INIT(bgpstream_ipv6_pfx_set /* name */,
           bgpstream_ipv6_pfx_t /* khkey_t */, char /* khval_t */,
           0 /* kh_is_set */,
           bgpstream_ipv6_pfx_storage_hash_val /*__hash_func */,
           bgpstream_ipv6_pfx_storage_equal_val /* __hash_equal */);

struct bgpstream_ipv6_pfx_set {
  khash_t(bgpstream_ipv6_pfx_set) * hash;
};

/* STORAGE */

bgpstream_pfx_storage_set_t *bgpstream_pfx_storage_set_create()
{
  bgpstream_pfx_storage_set_t *set;

  if ((set = (bgpstream_pfx_storage_set_t *)malloc(
         sizeof(bgpstream_pfx_storage_set_t))) == NULL) {
    return NULL;
  }

  if ((set->hash = kh_init(bgpstream_pfx_storage_set)) == NULL) {
    bgpstream_pfx_storage_set_destroy(set);
    return NULL;
  }
  set->ipv4_size = 0;
  set->ipv6_size = 0;
  return set;
}

int bgpstream_pfx_storage_set_insert(bgpstream_pfx_storage_set_t *set,
                                     bgpstream_pfx_storage_t *pfx)
{
  int khret;
  khiter_t k;
  if ((k = kh_get(bgpstream_pfx_storage_set, set->hash, *pfx)) ==
      kh_end(set->hash)) {
    k = kh_put(bgpstream_pfx_storage_set, set->hash, *pfx, &khret);
    if (pfx->address.version == BGPSTREAM_ADDR_VERSION_IPV4) {
      set->ipv4_size++;
    } else {
      set->ipv6_size++;
    }
    return 1;
  }
  return 0;
}

int bgpstream_pfx_storage_set_exists(bgpstream_pfx_storage_set_t *set,
                                     bgpstream_pfx_storage_t *pfx)
{
  khiter_t k;
  if ((k = kh_get(bgpstream_pfx_storage_set, set->hash, *pfx)) ==
      kh_end(set->hash)) {
    return 0;
  }
  return 1;
}

int bgpstream_pfx_storage_set_size(bgpstream_pfx_storage_set_t *set)
{
  return kh_size(set->hash);
}

int bgpstream_pfx_storage_set_version_size(bgpstream_pfx_storage_set_t *set,
                                           bgpstream_addr_version_t v)
{
  switch (v) {
  case BGPSTREAM_ADDR_VERSION_IPV4:
    return set->ipv4_size;
  case BGPSTREAM_ADDR_VERSION_IPV6:
    return set->ipv6_size;
  default:
    return -1;
  }
}

int bgpstream_pfx_storage_set_merge(bgpstream_pfx_storage_set_t *dst_set,
                                    bgpstream_pfx_storage_set_t *src_set)
{
  khiter_t k;
  for (k = kh_begin(src_set->hash); k != kh_end(src_set->hash); ++k) {
    if (kh_exist(src_set->hash, k)) {
      if (bgpstream_pfx_storage_set_insert(dst_set,
                                           &(kh_key(src_set->hash, k))) < 0) {
        return -1;
      }
    }
  }
  return 0;
}

void bgpstream_pfx_storage_set_destroy(bgpstream_pfx_storage_set_t *set)
{
  kh_destroy(bgpstream_pfx_storage_set, set->hash);
  free(set);
}

void bgpstream_pfx_storage_set_clear(bgpstream_pfx_storage_set_t *set)
{
  kh_clear(bgpstream_pfx_storage_set, set->hash);
  set->ipv4_size = 0;
  set->ipv6_size = 0;
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

int bgpstream_ipv4_pfx_set_insert(bgpstream_ipv4_pfx_set_t *set,
                                  bgpstream_ipv4_pfx_t *pfx)
{
  int khret;
  khiter_t k;
  if ((k = kh_get(bgpstream_ipv4_pfx_set, set->hash, *pfx)) ==
      kh_end(set->hash)) {
    k = kh_put(bgpstream_ipv4_pfx_set, set->hash, *pfx, &khret);
    return 1;
  }
  return 0;
}

int bgpstream_ipv4_pfx_set_exists(bgpstream_ipv4_pfx_set_t *set,
                                  bgpstream_ipv4_pfx_t *pfx)
{
  khiter_t k;
  if ((k = kh_get(bgpstream_ipv4_pfx_set, set->hash, *pfx)) ==
      kh_end(set->hash)) {
    return 0;
  }
  return 1;
}

int bgpstream_ipv4_pfx_set_size(bgpstream_ipv4_pfx_set_t *set)
{
  return kh_size(set->hash);
}

int bgpstream_ipv4_pfx_set_merge(bgpstream_ipv4_pfx_set_t *dst_set,
                                 bgpstream_ipv4_pfx_set_t *src_set)
{
  khiter_t k;
  for (k = kh_begin(src_set->hash); k != kh_end(src_set->hash); ++k) {
    if (kh_exist(src_set->hash, k)) {
      if (bgpstream_ipv4_pfx_set_insert(dst_set, &(kh_key(src_set->hash, k))) <
          0) {
        return -1;
      }
    }
  }
  return 0;
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

int bgpstream_ipv6_pfx_set_insert(bgpstream_ipv6_pfx_set_t *set,
                                  bgpstream_ipv6_pfx_t *pfx)
{
  int khret;
  khiter_t k;
  if ((k = kh_get(bgpstream_ipv6_pfx_set, set->hash, *pfx)) ==
      kh_end(set->hash)) {
    k = kh_put(bgpstream_ipv6_pfx_set, set->hash, *pfx, &khret);
    return 1;
  }
  return 0;
}

int bgpstream_ipv6_pfx_set_exists(bgpstream_ipv6_pfx_set_t *set,
                                  bgpstream_ipv6_pfx_t *pfx)
{
  khiter_t k;
  if ((k = kh_get(bgpstream_ipv6_pfx_set, set->hash, *pfx)) ==
      kh_end(set->hash)) {
    return 0;
  }
  return 1;
}

int bgpstream_ipv6_pfx_set_size(bgpstream_ipv6_pfx_set_t *set)
{
  return kh_size(set->hash);
}

int bgpstream_ipv6_pfx_set_merge(bgpstream_ipv6_pfx_set_t *dst_set,
                                 bgpstream_ipv6_pfx_set_t *src_set)
{
  khiter_t k;
  for (k = kh_begin(src_set->hash); k != kh_end(src_set->hash); ++k) {
    if (kh_exist(src_set->hash, k)) {
      if (bgpstream_ipv6_pfx_set_insert(dst_set, &(kh_key(src_set->hash, k))) <
          0) {
        return -1;
      }
    }
  }
  return 0;
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
