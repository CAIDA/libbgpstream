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
  else if(zframe_size(frame) == sizeof(uint64_t)*2)
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

bgpwatcher_msg_type_t bgpwatcher_msg_type(zmsg_t *msg)
{
  zframe_t *frame;
  uint8_t type;

  /* first frame should be our type */
  if((frame = zmsg_pop(msg)) == NULL)
    {
      return BGPWATCHER_MSG_TYPE_UNKNOWN;
    }

  if((type = *zframe_data(frame)) > BGPWATCHER_MSG_TYPE_MAX)
    {
      zframe_destroy(&frame);
      return BGPWATCHER_MSG_TYPE_UNKNOWN;
    }

  zframe_destroy(&frame);

  return (bgpwatcher_msg_type_t)type;
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
      if(pfx->collector_name != NULL)
	{
	  free(pfx->collector_name);
	}
      free(pfx);
    }

  *pfx_p = NULL;
}

bgpwatcher_pfx_record_t *bgpwatcher_pfx_record_deserialize(zmsg_t *msg)
{
  bgpwatcher_pfx_record_t *pfx;
  zframe_t *frame;

  if((pfx = bgpwatcher_pfx_record_init()) == NULL)
    {
      return NULL;
    }

  /* prefix */
  if(msg_pop_ip(msg, &pfx->prefix) != 0)
    {
      goto err;
    }

  /* pfx len */
  if((frame = zmsg_pop(msg)) == NULL || zframe_size(frame) != sizeof(uint8_t))
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
  if((frame = zmsg_pop(msg)) == NULL || zframe_size(frame) != sizeof(uint32_t))
    {
      goto err;
    }
  memcpy(&pfx->orig_asn, zframe_data(frame), sizeof(uint32_t));
  pfx->orig_asn = ntohl(pfx->orig_asn);
  zframe_destroy(&frame);

  /* name */
  if((pfx->collector_name = zmsg_popstr(msg)) == NULL)
    {
      goto err;
    }

  return pfx;

 err:
  zframe_destroy(&frame);
  bgpwatcher_pfx_record_free(&pfx);
  return NULL;
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
  if(zmsg_addmem(msg, &pfx->prefix_len, sizeof(uint8_t)) != 0)
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
