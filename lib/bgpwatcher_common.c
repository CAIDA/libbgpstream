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

#include <assert.h>
#include <czmq.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>

#include "bgpwatcher_common_int.h"
#include "bgpwatcher_view_int.h"


#include "utils.h"

#define BUFFER_LEN 16384

/* because the values of AF_INET* vary from system to system we need to use
   our own encoding for the version */
#define BW_INTERNAL_AF_INET  4
#define BW_INTERNAL_AF_INET6 6


static char *recv_str(void *src)
{
  zmq_msg_t llm;
  size_t len;
  char *str = NULL;

  if(zmq_msg_init(&llm) == -1 || zmq_msg_recv(&llm, src, 0) == -1)
    {
      goto err;
    }
  len = zmq_msg_size(&llm);
  if((str = malloc(len + 1)) == NULL)
    {
      goto err;
    }
  memcpy(str, zmq_msg_data(&llm), len);
  str[len] = '\0';
  zmq_msg_close(&llm);

  return str;

 err:
  free(str);
  return NULL;
}

static int send_table(void *dest, bgpwatcher_table_type_t type,
		      uint32_t time, int sndmore)
{
#if 0
  /* table type */
  if(zmq_send(dest, &type, bgpwatcher_table_type_size_t, ZMQ_SNDMORE)
     != bgpwatcher_table_type_size_t)
    {
      goto err;
    }
#endif

  /* time */
  time = htonl(time);
  if(zmq_send(dest, &time, sizeof(time),
              (sndmore != 0) ? ZMQ_SNDMORE : 0) != sizeof(time))
    {
      goto err;
    }

  return 0;

 err:
  return -1;
}

static int send_peer(void *dest, bgpwatcher_peer_t *peer, int sndmore)
{
  uint32_t u32;

  /* peer ip */
  if(bw_send_ip(dest, (bgpstream_ip_addr_t *)(&peer->ip), ZMQ_SNDMORE) != 0)
    {
      goto err;
    }

  /* peer asn */
  u32 = peer->asn;
  u32 = htonl(u32);            
  if(zmq_send(dest, &u32, sizeof(u32), ZMQ_SNDMORE) != sizeof(u32))
    {
      goto err;
    }

  /* status */
  if(zmq_send(dest, &peer->status, sizeof(peer->status),
              (sndmore != 0) ? ZMQ_SNDMORE : 0) != sizeof(peer->status))
    {
      goto err;
    }

  return 0;

 err:
  return -1;
}

static int recv_peer(void *src, bgpwatcher_peer_t *peer)
{
  /* peer ip */
  if(bw_recv_ip(src, &peer->ip) != 0)
    {
      goto err;
    }

  if(zsocket_rcvmore(src) == 0)
    {
      goto err;
    }

  /* peer asn */
  if(zmq_recv(src, &peer->asn, sizeof(peer->asn), 0) != sizeof(peer->asn))
    {
      fprintf(stderr, "Could not receive peer AS number\n");
      goto err;
    }
  peer->asn = ntohl(peer->asn);            

  if(zsocket_rcvmore(src) == 0)
    {
      goto err;
    }

  /* status */
  if(zmq_recv(src, &peer->status, sizeof(peer->status), 0)
     != sizeof(peer->status))
    {
      goto err;
    }

  return 0;

 err:
  return -1;
}

#if 0
static int send_pfx_peer_info(void *dest, bgpwatcher_pfx_peer_info_t *info,
                              int sndmore)
{
  uint32_t n32;

  /* in use */
  if(zmq_send(dest, &info->state, sizeof(info->state),
              (info->state == BGPWATCHER_VIEW_FIELD_ACTIVE || sndmore != 0) ? ZMQ_SNDMORE : 0)
     != sizeof(info->state))
    {
      goto err;
    }

  if(info->state != BGPWATCHER_VIEW_FIELD_ACTIVE)
    {
      return 0;
    }

  /* orig asn */
  n32 = htonl(info->orig_asn);
  if(zmq_send(dest, &n32, sizeof(uint32_t), (sndmore != 0) ? ZMQ_SNDMORE : 0)
     != sizeof(uint32_t))
    {
      goto err;
    }

  /** @todo add more info fields here */

  return 0;

 err:
  return -1;
}
#endif

