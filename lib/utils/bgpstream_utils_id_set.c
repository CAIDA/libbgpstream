/*
 * This file is part of bgpstream
 *
 * Copyright (C) 2015 The Regents of the University of California.
 * Authors: Alistair King, Chiara Orsini
 *
 * All rights reserved.
 *
 * This code has been developed by CAIDA at UC San Diego.
 * For more information, contact bgpstream-info@caida.org
 *
 * This source code is proprietary to the CAIDA group at UC San Diego and may
 * not be redistributed, published or disclosed without prior permission from
 * CAIDA.
 *
 * Report any bugs, questions or comments to bgpstream-info@caida.org
 *
 */

#include <assert.h>
#include <stdio.h>

#include "khash.h"
#include "utils.h"

#include <bgpstream_utils_id_set.h>

/* PRIVATE */

/** set of unique ids
 *  this structure maintains a set of unique
 *  ids (using a uint32 type)
 */
KHASH_INIT(bgpstream_id_set /* name */,
	   uint32_t  /* khkey_t */,
	   char /* khval_t */,
	   0  /* kh_is_set */,
	   kh_int_hash_func /*__hash_func */,
	   kh_int_hash_equal /* __hash_equal */);


struct bgpstream_id_set {
  khash_t(bgpstream_id_set) *hash;
};


/* PUBLIC FUNCTIONS */

bgpstream_id_set_t *bgpstream_id_set_create()
{
  bgpstream_id_set_t *set;

  if((set = (bgpstream_id_set_t*)malloc(sizeof(bgpstream_id_set_t))) == NULL)
    {
      return NULL;
    }

  if((set->hash = kh_init(bgpstream_id_set)) == NULL)
    {
      bgpstream_id_set_destroy(set);
      return NULL;
    }

  return set;
}

int bgpstream_id_set_insert(bgpstream_id_set_t *set,  uint32_t id)
{
  int khret;
  khiter_t k;
  if((k = kh_get(bgpstream_id_set, set->hash, id)) == kh_end(set->hash))
    {
      /** @todo we should always check the return value from khash funcs */
      k = kh_put(bgpstream_id_set, set->hash, id, &khret);
      return 1;
    }
  return 0;
}

int bgpstream_id_set_exists(bgpstream_id_set_t *set,  uint32_t id)
{
  khiter_t k;
  if((k = kh_get(bgpstream_id_set, set->hash, id)) == kh_end(set->hash))
    {
      return 0;
    }
  return 1;
}

int bgpstream_id_set_merge(bgpstream_id_set_t *dst_set,
                           bgpstream_id_set_t *src_set)
{
  khiter_t k;
  for(k = kh_begin(src_set->hash); k != kh_end(src_set->hash); ++k)
    {
      if(kh_exist(src_set->hash, k))
	{
	  if(bgpstream_id_set_insert(dst_set, kh_key(src_set->hash, k)) < 0)
            {
              return -1;
            }
	}
    }
  return 0;
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
  kh_clear(bgpstream_id_set, set->hash);
}
