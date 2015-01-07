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

/* we need to poke our fingers into the peersign map */
#include "bl_peersign_map_int.h"

#include "bgpwatcher_common_int.h"
#include "bgpwatcher_view_int.h"

#define ASSERT_MORE				\
  if(zsocket_rcvmore(src) == 0)			\
    {						\
      goto err;					\
    }

/* ========== PRIVATE FUNCTIONS ========== */

static void peers_dump(bgpwatcher_view_t *view,
		       bgpwatcher_view_iter_t *it)
{
  bl_peerid_t peerid;
  bl_peer_signature_t *ps;
  char peer_str[INET6_ADDRSTRLEN] = "";

  fprintf(stdout, "Peers (%d):\n", bgpwatcher_view_peer_size(view));

  for(bgpwatcher_view_iter_first(it, BGPWATCHER_VIEW_ITER_FIELD_PEER);
      !bgpwatcher_view_iter_is_end(it, BGPWATCHER_VIEW_ITER_FIELD_PEER);
      bgpwatcher_view_iter_next(it, BGPWATCHER_VIEW_ITER_FIELD_PEER))
    {
      peerid = bgpwatcher_view_iter_get_peerid(it);
      ps = bgpwatcher_view_iter_get_peersig(it);
      assert(ps);

      inet_ntop(ps->peer_ip_addr.version, &(ps->peer_ip_addr.ipv4),
		peer_str, INET6_ADDRSTRLEN);

      fprintf(stdout, "  %"PRIu16":\t%s, %s\n",
	      peerid, ps->collector_str, peer_str);
    }
}

static void peerids_dump(bgpwatcher_view_iter_t *it,
			 bgpwatcher_view_iter_field_t field,
			 int version)
{
  bl_peerid_t peerid;
  bgpwatcher_pfx_peer_info_t *pfxinfo;

  for(bgpwatcher_view_iter_first(it, field);
      !bgpwatcher_view_iter_is_end(it, field);
      bgpwatcher_view_iter_next(it, field))
    {
      peerid = (version == 4) ?
	bgpwatcher_view_iter_get_v4pfx_peerid(it) :
	bgpwatcher_view_iter_get_v6pfx_peerid(it);

      pfxinfo = (version == 4) ?
	bgpwatcher_view_iter_get_v4pfx_pfxinfo(it) :
	bgpwatcher_view_iter_get_v6pfx_pfxinfo(it);

      fprintf(stdout, "    %"PRIu16":\t%"PRIu32"\n",
	      peerid, pfxinfo->orig_asn);
    }
}

static void v4pfxs_dump(bgpwatcher_view_t *view,
			bgpwatcher_view_iter_t *it)
{
  bl_ipv4_pfx_t *pfx;
  char pfx_str[INET6_ADDRSTRLEN] = "";

  fprintf(stdout, "V4 Prefixes (%d):\n", bgpwatcher_view_v4pfx_size(view));

  for(bgpwatcher_view_iter_first(it, BGPWATCHER_VIEW_ITER_FIELD_V4PFX);
      !bgpwatcher_view_iter_is_end(it, BGPWATCHER_VIEW_ITER_FIELD_V4PFX);
      bgpwatcher_view_iter_next(it, BGPWATCHER_VIEW_ITER_FIELD_V4PFX))
    {
      pfx = bgpwatcher_view_iter_get_v4pfx(it);
      assert(pfx);

      inet_ntop(pfx->address.version,
		&(pfx->address.ipv4),
		pfx_str, INET6_ADDRSTRLEN);

      fprintf(stdout, "  %s/%d (%"PRIu64" peers)\n",
	      pfx_str, pfx->mask_len,
	      bgpwatcher_view_iter_size(it, BGPWATCHER_VIEW_ITER_FIELD_V4PFX_PEER));
      peerids_dump(it, BGPWATCHER_VIEW_ITER_FIELD_V4PFX_PEER, 4);
    }
}

static void v6pfxs_dump(bgpwatcher_view_t *view,
			bgpwatcher_view_iter_t *it)
{
  bl_ipv6_pfx_t *pfx;
  char pfx_str[INET6_ADDRSTRLEN] = "";

  fprintf(stdout, "V6 Prefixes (%d):\n", bgpwatcher_view_v6pfx_size(view));

  for(bgpwatcher_view_iter_first(it, BGPWATCHER_VIEW_ITER_FIELD_V6PFX);
      !bgpwatcher_view_iter_is_end(it, BGPWATCHER_VIEW_ITER_FIELD_V6PFX);
      bgpwatcher_view_iter_next(it, BGPWATCHER_VIEW_ITER_FIELD_V6PFX))
    {
      pfx = bgpwatcher_view_iter_get_v6pfx(it);
      assert(pfx);

      inet_ntop(pfx->address.version,
		&(pfx->address.ipv6),
		pfx_str, INET6_ADDRSTRLEN);

      fprintf(stdout, "  %s/%d (%"PRIu64" peers)\n",
	      pfx_str, pfx->mask_len,
	      bgpwatcher_view_iter_size(it, BGPWATCHER_VIEW_ITER_FIELD_V6PFX_PEER));
      peerids_dump(it, BGPWATCHER_VIEW_ITER_FIELD_V6PFX_PEER, 4);
    }
}

