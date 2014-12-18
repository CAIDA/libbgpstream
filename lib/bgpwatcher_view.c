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

#include <stdio.h>

#include <czmq.h>

#include "bgpwatcher_common_int.h"
#include "bgpwatcher_view_int.h"

/* we need to poke our fingers into the peersign map */
#include "bl_peersign_map_int.h"

#define ASSERT_MORE				\
  if(zsocket_rcvmore(src) == 0)			\
    {						\
      goto err;					\
    }

/* ========== PRIVATE FUNCTIONS ========== */

static bwv_peerid_pfxinfo_t* peerid_pfxinfo_create()
{
  bwv_peerid_pfxinfo_t *v;

  if((v = malloc(sizeof(bwv_peerid_pfxinfo_t))) == NULL)
    {
      return NULL;
    }

  if((v->peers = kh_init(bwv_peerid_pfxinfo)) == NULL)
    {
      free(v);
      return NULL;
    }

  v->peers_cnt = 0;

  return v;
}

static int peerid_pfxinfo_insert(bwv_peerid_pfxinfo_t *v,
                                 bl_peerid_t peerid,
                                 bgpwatcher_pfx_peer_info_t *pfx_info)
{
  khiter_t k;
  int khret;

  /* if we are the first to insert a peer for this prefix after it was cleared,
     we are also responsible for clearing all the peer info */
  if(v->peers_cnt == 0)
    {
      for (k = kh_begin(v->peers); k != kh_end(v->peers); ++k)
	{
	  if (kh_exist(v->peers, k))
	    {
	      kh_value(v->peers, k).in_use = 0;
	    }
	}
    }

  if((k = kh_get(bwv_peerid_pfxinfo, v->peers, peerid)) == kh_end(v->peers))
    {
      k = kh_put(bwv_peerid_pfxinfo,v->peers, peerid, &khret);

      /* we need to at least mark this info as unused */
      kh_value(v->peers, k).in_use = 0;
    }

  /* if this peer was not previously used, we need to count it */
  if(kh_value(v->peers, k).in_use == 0)
    {
      v->peers_cnt++;
    }

  kh_value(v->peers, k) = *pfx_info;
  kh_value(v->peers, k).in_use = 1;
  return 0;
}

static void peerid_pfxinfo_destroy(bwv_peerid_pfxinfo_t *v)
{
  if(v == NULL)
    {
      return;
    }

  kh_destroy(bwv_peerid_pfxinfo, v->peers);
  v->peers_cnt = 0;
  free(v);
}

/** @todo consider making these macros? */
static bwv_peerid_pfxinfo_t *get_v4pfx_peerids(bgpwatcher_view_t *view,
                                               bl_ipv4_pfx_t *v4pfx)
{
  bwv_peerid_pfxinfo_t *peerids_pfxinfo;
  khiter_t k;
  int khret;

  if((k = kh_get(bwv_v4pfx_peerid_pfxinfo, view->v4pfxs, *v4pfx))
     == kh_end(view->v4pfxs))
    {
      if((peerids_pfxinfo = peerid_pfxinfo_create()) == NULL)
	{
	  return NULL;
	}
      k = kh_put(bwv_v4pfx_peerid_pfxinfo, view->v4pfxs, *v4pfx, &khret);
      kh_value(view->v4pfxs, k) = peerids_pfxinfo;
    }
  else
    {
      peerids_pfxinfo =  kh_value(view->v4pfxs, k);
    }

  if(peerids_pfxinfo->peers_cnt == 0)
    {
      view->v4pfxs_cnt++;
    }

  return peerids_pfxinfo;
}

static bwv_peerid_pfxinfo_t *get_v6pfx_peerids(bgpwatcher_view_t *view,
                                               bl_ipv6_pfx_t *v6pfx)
{
  bwv_peerid_pfxinfo_t *peerids_pfxinfo;
  khiter_t k;
  int khret;

  if((k = kh_get(bwv_v6pfx_peerid_pfxinfo, view->v6pfxs, *v6pfx))
     == kh_end(view->v6pfxs))
    {
      if((peerids_pfxinfo = peerid_pfxinfo_create()) == NULL)
	{
	  return NULL;
	}
      k = kh_put(bwv_v6pfx_peerid_pfxinfo, view->v6pfxs, *v6pfx, &khret);
      kh_value(view->v6pfxs, k) = peerids_pfxinfo;
    }
  else
    {
      peerids_pfxinfo = kh_value(view->v6pfxs, k);
    }

  if(peerids_pfxinfo->peers_cnt == 0)
    {
      view->v6pfxs_cnt++;
    }

  return peerids_pfxinfo;
}

