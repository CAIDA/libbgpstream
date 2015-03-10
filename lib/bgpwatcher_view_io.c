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

#include <stdio.h>

#include <czmq.h>

/* for bgpstream_peer_sig_map_set */
#include <bgpstream_utils_peer_sig_map_int.h>

#include "bgpwatcher_common_int.h"
#include "bgpwatcher_view_int.h"


#define BUFFER_LEN 16384


#define ASSERT_MORE				\
  if(zsocket_rcvmore(src) == 0)			\
    {						\
      fprintf(stderr, "ERROR: Malformed view message at line %d\n", __LINE__); \
      goto err;					\
    }

/* ========== PRIVATE FUNCTIONS ========== */

static void peers_dump(bgpwatcher_view_t *view,
		       bgpwatcher_view_iter_t *it)
{
  bgpstream_peer_id_t peerid;
  bgpstream_peer_sig_t *ps;
  int v4pfx_cnt = -1;
  int v6pfx_cnt = -1;
  char peer_str[INET6_ADDRSTRLEN] = "";

  fprintf(stdout, "Peers (%d):\n", bgpwatcher_view_peer_size(view));

  for(bgpwatcher_view_iter_first_peer(it, BGPWATCHER_VIEW_FIELD_ACTIVE);
      bgpwatcher_view_iter_has_more_peer(it);
      bgpwatcher_view_iter_next_peer(it))
    {
      peerid = bgpwatcher_view_iter_peer_get_peer(it);
      ps = bgpwatcher_view_iter_peer_get_sign(it);
      assert(ps);
      v4pfx_cnt = bgpwatcher_view_iter_peer_get_pfx_count(it, BGPSTREAM_ADDR_VERSION_IPV4);
      assert(v4pfx_cnt >= 0);
      v6pfx_cnt = bgpwatcher_view_iter_peer_get_pfx_count(it, BGPSTREAM_ADDR_VERSION_IPV6);
      assert(v6pfx_cnt >= 0);

      inet_ntop(ps->peer_ip_addr.version, &(ps->peer_ip_addr.ipv4),
		peer_str, INET6_ADDRSTRLEN);

      fprintf(stdout, "  %"PRIu16":\t%s, %s (%d v4 pfxs, %d v6 pfxs)\n",
	      peerid, ps->collector_str, peer_str, v4pfx_cnt, v6pfx_cnt);
    }
}

