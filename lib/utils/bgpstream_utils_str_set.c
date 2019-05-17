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

#include "bgpstream_utils_str_set.h"

/* PRIVATE */

KHASH_INIT(bgpstream_str_set, char *, char, 0, kh_str_hash_func,
           kh_str_hash_equal)

struct bgpstream_str_set_t {
  khiter_t k;
  khash_t(bgpstream_str_set) * hash;
};

/* PUBLIC FUNCTIONS */

bgpstream_str_set_t *bgpstream_str_set_create()
{
  bgpstream_str_set_t *set;

  if ((set = (bgpstream_str_set_t *)malloc(sizeof(bgpstream_str_set_t))) ==
      NULL) {
    return NULL;
  }

  if ((set->hash = kh_init(bgpstream_str_set)) == NULL) {
    bgpstream_str_set_destroy(set);
    return NULL;
  }

  bgpstream_str_set_rewind(set);
  return set;
}

int bgpstream_str_set_insert(bgpstream_str_set_t *set, const char *val)
{
  int khret;
  khiter_t k;
  char *cpy;
  if ((cpy = strdup(val)) == NULL) {
    return -1;
  }

  if ((k = kh_get(bgpstream_str_set, set->hash, cpy)) == kh_end(set->hash)) {
    k = kh_put(bgpstream_str_set, set->hash, cpy, &khret);
    return 1;
  } else {
    free(cpy);
  }
  return 0;
}

int bgpstream_str_set_remove(bgpstream_str_set_t *set, char *val)
{
  khiter_t k;
  bgpstream_str_set_rewind(set);
  if ((k = kh_get(bgpstream_str_set, set->hash, val)) != kh_end(set->hash)) {
    // free memory allocated for the key (string)
    free(kh_key(set->hash, k));
    // delete entry
    kh_del(bgpstream_str_set, set->hash, k);
    return 1;
  }
  return 0;
}

int bgpstream_str_set_exists(bgpstream_str_set_t *set, char *val)
{
  khiter_t k;
  if ((k = kh_get(bgpstream_str_set, set->hash, val)) == kh_end(set->hash)) {
    return 0;
  }
  return 1;
}

int bgpstream_str_set_size(bgpstream_str_set_t *set)
{
  return kh_size(set->hash);
}

int bgpstream_str_set_merge(bgpstream_str_set_t *dst_set,
                            bgpstream_str_set_t *src_set)
{
  khiter_t k;

  for (k = kh_begin(src_set->hash); k != kh_end(src_set->hash); ++k) {
    if (kh_exist(src_set->hash, k)) {
      if (bgpstream_str_set_insert(dst_set, kh_key(src_set->hash, k)) < 0) {
        return -1;
      }
    }
  }
  bgpstream_str_set_rewind(dst_set);
  bgpstream_str_set_rewind(src_set);
  return 0;
}

void bgpstream_str_set_rewind(bgpstream_str_set_t *set)
{
  set->k = kh_begin(set->hash);
}

char *bgpstream_str_set_next(bgpstream_str_set_t *set)
{
  char *v = NULL;
  for (; set->k != kh_end(set->hash); ++set->k) {
    if (kh_exist(set->hash, set->k)) {
      v = kh_key(set->hash, set->k);
      set->k++;
      return v;
    }
  }
  return v;
}

void bgpstream_str_set_clear(bgpstream_str_set_t *set)
{
  khiter_t k;
  bgpstream_str_set_rewind(set);
  for (k = kh_begin(set->hash); k != kh_end(set->hash); ++k) {
    if (kh_exist(set->hash, k)) {
      free(kh_key(set->hash, k));
    }
  }
  kh_clear(bgpstream_str_set, set->hash);
}

void bgpstream_str_set_destroy(bgpstream_str_set_t *set)
{
  khiter_t k;
  if (set->hash != NULL) {
    for (k = kh_begin(set->hash); k != kh_end(set->hash); ++k) {
      if (kh_exist(set->hash, k)) {
        free(kh_key(set->hash, k));
      }
    }
    kh_destroy(bgpstream_str_set, set->hash);
    set->hash = NULL;
  }
  free(set);
}