static bwv_peerid_pfxinfo_t *get_pfx_peerids(bgpwatcher_view_t *view,
                                             bl_pfx_storage_t *prefix)
{
  if(prefix->address.version == BL_ADDR_IPV4)
    {
      return get_v4pfx_peerids(view, bl_pfx_storage2ipv4(prefix));
    }
  else if(prefix->address.version == BL_ADDR_IPV6)
    {
      return get_v6pfx_peerids(view, bl_pfx_storage2ipv6(prefix));
    }

  return NULL;
}

static void peers_dump(bgpwatcher_view_t *view)
{
  khiter_t k;
  bl_peerid_t peerid;
  bl_peer_signature_t *ps;
  char peer_str[INET6_ADDRSTRLEN] = "";

  fprintf(stdout, "Peers (%d):\n", bl_peersign_map_get_size(view->peersigns));

  for(k = kh_begin(view->peersigns); k != kh_end(view->peersigns->id_ps); ++k)
    {
      if(kh_exist(view->peersigns->id_ps, k))
	{
	  peerid = kh_key(view->peersigns->id_ps, k);
	  ps = &(kh_val(view->peersigns->id_ps, k));

          inet_ntop(ps->peer_ip_addr.version,
                    &(ps->peer_ip_addr.ipv4),
                    peer_str, INET6_ADDRSTRLEN);

	  fprintf(stdout, "  %"PRIu16":\t%s, %s\n",
		  peerid,
		  ps->collector_str,
		  peer_str);
	}
    }
}

static void peerids_dump(bwv_peerid_pfxinfo_t *v)
{
  khiter_t k;
  bl_peerid_t key;
  bgpwatcher_pfx_peer_info_t *val;

  for(k = kh_begin(v->peers); k != kh_end(v->peers); ++k)
    {
      if(!kh_exist(v->peers, k))
	{
	  continue;
	}
      key = kh_key(v->peers, k);
      val = &kh_val(v->peers, k);

      if(val->in_use == 0)
	{
	  continue;
	}

      fprintf(stdout, "    %"PRIu16":\t%"PRIu32"\n",
	      key, val->orig_asn);
    }
}

static void v4pfxs_dump(bgpwatcher_view_t *view)
{
  khiter_t k;
  bl_ipv4_pfx_t *key;
  bwv_peerid_pfxinfo_t *v;
  char pfx_str[INET6_ADDRSTRLEN] = "";

  fprintf(stdout, "V4 Prefixes (%d):\n", view->v4pfxs_cnt);

  for(k = kh_begin(view->v4pfxs); k != kh_end(view->v4pfxs); ++k)
    {
      if(kh_exist(view->v4pfxs, k))
	{
	  key = &kh_key(view->v4pfxs, k);
	  v = kh_value(view->v4pfxs, k);

	  /* is this prefix unused? */
	  if(v->peers_cnt == 0)
	    {
	      continue;
	    }

          inet_ntop(key->address.version,
                    &(key->address.ipv4),
                    pfx_str, INET6_ADDRSTRLEN);

	  fprintf(stdout, "  %s/%d (%"PRIu16" peers)\n",
		  pfx_str, key->mask_len, v->peers_cnt);
	  peerids_dump(v);
	}
    }
}

