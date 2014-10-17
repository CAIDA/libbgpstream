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

#include <bgpwatcher_common.h>

#include "utils.h"

static void *get_in_addr(struct sockaddr *sa) {
  return sa->sa_family == AF_INET
    ? (void *) &(((struct sockaddr_in*)sa)->sin_addr)
    : (void *) &(((struct sockaddr_in6*)sa)->sin6_addr);
}

/** @todo consider exporting this */
static int msg_pop_ip(zmsg_t *msg, struct sockaddr_storage *ss)
{
  zframe_t *frame;

  if((frame = zmsg_pop(msg)) == NULL)
    {
      return -1;
    }

  /* 4 bytes means ipv4, 16 means ipv6 */
  if(zframe_size(frame) == sizeof(uint32_t))
    {
      /* v4 */
      ss->ss_family = AF_INET;
      memcpy(&((struct sockaddr_in *)ss)->sin_addr.s_addr,
	     zframe_data(frame),
	     sizeof(uint32_t));
    }
  else if(zframe_size(frame) == sizeof(uint8_t)*16)
    {
      /* v6 */
      ss->ss_family = AF_INET6;
      memcpy(&((struct sockaddr_in6 *)ss)->sin6_addr.s6_addr,
	     zframe_data(frame),
	     sizeof(uint8_t)*16);
    }
  else
    {
      /* invalid ip address */
      fprintf(stderr, "Invalid IP address\n");
      zframe_print(frame, NULL);
      goto err;
    }

  zframe_destroy(&frame);
  return 0;

 err:
  zframe_destroy(&frame);
  return -1;
}

static int msg_append_ip(zmsg_t *msg, struct sockaddr_storage *ss)
{
  if(ss->ss_family == AF_INET)
    {
      /* v4 */
      if(zmsg_addmem(msg, &((struct sockaddr_in *)ss)->sin_addr.s_addr,
		     sizeof(uint32_t)) != 0)
	{
	  return -1;
	}
    }
  else if(ss->ss_family == AF_INET6)
    {
      /* v6 */
      if(zmsg_addmem(msg, &((struct sockaddr_in6 *)ss)->sin6_addr.s6_addr,
		     (sizeof(uint8_t)*16)) != 0)
	{
	  return -1;
	}
    }
  else
    {
      return -1;
    }

  return 0;
}

static int msg_append_bgpstream_ip(zmsg_t *msg, bgpstream_ip_address_t *ip)
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

bgpwatcher_pfx_record_t *bgpwatcher_pfx_record_init()
{
  return malloc_zero(sizeof(bgpwatcher_pfx_record_t));
}

void bgpwatcher_pfx_record_free(bgpwatcher_pfx_record_t **pfx_p)
{
  bgpwatcher_pfx_record_t *pfx = *pfx_p;

  if(pfx != NULL)
    {
      free(pfx);
    }

  *pfx_p = NULL;
}

int bgpwatcher_pfx_record_deserialize(zmsg_t *msg,
				      bgpwatcher_pfx_record_t *pfx)
{
  zframe_t *frame;

  /* prefix */
  if(msg_pop_ip(msg, &pfx->prefix) != 0)
    {
      goto err;
    }

  /* pfx len */
  if((frame = zmsg_pop(msg)) == NULL ||
     zframe_size(frame) != sizeof(pfx->prefix_len))
    {
      goto err;
    }
  pfx->prefix_len = *zframe_data(frame);
  zframe_destroy(&frame);

  /* peer ip */
  if(msg_pop_ip(msg, &pfx->peer_ip) != 0)
    {
      goto err;
    }

  /* orig asn */
  if((frame = zmsg_pop(msg)) == NULL ||
     zframe_size(frame) != sizeof(pfx->orig_asn))
    {
      goto err;
    }
  memcpy(&pfx->orig_asn, zframe_data(frame), sizeof(pfx->orig_asn));
  pfx->orig_asn = ntohl(pfx->orig_asn);
  zframe_destroy(&frame);

  /* name */
  if((frame = zmsg_pop(msg)) == NULL ||
     zframe_size(frame) >= BGPWATCHER_COLLECTOR_NAME_LEN)
    {
      goto err;
    }
  memcpy(&pfx->collector_name, zframe_data(frame), zframe_size(frame));
  pfx->collector_name[zframe_size(frame)] = '\0';
  zframe_destroy(&frame);

  return 0;

 err:
  zframe_destroy(&frame);
  bgpwatcher_pfx_record_free(&pfx);
  return -1;
}

zmsg_t *bgpwatcher_pfx_record_serialize(bgpwatcher_pfx_record_t *pfx)
{
  zmsg_t *msg = NULL;
  uint32_t n32;

  if((msg = zmsg_new()) == NULL)
    {
      goto err;
    }

  /* prefix */
  if(msg_append_ip(msg, &pfx->prefix) != 0)
    {
      goto err;
    }

  /* length */
  if(zmsg_addmem(msg, &pfx->prefix_len, sizeof(pfx->prefix_len)) != 0)
    {
      goto err;
    }

  /* peer ip */
  if(msg_append_ip(msg, &pfx->peer_ip) != 0)
    {
      goto err;
    }

  /* orig asn */
  n32 = htonl(pfx->orig_asn);
  if(zmsg_addmem(msg, &n32, sizeof(uint32_t)) != 0)
    {
      goto err;
    }

  /* name */
  if(zmsg_addstr(msg, pfx->collector_name) != 0)
    {
      goto err;
    }

  return msg;

 err:
  zmsg_destroy(&msg);
  return NULL;
}

