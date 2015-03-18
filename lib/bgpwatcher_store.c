/*
 * This file is part of bgpwatcher
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

/** @todo this should all use the bgpwatcher error framework */

#include "bgpwatcher_store.h"

#include "bgpwatcher_server_int.h"
#include "bgpwatcher_view.h"

#include "bgpstream_utils_str_set.h"
#include "bgpstream_utils_id_set.h"
#include "bgpstream_utils_peer_sig_map.h"
#include "utils.h"

#define WDW_LEN         (store->sviews_cnt)
#define WDW_ITEM_TIME   60
#define WDW_DURATION    WDW_LEN * WDW_ITEM_TIME

#define BGPWATCHER_STORE_BGPVIEW_TIMEOUT 3600
#define BGPWATCHER_STORE_MAX_PEERS_CNT   1024

#define METRIC_PREFIX "bgp.meta.bgpwatcher.server.store"

#define DUMP_METRIC(value, time, fmt, ...)                      \
do {                                                            \
  fprintf(stdout, METRIC_PREFIX"."fmt" %"PRIu64" %"PRIu32"\n",  \
          __VA_ARGS__, value, time);                            \
 } while(0)

#define VIEW_GET_SVIEW(store, viewp)                                     \
  (store->sviews[                                                       \
                 (store->sviews_first_idx +                             \
                  ((bgpwatcher_view_get_time(viewp)                     \
                    - store->sviews_first_time) / WDW_ITEM_TIME))       \
                 % WDW_LEN                                              \
                                                                        ])
#define SVIEW_TIME(sview)                       \
  (bgpwatcher_view_get_time(sview->view))

typedef enum {
  COMPLETION_TRIGGER_STATE_UNKNOWN        = 0,
  COMPLETION_TRIGGER_WDW_EXCEEDED         = 1,
  COMPLETION_TRIGGER_CLIENT_DISCONNECT    = 2,
  COMPLETION_TRIGGER_TABLE_END            = 3,
  COMPLETION_TRIGGER_TIMEOUT_EXPIRED      = 4
} completion_trigger_t;

typedef enum {
  STORE_VIEW_UNUSED         = 0,
  STORE_VIEW_UNKNOWN        = 1,
  STORE_VIEW_PARTIAL        = 2,
  STORE_VIEW_FULL           = 3,
  STORE_VIEW_STATE_MAX      = STORE_VIEW_FULL,
} store_view_state_t;

static char *store_view_state_names[] = {
  "unused",
  "unknown",
  "partial",
  "full",
};

/* one view will be hard-cleared at each cycle through the window */
#define STORE_VIEW_REUSE_MAX WDW_LEN

/* dispatcher status */
typedef struct dispatch_status {
  uint8_t sent;
  uint8_t modified;
} dispatch_status_t;

/** Wrapper around a bgpwatcher_view_t structure */
typedef struct store_view {

  /* Index of this view within the circular buffer */
  int id;

  /** State of this view (unused, partial, full) */
  store_view_state_t state;

  /** Number of times that this store has been reused */
  int reuse_cnt;

  /** Number of uses remaining before this view must be hard-cleared */
  int reuse_remaining;

  /** Number of times this view has been published since it was last cleared */
  int pub_cnt;

  dispatch_status_t dis_status[STORE_VIEW_STATE_MAX+1];

  /** whether the bgpview has been modified
   *  since the last dump */
  uint8_t modified;

  /** list of clients that have sent at least one complete table */
  bgpstream_str_set_t *done_clients;

  /** BGP Watcher View that this view represents */
  bgpwatcher_view_t *view;

} store_view_t;

KHASH_INIT(strclientstatus, char*, bgpwatcher_server_client_info_t , 1,
	   kh_str_hash_func, kh_str_hash_equal)
typedef khash_t(strclientstatus) clientinfo_map_t;

struct bgpwatcher_store {
  /** BGP Watcher server handle */
  bgpwatcher_server_t *server;

  /** Circular buffer of views */
  store_view_t **sviews;

  /** Number of views in the circular buffer */
  int sviews_cnt;

  /** The index of the first (oldest) view */
  uint32_t sviews_first_idx;

  /** The time of the first (oldest) view */
  uint32_t sviews_first_time;

  /** active_clients contains, for each registered/active client (i.e. those
   *  that are currently connected) its status.*/
  clientinfo_map_t *active_clients;

