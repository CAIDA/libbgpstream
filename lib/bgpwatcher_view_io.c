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

#include "config.h"

#include <stdio.h>

#include <czmq.h>

/* for bgpstream_peer_sig_map_set */
#include <bgpstream_utils_peer_sig_map_int.h>

#include "bgpwatcher_common_int.h"
#include "bgpwatcher_view_io.h"


#define BUFFER_LEN 16384

/* because the values of AF_INET* vary from system to system we need to use
   our own encoding for the version */
#define BW_INTERNAL_AF_INET  4
#define BW_INTERNAL_AF_INET6 6

#define ASSERT_MORE				\
  if(zsocket_rcvmore(src) == 0)			\
    {						\
      fprintf(stderr, "ERROR: Malformed view message at line %d\n", __LINE__); \
      goto err;					\
    }

/* ========== PRIVATE FUNCTIONS ========== */

/* ========== UTILITIES ========== */

static int send_ip(void *dest, bgpstream_ip_addr_t *ip, int flags)
{
  switch(ip->version)
    {
    case BGPSTREAM_ADDR_VERSION_IPV4:
      if(zmq_send(dest, &((bgpstream_ipv4_addr_t *)ip)->ipv4.s_addr,
                  sizeof(uint32_t), flags) == sizeof(uint32_t))
        {
          return 0;
        }
      break;

    case BGPSTREAM_ADDR_VERSION_IPV6:
      if(zmq_send(dest, &((bgpstream_ipv6_addr_t *)ip)->ipv6.s6_addr,
                  (sizeof(uint8_t)*16), flags) == sizeof(uint8_t)*16)
        {
          return 0;
        }
      break;

    case BGPSTREAM_ADDR_VERSION_UNKNOWN:
      return -1;
    }

  return -1;
}

static int recv_ip(void *src, bgpstream_addr_storage_t *ip)
{
  zmq_msg_t llm;
  assert(ip != NULL);

  if(zmq_msg_init(&llm) == -1 || zmq_msg_recv(&llm, src, 0) == -1)
    {
      goto err;
    }

  /* 4 bytes means ipv4, 16 means ipv6 */
  if(zmq_msg_size(&llm) == sizeof(uint32_t))
    {
      /* v4 */
      ip->version = BGPSTREAM_ADDR_VERSION_IPV4;
      memcpy(&ip->ipv4.s_addr,
	     zmq_msg_data(&llm),
	     sizeof(uint32_t));
    }
  else if(zmq_msg_size(&llm) == sizeof(uint8_t)*16)
    {
      /* v6 */
      ip->version = BGPSTREAM_ADDR_VERSION_IPV6;
      memcpy(&ip->ipv6.s6_addr,
	     zmq_msg_data(&llm),
	     sizeof(uint8_t)*16);
    }
  else
    {
      /* invalid ip address */
      fprintf(stderr, "Invalid IP address\n");
      goto err;
    }

  zmq_msg_close(&llm);
  return 0;

 err:
  zmq_msg_close(&llm);
  return -1;
}

static int serialize_ip(uint8_t *buf, size_t len, bgpstream_ip_addr_t *ip)
{
  size_t written = 0;

  /* now serialize the actual address */
  switch(ip->version)
    {
    case BGPSTREAM_ADDR_VERSION_IPV4:
      /* serialize the version */
      assert(len >= 1);
      *buf = BW_INTERNAL_AF_INET;
      buf++;
      written++;

      assert((len-written) >= sizeof(uint32_t));
      memcpy(buf, &((bgpstream_ipv4_addr_t *)ip)->ipv4.s_addr,
             sizeof(uint32_t));
      return written + sizeof(uint32_t);
      break;

    case BGPSTREAM_ADDR_VERSION_IPV6:
      /* serialize the version */
      assert(len >= 1);
      *buf = BW_INTERNAL_AF_INET6;
      buf++;
      written++;

      assert((len-written) >= (sizeof(uint8_t)*16));
      memcpy(buf, &((bgpstream_ipv6_addr_t *)ip)->ipv6.s6_addr,
             sizeof(uint8_t)*16);
      return written + sizeof(uint8_t)*16;
      break;

    case BGPSTREAM_ADDR_VERSION_UNKNOWN:
      return -1;
    }

  return -1;
}