inline zmsg_t *bgpwatcher_pfx_msg_create(bgpstream_prefix_t *prefix,
                                         bgpstream_ip_address_t *peer_ip,
                                         uint32_t orig_asn,
                                         char *collector_name)
{
  zmsg_t *msg = NULL;
  uint32_t n32;

  if((msg = zmsg_new()) == NULL)
    {
      goto err;
    }

  /* prefix */
  if(msg_append_bgpstream_ip(msg, &prefix->number) != 0)
    {
      goto err;
    }

  /* length */
  if(zmsg_addmem(msg, &prefix->len, sizeof(prefix->len)) != 0)
    {
      goto err;
    }

  /* peer ip */
  if(msg_append_bgpstream_ip(msg, peer_ip) != 0)
    {
      goto err;
    }

  /* orig asn */
  n32 = htonl(orig_asn);
  if(zmsg_addmem(msg, &n32, sizeof(uint32_t)) != 0)
    {
      goto err;
    }

  /* name */
  if(zmsg_addstr(msg, collector_name) != 0)
    {
      goto err;
    }

  return msg;

 err:
  zmsg_destroy(&msg);
  return NULL;
}

void bgpwatcher_pfx_record_dump(bgpwatcher_pfx_record_t *pfx)
{
  char pfx_str[INET6_ADDRSTRLEN] = "";
  char peer_str[INET6_ADDRSTRLEN] = "";

  if(pfx == NULL)
    {
      fprintf(stderr,
	      "------------------------------\n"
	      "NULL\n"
	      "------------------------------\n");
    }
  else
    {
      inet_ntop(pfx->prefix.ss_family,
		get_in_addr((struct sockaddr *)&pfx->prefix),
		pfx_str, INET6_ADDRSTRLEN);

      inet_ntop(pfx->peer_ip.ss_family,
		get_in_addr((struct sockaddr *)&pfx->peer_ip),
		peer_str, INET6_ADDRSTRLEN);

      fprintf(stderr,
	      "------------------------------\n"
	      "Prefix:\t%s/%"PRIu8"\n"
	      "Peer:\t%s\n"
	      "ASN:\t%"PRIu32"\n"
	      "Name:\t%s\n"
	      "------------------------------\n",
	      pfx_str, pfx->prefix_len,
	      peer_str,
	      pfx->orig_asn,
	      pfx->collector_name);
    }
}

bgpwatcher_peer_record_t *bgpwatcher_peer_record_init()
{
  return malloc_zero(sizeof(bgpwatcher_peer_record_t));
}

void bgpwatcher_peer_record_free(bgpwatcher_peer_record_t **peer_p)
{
  bgpwatcher_peer_record_t *peer = *peer_p;

  if(peer == NULL)
    {
      return;
    }

  free(peer);

  *peer_p = NULL;
}

bgpwatcher_peer_record_t *bgpwatcher_peer_record_deserialize(zmsg_t *msg)
{
  bgpwatcher_peer_record_t *peer;
  zframe_t *frame;

  if((peer = bgpwatcher_peer_record_init()) == NULL)
    {
      return NULL;
    }

  /* peer ip */
  if(msg_pop_ip(msg, &peer->ip) != 0)
    {
      goto err;
    }

  /* status */
  if((frame = zmsg_pop(msg)) == NULL ||
     zframe_size(frame) != sizeof(peer->status))
    {
      goto err;
    }
  peer->status = *zframe_data(frame);
  zframe_destroy(&frame);

  return peer;

 err:
  zframe_destroy(&frame);
  bgpwatcher_peer_record_free(&peer);
  return NULL;
}

zmsg_t *bgpwatcher_peer_record_serialize(bgpwatcher_peer_record_t *peer)
{
  zmsg_t *msg = NULL;

  if((msg = zmsg_new()) == NULL)
    {
      goto err;
    }

  /* peer ip */
  if(msg_append_ip(msg, &peer->ip) != 0)
    {
      goto err;
    }

  /* status */
  if(zmsg_addmem(msg, &peer->status, sizeof(peer->status)) != 0)
    {
      goto err;
    }

  return msg;

 err:
  zmsg_destroy(&msg);
  return NULL;
}

void bgpwatcher_peer_record_dump(bgpwatcher_peer_record_t *peer)
{
  char ip_str[INET6_ADDRSTRLEN] = "";

  if(peer == NULL)
    {
      fprintf(stderr,
	      "------------------------------\n"
	      "NULL\n"
	      "------------------------------\n");
    }
  else
    {
      inet_ntop(peer->ip.ss_family,
		get_in_addr((struct sockaddr *)&peer->ip),
		ip_str, INET6_ADDRSTRLEN);

      fprintf(stderr,
	      "------------------------------\n"
	      "IP:\t%s\n"
	      "Status:\t%"PRIu8"\n"
	      "------------------------------\n",
	      ip_str,
	      peer->status);
    }
}