  /** Shared peersign table (each sview->view borrows a reference to this) */
  bgpstream_peer_sig_map_t *peersigns;
};

enum {
  WINDOW_TIME_EXCEEDED,
  WINDOW_TIME_VALID,
};


/* ========== PRIVATE FUNCTIONS ========== */

static void store_view_destroy(store_view_t *sview)
{
  if(sview == NULL)
    {
      return;
    }

  if(sview->done_clients != NULL)
    {
      bgpstream_str_set_destroy(sview->done_clients);
      sview->done_clients = NULL;
    }

  bgpwatcher_view_destroy(sview->view);
  sview->view = NULL;

  free(sview);
}

store_view_t *store_view_create(bgpwatcher_store_t *store, int id)
{
  store_view_t *sview;
  if((sview = malloc_zero(sizeof(store_view_t))) == NULL)
    {
      return NULL;
    }

  sview->id = id;

  sview->reuse_remaining = STORE_VIEW_REUSE_MAX-1;

  if((sview->done_clients = bgpstream_str_set_create()) == NULL)
    {
      goto err;
    }

  sview->state = STORE_VIEW_UNUSED;

  // dis_status -> everything is set to zero

  assert(store->peersigns != NULL);
  if((sview->view =
      bgpwatcher_view_create_shared(store->peersigns,
                                    NULL, NULL, NULL, NULL)) == NULL)
    {
      goto err;
    }

  return sview;

 err:
  store_view_destroy(sview);
  return NULL;
}

static store_view_t *store_view_clear(bgpwatcher_store_t *store,
				      store_view_t *sview)
{
  int i, idx;

  assert(sview != NULL);

  /* after many soft-clears we force a hard-clear of the view to prevent the
     accumulation of prefix info for prefixes that are no longer in use */
  if(sview->reuse_remaining == 0)
    {
      fprintf(stderr, "DEBUG: Forcing hard-clear of sview\n");
      /* we need the index of this view */
      idx = sview->id;

      store_view_destroy(sview);
      if((store->sviews[idx] = store_view_create(store, idx)) == NULL)
	{
	  return NULL;
	}
      return store->sviews[idx];
    }

  fprintf(stderr, "DEBUG: Clearing store (%d)\n", SVIEW_TIME(sview));

  sview->state = STORE_VIEW_UNUSED;

  sview->reuse_cnt++;
  sview->reuse_remaining--;

  for(i=0; i<=STORE_VIEW_STATE_MAX; i++)
    {
      sview->dis_status[i].modified = 0;
      sview->dis_status[i].sent = 0;
    }

  sview->modified = 0;

  bgpstream_str_set_clear(sview->done_clients);

  sview->pub_cnt = 0;

  /* now clear the child view */
  bgpwatcher_view_clear(sview->view);

  return sview;
}

static int store_view_completion_check(bgpwatcher_store_t *store,
                                       store_view_t *sview)
{
  khiter_t k;
  bgpwatcher_server_client_info_t *client;

  for (k = kh_begin(store->active_clients);
       k != kh_end(store->active_clients); ++k)
    {
      if (!kh_exist(store->active_clients, k))
	{
	  continue;
	}

      client = &(kh_value(store->active_clients, k));
      if(client->intents & BGPWATCHER_PRODUCER_INTENT_PREFIX)
	{
	  // check if all the producers are done with sending pfx tables
	  if(bgpstream_str_set_exists(sview->done_clients, client->name) == 0)
	    {
	      sview->state = STORE_VIEW_PARTIAL;
	      return 0;
	    }
	}
    }

  // view complete
  sview->state = STORE_VIEW_FULL;
  return 1;
}

static int store_view_remove(bgpwatcher_store_t *store, store_view_t *sview)
{
  /* slide the window? */
  /* only if SVIEW_TIME(sview) == first_time */
  if(SVIEW_TIME(sview) == store->sviews_first_time)
    {
      store->sviews_first_time += WDW_ITEM_TIME;
      store->sviews_first_idx = (store->sviews_first_idx+1) % WDW_LEN;
    }

  /* clear out stuff */
  if((sview = store_view_clear(store, sview)) == NULL)
    {
      return -1;
    }

  return 0;
}