static int serialize_pfx_peer_info(uint8_t *buf, size_t len,
                                   bgpwatcher_pfx_peer_info_t *info)
{
  uint32_t n32;
  size_t written = 0;
  size_t s = 0;

  assert(len >= sizeof(info->state));
  memcpy(buf, &info->state, sizeof(info->state));
  s = sizeof(info->state);
  written += s;

  if(info->state != BGPWATCHER_VIEW_FIELD_ACTIVE)
    {
      return written;
    }

  buf += s;

  assert((len-written) >= sizeof(uint32_t));
  n32 = htonl(info->orig_asn);
  memcpy(buf, &n32, sizeof(uint32_t));
  s = sizeof(uint32_t);
  written += s;

  return written;
}

static int deserialize_pfx_peer_info(uint8_t *buf, size_t len,
                                     bgpwatcher_pfx_peer_info_t *info)
{
  uint32_t n32;
  size_t read = 0;
  size_t s = 0;

  assert(len >= sizeof(info->state));
  memcpy(&info->state, buf, sizeof(info->state));
  s = sizeof(info->state);
  read += s;

  if(info->state != BGPWATCHER_VIEW_FIELD_ACTIVE)
    {
      return read;
    }

  buf += s;

  /* orig asn */
  assert(len >= sizeof(uint32_t));      /* enough remaining in buffer */
  memcpy(&n32, buf, sizeof(uint32_t));  /* copy into struct */
  n32 = ntohl(n32);                     /* convert to host order */
  info->orig_asn = n32;                 /* save */
  s = sizeof(uint32_t);
  read += s;

  return read;
}


/* ========== PROTECTED FUNCTIONS BELOW HERE ========== */
/*     See bgpwatcher_common_int.h for declarations     */

/* ========== UTILITIES ========== */

int bw_send_ip(void *dest, bgpstream_ip_addr_t *ip, int flags)
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


int bw_recv_ip(void *src, bgpstream_addr_storage_t *ip)
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

int bw_serialize_ip(uint8_t *buf, size_t len, bgpstream_ip_addr_t *ip)
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
      memcpy(buf, &((bgpstream_ipv4_addr_t *)ip)->ipv4.s_addr, sizeof(uint32_t));
      return written + sizeof(uint32_t);
      break;

    case BGPSTREAM_ADDR_VERSION_IPV6:
      /* serialize the version */
      assert(len >= 1);
      *buf = BW_INTERNAL_AF_INET6;
      buf++;
      written++;

      assert((len-written) >= (sizeof(uint8_t)*16));
      memcpy(buf, &((bgpstream_ipv6_addr_t *)ip)->ipv6.s6_addr, sizeof(uint8_t)*16);
      return written + sizeof(uint8_t)*16;
      break;

    case BGPSTREAM_ADDR_VERSION_UNKNOWN:
      return -1;
    }

  return -1;
}

int bw_deserialize_ip(uint8_t *buf, size_t len, bgpstream_addr_storage_t *ip)
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

/* ========== MESSAGE TYPES ========== */

bgpwatcher_msg_type_t bgpwatcher_recv_type(void *src, int flags)
{
  bgpwatcher_msg_type_t type = BGPWATCHER_MSG_TYPE_UNKNOWN;

  if((zmq_recv(src, &type, bgpwatcher_msg_type_size_t, flags)
      != bgpwatcher_msg_type_size_t) ||
     (type > BGPWATCHER_MSG_TYPE_MAX))
    {
      return BGPWATCHER_MSG_TYPE_UNKNOWN;
    }

  return type;
}

#if 0
bgpwatcher_data_msg_type_t bgpwatcher_recv_data_type(void *src)
{
  bgpwatcher_data_msg_type_t type = BGPWATCHER_DATA_MSG_TYPE_UNKNOWN;

  if((zmq_recv(src, &type, bgpwatcher_data_msg_type_size_t, 0)
      != bgpwatcher_data_msg_type_size_t) ||
     (type > BGPWATCHER_DATA_MSG_TYPE_MAX))
    {
      return BGPWATCHER_DATA_MSG_TYPE_UNKNOWN;
    }

  return type;
}
#endif



