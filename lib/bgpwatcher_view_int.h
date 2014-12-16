/*
 * bgpwatcher
 *
 * Chiara Orsini, CAIDA, UC San Diego
 * corsaro-info@caida.org
 *
 * Copyright (C) 2014 The Regents of the University of California.
 *
 * This file is part of bgpwatcher.
 *
 * bgpwatcher is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * bgpwatcher is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with bgpwatcher.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef __BGPWATCHER_VIEW_INT_H
#define __BGPWATCHER_VIEW_INT_H

#include "config.h" // needed for time header detection

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_TIME_H
#include <time.h>
#endif

#include "khash.h"
#include "utils.h"

#include "bl_bgp_utils.h"

#include "bgpwatcher_view.h"
#include "bgpwatcher_common.h"

/************ map from peer -> prefix info ************/

KHASH_INIT(bwv_peerid_pfxinfo, bl_peerid_t, bgpwatcher_pfx_peer_info_t, 1,
	   kh_int_hash_func, kh_int_hash_equal)
typedef khash_t(bwv_peerid_pfxinfo) bwv_peerid_pfxinfo_t;
// TODO: add documentation



/************ map from prefix -> peers [-> prefix info] ************/

KHASH_INIT(bwv_v4pfx_peerid_pfxinfo, bl_ipv4_pfx_t, bwv_peerid_pfxinfo_t *, 1,
	   bl_ipv4_pfx_hash_func, bl_ipv4_pfx_hash_equal)
typedef khash_t(bwv_v4pfx_peerid_pfxinfo) bwv_v4pfx_peerid_pfxinfo_t;

KHASH_INIT(bwv_v6pfx_peerid_pfxinfo, bl_ipv6_pfx_t, bwv_peerid_pfxinfo_t *, 1,
	   bl_ipv6_pfx_hash_func, bl_ipv6_pfx_hash_equal)
typedef khash_t(bwv_v6pfx_peerid_pfxinfo) bwv_v6pfx_peerid_pfxinfo_t;


/************ bgpview ************/

// TODO: documentation
struct bgpwatcher_view {

  /** BGP Time that the view represents */
  uint32_t time;

  /** Wall time when the view was created */
  struct timeval time_created;

  /** Table of prefix info for v4 prefixes */
  bwv_v4pfx_peerid_pfxinfo_t *v4pfxs;

  /** Table of prefix info for v6 prefixes */
  bwv_v6pfx_peerid_pfxinfo_t *v6pfxs;

  /** Table of peerid -> peersign (could be shared) */
  bl_peersign_map_t *peersigns;

  /** Is the peersigns table shared? */
  int peersigns_shared;
};

/** Add a prefix to a view
 *
 * @param view          view to add prefix to
 * @param prefix        borrowed pointer to prefix to add
 * @param peerid        id of peer to add info for
 * @param pfx_info      prefix info to add for given peer/prefix
 * @return 0 if successful, -1 otherwise
 */
int bgpwatcher_view_add_prefix(bgpwatcher_view_t *view,
                               bl_pfx_storage_t *prefix,
                               bl_peerid_t peerid,
                               bgpwatcher_pfx_peer_info_t *pfx_info);

#endif /* __BGPWATCHER_VIEW_INT_H */