static int dispatcher_run(bgpwatcher_store_t *store,
                          store_view_t *sview,
                          completion_trigger_t trigger)
{
#if 0
  bgpstream_peer_id_t valid_peers[BGPWATCHER_STORE_MAX_PEERS_CNT];
  int valid_peers_cnt;
#endif
  int dispatch_interests = 0;
  int i;
  int states_cnt[STORE_VIEW_STATE_MAX+1];

  /* interests are hierarchical, so we check the most specific first */
  if(sview->state == STORE_VIEW_FULL &&
     sview->dis_status[STORE_VIEW_FULL].modified != 0)
    {
      if(sview->dis_status[STORE_VIEW_FULL].sent == 0)
        {
          /* send to FIRST-FULL customers */
          dispatch_interests = BGPWATCHER_CONSUMER_INTEREST_FIRSTFULL;
        }
      else
        {
          /* send to FULL customers */
          dispatch_interests = BGPWATCHER_CONSUMER_INTEREST_FULL;
        }
      sview->dis_status[STORE_VIEW_FULL].modified = 0;
      sview->dis_status[STORE_VIEW_FULL].sent = 1;
    }
  else if(sview->state == STORE_VIEW_PARTIAL &&
          sview->dis_status[STORE_VIEW_PARTIAL].modified != 0)
    {
      /* send to PARTIAL customers */
      dispatch_interests = BGPWATCHER_CONSUMER_INTEREST_PARTIAL;
      sview->dis_status[STORE_VIEW_PARTIAL].modified = 0;
      sview->dis_status[STORE_VIEW_PARTIAL].sent = 1;
    }

#if 0
  /* nothing to dispatch */
  if(dispatch_interests == 0)
    {
      return 0;
    }
#endif

  /* hax: we can't handle publishing partial tables */
  if(dispatch_interests != BGPWATCHER_CONSUMER_INTEREST_FIRSTFULL)
    {
      return 0;
    }

  /** @todo Chiara we need to build the list of valid peers! */

  /* this metric is the only reason we pass the trigger to this func */
  DUMP_METRIC((uint64_t)trigger,
              SVIEW_TIME(sview),
              "%s", "completion_trigger");

  DUMP_METRIC((uint64_t)bgpstream_str_set_size(sview->done_clients),
              SVIEW_TIME(sview),
              "%s", "done_clients_cnt");
  DUMP_METRIC((uint64_t)kh_size(store->active_clients),
              SVIEW_TIME(sview),
              "%s", "active_clients_cnt");

  DUMP_METRIC((uint64_t)bgpwatcher_view_peer_cnt(sview->view,
						 BGPWATCHER_VIEW_FIELD_ACTIVE),
              SVIEW_TIME(sview),
              "%s", "active_peers_cnt");
  DUMP_METRIC((uint64_t)bgpwatcher_view_peer_cnt(sview->view,
						 BGPWATCHER_VIEW_FIELD_INACTIVE),
              SVIEW_TIME(sview),
	      "%s", "inactive_peers_cnt");

  DUMP_METRIC((uint64_t)bgpstream_peer_sig_map_get_size(store->peersigns),
              SVIEW_TIME(sview),
              "%s", "peersigns_hash_size");

  DUMP_METRIC((uint64_t)store->sviews_first_idx,
              SVIEW_TIME(sview),
              "%s", "view_buffer_head_idx");

  DUMP_METRIC((uint64_t)store->sviews_first_time,
              SVIEW_TIME(sview),
              "%s", "view_buffer_head_time");

  /* count the number of views in each state */
  states_cnt[STORE_VIEW_UNUSED] = 0;
  states_cnt[STORE_VIEW_UNKNOWN] = 0;
  states_cnt[STORE_VIEW_PARTIAL] = 0;
  states_cnt[STORE_VIEW_FULL] = 0;
  for(i=0; i<store->sviews_cnt; i++)
    {
      states_cnt[store->sviews[i]->state]++;
    }
  for(i=0; i<=STORE_VIEW_STATE_MAX; i++)
    {
      DUMP_METRIC((uint64_t)states_cnt[i],
                  SVIEW_TIME(sview),
                  "view_state_%s_cnt", store_view_state_names[i]);
    }

  DUMP_METRIC((uint64_t)bgpwatcher_view_v4pfx_cnt(sview->view,
						  BGPWATCHER_VIEW_FIELD_ACTIVE),
              SVIEW_TIME(sview),
              "views.%d.%s", sview->id, "v4pfxs_cnt");
  DUMP_METRIC((uint64_t)bgpwatcher_view_v6pfx_cnt(sview->view,
						  BGPWATCHER_VIEW_FIELD_ACTIVE),
              SVIEW_TIME(sview),
              "views.%d.%s", sview->id, "v6pfxs_cnt");

  DUMP_METRIC((uint64_t)sview->reuse_cnt,
              SVIEW_TIME(sview),
              "views.%d.%s", sview->id, "reuse_cnt");

  DUMP_METRIC((uint64_t)bgpwatcher_view_get_time_created(sview->view),
              SVIEW_TIME(sview),
              "views.%d.%s", sview->id, "time_created");

  /* now publish the view */
  if(bgpwatcher_server_publish_view(store->server, sview->view,
                                    dispatch_interests) != 0)
    {
      return -1;
    }

  sview->pub_cnt++;

  DUMP_METRIC((uint64_t)sview->pub_cnt,
              SVIEW_TIME(sview),
              "views.%d.%s", sview->id, "publication_cnt");

  return 0;
}