static int deserialize_ip(uint8_t *buf, size_t len,
                          bgpstream_addr_storage_t *ip)
{
  size_t read = 0;

  assert(len >= 1);

  /* switch on the internal version */
  switch(*buf)
    {
    case BW_INTERNAL_AF_INET:
      ip->version = BGPSTREAM_ADDR_VERSION_IPV4;
      buf++;
      read++;

      assert((len-read) >= sizeof(uint32_t));
      memcpy(&ip->ipv4.s_addr, buf, sizeof(uint32_t));
      return read + sizeof(uint32_t);
      break;

    case BW_INTERNAL_AF_INET6:
      ip->version = BGPSTREAM_ADDR_VERSION_IPV6;
      buf++;
      read++;

      assert((len-read) >= (sizeof(uint8_t)*16));
      memcpy(&ip->ipv6.s6_addr, buf, sizeof(uint8_t)*16);
      return read + (sizeof(uint8_t) * 16);
      break;

    case BGPSTREAM_ADDR_VERSION_UNKNOWN:
      return -1;
    }

  return -1;
}

static void peers_dump(bgpwatcher_view_t *view,
		       bgpwatcher_view_iter_t *it)
{
  bgpstream_peer_id_t peerid;
  bgpstream_peer_sig_t *ps;
  int v4pfx_cnt = -1;
  int v6pfx_cnt = -1;
  char peer_str[INET6_ADDRSTRLEN] = "";

  fprintf(stdout, "Peers (%d):\n",
          bgpwatcher_view_peer_cnt(view, BGPWATCHER_VIEW_FIELD_ACTIVE));

  for(bgpwatcher_view_iter_first_peer(it, BGPWATCHER_VIEW_FIELD_ACTIVE);
      bgpwatcher_view_iter_has_more_peer(it);
      bgpwatcher_view_iter_next_peer(it))
    {
      peerid = bgpwatcher_view_iter_peer_get_peer_id(it);
      ps = bgpwatcher_view_iter_peer_get_sig(it);
      assert(ps);
      v4pfx_cnt =
        bgpwatcher_view_iter_peer_get_pfx_cnt(it,
                                              BGPSTREAM_ADDR_VERSION_IPV4,
                                              BGPWATCHER_VIEW_FIELD_ACTIVE);
      assert(v4pfx_cnt >= 0);
      v6pfx_cnt =
        bgpwatcher_view_iter_peer_get_pfx_cnt(it,
                                              BGPSTREAM_ADDR_VERSION_IPV6,
                                              BGPWATCHER_VIEW_FIELD_ACTIVE);
      assert(v6pfx_cnt >= 0);

      inet_ntop(ps->peer_ip_addr.version, &(ps->peer_ip_addr.ipv4),
		peer_str, INET6_ADDRSTRLEN);

      fprintf(stdout,
              "  %"PRIu16":\t%s, %s %"PRIu32" (%d v4 pfxs, %d v6 pfxs)\n",
	      peerid, ps->collector_str, peer_str,
              ps->peer_asnumber, v4pfx_cnt, v6pfx_cnt);
    }
}