static int send_pfx_peers(void *dest, bwv_peerid_pfxinfo_t *pfxpeers)
{
  khiter_t k;
  uint16_t u16;
  uint32_t u32;

  for(k = kh_begin(pfxpeers->peers); k != kh_end(pfxpeers->peers); ++k)
    {
      if(kh_exist(pfxpeers->peers, k))
	{
	  /* peer id */
	  u16 = kh_key(pfxpeers->peers, k);
	  u16 = htons(u16);
	  if(zmq_send(dest, &u16, sizeof(u16), ZMQ_SNDMORE) != sizeof(u16))
	    {
	      goto err;
	    }

	  /* orig_asn */
	  u32 = kh_val(pfxpeers->peers, k).orig_asn;
	  u32 = htonl(u32);
	  if(zmq_send(dest, &u32, sizeof(u32), ZMQ_SNDMORE) != sizeof(u32))
	    {
	      goto err;
	    }
	}
    }

  return 0;

 err:
  return -1;
}

/** @todo serialize each prefix to a byte array and send that */
static int send_v4pfxs(void *dest, bgpwatcher_view_t *view)
{
  uint16_t u16;
  uint32_t u32;

  khiter_t k;

  bl_ipv4_pfx_t *key;
  bwv_peerid_pfxinfo_t *v;

  /* pfx cnt */
  u32 = htonl(kh_size(view->v4pfxs));
  if(zmq_send(dest, &u32, sizeof(u32), ZMQ_SNDMORE) != sizeof(u32))
    {
      goto err;
    }

  /* foreach pfx, send pfx-ip, pfx-len, [peer-info] */
  for(k = kh_begin(view->v4pfxs); k != kh_end(view->v4pfxs); ++k)
    {
      if(kh_exist(view->v4pfxs, k))
	{
	  key = &kh_key(view->v4pfxs, k);
	  v = kh_val(view->v4pfxs, k);

	  if(v->peers_cnt == 0)
	    {
	      continue;
	    }

	  /* pfx address */
	  if(bw_send_ip(dest, bl_addr_ipv42storage(&key->address), ZMQ_SNDMORE) != 0)
	    {
	      goto err;
	    }

	  /* pfx len */
	  if(zmq_send(dest, &key->mask_len, sizeof(key->mask_len), ZMQ_SNDMORE)
	     != sizeof(key->mask_len))
	    {
	      goto err;
	    }

	  /* peer cnt */
	  u16 = htons(v->peers_cnt);
	  if(zmq_send(dest, &u16, sizeof(u16), ZMQ_SNDMORE) != sizeof(u16))
	    {
	      goto err;
	    }

	  /* send the peers */
	  if(send_pfx_peers(dest, v) != 0)
	    {
	      goto err;
	    }
	}
    }

  return 0;

 err:
  return -1;
}

static int send_v6pfxs(void *dest, bgpwatcher_view_t *view)
{
  uint16_t u16;
  uint32_t u32;

  khiter_t k;

  bl_ipv6_pfx_t *key;
  bwv_peerid_pfxinfo_t *v;

  /* pfx cnt */
  u32 = htonl(kh_size(view->v6pfxs));
  if(zmq_send(dest, &u32, sizeof(u32), ZMQ_SNDMORE) != sizeof(u32))
    {
      goto err;
    }

  /* foreach pfx, send pfx-ip, pfx-len, [peer-info] */
  for(k = kh_begin(view->v6pfxs); k != kh_end(view->v6pfxs); ++k)
    {
      if(kh_exist(view->v6pfxs, k))
	{
	  key = &kh_key(view->v6pfxs, k);
	  v = kh_val(view->v6pfxs, k);

	  if(v->peers_cnt == 0)
	    {
	      continue;
	    }

	  /* pfx address */
	  if(bw_send_ip(dest, bl_addr_ipv62storage(&key->address), ZMQ_SNDMORE) != 0)
	    {
	      goto err;
	    }

	  /* pfx len */
	  if(zmq_send(dest, &key->mask_len, sizeof(key->mask_len), ZMQ_SNDMORE)
	     != sizeof(key->mask_len))
	    {
	      goto err;
	    }

	  /* peer cnt */
	  u16 = htons(v->peers_cnt);
	  if(zmq_send(dest, &u16, sizeof(u16), ZMQ_SNDMORE) != sizeof(u16))
	    {
	      goto err;
	    }

	  /* send the peers */
	  if(send_pfx_peers(dest, v) != 0)
	    {
	      goto err;
	    }
	}
    }

  return 0;

 err:
  return -1;
}