static int completion_check(bgpwatcher_store_t *store, store_view_t *sview,
                            completion_trigger_t trigger)
{
  int to_remove = 0;

  /* returns 1 if full, 0 if partial, but the dispatcher handles partial tables
     so we ignore the return code */
  if(store_view_completion_check(store, sview) < 0)
    {
      return -1;
    }

  /** The completion check can be triggered by different events:
   *  COMPLETION_TRIGGER_TABLE_END
   *            a new prefix table has been completely received
   *  COMPLETION_TRIGGER_WDW_EXCEEDED
   *            the view sliding window has moved forward and some "old"
   *            views need to be destroyed
   *  COMPLETION_TRIGGER_CLIENT_DISCONNECT
   *            a client has disconnected
   *  COMPLETION_TRIGGER_TIMEOUT_EXPIRED
   *            the timeout for a given view is expired
   *
   *  if the trigger is either a timeout expired or a window exceeded, the view
   *  is passed to the dispatcher and never processed again
   *
   *  in any other case the view is passed to the dispatcher but it is not
   *  destroyed, as further processing may be performed
   */

  if((trigger == COMPLETION_TRIGGER_WDW_EXCEEDED ||
      trigger == COMPLETION_TRIGGER_TIMEOUT_EXPIRED))
    {
      sview->state = STORE_VIEW_FULL;
      to_remove = 1;
    }

  // DEBUG information about the current status
  // dump_bgpwatcher_store_cc_status(store, bgp_view, ts, trigger, remove_view);

  // TODO: documentation
  if(dispatcher_run(store, sview, trigger) != 0)
    {
      return -1;
    }

  // TODO: documentation
  if(to_remove == 1)
    {
      return store_view_remove(store, sview);
    }

  return 0;
}

static int store_view_get(bgpwatcher_store_t *store, uint32_t new_time,
                          store_view_t **sview_p)
{
  int i, idx, idx_offset;
  uint32_t min_first_time, slot_time, time_offset;
  store_view_t *sview;

  assert(sview_p != NULL);
  *sview_p = NULL;

  /* new_time MUST be a multiple of the window size */
  assert(((new_time / WDW_ITEM_TIME) * WDW_ITEM_TIME) == new_time);

  /* no need to explicitly handle this case, just assume the first time is 0 at
     start up and then let the window slide code handle everything */
#if 0
  /* is this the first insertion? */
  if(store->sviews_first_time == 0)
    {
      store->sviews_first_time = (new_time - WDW_DURATION + WDW_ITEM_TIME);
      sview = store->sviews[WDW_LEN-1];
      goto valid;
    }
#endif

  if(new_time < store->sviews_first_time)
    {
      /* before the window */
      return WINDOW_TIME_EXCEEDED;
    }

  if(new_time < (store->sviews_first_time + WDW_DURATION))
    {
      /* inside window */
      idx = (( (new_time - store->sviews_first_time) / WDW_ITEM_TIME )
             + store->sviews_first_idx) % WDW_LEN;

      assert(idx >= 0 && idx < WDW_LEN);
      sview = store->sviews[idx];
      goto valid;
    }

  /* if we reach here, we must slide the window */

  /* this will be the first valid view in the window */
  min_first_time =
    (new_time - WDW_DURATION) + WDW_ITEM_TIME;

  idx_offset = store->sviews_first_idx;
  time_offset = store->sviews_first_time;
  for(i=0; i<WDW_LEN; i++)
    {
      idx = (i + idx_offset) % WDW_LEN;
      slot_time = (i * WDW_ITEM_TIME) + time_offset;

      sview = store->sviews[idx];
      assert(sview != NULL);

      /* update the head of window */
      store->sviews_first_idx = idx;
      store->sviews_first_time = slot_time;

      /* check if we have slid enough */
      if(slot_time >= min_first_time)
        {
          break;
        }

      if(sview->state == STORE_VIEW_UNUSED)
        {
          continue;
        }

      /* expire tables with time < new_first_time */
      if(completion_check(store, sview,
			  COMPLETION_TRIGGER_WDW_EXCEEDED) < 0)
	{
	  return -1;
	}
    }

  /* special case when the new time causes the whole window to be cleared */
  /* without this, the new time would be inserted somewhere in the window */
  if(store->sviews_first_time < min_first_time)
    {
      store->sviews_first_time = min_first_time;
    }

  idx = (store->sviews_first_idx +
         ((new_time - store->sviews_first_time) / WDW_ITEM_TIME)) % WDW_LEN;
  sview = store->sviews[idx];
  goto valid;

 valid:
  sview->state = STORE_VIEW_UNKNOWN;
  bgpwatcher_view_set_time(sview->view, new_time);
  *sview_p = sview;
  return WINDOW_TIME_VALID;
}

