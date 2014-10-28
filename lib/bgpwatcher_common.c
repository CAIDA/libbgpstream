/*
 * bgpwatcher
 *
 * Alistair King, CAIDA, UC San Diego
 * corsaro-info@caida.org
 *
 * Copyright (C) 2012 The Regents of the University of California.
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

#include <assert.h>
#include <czmq.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>

#include <bgpstream_elem.h>

#include <bgpwatcher_common_int.h>

#include "utils.h"

static void *get_in_addr(bgpstream_ip_address_t *ip) {
  return ip->type == BST_IPV4
    ? (void *) &(ip->address.v4_addr)
    : (void *) &(ip->address.v6_addr);
}

static int send_ip(void *dest, bgpstream_ip_address_t *ip, int flags)
{
  switch(ip->type)
    {
    case BST_IPV4:
      if(zmq_send(dest, &ip->address.v4_addr.s_addr,
                  sizeof(uint32_t), flags) == sizeof(uint32_t))
        {
          return 0;
        }
      break;

    case BST_IPV6:
      if(zmq_send(dest, &ip->address.v6_addr.s6_addr,
                  (sizeof(uint8_t)*16), flags) == sizeof(uint8_t)*16)
        {
          return 0;
        }
      break;
    }

  return 0;
}

static int recv_ip(void *src, bgpstream_ip_address_t *ip)
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
      ip->type = BST_IPV4;
      memcpy(&ip->address.v4_addr.s_addr,
	     zmq_msg_data(&llm),
	     sizeof(uint32_t));
    }
  else if(zmq_msg_size(&llm) == sizeof(uint8_t)*16)
    {
      /* v6 */
      ip->type = BST_IPV6;
      memcpy(&ip->address.v6_addr.s6_addr,
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

static zmsg_t *table_msg_create(bgpwatcher_table_type_t type,
                                uint32_t time,
                                const char *collector)
{
  zmsg_t *msg = NULL;

  if((msg = zmsg_new()) == NULL)
    {
      goto err;
    }

  /* table type */
  if(zmsg_addmem(msg, &type, bgpwatcher_table_type_size_t) != 0)
    {
      goto err;
    }

  /* time */
  time = htonl(time);
  if(zmsg_addmem(msg, &time, sizeof(time)) != 0)
    {
      goto err;
    }

  /* collector */
  if(zmsg_addstr(msg, collector) != 0)
    {
      goto err;
    }

  return msg;

 err:
  zmsg_destroy(&msg);
  return NULL;
}

/* ========== PROTECTED FUNCTIONS BELOW HERE ========== */
/*     See bgpwatcher_common_int.h for declarations     */

/* ========== UTILITIES ========== */

/** @deprecated */
int bgpwatcher_msg_addip(zmsg_t *msg, bgpstream_ip_address_t *ip)
{
  int rc = -1;
  switch(ip->type)
    {
    case BST_IPV4:
      rc = zmsg_addmem(msg, &ip->address.v4_addr.s_addr, sizeof(uint32_t));
      break;

    case BST_IPV6:
      rc = zmsg_addmem(msg, &ip->address.v6_addr.s6_addr, (sizeof(uint8_t)*16));
      break;
    }

  return rc;
}



/* ========== MESSAGE TYPES ========== */

bgpwatcher_msg_type_t bgpwatcher_msg_type_frame(zframe_t *frame)
{
  uint8_t type;
  if((zframe_size(frame) > bgpwatcher_msg_type_size_t) ||
     (type = *zframe_data(frame)) > BGPWATCHER_MSG_TYPE_MAX)
    {
      return BGPWATCHER_MSG_TYPE_UNKNOWN;
    }

  return (bgpwatcher_msg_type_t)type;
}

bgpwatcher_msg_type_t bgpwatcher_msg_type(zmsg_t *msg, int peek)
{
  zframe_t *frame;
  bgpwatcher_msg_type_t type;

  /* first frame should be our type */
  if(peek == 0)
    {
      if((frame = zmsg_pop(msg)) == NULL)
	{
	  return BGPWATCHER_MSG_TYPE_UNKNOWN;
	}
    }
  else
    {
      if((frame = zmsg_first(msg)) == NULL)
	{
	  return BGPWATCHER_MSG_TYPE_UNKNOWN;
	}
    }

  type = bgpwatcher_msg_type_frame(frame);

  if(peek == 0)
    {
      zframe_destroy(&frame);
    }

  return type;
}

bgpwatcher_msg_type_t bgpwatcher_recv_type(void *src)
{
  bgpwatcher_msg_type_t type = BGPWATCHER_MSG_TYPE_UNKNOWN;

  if((zmq_recv(src, &type, bgpwatcher_msg_type_size_t, 0)
      != bgpwatcher_msg_type_size_t) ||
     (type > BGPWATCHER_MSG_TYPE_MAX))
    {
      return BGPWATCHER_MSG_TYPE_UNKNOWN;
    }

  return type;
}

bgpwatcher_data_msg_type_t bgpwatcher_data_msg_type(zmsg_t *msg)
{
  zframe_t *frame;
  uint8_t type;

  /* first frame should be our type */
  if((frame = zmsg_pop(msg)) == NULL)
    {
      return BGPWATCHER_DATA_MSG_TYPE_UNKNOWN;
    }

  if((type = *zframe_data(frame)) > BGPWATCHER_DATA_MSG_TYPE_MAX)
    {
      zframe_destroy(&frame);
      return BGPWATCHER_DATA_MSG_TYPE_UNKNOWN;
    }

  zframe_destroy(&frame);

  return (bgpwatcher_data_msg_type_t)type;
}

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



/* ========== PREFIX TABLES ========== */

zmsg_t *bgpwatcher_pfx_table_msg_create(bgpwatcher_pfx_table_t *table)
{
  zmsg_t *msg = NULL;

  /* prepare the common table headers */
  if((msg =
      table_msg_create(BGPWATCHER_TABLE_TYPE_PREFIX,
                       table->time, table->collector)) == NULL)
    {
      goto err;
    }

  /* peer ip */
  if(bgpwatcher_msg_addip(msg, &table->peer_ip) != 0)
    {
      goto err;
    }

  return msg;

 err:
  zmsg_destroy(&msg);
  return NULL;
}

int bgpwatcher_pfx_table_recv(void *src, bgpwatcher_pfx_table_t *table)
{
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

  /* ip */
  if(recv_ip(src, &(table->peer_ip)) != 0)
    {
      goto err;
    }

  return 0;

 err:
  return -1;
}

void bgpwatcher_pfx_table_dump(bgpwatcher_pfx_table_t *table)
{
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
      inet_ntop((table->peer_ip.type == BST_IPV4) ? AF_INET : AF_INET6,
		get_in_addr(&table->peer_ip),
		peer_str, INET6_ADDRSTRLEN);

      fprintf(stdout,
	      "------------------------------\n"
	      "Time:\t%"PRIu32"\n"
              "Collector:\t%s\n"
              "Peer:\t%s\n"
	      "------------------------------\n",
              table->time,
              table->collector,
              peer_str);
    }
}