/* ========== PREFIX TABLES ========== */

int bgpwatcher_pfx_table_begin_send(void *dest, bgpwatcher_pfx_table_t *table)
{
  size_t len;
  uint16_t cnt;
  uint32_t cnt32;
  int i;

  /* send the common table headers (TYPE, TIME) */
  if(send_table(dest, BGPWATCHER_TABLE_TYPE_PREFIX,
                table->time, ZMQ_SNDMORE) != 0)
    {
      goto err;
    }

  /* collector */
  len = strlen(table->collector);
  if(zmq_send(dest, table->collector, len, ZMQ_SNDMORE) != len)
    {
      goto err;
    }

  /* prefix count */
  cnt32 = htonl(table->prefix_cnt);
  if(zmq_send(dest, &cnt32, sizeof(cnt32), ZMQ_SNDMORE) != sizeof(cnt32))
    {
      goto err;
    }

  /* peer cnt */
  assert(table->peers_cnt <= UINT16_MAX);
  cnt = htons(table->peers_cnt);
  if(zmq_send(dest, &cnt, sizeof(cnt), ZMQ_SNDMORE) != sizeof(cnt))
    {
      goto err;
    }

  for(i=0; i<table->peers_cnt; i++)
    {
      /* always SNDMORE as there must be at least table end message coming */
      if(send_peer(dest, &table->peers[i], ZMQ_SNDMORE) != 0)
        {
          goto err;
        }
    }

  return 0;

 err:
  return -1;
}

int bgpwatcher_pfx_table_end_send(void *dest, bgpwatcher_pfx_table_t *table)
{
  /* just send the common table headers (TYPE, TIME) */
  if(send_table(dest, BGPWATCHER_TABLE_TYPE_PREFIX, table->time, 0) != 0)
    {
      goto err;
    }

  return 0;

 err:
  return -1;
}

int bgpwatcher_pfx_table_begin_recv(void *src, bgpwatcher_pfx_table_t *table)
{
  int i;

  /* time */
  if(zmq_recv(src, &table->time, sizeof(table->time), 0) != sizeof(table->time))
    {
      goto err;
    }
  table->time = ntohl(table->time);

  if(zsocket_rcvmore(src) == 0)
    {
      goto err;
    }

  /* collector */
  free(table->collector);
  if((table->collector = recv_str(src)) == NULL)
    {
      goto err;
    }

  if(zsocket_rcvmore(src) == 0)
    {
      goto err;
    }

  /* prefix cnt */
  if(zmq_recv(src, &table->prefix_cnt, sizeof(uint32_t), 0) != sizeof(uint32_t))
    {
      goto err;
    }
  table->prefix_cnt = ntohl(table->prefix_cnt);

  if(zsocket_rcvmore(src) == 0)
    {
      goto err;
    }

  /* peer cnt */
  if(zmq_recv(src, &table->peers_cnt, sizeof(uint16_t), 0) != sizeof(uint16_t))
    {
      goto err;
    }
  table->peers_cnt = ntohs(table->peers_cnt);

  /* ensure we have enough space to store all the peers */
  if(table->peers_alloc_cnt < table->peers_cnt)
    {
      if((table->peers =
          realloc(table->peers,
                  sizeof(bgpwatcher_peer_t)*table->peers_cnt)) == NULL)
        {
          return -1;
        }
      table->peers_alloc_cnt = table->peers_cnt;
    }

  /* receive all the peers */
  for(i=0; i<table->peers_cnt; i++)
    {
      if(recv_peer(src, &(table->peers[i])) != 0)
        {
          goto err;
        }
    }

  return 0;

 err:
  return -1;
}

int bgpwatcher_pfx_table_end_recv(void *src, bgpwatcher_pfx_table_t *table)
{
  uint32_t time;

  /* time */
  if(zmq_recv(src, &time, sizeof(time), 0) != sizeof(time))
    {
      goto err;
    }
  time = ntohl(time);

  if(time != table->time)
    {
      goto err;
    }

  if(zsocket_rcvmore(src) != 0)
    {
      goto err;
    }

  return 0;

 err:
  return -1;
}