static int recv_pfxs(void *src, bgpwatcher_view_t *view)
{
  uint32_t pfx_cnt;
  uint16_t peer_cnt;
  int i, j;

  bl_pfx_storage_t pfx;
  bl_peerid_t peerid;
  bgpwatcher_pfx_peer_info_t pfx_info;
  void *cache = NULL;

  pfx_info.in_use = 1;

  ASSERT_MORE;

  /* pfx cnt */
  if(zmq_recv(src, &pfx_cnt, sizeof(pfx_cnt), 0) != sizeof(pfx_cnt))
    {
      goto err;
    }
  pfx_cnt = ntohl(pfx_cnt);
  ASSERT_MORE;

  /* foreach pfx, recv pfx.ip, pfx.len, [peers_cnt, peer_info] */
  for(i=0; i<pfx_cnt; i++)
    {
      /* this is a new pfx, reset the cache */
      cache = NULL;

      /* pfx_ip */
      if(bw_recv_ip(src, &pfx.address) != 0)
	{
	  goto err;
	}
      ASSERT_MORE;

      /* pfx len */
      if(zmq_recv(src, &pfx.mask_len, sizeof(pfx.mask_len), 0)
	 != sizeof(pfx.mask_len))
	{
	  goto err;
	}
      ASSERT_MORE;

      /* peer cnt */
      if(zmq_recv(src, &peer_cnt, sizeof(peer_cnt), 0) != sizeof(peer_cnt))
	{
	  goto err;
	}
      peer_cnt = ntohs(peer_cnt);
      ASSERT_MORE;

      for(j=0; j<peer_cnt; j++)
	{
	  /* peer id */
	  if(zmq_recv(src, &peerid, sizeof(peerid), 0) != sizeof(peerid))
	    {
	      goto err;
	    }
	  peerid = ntohs(peerid);
	  ASSERT_MORE;

	  /* orig asn */
	  if(zmq_recv(src, &pfx_info.orig_asn, sizeof(pfx_info.orig_asn), 0)
	     != sizeof(pfx_info.orig_asn))
	    {
	      goto err;
	    }
	  pfx_info.orig_asn = ntohl(pfx_info.orig_asn);
	  ASSERT_MORE;

	  if(bgpwatcher_view_add_prefix(view, &pfx, peerid,
					&pfx_info, &cache) != 0)
	    {
	      goto err;
	    }
	}
    }

  return 0;

 err:
  return -1;
}

static int send_peers(void *dest, bgpwatcher_view_t *view)
{
  uint16_t u16;
  khiter_t k;

  bl_peer_signature_t *ps;
  size_t len;

  uint16_t peers_cnt;

  /* peer cnt */
  peers_cnt = (uint16_t)bl_peersign_map_get_size(view->peersigns);
  u16 = htons(peers_cnt);
  if(zmq_send(dest, &u16, sizeof(u16), ZMQ_SNDMORE) != sizeof(u16))
    {
      goto err;
    }

  /* foreach peer, send peerid, collector string, peer ip (version, address) */
  for(k = kh_begin(view->peersigns); k != kh_end(view->peersigns->id_ps); ++k)
    {
      if(kh_exist(view->peersigns->id_ps, k))
	{
	  /* peer id */
	  u16 = kh_key(view->peersigns->id_ps, k);
	  u16 = htons(u16);
	  if(zmq_send(dest, &u16, sizeof(u16), ZMQ_SNDMORE) != sizeof(u16))
	    {
	      goto err;
	    }

	  ps = &(kh_value(view->peersigns->id_ps, k));
	  len = strlen(ps->collector_str);
	  if(zmq_send(dest, &ps->collector_str, len, ZMQ_SNDMORE) != len)
	    {
	      goto err;
	    }

	  if(bw_send_ip(dest, &ps->peer_ip_addr, ZMQ_SNDMORE) != 0)
	    {
	      goto err;
	    }
	}
    }

  return 0;

 err:
  return -1;
}