/* ========== PREFIX RECORDS ========== */

int bgpwatcher_pfx_record_send(void *dest,
                               bgpstream_prefix_t *prefix,
                               uint32_t orig_asn,
                               int sendmore)
{
  uint32_t n32;

  /* prefix */
  if(send_ip(dest, &prefix->number, ZMQ_SNDMORE) != 0)
    {
      goto err;
    }

  /* length */
  if(zmq_send(dest, &prefix->len, sizeof(prefix->len), ZMQ_SNDMORE)
     != sizeof(prefix->len))
    {
      goto err;
    }

  /* orig asn */
  n32 = htonl(orig_asn);
  if(zmq_send(dest, &n32, sizeof(uint32_t), sendmore)
     != sizeof(uint32_t))
    {
      goto err;
    }

  return 0;

 err:
  return -1;
}

int bgpwatcher_pfx_recv(void *src, bgpstream_prefix_t *pfx_out,
			uint32_t *orig_asn_out)
{
  /* prefix */
  if(recv_ip(src, &(pfx_out->number)) != 0)
    {
      goto err;
    }

  if(zsocket_rcvmore(src) == 0)
    {
      goto err;
    }

  /* pfx len */
  if(zmq_recv(src, &pfx_out->len, sizeof(pfx_out->len), 0)
     != sizeof(pfx_out->len))
    {
      goto err;
    }

  if(zsocket_rcvmore(src) == 0)
    {
      goto err;
    }

  /* orig asn */
  if(zmq_recv(src, orig_asn_out, sizeof(*orig_asn_out), 0)
     != sizeof(*orig_asn_out))
    {
      goto err;
    }
  *orig_asn_out = ntohl(*orig_asn_out);

  return 0;

 err:
  return -1;
}