static void pfxs_dump(bgpwatcher_view_t *view,
                      bgpwatcher_view_iter_t *it)
{
  bgpstream_pfx_t *pfx;
  char pfx_str[INET6_ADDRSTRLEN+3] = "";

  fprintf(stdout, "Prefixes (v4 %d, v6 %d):\n", bgpwatcher_view_v4pfx_size(view), bgpwatcher_view_v6pfx_size(view));

  for(bgpwatcher_view_iter_first_pfx(it, 0, BGPWATCHER_VIEW_FIELD_ACTIVE);
      bgpwatcher_view_iter_has_more_pfx(it);
      bgpwatcher_view_iter_next_pfx(it))
    {
      pfx = bgpwatcher_view_iter_pfx_get_pfx(it);
      bgpstream_pfx_snprintf(pfx_str, INET6_ADDRSTRLEN+3, pfx);      
      fprintf(stdout, "  %s (%d peers)\n",
              pfx_str, bgpwatcher_view_iter_pfx_get_peers_cnt(it));
      
      for(bgpwatcher_view_iter_pfx_first_peer(it, BGPWATCHER_VIEW_FIELD_ACTIVE);
          bgpwatcher_view_iter_pfx_has_more_peer(it);
          bgpwatcher_view_iter_pfx_next_peer(it))
        {
          fprintf(stdout, "    %"PRIu16":\t%d\n",
                  bgpwatcher_view_iter_peer_get_peer(it),
                  bgpwatcher_view_iter_pfx_peer_get_orig_asn(it));
        }
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
  int i;
  uint16_t u16;
  uint32_t u32;

  uint8_t *ptr = buf;
  size_t written = 0;
  size_t s;

  for(i=0; i<pfxpeers->peers_alloc_cnt; i++)
    {
      if(pfxpeers->peers[i].state == BGPWATCHER_VIEW_FIELD_ACTIVE)
	{
	  /* peer id */
	  u16 = i;
	  u16 = htons(u16);
	  SERIALIZE_VAL(u16);

	  /* orig_asn */
	  u32 = pfxpeers->peers[i].orig_asn;
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

  bgpstream_ipv4_pfx_t *key;
  bwv_peerid_pfxinfo_t *v;

  size_t len = BUFFER_LEN;
  uint8_t buf[BUFFER_LEN];
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
      len = BUFFER_LEN;
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
                              (bgpstream_ip_addr_t *) (&key->address))) == -1)
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

  bgpstream_ipv6_pfx_t *key;
  bwv_peerid_pfxinfo_t *v;

  size_t len = BUFFER_LEN;
  uint8_t buf[BUFFER_LEN];
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
      len = BUFFER_LEN;
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
			      (bgpstream_ip_addr_t *)(&key->address))) == -1)
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

  bgpstream_pfx_storage_t pfx;
  bgpstream_peer_id_t peerid;
  bgpwatcher_pfx_peer_info_t pfx_info;
  void *cache = NULL;

  zmq_msg_t msg;
  uint8_t *buf;
  size_t len;
  size_t read = 0;
  size_t s = 0;

  pfx_info.state = BGPWATCHER_VIEW_FIELD_ACTIVE;

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
          fprintf(stderr, "Could not receive pfx message\n");
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
          fprintf(stderr, "Could not deserialize pfx ip\n");
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

	  if(bgpwatcher_view_add_prefix(view, (bgpstream_pfx_t *)&pfx, peerid,
					&pfx_info, &cache) != 0)
	    {
              fprintf(stderr, "Could not add prefix\n");
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

static int send_peers(void *dest, bgpwatcher_view_iter_t *it)
{
  uint16_t u16;

  bgpstream_peer_sig_t *ps;
  size_t len;

  uint16_t peers_cnt;
  int peers_tx = 0;

  /* peer cnt ( @todo make sure we just count the active) */
  peers_cnt = (uint16_t)bgpwatcher_view_peer_size(bgpwatcher_view_iter_get_view(it));
  u16 = htons(peers_cnt);
  if(zmq_send(dest, &u16, sizeof(u16), ZMQ_SNDMORE) != sizeof(u16))
    {
      goto err;
    }

  /* foreach peer, send peerid, collector string, peer ip (version, address) */
  for(bgpwatcher_view_iter_first_peer(it, BGPWATCHER_VIEW_FIELD_ACTIVE);
      bgpwatcher_view_iter_has_more_peer(it);
      bgpwatcher_view_iter_next_peer(it))
    {
      /* peer id */
      u16 = bgpwatcher_view_iter_peer_get_peer(it);
      u16 = htons(u16);
      if(zmq_send(dest, &u16, sizeof(u16), ZMQ_SNDMORE) != sizeof(u16))
	{
	  goto err;
	}

      ps = bgpwatcher_view_iter_peer_get_sign(it);
      assert(ps);
      len = strlen(ps->collector_str);
      if(zmq_send(dest, &ps->collector_str, len, ZMQ_SNDMORE) != len)
	{
	  goto err;
	}

      if(bw_send_ip(dest, (bgpstream_ip_addr_t *)(&ps->peer_ip_addr), ZMQ_SNDMORE) != 0)
	{
	  goto err;
	}
      peers_tx++;
    }

  assert(peers_cnt == peers_tx);

  return 0;

 err:
  return -1;
}

static int recv_peers(void *src, bgpwatcher_view_t *view)
{
  uint16_t pc;
  int i;

  bgpstream_peer_id_t peerid;

  bgpstream_peer_sig_t ps;
  int len;

  ASSERT_MORE;

  /* peer cnt */
  if(zmq_recv(src, &pc, sizeof(pc), 0) != sizeof(pc))
    {
      fprintf(stderr, "Could not receive peer cnt\n");
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
          fprintf(stderr, "Could not receive peer id\n");
	  goto err;
	}
      peerid = ntohs(peerid);
      ASSERT_MORE;

      /* collector name */
      if((len = zmq_recv(src, ps.collector_str, BGPSTREAM_UTILS_STR_NAME_LEN, 0)) <= 0)
	{
          fprintf(stderr, "Could not receive collector name\n");
	  goto err;
	}
      ps.collector_str[len] = '\0';
      ASSERT_MORE;

      /* peer ip */
      if(bw_recv_ip(src, &ps.peer_ip_addr) != 0)
	{
          fprintf(stderr, "Could not receive peer ip\n");
	  goto err;
	}

      if(bgpstream_peer_sig_map_set(view->peersigns,
                                    peerid,
                                    ps.collector_str,
                                    &ps.peer_ip_addr) != 0)
	{
          fprintf(stderr, "Could not add peer to peersigns\n");
	  fprintf(stderr,
                  "Consider making bgpstream_peer_sig_map_set more robust\n");
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

  bgpwatcher_view_iter_t *it = NULL;

#ifdef DEBUG
  fprintf(stderr, "DEBUG: Sending view...\n");
#endif

  if((it = bgpwatcher_view_iter_create(view)) == NULL)
    {
      goto err;
    }

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

  if(send_peers(dest, it) != 0)
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

  bgpwatcher_view_iter_destroy(it);

  view->pub_cnt++;

  return 0;

 err:
  return -1;
}

int bgpwatcher_view_recv(void *src, bgpwatcher_view_t *view)
{
  uint32_t u32;

  assert(view != NULL);

  /* to ensure that we never try to set peer ids for existing peer signatures,
     we clear the peersign table manually */
  /* this is not really needed, but if the server is ever restarted while the
     consumer is running, this will prevent any issues. It shouldn't cause too
     much performance problems (famous last words) */
  bgpstream_peer_sig_map_clear(view->peersigns);

  /* time */
  if(zmq_recv(src, &u32, sizeof(u32), 0) != sizeof(u32))
    {
      fprintf(stderr, "Could not receive 'time'\n");
      goto err;
    }
  view->time = ntohl(u32);
  ASSERT_MORE;

  /* time_created */
  if(zmq_recv(src, &u32, sizeof(u32), 0) != sizeof(u32))
    {
      fprintf(stderr, "Could not receive 'time created sec'\n");
      goto err;
    }
  view->time_created.tv_sec = ntohl(u32);
  ASSERT_MORE;

  if(zmq_recv(src, &u32, sizeof(u32), 0) != sizeof(u32))
    {
      fprintf(stderr, "Could not receive 'time created usec'\n");
      goto err;
    }
  view->time_created.tv_usec = ntohl(u32);
  ASSERT_MORE;

  if(recv_peers(src, view) != 0)
    {
      fprintf(stderr, "Could not receive peers\n");
      goto err;
    }
  ASSERT_MORE;

  /* v4 pfxs */
  if(recv_pfxs(src, view) != 0)
    {
      fprintf(stderr, "Could not receive v4 prefixes\n");
      goto err;
    }
  ASSERT_MORE;

  /* v6 pfxs */
  if(recv_pfxs(src, view) != 0)
    {
      fprintf(stderr, "Could not receive v6 prefixes\n");
      goto err;
    }
  ASSERT_MORE;

  if(zmq_recv(src, NULL, 0, 0) != 0)
    {
      fprintf(stderr, "Could not receive empty frame\n");
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

      pfxs_dump(view, it);

      fprintf(stdout,
	      "------------------------------\n\n");

      bgpwatcher_view_iter_destroy(it);
    }
}