void bgpwatcher_pfx_table_dump(bgpwatcher_pfx_table_t *table)
{
  int i;
  char peer_str[INET6_ADDRSTRLEN] = "";

  if(table == NULL)
    {
      fprintf(stdout,
	      "------------------------------\n"
	      "NULL\n"
	      "------------------------------\n");
    }
  else
    {
      fprintf(stdout,
	      "------------------------------\n"
	      "Time:\t%"PRIu32"\n"
              "Collector:\t%s\n"
              "Prefix Cnt:\t%d\n"
              "Peer Cnt:\t%d\n",
              table->time,
              table->collector,
              table->prefix_cnt,
              table->peers_cnt);

      for(i=0; i<table->peers_cnt; i++)
        {
          bgpstream_addr_ntop(peer_str,INET6_ADDRSTRLEN, &table->peers[i].ip);
          fprintf(stdout,
                  "Peer %03d:\t%s (%d)\n",
                  i,
                  peer_str,
                  table->peers[i].status);
        }
    }
}



/* ========== PREFIX ROWS ========== */

/* how many bytes in a row?
   max:
   IP_VERSION [01]
   IP_ADDRESS [16]
   <PEER_INFO> * peer_cnt

   PEER_INFO
   IN_USE   [01]
   ORIG_ASN [04]
*/
/* @todo consider moving the buffer into the client */
int bgpwatcher_pfx_row_send(void *dest,
                            bgpstream_pfx_t *pfx,
                            bgpwatcher_pfx_peer_info_t *peer_infos,
                            int peer_cnt)
{
  int i;

  size_t len = BUFFER_LEN;
  uint8_t buf[BUFFER_LEN];
  uint8_t *ptr = buf;
  size_t written = 0;
  size_t s = 0;

  /* prefix */
  if((s = bw_serialize_ip(ptr, len, &pfx->address)) == -1)
    {
      goto err;
    }
  written += s;
  ptr += s;

  /* length */
  assert((len-written) >= sizeof(pfx->mask_len)); /* duh */
  memcpy(ptr, &pfx->mask_len, sizeof(pfx->mask_len));
  s = sizeof(pfx->mask_len);
  written += s;
  ptr += s;

  /* foreach peer, dump the info */
  for(i=0; i<peer_cnt; i++)
    {
      /* always SNDMORE as there must be a table end coming */
      if((s = serialize_pfx_peer_info(ptr, (len-written),
                                      &peer_infos[i])) == -1)
        {
          goto err;
        }
      ptr += s;
      written += s;
    }

  /* now send the buffer */
  if(zmq_send(dest, buf, written, ZMQ_SNDMORE) != written)
    {
      goto err;
    }

  return 0;

 err:
  return -1;
}

int bgpwatcher_pfx_row_recv(void *src,
                            bgpstream_pfx_storage_t *pfx,
                            bgpwatcher_pfx_peer_info_t *peer_infos,
                            int peer_cnt)
{
  int i;
  zmq_msg_t msg;
  uint8_t *buf;
  size_t len;
  size_t read = 0;
  size_t s = 0;

  assert(pfx != NULL);
  assert(peer_infos != NULL);

  /* first receive the message */
  if(zmq_msg_init(&msg) == -1 || zmq_msg_recv(&msg, src, 0) == -1)
    {
      goto err;
    }
  buf = zmq_msg_data(&msg);
  len = zmq_msg_size(&msg);

  assert(len > 0);

  /* prefix */
  if((s = bw_deserialize_ip(buf, (len-read), &(pfx->address))) == -1)
    {
      goto err;
    }
  read += s;
  buf += s;

  /* pfx len */
  assert((len-read) >= sizeof(pfx->mask_len));
  memcpy(&pfx->mask_len, buf, sizeof(pfx->mask_len));
  s = sizeof(pfx->mask_len);
  read += s;
  buf += s;

  for(i=0; i<peer_cnt; i++)
    {
      if((s = deserialize_pfx_peer_info(buf, (len-read),
                                        &(peer_infos[i]))) == -1)
        {
          goto err;
        }
      read += s;
      buf+= s;
    }

  assert(read == len);

  zmq_msg_close(&msg);

  return 0;

 err:
  zmq_msg_close(&msg);
  return -1;
}

