/* 
 * corsaro
 *
 * Alistair King, CAIDA, UC San Diego
 * corsaro-info@caida.org
 * 
 * Copyright (C) 2012 The Regents of the University of California.
 * 
 * This file is part of corsaro.
 *
 * corsaro is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * corsaro is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with corsaro.  If not, see <http://www.gnu.org/licenses/>.
 *
 * This file is a modified version of the 'ipanon.c' file included with 
 * libtrace (http://research.wand.net.nz/software/libtrace.php)
 *
 */

#include "config.h"

#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include <libtrace.h>

#include "corsaro_libanon.h"
#include "panon.h"

#ifndef HAVE_STRLCPY
#if !HAVE_DECL_STRLCPY
static size_t strlcpy(char *dest, const char *src, size_t size)
{
  size_t ret;
  for(ret=0;src[ret] && ret<size; ret++) {
    dest[ret]=src[ret];
  }
  dest[ret++]='\0';
  return ret;
}
#endif
#endif

static corsaro_anon_enc_type_t enc_type = CORSARO_ANON_ENC_NONE;

static uint32_t masks[33] = {
  0x00000000, 0x80000000, 0xC0000000, 0xe0000000, 0xf0000000,
  0xf8000000, 0xfC000000, 0xfe000000, 0xff000000, 0xff800000,
  0xffC00000, 0xffe00000, 0xfff00000, 0xfff80000, 0xfffC0000,
  0xfffe0000, 0xffff0000, 0xffff8000, 0xffffC000, 0xffffe000,
  0xfffff000, 0xfffff800, 0xfffffC00, 0xfffffe00, 0xffffff00,
  0xffffff80, 0xffffffC0, 0xffffffe0, 0xfffffff0, 0xfffffff8,
  0xfffffffC, 0xfffffffe, 0xffffffff,
};

static uint32_t prefix;
static uint32_t netmask;

/** @todo change to detect invalid prefixes */
static void init_prefix(const char *key)
{
  int a,b,c,d;
  int bits;
  sscanf(key,"%i.%i.%i.%i/%i",
	 &a, &b, &c, &d, &bits);
  prefix=(a<<24) + (b<<16) + (c<<8) + d;
  assert(bits>=0 && bits<=32);
  netmask = masks[bits];
}

static uint32_t prefix_substitute(uint32_t ip)
{
  return (prefix & netmask) | (ip & ~netmask);
}

/* Incrementally update a checksum */
static void update_in_cksum(uint16_t *csum, uint16_t old, uint16_t new)
{
  uint32_t sum = (~htons(*csum) & 0xFFFF) 
    + (~htons(old) & 0xFFFF) 
    + htons(new);
  sum = (sum & 0xFFFF) + (sum >> 16);
  *csum = htons(~(sum + (sum >> 16)));
}

static void update_in_cksum32(uint16_t *csum, uint32_t old, uint32_t new)
{
  update_in_cksum(csum,(uint16_t)(old>>16),(uint16_t)(new>>16));
  update_in_cksum(csum,(uint16_t)(old&0xFFFF),(uint16_t)(new&0xFFFF));
}

void corsaro_anon_init(corsaro_anon_enc_type_t type, char *key)
{
  char cryptopan_key[32];
  memset(cryptopan_key,0,sizeof(cryptopan_key));
  enc_type = type;
  switch (enc_type) {
  case CORSARO_ANON_ENC_NONE:
    break;
  case CORSARO_ANON_ENC_PREFIX_SUBSTITUTION:
    init_prefix(key);
    break;
  case CORSARO_ANON_ENC_CRYPTOPAN:
    strlcpy(cryptopan_key,key,sizeof(cryptopan_key));
    panon_init(cryptopan_key);
    break;
  default:
    /* unknown encryption type */
    assert(0 && "Unknown encryption type");
  }
}

uint32_t corsaro_anon_ip(uint32_t orig_addr) 
{
  switch (enc_type) {
  case CORSARO_ANON_ENC_NONE:
    return orig_addr;
  case CORSARO_ANON_ENC_PREFIX_SUBSTITUTION:
    return prefix_substitute(orig_addr);
  case CORSARO_ANON_ENC_CRYPTOPAN:
    return cpp_anonymize(orig_addr);
  default:
    /* unknown encryption type */
    assert(0 && "Unknown encryption type");
    return -1;
  }
}

/* Ok this is remarkably complicated
 *
 * We want to change one, or the other IP address, while preserving
 * the checksum.  TCP and UDP both include the faux header in their
 * checksum calculations, so you have to update them too.  ICMP is
 * even worse -- it can include the original IP packet that caused the
 * error!  So anonymise that too, but remember that it's travelling in
 * the opposite direction so we need to encrypt the destination and
 * source instead of the source and destination!
 */
void corsaro_anon_ip_header(struct libtrace_ip *ip,
			    int enc_source,
			    int enc_dest)
{
  struct libtrace_tcp *tcp;
  struct libtrace_udp *udp;
  struct libtrace_icmp *icmp;

  tcp=trace_get_tcp_from_ip(ip,NULL);
  udp=trace_get_udp_from_ip(ip,NULL);
  icmp=trace_get_icmp_from_ip(ip,NULL);

  if (enc_source) {
    uint32_t old_ip=ip->ip_src.s_addr;
    uint32_t new_ip=htonl(corsaro_anon_ip(
					  htonl(ip->ip_src.s_addr)
					  ));
    update_in_cksum32(&ip->ip_sum,old_ip,new_ip);
    if (tcp) update_in_cksum32(&tcp->check,old_ip,new_ip);
    if (udp) update_in_cksum32(&udp->check,old_ip,new_ip);
    ip->ip_src.s_addr = new_ip;
  }

  if (enc_dest) {
    uint32_t old_ip=ip->ip_dst.s_addr;
    uint32_t new_ip=htonl(corsaro_anon_ip(
				 htonl(ip->ip_dst.s_addr)
				 ));
    update_in_cksum32(&ip->ip_sum,old_ip,new_ip);
    if (tcp) update_in_cksum32(&tcp->check,old_ip,new_ip);
    if (udp) update_in_cksum32(&udp->check,old_ip,new_ip);
    ip->ip_dst.s_addr = new_ip;
  }

  if (icmp) {
    /* These are error codes that return the IP packet
     * internally 
     */
    if (icmp->type == 3 
	|| icmp->type == 5 
	|| icmp->type == 11) {
      corsaro_anon_ip_header((struct libtrace_ip*)icmp+
			     sizeof(struct libtrace_icmp),
			     enc_dest,
			     enc_source);
    }
  }
}
