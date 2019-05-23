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

#include "bgpstream_utils_id_set.h"

/* PRIVATE */

/** set of unique ids
 *  this structure maintains a set of unique
 *  ids (using a uint32 type)
 */
KHASH_INIT(bgpstream_id_set /* name */, uint32_t /* khkey_t */,
           char /* khval_t */, 0 /* kh_is_set */,
           kh_int_hash_func /*__hash_func */,
           kh_int_hash_equal /* __hash_equal */)

struct bgpstream_id_set {
  khiter_t k;
  khash_t(bgpstream_id_set) * hash;
};

/* PUBLIC FUNCTIONS */

bgpstream_id_set_t *bgpstream_id_set_create()
{
  bgpstream_id_set_t *set;

  if ((set = (bgpstream_id_set_t *)malloc(sizeof(bgpstream_id_set_t))) ==
      NULL) {
    return NULL;
  }

  if ((set->hash = kh_init(bgpstream_id_set)) == NULL) {
    bgpstream_id_set_destroy(set);
    return NULL;
  }
  bgpstream_id_set_rewind(set);
  return set;
}

int bgpstream_id_set_insert(bgpstream_id_set_t *set, uint32_t id)
{
  int khret;
  khiter_t k;
  if ((k = kh_get(bgpstream_id_set, set->hash, id)) == kh_end(set->hash)) {
    /** @todo we should always check the return value from khash funcs */
    k = kh_put(bgpstream_id_set, set->hash, id, &khret);
    return 1;
  }
  return 0;
}

int bgpstream_id_set_exists(bgpstream_id_set_t *set, uint32_t id)
{
  khiter_t k;
  if ((k = kh_get(bgpstream_id_set, set->hash, id)) == kh_end(set->hash)) {
    return 0;
  }
  return 1;
}

int bgpstream_id_set_merge(bgpstream_id_set_t *dst_set,
                           bgpstream_id_set_t *src_set)
{
  khiter_t k;
  for (k = kh_begin(src_set->hash); k != kh_end(src_set->hash); ++k) {
    if (kh_exist(src_set->hash, k)) {
      if (bgpstream_id_set_insert(dst_set, kh_key(src_set->hash, k)) < 0) {
        return -1;
      }
    }
  }
  bgpstream_id_set_rewind(dst_set);
  bgpstream_id_set_rewind(src_set);
  return 0;
}

void bgpstream_id_set_rewind(bgpstream_id_set_t *set)
{
  set->k = kh_begin(set->hash);
}

uint32_t *bgpstream_id_set_next(bgpstream_id_set_t *set)
{
  uint32_t *v = NULL;
  for (; set->k != kh_end(set->hash); ++set->k) {
    if (kh_exist(set->hash, set->k)) {
      v = &kh_key(set->hash, set->k);
      set->k++;
      return v;
    }
  }
  return NULL;
}

int bgpstream_id_set_size(bgpstream_id_set_t *set)
{
  return kh_size(set->hash);
}

void bgpstream_id_set_destroy(bgpstream_id_set_t *set)
{
  kh_destroy(bgpstream_id_set, set->hash);
  free(set);
}

void bgpstream_id_set_clear(bgpstream_id_set_t *set)
{
  bgpstream_id_set_rewind(set);
  kh_clear(bgpstream_id_set, set->hash);
}