static void store_views_dump(bgpwatcher_store_t *store)
{
  int i, idx;

  fprintf(stderr, "--------------------\n");

  for(i=0; i<WDW_LEN; i++)
    {
      idx = (i + store->sviews_first_idx) % WDW_LEN;

      fprintf(stderr, "%d (%d): ", i, idx);

      if(store->sviews[idx]->state == STORE_VIEW_UNUSED)
        {
          fprintf(stderr, "unused\n");
        }
      else
        {
          fprintf(stderr, "%d\n",
                  bgpwatcher_view_get_time(store->sviews[idx]->view));
        }
    }

  fprintf(stderr, "--------------------\n\n");
}

/* ========== PROTECTED FUNCTIONS ========== */

bgpwatcher_store_t *bgpwatcher_store_create(bgpwatcher_server_t *server,
					    int window_len)
{
  bgpwatcher_store_t *store;
  int i;

  // allocate memory for the structure
  if((store = malloc_zero(sizeof(bgpwatcher_store_t))) == NULL)
    {
      return NULL;
    }

  store->server = server;

  if((store->active_clients = kh_init(strclientstatus)) == NULL)
    {
      fprintf(stderr, "Failed to create active_clients\n");
      goto err;
    }

  if((store->peersigns = bgpstream_peer_sig_map_create()) == NULL)
    {
      fprintf(stderr, "Failed to create peersigns table\n");
      goto err;
    }

  if((store->sviews = malloc(sizeof(store_view_t*) * window_len)) == NULL)
    {
      fprintf(stderr, "Failed to malloc the store view buffer\n");
      goto err;
    }

  store->sviews_cnt = window_len;

  /* must be created after peersigns */
  for(i=0; i<WDW_LEN; i++)
    {
      if((store->sviews[i] = store_view_create(store, i)) == NULL)
        {
          goto err;
        }
      /* tweak the reuse_remaining to stagger hard-clears */
      store->sviews[i]->reuse_remaining += i;
    }

  return store;

 err:
  if(store != NULL)
    {
      bgpwatcher_store_destroy(store);
    }
  return NULL;
}

static void str_free(char *str)
{
  free(str);
}

void bgpwatcher_store_destroy(bgpwatcher_store_t *store)
{
  int i;

  if(store == NULL)
    {
      return;
    }

  for(i=0; i<WDW_LEN; i++)
    {
      store_view_destroy(store->sviews[i]);
      store->sviews[i] = NULL;
    }

  free(store->sviews);
  store->sviews = NULL;
  store->sviews_cnt = 0;

  if(store->active_clients != NULL)
    {
      kh_free(strclientstatus, store->active_clients, str_free);
      kh_destroy(strclientstatus, store->active_clients);
      store->active_clients = NULL;
    }

  if(store->peersigns != NULL)
    {
      bgpstream_peer_sig_map_destroy(store->peersigns);
      store->peersigns = NULL;
    }

  free(store);
}