#if 0
int bgpwatcher_pfx_row_send(void *dest, bgpwatcher_pfx_row_t *row,
                            int peer_cnt)
{
  int i;

  assert(peer_cnt <= BGPWATCHER_PEER_MAX_CNT);

  /* prefix */
  if(bw_send_ip(dest, &row->prefix.address, ZMQ_SNDMORE) != 0)
    {
      goto err;
    }

  /* length */
  if(zmq_send(dest, &row->prefix.mask_len, sizeof(row->prefix.mask_len),
              ZMQ_SNDMORE)
     != sizeof(row->prefix.mask_len))
    {
      goto err;
    }

  /* foreach peer, dump the info */
  for(i=0; i<peer_cnt; i++)
    {
      /* always SNDMORE as there must be a table end coming */
      if(send_pfx_peer_info(dest, &row->info[i], ZMQ_SNDMORE) != 0)
        {
          goto err;
        }
    }

  return 0;

 err:
  return -1;
}

int bgpwatcher_pfx_row_recv(void *src, bgpwatcher_pfx_row_t *row_out,
                            int peer_cnt)
{
  int i;

  /* prefix */
  if(bw_recv_ip(src, &(row_out->prefix.address)) != 0)
    {
      goto err;
    }

  if(zsocket_rcvmore(src) == 0)
    {
      goto err;
    }

  /* pfx len */
  if(zmq_recv(src, &row_out->prefix.mask_len,
              sizeof(row_out->prefix.mask_len), 0)
     != sizeof(row_out->prefix.mask_len))
    {
      goto err;
    }

  if(zsocket_rcvmore(src) == 0)
    {
      goto err;
    }

  for(i=0; i<peer_cnt; i++)
    {
      if(zsocket_rcvmore(src) == 0)
        {
          goto err;
        }
      if(recv_pfx_peer_info(src, &(row_out->info[i])) != 0)
        {
          goto err;
        }
    }

  return 0;

 err:
  return -1;
}
#endif

void bgpwatcher_pfx_row_dump(bgpwatcher_pfx_table_t *table,
                             bgpstream_pfx_storage_t *pfx,
                             bgpwatcher_pfx_peer_info_t *peer_infos)
{
  char pfx_str[INET6_ADDRSTRLEN+3] = "";
  int i;
  char peer_str[INET6_ADDRSTRLEN] = "";

  if(pfx == NULL)
    {
      fprintf(stdout,
	      "------------------------------\n"
	      "NULL\n"
	      "------------------------------\n");
    }
  else
    {

      bgpstream_pfx_snprintf(pfx_str,INET6_ADDRSTRLEN+3, (bgpstream_pfx_t *)pfx);
      fprintf(stdout,
	      "------------------------------\n"
	      "Prefix:\t%s\n", pfx_str);

      for(i=0; i<table->peers_cnt; i++)
        {
          if(peer_infos[i].state == BGPWATCHER_VIEW_FIELD_ACTIVE)
            {
              bgpstream_addr_ntop(peer_str,INET6_ADDRSTRLEN, &table->peers[i].ip);
              fprintf(stdout,
                      "\tPeer:\t%s\n"
                      "\t\tOrig ASN:\t%"PRIu32"\n",
                      peer_str,
                      peer_infos[i].orig_asn);
            }
        }
    }
}

/* ========== INTERESTS/VIEWS ========== */

const char *bgpwatcher_consumer_interest_pub(int interests)
{
  /* start with the most specific and work backward */
  /* NOTE: a view CANNOT satisfy FIRSTFULL and NOT satisfy FULL/PARTIAL */
  if(interests & BGPWATCHER_CONSUMER_INTEREST_FIRSTFULL)
    {
      return BGPWATCHER_CONSUMER_INTEREST_SUB_FIRSTFULL;
    }
  else if(interests & BGPWATCHER_CONSUMER_INTEREST_FULL)
    {
      return BGPWATCHER_CONSUMER_INTEREST_SUB_FULL;
    }
  else if(interests & BGPWATCHER_CONSUMER_INTEREST_PARTIAL)
    {
      return BGPWATCHER_CONSUMER_INTEREST_SUB_PARTIAL;
    }

  return NULL;
}