static int recv_peers(void *src, bgpwatcher_view_t *view)
{
  uint16_t pc;
  int i;

  bl_peerid_t peerid;

  bl_peer_signature_t ps;
  int len;

  ASSERT_MORE;

  /* peer cnt */
  if(zmq_recv(src, &pc, sizeof(pc), 0) != sizeof(pc))
    {
      goto err;
    }
  pc = ntohs(pc);
  ASSERT_MORE;

  /* foreach peer, recv peerid, collector string, peer ip (version, address) */
  for(i=0; i<pc; i++)
    {
      /* peerid */
      if(zmq_recv(src, &peerid, sizeof(peerid), 0) != sizeof(peerid))
	{
	  goto err;
	}
      peerid = ntohs(peerid);
      ASSERT_MORE;

      /* collector name */
      if((len = zmq_recv(src, ps.collector_str, BGPCOMMON_COLLECTOR_NAME_LEN, 0)) <= 0)
	{
	  goto err;
	}
      ps.collector_str[len] = '\0';
      ASSERT_MORE;

      /* peer ip */
      if(bw_recv_ip(src, &ps.peer_ip_addr) != 0)
	{
	  goto err;
	}

      if(bl_peersign_map_set(view->peersigns, peerid, ps.collector_str,
			     &ps.peer_ip_addr) != 0)
	{
	  goto err;
	}
    }

  return 0;

 err:
  return -1;
}

/* ========== PROTECTED FUNCTIONS ========== */

void bgpwatcher_view_clear(bgpwatcher_view_t *view)
{
  khiter_t k;

  view->time = 0;

  /* mark all ipv4 prefixes as unused */
  for(k = kh_begin(view->v4pfxs); k != kh_end(view->v4pfxs); ++k)
    {
      if(kh_exist(view->v4pfxs, k))
	{
	  kh_value(view->v4pfxs, k)->peers_cnt = 0;
	}
    }
  view->v4pfxs_cnt = 0;

  /* mark all ipv4 prefixes as unused */
  for(k = kh_begin(view->v6pfxs); k != kh_end(view->v6pfxs); ++k)
    {
      if(kh_exist(view->v6pfxs, k))
	{
	  kh_value(view->v6pfxs, k)->peers_cnt = 0;
	}
    }
  view->v4pfxs_cnt = 0;
}

int bgpwatcher_view_add_prefix(bgpwatcher_view_t *view,
                               bl_pfx_storage_t *prefix,
                               bl_peerid_t peerid,
                               bgpwatcher_pfx_peer_info_t *pfx_info,
			       void **cache)
{
  bwv_peerid_pfxinfo_t *peerids_pfxinfo;

  if(*cache == NULL)
    {
      if((peerids_pfxinfo = get_pfx_peerids(view, prefix)) == NULL)
	{
	  fprintf(stderr, "Unknown prefix provided!\n");
	  return -1;
	}
      *cache = peerids_pfxinfo;
    }
  else
    {
      peerids_pfxinfo = *cache;
    }

  if(peerid_pfxinfo_insert(peerids_pfxinfo, peerid, pfx_info) < 0)
    {
      return -1;
    }

  return 0;
}

int bgpwatcher_view_send(void *dest, bgpwatcher_view_t *view)
{
  uint32_t u32;

  /* time */
  u32 = htonl(view->time);
  if(zmq_send(dest, &u32, sizeof(u32), ZMQ_SNDMORE) != sizeof(u32))
    {
      goto err;
    }

  /* time_created */
  u32 = htonl(view->time_created.tv_sec);
  if(zmq_send(dest, &u32, sizeof(u32), ZMQ_SNDMORE) != sizeof(u32))
    {
      goto err;
    }
  u32 = htonl(view->time_created.tv_usec);
  if(zmq_send(dest, &u32, sizeof(u32), ZMQ_SNDMORE) != sizeof(u32))
    {
      goto err;
    }

  if(send_peers(dest, view) != 0)
    {
      goto err;
    }

  if(send_v4pfxs(dest, view) != 0)
    {
      goto err;
    }

  if(send_v6pfxs(dest, view) != 0)
    {
      goto err;
    }

  zmq_send(dest, "", 0, 0); /* DEBUG */

  fprintf(stderr, "DEBUG: Sending view...\n");

  return 0;

 err:
  return -1;
}