int bgpwatcher_store_client_connect(bgpwatcher_store_t *store,
                                    bgpwatcher_server_client_info_t *client)
{
  khiter_t k;
  int khret;

  char *name_cpy;

  // check if it does not exist
  if((k = kh_get(strclientstatus, store->active_clients, client->name))
     == kh_end(store->active_clients))
    {
      // allocate new memory for the string
      if((name_cpy = strdup(client->name)) == NULL)
	{
	  return -1;
	}
      // put key in table
      k = kh_put(strclientstatus, store->active_clients, name_cpy, &khret);
    }

  // update or insert new client info
  kh_value(store->active_clients, k) = *client;

  return 0;
}



int bgpwatcher_store_client_disconnect(bgpwatcher_store_t *store,
                                       bgpwatcher_server_client_info_t *client)
{
  int i;
  khiter_t k;
  store_view_t *sview;

  // check if it exists
  if((k = kh_get(strclientstatus, store->active_clients, client->name))
     != kh_end(store->active_clients))
    {
      // free memory allocated for the key (string)
      free(kh_key(store->active_clients,k));
      // delete entry
      kh_del(strclientstatus,store->active_clients,k);
    }

  /* notify each view that a client has disconnected */
  for(i=0; i<WDW_LEN; i++)
    {
      sview = store->sviews[i];
      if(sview->state != STORE_VIEW_UNUSED)
        {
	  completion_check(store, sview, COMPLETION_TRIGGER_CLIENT_DISCONNECT);
	}
    }
  return 0;
}

bgpwatcher_view_t * bgpwatcher_store_get_view(bgpwatcher_store_t *store,
                                              uint32_t time)
{
  store_view_t *sview = NULL;
  int ret;
  uint32_t truncated_time = (time / WDW_ITEM_TIME) * WDW_ITEM_TIME;

  if((ret = store_view_get(store, truncated_time, &sview)) < 0)
    {
      return NULL;
    }

  store_views_dump(store);

  if(ret == WINDOW_TIME_EXCEEDED)
    {
      fprintf(stderr,
              "BGP Views for time %"PRIu32" have been already processed\n",
              truncated_time);
      // signal to server that this table should be ignored
      return NULL;
    }

  sview->state = STORE_VIEW_UNKNOWN;

  return sview->view;
}

int bgpwatcher_store_view_updated(bgpwatcher_store_t *store,
                                  bgpwatcher_view_t *view,
                                  bgpwatcher_server_client_info_t *client)
{
  store_view_t * sview;
  int i;

  if(view == NULL)
    {
      return 0;
    }

  sview = VIEW_GET_SVIEW(store, view);
  assert(sview);

  // add this client to the list of clients done
  bgpstream_str_set_insert(sview->done_clients, client->name);

  for(i = 0; i <= STORE_VIEW_STATE_MAX; i++)
    {
      sview->dis_status[i].modified = 1;
    }

  completion_check(store, sview, COMPLETION_TRIGGER_TABLE_END);
  return 0;
}

#if 0
int bgpwatcher_store_prefix_table_row(bgpwatcher_store_t *store,
                                      bgpwatcher_pfx_table_t *table,
                                      bgpstream_pfx_t *pfx,
                                      bgpwatcher_pfx_peer_info_t *peer_infos)
{
  store_view_t *sview;

  int i;
  bgpwatcher_pfx_peer_info_t *pfx_info;
  bgpstream_peer_id_t server_id;

  active_peer_status_t *ap_status;

  /* sneaky trick to cache the prefix info */
  void *view_cache = NULL;

  if(table->sview == NULL)
    {
      // the view for this ts has been already removed
      // ignore this message
      // AK removes call to check_timeouts as it is redundant
      return 0;
    }

  // retrieve sview from the cache
  sview = (store_view_t*)table->sview;
  assert(sview != NULL);

  sview->state = STORE_VIEW_UNKNOWN;

  for(i=0; i<table->peers_cnt; i++)
    {
      if(peer_infos[i].state != BGPWATCHER_VIEW_FIELD_ACTIVE)
	{
          continue;
        }
      server_id = table->peers[i].server_id;
      pfx_info = &(peer_infos[i]);

      if(bgpwatcher_view_add_prefix(sview->view, pfx,
                                    server_id, pfx_info->orig_asn, &view_cache) != 0)
        {
          return -1;
        }

      // get the active peer status ptr for the current id
      ap_status = (active_peer_status_t *)table->peers[i].ap_status;
      assert(ap_status != NULL);

      // update counters
      if(pfx->address.version == BGPSTREAM_ADDR_VERSION_IPV4)
        {
          ap_status->recived_ipv4_pfx_cnt++;
        }
      else
        {
          if(pfx->address.version == BGPSTREAM_ADDR_VERSION_IPV6)
            {
              ap_status->recived_ipv6_pfx_cnt++;
            }
        }
    }

  return 0;
}
#endif