const char *bgpwatcher_consumer_interest_sub(int interests)
{
  /* start with the least specific and work backward */
  if(interests & BGPWATCHER_CONSUMER_INTEREST_PARTIAL)
    {
      return BGPWATCHER_CONSUMER_INTEREST_SUB_PARTIAL;
    }
  else if(interests & BGPWATCHER_CONSUMER_INTEREST_FULL)
    {
      return BGPWATCHER_CONSUMER_INTEREST_SUB_FULL;
    }
  else if(interests & BGPWATCHER_CONSUMER_INTEREST_FIRSTFULL)
    {
      return BGPWATCHER_CONSUMER_INTEREST_SUB_FIRSTFULL;
    }

  return NULL;
}

uint8_t bgpwatcher_consumer_interest_recv(void *src)
{
  char *pub_str = NULL;
  uint8_t interests = 0;

  /* grab the subscription frame and convert to interests */
  if((pub_str = recv_str(src)) == NULL)
    {
      goto err;
    }

  /** @todo make all this stuff less hard-coded and extensible */
  if(strcmp(pub_str, BGPWATCHER_CONSUMER_INTEREST_SUB_FIRSTFULL) == 0)
    {
      interests |= BGPWATCHER_CONSUMER_INTEREST_PARTIAL;
      interests |= BGPWATCHER_CONSUMER_INTEREST_FULL;
      interests |= BGPWATCHER_CONSUMER_INTEREST_FIRSTFULL;
    }
  else if(strcmp(pub_str, BGPWATCHER_CONSUMER_INTEREST_SUB_FULL) == 0)
    {
      interests |= BGPWATCHER_CONSUMER_INTEREST_PARTIAL;
      interests |= BGPWATCHER_CONSUMER_INTEREST_FULL;
    }
  else if(strcmp(pub_str, BGPWATCHER_CONSUMER_INTEREST_SUB_PARTIAL) == 0)
    {
      interests |= BGPWATCHER_CONSUMER_INTEREST_PARTIAL;
    }
  else
    {
      goto err;
    }

  free(pub_str);
  return interests;

 err:
  free(pub_str);
  return 0;
}

void bgpwatcher_consumer_interest_dump(int interests)
{
  if(interests & BGPWATCHER_CONSUMER_INTEREST_FIRSTFULL)
    {
      fprintf(stdout, "first-full ");
    }
  if(interests & BGPWATCHER_CONSUMER_INTEREST_FULL)
    {
      fprintf(stdout, "full ");
    }
  if(interests & BGPWATCHER_CONSUMER_INTEREST_PARTIAL)
    {
      fprintf(stdout, "partial");
    }
}

/* ========== PUBLIC FUNCTIONS BELOW HERE ========== */
/*      See bgpwatcher_common.h for declarations     */

void bgpwatcher_err_set_err(bgpwatcher_err_t *err, int errcode,
			const char *msg, ...)
{
  char buf[256];
  va_list va;

  va_start(va,msg);

  assert(errcode != 0 && "An error occurred, but it is unknown what it is");

  err->err_num=errcode;

  if (errcode>0) {
    vsnprintf(buf, sizeof(buf), msg, va);
    snprintf(err->problem, sizeof(err->problem), "%s: %s", buf,
	     strerror(errcode));
  } else {
    vsnprintf(err->problem, sizeof(err->problem), msg, va);
  }

  va_end(va);
}

int bgpwatcher_err_is_err(bgpwatcher_err_t *err)
{
  return err->err_num != 0;
}

void bgpwatcher_err_perr(bgpwatcher_err_t *err)
{
  if(err->err_num) {
    fprintf(stderr,"%s (%d)\n", err->problem, err->err_num);
  } else {
    fprintf(stderr,"No error\n");
  }
  err->err_num = 0; /* "OK" */
  err->problem[0]='\0';
}
