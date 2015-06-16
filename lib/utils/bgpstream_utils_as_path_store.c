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

#include "config.h"

#include <assert.h>
#include <inttypes.h>
#include <stdio.h>

#include "khash.h"
#include "utils.h"

#include "bgpstream_utils_as_path_store.h"

/* wrapper around an AS path */
struct bgpstream_as_path_store_path {
  bgpstream_as_path_t *path;

  /** @todo add flag for Core Paths */
};

#define STORE_PATH_HASH(sp)                     \
  (bgpstream_as_path_hash((sp)->path))

#define STORE_PATH_EQUAL(sp1, sp2)                  \
  (bgpstream_as_path_equal((sp1)->path, (sp2)->path))

KHASH_INIT(pathset, bgpstream_as_path_store_path_t*, char, 0,
           STORE_PATH_HASH, STORE_PATH_EQUAL);

struct bgpstream_as_path_store {

  khash_t(pathset) *paths;

};

static void store_path_destroy(bgpstream_as_path_store_path_t *spath)
{
  if(spath == NULL)
    {
      return;
    }

  bgpstream_as_path_destroy(spath->path);
  spath->path = NULL;
}

static bgpstream_as_path_store_path_t *
store_path_create(bgpstream_as_path_t *path)
{
  bgpstream_as_path_store_path_t *spath;

  if((spath = malloc(sizeof(bgpstream_as_path_store_path_t))) == NULL)
    {
      goto err;
    }

  if((spath->path = bgpstream_as_path_create()) == NULL)
    {
      goto err;
    }

  if(bgpstream_as_path_copy(spath->path, path, 0, 0) != 0)
    {
      goto err;
    }

  return spath;

 err:
  store_path_destroy(spath);
  return NULL;
}

/* ==================== PUBLIC FUNCTIONS ==================== */

bgpstream_as_path_store_t *bgpstream_as_path_store_create()
{
  bgpstream_as_path_store_t *store;

  if((store = malloc_zero(sizeof(bgpstream_as_path_store_t))) == NULL)
    {
      return NULL;
    }

  if((store->paths = kh_init(pathset)) == NULL)
    {
      goto err;
    }

 err:
  bgpstream_as_path_store_destroy(store);
  return NULL;
}

void bgpstream_as_path_store_destroy(bgpstream_as_path_store_t *store)
{
  if(store == NULL)
    {
      return;
    }

  if(store->paths != NULL)
    {
      kh_free(pathset, store->paths, store_path_destroy);
      kh_destroy(pathset, store->paths);
      store->paths = NULL;
    }
}

uint32_t bgpstream_as_path_store_get_path_id(bgpstream_as_path_store_t *store,
                                             bgpstream_as_path_t *path)
{
  khiter_t k;

  bgpstream_as_path_store_path_t findme;
  findme.path = path;

  bgpstream_as_path_store_path_t *addme = NULL;
  int khret;

  if((k = kh_get(pathset, store->paths, &findme)) == kh_end(store->paths))
    {
      /* need to add this path */
      if((addme = store_path_create(path)) == NULL)
        {
          goto err;
        }
      k = kh_put(pathset, store->paths, addme, &khret);
      if(khret == -1)
        {
          goto err;
        }
    }

  /* always increment the k value by one to ensure we never get 0 */
  assert(k < UINT32_MAX);
  return k+1;

 err:
  store_path_destroy(addme);
  return 0;
}

bgpstream_as_path_store_path_t *
bgpstream_as_path_store_get_store_path(bgpstream_as_path_store_t *store,
                                       bgpstream_as_path_t *path)
{
  bgpstream_as_path_store_path_t findme;
  findme.path = path;
  khiter_t k;

  if((k = kh_get(pathset, store->paths, &findme)) == kh_end(store->paths))
    {
      return NULL;
    }

  return kh_key(store->paths, k);
}

bgpstream_as_path_t *
bgpstream_as_path_store_path_get_path(bgpstream_as_path_store_path_t *spath)
{
  bgpstream_as_path_t *newpath;

  if((newpath = bgpstream_as_path_create()) == NULL ||
     bgpstream_as_path_copy(newpath, spath->path, 0, 0) != 0)
    {
      goto err;
    }

  return newpath;

  err:
  bgpstream_as_path_destroy(newpath);
  return NULL;
}