int bgpwatcher_store_check_timeouts(bgpwatcher_store_t *store)
{
  store_view_t *sview = NULL;
  int i, idx;
  struct timeval time_now;
  gettimeofday(&time_now, NULL);

  for(i=0; i<WDW_LEN; i++)
    {
      idx = (i + store->sviews_first_idx) % WDW_LEN;

      sview = store->sviews[idx];
      if(sview->state == STORE_VIEW_UNUSED)
        {
          continue;
        }

      if((time_now.tv_sec - bgpwatcher_view_get_time_created(sview->view))
         > BGPWATCHER_STORE_BGPVIEW_TIMEOUT)
        {
          if(completion_check(store, sview,
                              COMPLETION_TRIGGER_TIMEOUT_EXPIRED) != 0)
            {
              return -1;
            }
        }
    }
  return 0;
}

/* ========== DISABLED FUNCTIONS ========== */

#if 0
static void dump_bgpwatcher_store_cc_status(bgpwatcher_store_t *store, bgpview_t *bgp_view, uint32_t ts,
				    bgpwatcher_store_completion_trigger_t trigger,
				    uint8_t remove_view)
{
  time_t timer;
  char buffer[25];
  struct tm* tm_info;
  time(&timer);
  tm_info = localtime(&timer);
  strftime(buffer, sizeof(buffer), "%H:%M:%S", tm_info);

  fprintf(stderr,"\n[%s] CC on bgp time: %d \n", buffer, ts);
  switch(trigger)
    {
    case BGPWATCHER_STORE_TABLE_END:
      fprintf(stderr,"\tReason:\t\tTABLE_END\n");
      break;
    case BGPWATCHER_STORE_TIMEOUT_EXPIRED:
      fprintf(stderr,"\tReason:\t\tTIMEOUT_EXPIRED\n");
      break;
    case BGPWATCHER_STORE_CLIENT_DISCONNECT:
      fprintf(stderr,"\tReason:\t\tCLIENT_DISCONNECT\n");
      break;
    case BGPWATCHER_STORE_WDW_EXCEEDED:
      fprintf(stderr,"\tReason:\t\tWDW_EXCEEDED\n");
      break;
    default:
      fprintf(stderr,"\tReason:\t\tUNKNOWN\n");
      break;
    }
  switch(bgp_view->state)
    {
    case BGPVIEW_PARTIAL:
      fprintf(stderr,"\tView state:\tPARTIAL\n");
      break;
    case BGPVIEW_FULL:
      fprintf(stderr,"\tView state:\tCOMPLETE\n");
      break;
    default:
      fprintf(stderr,"\tView state:\tUNKNOWN\n");
      break;
    }
  fprintf(stderr,"\tView removal:\t%d\n", remove_view);
  fprintf(stderr,"\tConnected clients:\t%d\n", kh_size(store->active_clients));
  fprintf(stderr,"\tts window:\t[%d,%d]\n", store->min_ts,
	  store->min_ts + BGPWATCHER_STORE_TS_WDW_SIZE - BGPWATCHER_STORE_TS_WDW_LEN);
  fprintf(stderr,"\ttimeseries size:\t%d\n", kh_size(store->bgp_timeseries));

  fprintf(stderr,"\n");
}

int bgpwatcher_store_ts_completed_handler(bgpwatcher_store_t *store, uint32_t ts)
{
  // get current completed bgpview
  khiter_t k;
  if((k = kh_get(timebgpview, store->bgp_timeseries,
		 ts)) == kh_end(store->bgp_timeseries))
    {
      // view for this time must exist ? TODO: check policies!
      fprintf(stderr, "A bgpview for time %"PRIu32" must exist!\n", ts);
      return -1;
    }
  bgpview_t *bgp_view = kh_value(store->bgp_timeseries,k);

  int ret = bgpwatcher_store_interests_dispatcher_run(store->active_clients, bgp_view, ts);

  // TODO: decide whether to destroy the bgp_view or not

  // destroy view
  bgpview_destroy(bgp_view);

  // destroy time entry
  kh_del(timebgpview,store->bgp_timeseries,k);

  return ret;
}
#endif