#define SERIALIZE_VAL(from)				\
  do {							\
    assert((len-written) >= sizeof(from));		\
    memcpy(ptr, &from, sizeof(from));			\
    s = sizeof(from);					\
    written += s;					\
    ptr += s;						\
  } while(0)

static int send_pfx_peers(uint8_t *buf, size_t len, bwv_peerid_pfxinfo_t *pfxpeers)
{
  khiter_t k;
  uint16_t u16;
  uint32_t u32;

  uint8_t *ptr = buf;
  size_t written = 0;
  size_t s;

  for(k = kh_begin(pfxpeers->peers); k != kh_end(pfxpeers->peers); ++k)
    {
      if(kh_exist(pfxpeers->peers, k) && kh_val(pfxpeers->peers, k).in_use)
	{
	  /* peer id */
	  u16 = kh_key(pfxpeers->peers, k);
	  u16 = htons(u16);
	  SERIALIZE_VAL(u16);

	  /* orig_asn */
	  u32 = kh_val(pfxpeers->peers, k).orig_asn;
	  u32 = htonl(u32);
	  SERIALIZE_VAL(u32);
	}
    }

  return written;
}

static int send_v4pfxs(void *dest, bgpwatcher_view_t *view)
{
  uint16_t u16;
  uint32_t u32;

  khiter_t k;

  bl_ipv4_pfx_t *key;
  bwv_peerid_pfxinfo_t *v;

  size_t len = BW_PFX_ROW_BUFFER_LEN;
  uint8_t buf[BW_PFX_ROW_BUFFER_LEN];
  uint8_t *ptr = buf;
  size_t written = 0;
  size_t s = 0;

  /* pfx cnt */
  u32 = htonl(bgpwatcher_view_v4pfx_size(view));
  if(zmq_send(dest, &u32, sizeof(u32), ZMQ_SNDMORE) != sizeof(u32))
    {
      goto err;
    }

  /* foreach pfx, send pfx-ip, pfx-len, [peer-info] */
  for(k = kh_begin(view->v4pfxs); k != kh_end(view->v4pfxs); ++k)
    {
      if(!kh_exist(view->v4pfxs, k))
	{
	  continue;
	}

      /* reset the buffer */
      len = BW_PFX_ROW_BUFFER_LEN;
      ptr = buf;
      written = 0;
      s = 0;

      key = &kh_key(view->v4pfxs, k);
      v = kh_val(view->v4pfxs, k);

      if(v->peers_cnt == 0)
	{
	  continue;
	}

      /* pfx address */
      if((s = bw_serialize_ip(ptr, (len-written),
			      bl_addr_ipv42storage(&key->address))) == -1)
	{
	  goto err;
	}
      written += s;
      ptr += s;

      /* pfx len */
      SERIALIZE_VAL(key->mask_len);

      /* peer cnt */
      u16 = htons(v->peers_cnt);
      SERIALIZE_VAL(u16);

      /* send the peers */
      if((s = send_pfx_peers(ptr, (len-written), v)) == -1)
	{
	  goto err;
	}
      written += s;
      ptr += s;

      /* send the buffer */
      if(zmq_send(dest, buf, written, ZMQ_SNDMORE) != written)
	{
	  goto err;
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

  size_t len = BW_PFX_ROW_BUFFER_LEN;
  uint8_t buf[BW_PFX_ROW_BUFFER_LEN];
  uint8_t *ptr = buf;
  size_t written = 0;
  size_t s = 0;

  /* pfx cnt */
  u32 = htonl(bgpwatcher_view_v6pfx_size(view));
  if(zmq_send(dest, &u32, sizeof(u32), ZMQ_SNDMORE) != sizeof(u32))
    {
      goto err;
    }

  /* foreach pfx, send pfx-ip, pfx-len, [peer-info] */
  for(k = kh_begin(view->v6pfxs); k != kh_end(view->v6pfxs); ++k)
    {
      if(!kh_exist(view->v6pfxs, k))
	{
	  continue;
	}

      /* reset the buffer */
      len = BW_PFX_ROW_BUFFER_LEN;
      ptr = buf;
      written = 0;
      s = 0;

      key = &kh_key(view->v6pfxs, k);
      v = kh_val(view->v6pfxs, k);

      if(v->peers_cnt == 0)
	{
	  continue;
	}

      /* pfx address */
      if((s = bw_serialize_ip(ptr, (len-written),
			      bl_addr_ipv62storage(&key->address))) == -1)
	{
	  goto err;
	}
      written += s;
      ptr += s;

      /* pfx len */
      SERIALIZE_VAL(key->mask_len);

      /* peer cnt */
      u16 = htons(v->peers_cnt);
      SERIALIZE_VAL(u16);

      /* send the peers */
      if((s = send_pfx_peers(ptr, (len-written), v)) == -1)
	{
	  goto err;
	}
      written += s;
      ptr += s;

      /* send the buffer */
      if(zmq_send(dest, buf, written, ZMQ_SNDMORE) != written)
	{
	  goto err;
	}
    }

  return 0;

 err:
  return -1;
}

#define DESERIALIZE_VAL(to)				\
  do {							\
    assert((len-read) >= sizeof(to));			\
    memcpy(&to, buf, sizeof(to));			\
    s = sizeof(to);					\
    read += s;						\
    buf += s;						\
  } while(0)

static int recv_pfxs(void *src, bgpwatcher_view_t *view)
{
  uint32_t pfx_cnt;
  uint16_t peer_cnt;
  int i, j;

  bl_pfx_storage_t pfx;
  bl_peerid_t peerid;
  bgpwatcher_pfx_peer_info_t pfx_info;
  void *cache = NULL;

  zmq_msg_t msg;
  uint8_t *buf;
  size_t len;
  size_t read = 0;
  size_t s = 0;

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

      /* first receive the message */
      if(zmq_msg_init(&msg) == -1 || zmq_msg_recv(&msg, src, 0) == -1)
	{
	  goto err;
	}
      buf = zmq_msg_data(&msg);
      len = zmq_msg_size(&msg);
      read = 0;
      s = 0;
      assert(len > 0);

      /* pfx_ip */
      if((s = bw_deserialize_ip(buf, (len-read), &pfx.address)) == -1)
	{
	  goto err;
	}
      read += s;
      buf += s;

      /* pfx len */
      DESERIALIZE_VAL(pfx.mask_len);
      ASSERT_MORE;

      /* peer cnt */
      DESERIALIZE_VAL(peer_cnt);
      peer_cnt = ntohs(peer_cnt);

      for(j=0; j<peer_cnt; j++)
	{
	  /* peer id */
	  DESERIALIZE_VAL(peerid);
	  peerid = ntohs(peerid);

	  /* orig asn */
	  DESERIALIZE_VAL(pfx_info.orig_asn);
	  pfx_info.orig_asn = ntohl(pfx_info.orig_asn);

	  if(bgpwatcher_view_add_prefix(view, &pfx, peerid,
					&pfx_info, &cache) != 0)
	    {
	      goto err;
	    }
	}

      assert(read == len);
      zmq_msg_close(&msg);
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
  peers_cnt = (uint16_t)bl_peersign_map_get_inuse_size(view->peersigns);
  u16 = htons(peers_cnt);
  if(zmq_send(dest, &u16, sizeof(u16), ZMQ_SNDMORE) != sizeof(u16))
    {
      goto err;
    }

  /* foreach peer, send peerid, collector string, peer ip (version, address) */
  for(k = kh_begin(view->peersigns); k != kh_end(view->peersigns->id_ps); ++k)
    {
      if(kh_exist(view->peersigns->id_ps, k) &&
	 kh_val(view->peersigns->id_ps, k)->in_use)
	{
	  /* peer id */
	  u16 = kh_key(view->peersigns->id_ps, k);
	  u16 = htons(u16);
	  if(zmq_send(dest, &u16, sizeof(u16), ZMQ_SNDMORE) != sizeof(u16))
	    {
	      goto err;
	    }

	  ps = kh_value(view->peersigns->id_ps, k);
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

int bgpwatcher_view_send(void *dest, bgpwatcher_view_t *view)
{
  uint32_t u32;

#ifdef DEBUG
  fprintf(stderr, "DEBUG: Sending view...\n");
#endif

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

  if(zmq_send(dest, "", 0, 0) != 0)
    {
      goto err;
    }

  return 0;

 err:
  return -1;
}

int bgpwatcher_view_recv(void *src, bgpwatcher_view_t *view)
{
  uint32_t u32;

  assert(view != NULL);

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

  /* v4 pfxs */
  if(recv_pfxs(src, view) != 0)
    {
      goto err;
    }
  ASSERT_MORE;

  /* v6 pfxs */
  if(recv_pfxs(src, view) != 0)
    {
      goto err;
    }
  ASSERT_MORE;

  if(zmq_recv(src, NULL, 0, 0) != 0)
    {
      goto err;
    }

  assert(zsocket_rcvmore(src) == 0);

  return 0;

 err:
  return -1;
}

/* ========== PUBLIC FUNCTIONS ========== */

void bgpwatcher_view_dump(bgpwatcher_view_t *view)
{
  bgpwatcher_view_iter_t *it = NULL;

  if(view == NULL)
    {
      fprintf(stdout,
	      "------------------------------\n"
              "NULL\n"
	      "------------------------------\n\n");
    }
  else
    {
      it = bgpwatcher_view_iter_create(view);
      assert(it);

      fprintf(stdout,
	      "------------------------------\n"
	      "Time:\t%"PRIu32"\n"
	      "Created:\t%ld.%ld\n",
	      view->time,
	      (long)view->time_created.tv_sec,
	      (long)view->time_created.tv_usec);

      peers_dump(view, it);

      v4pfxs_dump(view, it);

      v6pfxs_dump(view, it);

      fprintf(stdout,
	      "------------------------------\n\n");

      bgpwatcher_view_iter_destroy(it);
    }
}