bgpwatcher_view_t *bgpwatcher_view_recv(void *src)
{
  bgpwatcher_view_t *view;
  uint32_t u32;

  /* create a new independent view (no external peers table) */
  if((view = bgpwatcher_view_create(NULL)) == NULL)
    {
      goto err;
    }

  /* time */
  if(zmq_recv(src, &u32, sizeof(u32), 0) != sizeof(u32))
    {
      goto err;
    }
  view->time = ntohl(u32);
  ASSERT_MORE;

  /* time_created */
  if(zmq_recv(src, &u32, sizeof(u32), 0) != sizeof(u32))
    {
      goto err;
    }
  view->time_created.tv_sec = ntohl(u32);
  ASSERT_MORE;

  if(zmq_recv(src, &u32, sizeof(u32), 0) != sizeof(u32))
    {
      goto err;
    }
  view->time_created.tv_usec = ntohl(u32);
  ASSERT_MORE;

  if(recv_peers(src, view) != 0)
    {
      goto err;
    }
  ASSERT_MORE;

  if(recv_pfxs(src, view) != 0)
    {
      goto err;
    }
  ASSERT_MORE;

  zmq_recv(src, NULL, 0, 0); /* DEBUG */

  /* @todo replace with actual fields */
  fprintf(stderr, "DEBUG: Receiving dummy view...\n");

  return view;

 err:
  bgpwatcher_view_destroy(view);
  return NULL;
}

/* ========== PUBLIC FUNCTIONS ========== */

bgpwatcher_view_t *bgpwatcher_view_create(bl_peersign_map_t *peersigns)
{
  bgpwatcher_view_t *view;

  if((view = malloc_zero(sizeof(bgpwatcher_view_t))) == NULL)
    {
      return NULL;
    }

  if((view->v4pfxs = kh_init(bwv_v4pfx_peerid_pfxinfo)) == NULL)
    {
      goto err;
    }

  if((view->v6pfxs = kh_init(bwv_v6pfx_peerid_pfxinfo)) == NULL)
    {
      goto err;
    }

  if(peersigns != NULL)
    {
      view->peersigns = peersigns;
      view->peersigns_shared = 1;
    }
  else if((view->peersigns = bl_peersign_map_create()) == NULL)
    {
      fprintf(stderr, "Failed to create peersigns table\n");
      goto err;
    }

  gettimeofday(&view->time_created, NULL);

  return view;

 err:
  fprintf(stderr, "Failed to create BGP Watcher View\n");
  bgpwatcher_view_destroy(view);
  return NULL;
}

void bgpwatcher_view_destroy(bgpwatcher_view_t *view)
{
  if(view == NULL)
    {
      return;
    }

  if(view->v4pfxs != NULL)
    {
      kh_free_vals(bwv_v4pfx_peerid_pfxinfo, view->v4pfxs,
                   peerid_pfxinfo_destroy);
      kh_destroy(bwv_v4pfx_peerid_pfxinfo, view->v4pfxs);
      view->v4pfxs = NULL;
    }

  if(view->v6pfxs != NULL)
    {
      kh_free_vals(bwv_v6pfx_peerid_pfxinfo, view->v6pfxs,
                   peerid_pfxinfo_destroy);
      kh_destroy(bwv_v6pfx_peerid_pfxinfo, view->v6pfxs);
      view->v6pfxs = NULL;
    }

  if(view->peersigns_shared == 0 && view->peersigns != NULL)
    {
      bl_peersign_map_destroy(view->peersigns);
      view->peersigns = NULL;
    }

  free(view);
}

void bgpwatcher_view_dump(bgpwatcher_view_t *view)
{
  if(view == NULL)
    {
      fprintf(stdout,
	      "------------------------------\n"
              "NULL\n"
	      "------------------------------\n\n");
    }
  else
    {
      fprintf(stdout,
	      "------------------------------\n"
	      "Time:\t%"PRIu32"\n"
	      "Created:\t%ld.%ld\n",
	      view->time,
	      (long)view->time_created.tv_sec,
	      (long)view->time_created.tv_usec);

      peers_dump(view);

      v4pfxs_dump(view);

      fprintf(stdout,
	      "------------------------------\n\n");
    }
}