static void pfxs_dump(bgpwatcher_view_t *view,
                      bgpwatcher_view_iter_t *it)
{
  bgpstream_pfx_t *pfx;
  char pfx_str[INET6_ADDRSTRLEN+3] = "";

  fprintf(stdout, "Prefixes (v4 %d, v6 %d):\n",
          bgpwatcher_view_v4pfx_cnt(view, BGPWATCHER_VIEW_FIELD_ACTIVE),
          bgpwatcher_view_v6pfx_cnt(view, BGPWATCHER_VIEW_FIELD_ACTIVE));

  for(bgpwatcher_view_iter_first_pfx(it, 0, BGPWATCHER_VIEW_FIELD_ACTIVE);
      bgpwatcher_view_iter_has_more_pfx(it);
      bgpwatcher_view_iter_next_pfx(it))
    {
      pfx = bgpwatcher_view_iter_pfx_get_pfx(it);
      bgpstream_pfx_snprintf(pfx_str, INET6_ADDRSTRLEN+3, pfx);
      fprintf(stdout, "  %s (%d peers)\n",
              pfx_str,
              bgpwatcher_view_iter_pfx_get_peer_cnt(it,
                                                    BGPWATCHER_VIEW_FIELD_ACTIVE));

      for(bgpwatcher_view_iter_pfx_first_peer(it, BGPWATCHER_VIEW_FIELD_ACTIVE);
          bgpwatcher_view_iter_pfx_has_more_peer(it);
          bgpwatcher_view_iter_pfx_next_peer(it))
        {
          fprintf(stdout, "    %"PRIu16":\t%d\n",
                  bgpwatcher_view_iter_peer_get_peer_id(it),
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

static int send_pfx_peers(uint8_t *buf, size_t len,
                          bgpwatcher_view_iter_t *it,
                          int peers_cnt)
{
  uint16_t u16;
  uint32_t u32;

  uint8_t *ptr = buf;
  size_t written = 0;
  size_t s;

  int peers_tx = 0;

  for(bgpwatcher_view_iter_pfx_first_peer(it, BGPWATCHER_VIEW_FIELD_ACTIVE);
      bgpwatcher_view_iter_pfx_has_more_peer(it);
      bgpwatcher_view_iter_pfx_next_peer(it))
    {
      /* peer id */
      u16 = bgpwatcher_view_iter_peer_get_peer_id(it);
      assert(u16 > 0);
      u16 = htons(u16);
      SERIALIZE_VAL(u16);

      /* orig_asn */
      u32 = bgpwatcher_view_iter_pfx_peer_get_orig_asn(it);
      assert(u32 > 0);
      u32 = htonl(u32);
      SERIALIZE_VAL(u32);

      peers_tx++;
    }

  assert(peers_tx == peers_cnt);

  return written;
}

static int send_pfxs(void *dest, bgpwatcher_view_iter_t *it)
{
  uint16_t u16;
  uint32_t u32;

  size_t len = BUFFER_LEN;
  uint8_t buf[BUFFER_LEN];
  uint8_t *ptr = buf;
  size_t written = 0;
  size_t s = 0;

  bgpwatcher_view_t *view = bgpwatcher_view_iter_get_view(it);
  assert(view != NULL);

  bgpstream_pfx_t *pfx;
  int peers_cnt = 0;

  /* pfx cnt */
  u32 = htonl(bgpwatcher_view_pfx_cnt(view, BGPWATCHER_VIEW_FIELD_ACTIVE));
  if(zmq_send(dest, &u32, sizeof(u32), ZMQ_SNDMORE) != sizeof(u32))
    {
      goto err;
    }

  for(bgpwatcher_view_iter_first_pfx(it,
                                     0, /* all pfx versions */
                                     BGPWATCHER_VIEW_FIELD_ACTIVE);
      bgpwatcher_view_iter_has_more_pfx(it);
      bgpwatcher_view_iter_next_pfx(it))
    {
      /* reset the buffer */
      len = BUFFER_LEN;
      ptr = buf;
      written = 0;
      s = 0;

      pfx = bgpwatcher_view_iter_pfx_get_pfx(it);
      assert(pfx != NULL);

      /* pfx address */
      if((s = serialize_ip(ptr, (len-written),
                              (bgpstream_ip_addr_t *) (&pfx->address))) == -1)
	{
	  goto err;
	}
      written += s;
      ptr += s;

      /* pfx len */
      SERIALIZE_VAL(pfx->mask_len);

      /* peer cnt */
      peers_cnt =
        bgpwatcher_view_iter_pfx_get_peer_cnt(it, BGPWATCHER_VIEW_FIELD_ACTIVE);
      /* for a pfx to be active it must have active peers */
      assert(peers_cnt > 0);
      u16 = htons(peers_cnt);
      SERIALIZE_VAL(u16);

      /* send the peers */
      if((s = send_pfx_peers(ptr, (len-written), it, peers_cnt)) == -1)
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

static int recv_pfxs(void *src, bgpwatcher_view_iter_t *iter,
                     bgpstream_peer_id_t *peerid_map,
                     int peerid_map_cnt)
{
  uint32_t pfx_cnt;
  uint16_t peer_cnt;
  int i, j;

  bgpstream_pfx_storage_t pfx;
  bgpstream_peer_id_t peerid;

  uint32_t orig_asn;

  zmq_msg_t msg;
  uint8_t *buf;
  size_t len;
  size_t read = 0;
  size_t s = 0;

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
      if((s = deserialize_ip(buf, (len-read), &pfx.address)) == -1)
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
	  DESERIALIZE_VAL(orig_asn);
	  orig_asn = ntohl(orig_asn);

          if(iter == NULL)
            {
              continue;
            }
          /* all code below here has a valid iter */

          assert(peerid < peerid_map_cnt);

          if(j == 0)
            {
              /* we have to use add_pfx_peer */
              if(bgpwatcher_view_iter_add_pfx_peer(iter,
                                                   (bgpstream_pfx_t *)&pfx,
                                                   peerid_map[peerid],
                                                   orig_asn) != 0)
                {
                  fprintf(stderr, "Could not add prefix\n");
                  goto err;
                }
            }
          else
            {
              /* we can use pfx_add_peer for efficiency */
              if(bgpwatcher_view_iter_pfx_add_peer(iter,
                                                   peerid_map[peerid],
                                                   orig_asn) != 0)
                {
                  fprintf(stderr, "Could not add prefix\n");
                  goto err;
                }
            }

          /* now we have to activate it */
          if(bgpwatcher_view_iter_pfx_activate_peer(iter) < 0)
            {
              fprintf(stderr, "Could not activate prefix\n");
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
  uint32_t u32;

  bgpstream_peer_sig_t *ps;
  size_t len;

  int peers_cnt;
  int peers_tx = 0;

  bgpwatcher_view_t *view = bgpwatcher_view_iter_get_view(it);
  assert(view != NULL);

  /* peer cnt */
  peers_cnt =
    (uint16_t)bgpwatcher_view_peer_cnt(view, BGPWATCHER_VIEW_FIELD_ACTIVE);
  assert(peers_cnt <= UINT16_MAX);
  u16 = htons(peers_cnt);
  if(zmq_send(dest, &u16, sizeof(u16), ZMQ_SNDMORE) != sizeof(u16))
    {
      goto err;
    }

  /* foreach peer, send peerid, collector string, peer ip (version, address),
     peer asn */
  for(bgpwatcher_view_iter_first_peer(it, BGPWATCHER_VIEW_FIELD_ACTIVE);
      bgpwatcher_view_iter_has_more_peer(it);
      bgpwatcher_view_iter_next_peer(it))
    {
      /* peer id */
      u16 = bgpwatcher_view_iter_peer_get_peer_id(it);
      u16 = htons(u16);
      if(zmq_send(dest, &u16, sizeof(u16), ZMQ_SNDMORE) != sizeof(u16))
	{
	  goto err;
	}

      ps = bgpwatcher_view_iter_peer_get_sig(it);
      assert(ps);
      len = strlen(ps->collector_str);
      if(zmq_send(dest, &ps->collector_str, len, ZMQ_SNDMORE) != len)
	{
	  goto err;
	}

      /* peer IP address */
      if(send_ip(dest, (bgpstream_ip_addr_t *)(&ps->peer_ip_addr),
                    ZMQ_SNDMORE) != 0)
	{
	  goto err;
	}

      /* peer AS number */
      u32 = ps->peer_asnumber;
      u32 = htonl(u32);
      if(zmq_send(dest, &u32, sizeof(u32), ZMQ_SNDMORE) != sizeof(u32))
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

static int recv_peers(void *src, bgpwatcher_view_iter_t *iter,
                      bgpstream_peer_id_t **peerid_mapping)
{
  uint16_t pc;
  int i, j;

  bgpstream_peer_id_t peerid_orig;
  bgpstream_peer_id_t peerid_new;

  bgpstream_peer_sig_t ps;
  int len;

  bgpstream_peer_id_t *idmap = NULL;
  int idmap_cnt = 0;

  ASSERT_MORE;

  /* peer cnt */
  if(zmq_recv(src, &pc, sizeof(pc), 0) != sizeof(pc))
    {
      fprintf(stderr, "Could not receive peer cnt\n");
      goto err;
    }
  pc = ntohs(pc);
  ASSERT_MORE;

  /* foreach peer, recv peerid, collector string, peer ip (version, address),
     peer asn */
  for(i=0; i<pc; i++)
    {
      /* peerid */
      if(zmq_recv(src, &peerid_orig, sizeof(peerid_orig), 0)
         != sizeof(peerid_orig))
	{
          fprintf(stderr, "Could not receive peer id\n");
	  goto err;
	}
      peerid_orig = ntohs(peerid_orig);
      ASSERT_MORE;

      /* collector name */
      if((len = zmq_recv(src, ps.collector_str,
                         BGPSTREAM_UTILS_STR_NAME_LEN, 0)) <= 0)
	{
          fprintf(stderr, "Could not receive collector name\n");
	  goto err;
	}
      ps.collector_str[len] = '\0';
      ASSERT_MORE;

      /* peer ip */
      if(recv_ip(src, &ps.peer_ip_addr) != 0)
	{
          fprintf(stderr, "Could not receive peer ip\n");
	  goto err;
	}
      ASSERT_MORE;

      /* peer asn */
      if(zmq_recv(src, &ps.peer_asnumber, sizeof(ps.peer_asnumber), 0)
         != sizeof(ps.peer_asnumber))
	{
          fprintf(stderr, "Could not receive peer AS number\n");
	  goto err;
	}
      ps.peer_asnumber = ntohl(ps.peer_asnumber);

      if(iter == NULL)
        {
          continue;
        }
      /* all code below here has a valid view */

      /* ensure we have enough space in the id map */
      if((peerid_orig+1) > idmap_cnt)
        {
          if((idmap =
              realloc(idmap,
                      sizeof(bgpstream_peer_id_t) * (peerid_orig+1))) == NULL)
            {
              goto err;
            }

          /* now set all ids to 0 (reserved) */
          for(j=idmap_cnt; j<= peerid_orig; j++)
            {
              idmap[j] = 0;
            }
          idmap_cnt = peerid_orig+1;
        }

      /* now ask the view to add this peer */
      peerid_new = bgpwatcher_view_iter_add_peer(iter,
                                         ps.collector_str,
                                         (bgpstream_ip_addr_t*)&ps.peer_ip_addr,
                                         ps.peer_asnumber);
      assert(peerid_new != 0);
      idmap[peerid_orig] = peerid_new;

      bgpwatcher_view_iter_activate_peer(iter);
    }

  *peerid_mapping = idmap;
  return idmap_cnt;

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
  u32 = htonl(bgpwatcher_view_get_time(view));
  if(zmq_send(dest, &u32, sizeof(u32), ZMQ_SNDMORE) != sizeof(u32))
    {
      goto err;
    }

  if(send_peers(dest, it) != 0)
    {
      goto err;
    }

  if(send_pfxs(dest, it) != 0)
    {
      goto err;
    }

  if(zmq_send(dest, "", 0, 0) != 0)
    {
      goto err;
    }

  bgpwatcher_view_iter_destroy(it);

  return 0;

 err:
  return -1;
}

int bgpwatcher_view_recv(void *src, bgpwatcher_view_t *view)
{
  uint32_t u32;

  assert(view != NULL);

  bgpstream_peer_id_t *peerid_map = NULL;
  int peerid_map_cnt = 0;

  bgpwatcher_view_iter_t *it = NULL;
  if(view != NULL && (it = bgpwatcher_view_iter_create(view)) == NULL)
    {
      goto err;
    }

  /* time */
  if(zmq_recv(src, &u32, sizeof(u32), 0) != sizeof(u32))
    {
      fprintf(stderr, "Could not receive 'time'\n");
      goto err;
    }
  if(view != NULL)
    {
      bgpwatcher_view_set_time(view, ntohl(u32));
    }
  ASSERT_MORE;

  if((peerid_map_cnt = recv_peers(src, it, &peerid_map)) < 0)
    {
      fprintf(stderr, "Could not receive peers\n");
      goto err;
    }
  ASSERT_MORE;

  /* pfxs */
  if(recv_pfxs(src, it, peerid_map, peerid_map_cnt) != 0)
    {
      fprintf(stderr, "Could not receive prefixes\n");
      goto err;
    }
  ASSERT_MORE;

  if(zmq_recv(src, NULL, 0, 0) != 0)
    {
      fprintf(stderr, "Could not receive empty frame\n");
      goto err;
    }

  assert(zsocket_rcvmore(src) == 0);

  if(it != NULL)
    {
      bgpwatcher_view_iter_destroy(it);
    }

  free(peerid_map);

  return 0;

 err:
  if(it != NULL)
    {
      bgpwatcher_view_iter_destroy(it);
    }
  free(peerid_map);
  return -1;
}

/* ========== PUBLIC FUNCTIONS (exposed through bgpwatcher_view.h) ========== */

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
	      "Created:\t%ld\n",
	      bgpwatcher_view_get_time(view),
	      (long)bgpwatcher_view_get_time_created(view));

      peers_dump(view, it);

      pfxs_dump(view, it);

      fprintf(stdout,
	      "------------------------------\n\n");

      bgpwatcher_view_iter_destroy(it);
    }
}
