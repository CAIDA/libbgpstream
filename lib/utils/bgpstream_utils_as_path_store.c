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

/** A per-origin set of AS Paths */
typedef struct pathset {

  /** Array of AS Paths in the set */
  bgpstream_as_path_store_path_t **paths;

  /** Number of AS paths in the set */
  uint16_t paths_cnt;

} __attribute__((packed)) pathset_t;

KHASH_INIT(pathset, uint32_t, pathset_t, 1,
           kh_int_hash_func, kh_int_hash_equal);

struct bgpstream_as_path_store {

  khash_t(pathset) *path_set;

  /** The total number of paths in the store */
  uint32_t paths_cnt;

};

static void store_path_destroy(bgpstream_as_path_store_path_t *spath)
{
  if(spath == NULL)
    {
      return;
    }

  bgpstream_as_path_destroy(spath->path);
  spath->path = NULL;

  free(spath);
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

static int
store_path_equal(bgpstream_as_path_store_path_t *sp1,
                 bgpstream_as_path_store_path_t *sp2)
{
  return bgpstream_as_path_equal(sp1->path, sp2->path);
}

static void
pathset_destroy(pathset_t ps)
{
  int i;

  /* destroy each store path */
  for(i=0; i<ps.paths_cnt; i++)
    {
      store_path_destroy(ps.paths[i]);
      ps.paths[i] = NULL;
    }

  /* destroy the array of paths */
  free(ps.paths);
  ps.paths = NULL;
  ps.paths_cnt = 0;
}

static uint16_t
pathset_get_path_id(bgpstream_as_path_store_t *store,
                    pathset_t *ps, bgpstream_as_path_t *path)
{
  bgpstream_as_path_store_path_t findme;
  findme.path = path;
  int i;
  uint32_t path_id = UINT16_MAX;

  /* check if it is already in the set */
  for(i=0; i<ps->paths_cnt; i++)
    {
      if(store_path_equal(ps->paths[i], &findme) != 0)
        {
          path_id = i;
          break;
        }
    }

  if(path_id == UINT16_MAX)
    {
      /* need to append this path */
      if((ps->paths = realloc(ps->paths,
                              sizeof(bgpstream_as_path_store_path_t*) *
                              (ps->paths_cnt+1))) == NULL)
        {
          fprintf(stderr, "ERROR: Could not realloc paths\n");
          return UINT16_MAX;
        }
      if((ps->paths[ps->paths_cnt] = store_path_create(path)) == NULL)
        {
          fprintf(stderr, "ERROR: Could not create store path\n");
          return UINT16_MAX;
        }
      path_id = ps->paths_cnt++;
      assert(ps->paths_cnt <= UINT16_MAX);
      store->paths_cnt++;
    }

  return path_id;
}

/* ==================== PUBLIC FUNCTIONS ==================== */

bgpstream_as_path_store_t *bgpstream_as_path_store_create()
{
  bgpstream_as_path_store_t *store;

  if((store = malloc_zero(sizeof(bgpstream_as_path_store_t))) == NULL)
    {
      return NULL;
    }

  if((store->path_set = kh_init(pathset)) == NULL)
    {
      goto err;
    }

  return store;

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

  if(store->path_set != NULL)
    {
      kh_free_vals(pathset, store->path_set, pathset_destroy);
      kh_destroy(pathset, store->path_set);
      store->path_set = NULL;
    }

  free(store);
}

uint32_t bgpstream_as_path_store_get_size(bgpstream_as_path_store_t *store)
{
  return store->paths_cnt;
}

int
bgpstream_as_path_store_get_path_id(bgpstream_as_path_store_t *store,
                                    bgpstream_as_path_t *path,
                                    bgpstream_as_path_store_path_id_t *id)
{
  khiter_t k;
  int khret;

  assert(id != NULL);
  id->path_hash = bgpstream_as_path_hash(path);

#if 0
  char buf[1024];
  bgpstream_as_path_snprintf(buf, 1024, path);
  fprintf(stderr, "----\nINFO: Hashed |%s| to %"PRIu32"\n",
          buf, id->path_hash);
#endif

  k = kh_put(pathset, store->path_set, id->path_hash, &khret);
  if(khret == 1)
    {
      /* just added this origin */
      /* clear the pathset fields */
      kh_val(store->path_set, k).paths = NULL;
      kh_val(store->path_set, k).paths_cnt = 0;

#if 0
      fprintf(stderr, "INFO: Created new pathset at k=%d\n", k);
#endif
    }
  else if(khret != 0)
    {
      fprintf(stderr, "ERROR: Could not add origin set to the store\n");
      goto err;
    }

  /* now get the path id from the origin set */
  if((id->path_id =
      pathset_get_path_id(store, &kh_val(store->path_set, k), path))
     == UINT16_MAX)
    {
      fprintf(stderr, "ERROR: Could not add path to origin set\n");
      goto err;
    }

#if 0
  fprintf(stderr, "INFO: Added path at %"PRIu32":%"PRIu16"\n",
          k, id->path_id);
#endif

  return 0;

 err:
  return -1;
}

bgpstream_as_path_store_path_t *
bgpstream_as_path_store_get_store_path(bgpstream_as_path_store_t *store,
                                       bgpstream_as_path_store_path_id_t id)
{
  khiter_t k;

  if((k = kh_get(pathset, store->path_set, id.path_hash)) ==
     kh_end(store->path_set))
    {
      return NULL;
    }

  if(id.path_id > kh_val(store->path_set, k).paths_cnt-1)
    {
      return NULL;
    }

  return kh_val(store->path_set, k).paths[id.path_id];
}

bgpstream_as_path_t *
bgpstream_as_path_store_path_get_path(bgpstream_as_path_store_path_t *store_path)
{
  assert(store_path != NULL);
  return store_path->path;
}