void bgpwatcher_pfx_record_dump(bgpstream_prefix_t *prefix,
                                uint32_t orig_asn)
{
  char pfx_str[INET6_ADDRSTRLEN] = "";

  if(prefix == NULL)
    {
      fprintf(stdout,
	      "------------------------------\n"
	      "NULL\n"
	      "------------------------------\n");
    }
  else
    {
      inet_ntop((prefix->number.type == BST_IPV4) ? AF_INET : AF_INET6,
		get_in_addr(&prefix->number),
		pfx_str, INET6_ADDRSTRLEN);

      fprintf(stdout,
	      "------------------------------\n"
	      "Prefix:\t%s/%"PRIu8"\n"
	      "ASN:\t%"PRIu32"\n"
	      "------------------------------\n",
	      pfx_str, prefix->len,
	      orig_asn);
    }
}



/* ========== PEER TABLES ========== */

zmsg_t *bgpwatcher_peer_table_msg_create(bgpwatcher_peer_table_t *table)
{
  zmsg_t *msg = NULL;

  /* prepare the common table headers */
  if((msg =
      table_msg_create(BGPWATCHER_TABLE_TYPE_PEER,
                       table->time, table->collector)) == NULL)
    {
      goto err;
    }

  return msg;

 err:
  zmsg_destroy(&msg);
  return NULL;
}

int bgpwatcher_peer_table_recv(void *src, bgpwatcher_peer_table_t *table)
{
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

  return 0;

 err:
  return -1;
}

void bgpwatcher_peer_table_dump(bgpwatcher_peer_table_t *table)
{
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
	      "------------------------------\n",
              table->time,
              table->collector);
    }
}


/* ========== PEER RECORDS ========== */

zmsg_t *bgpwatcher_peer_msg_create(bgpstream_ip_address_t *peer_ip,
                                   uint8_t status)
{
  zmsg_t *msg = NULL;

  if((msg = zmsg_new()) == NULL)
    {
      goto err;
    }

  /* peer ip */
  if(bgpwatcher_msg_addip(msg, peer_ip) != 0)
    {
      goto err;
    }

  /* status */
  if(zmsg_addmem(msg, &status, sizeof(status)) != 0)
    {
      goto err;
    }

  return msg;

 err:
  zmsg_destroy(&msg);
  return NULL;
}

int bgpwatcher_peer_recv(void *src, bgpstream_ip_address_t *peer_ip_out,
			 uint8_t *status_out)
{
  /* peer ip */
  if(recv_ip(src, peer_ip_out) != 0)
    {
      goto err;
    }

  /* status */
  if(zmq_recv(src, status_out, sizeof(*status_out), 0) != sizeof(*status_out))
    {
      goto err;
    }

  return 0;

 err:
  return -1;
}

void bgpwatcher_peer_record_dump(bgpstream_ip_address_t *peer_ip,
                                 uint8_t status)
{
  char ip_str[INET6_ADDRSTRLEN] = "";

  if(peer_ip == NULL)
    {
      fprintf(stdout,
	      "------------------------------\n"
	      "NULL\n"
	      "------------------------------\n");
    }
  else
    {
      inet_ntop((peer_ip->type == BST_IPV4) ? AF_INET : AF_INET6,
		get_in_addr(peer_ip),
		ip_str, INET6_ADDRSTRLEN);

      fprintf(stdout,
	      "------------------------------\n"
	      "IP:\t%s\n"
	      "Status:\t%"PRIu8"\n"
	      "------------------------------\n",
	      ip_str,
	      status);
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
