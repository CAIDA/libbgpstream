/*
 * Copyright (C) 2015 The Regents of the University of California.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "bgpstream_test.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#define BUFFER_LEN 1024
static char buffer[BUFFER_LEN];

/* IPv4 Addresses */
#define IPV4_TEST_ADDR_A "192.0.43.8"
#define IPV4_TEST_ADDR_B "192.172.226.3"

static int test_v4_mask(const char *a4str, int len, const char *m4str)
{
  char buf[80];
  bgpstream_ipv4_addr_t a4, m4;
  bgpstream_ip_addr_t a, m;

  bgpstream_str2addr(a4str, (bgpstream_ip_addr_t *)&a4);
  bgpstream_str2addr(m4str, (bgpstream_ip_addr_t *)&m4);
  bgpstream_ipv4_addr_mask(&a4, len);
  sprintf(buf, "IPv4 mask %d (ipv4)", len);
  CHECK(buf, bgpstream_ipv4_addr_equal(&a4, &m4));

  bgpstream_str2addr(a4str, (bgpstream_ip_addr_t *)&a);
  bgpstream_str2addr(m4str, (bgpstream_ip_addr_t *)&m);
  bgpstream_addr_mask(&a, len);
  sprintf(buf, "IPv4 mask %d (generic)", len);
  CHECK(buf, bgpstream_addr_equal(&a, &m));

  return 0;
}

static int test_addresses_ipv4()
{
  bgpstream_ip_addr_t a;
  bgpstream_ip_addr_t b;

  bgpstream_ipv4_addr_t a4;
  bgpstream_ipv4_addr_t b4;

  /* IPv4 */
  CHECK("IPv4 address from string",
        bgpstream_str2addr(IPV4_TEST_ADDR_A, &a) != NULL);

  /* check conversion from and to string */
  bgpstream_addr_ntop(buffer, BUFFER_LEN, &a);
  CHECK("IPv4 address to string", strcmp(buffer, IPV4_TEST_ADDR_A) == 0);

  /* STORAGE CHECKS */

  /* populate address b */
  bgpstream_str2addr(IPV4_TEST_ADDR_B, &b);

  /* check generic equal */
  CHECK("IPv4 address generic-equals",
        bgpstream_addr_equal(&a, &b) == 0 &&
          bgpstream_addr_equal(&a, &a) != 0);

  /* IPV4-SPECIFIC CHECKS */

  /* populate ipv4 a */
  bgpstream_str2addr(IPV4_TEST_ADDR_A, (bgpstream_ip_addr_t *)&a4);
  /* populate ipv4 b */
  bgpstream_str2addr(IPV4_TEST_ADDR_B, (bgpstream_ip_addr_t *)&b4);

  /* check generic equal */
  CHECK("IPv4 address generic-equals (cast from ipv4)",
        bgpstream_addr_equal((bgpstream_ip_addr_t *)&a4,
                             (bgpstream_ip_addr_t *)&b4) == 0 &&
          bgpstream_addr_equal((bgpstream_ip_addr_t *)&a4,
                               (bgpstream_ip_addr_t *)&a4) != 0);

  /* check ipv4 equal */
  CHECK("IPv4 address ipv4-equals (ipv4)",
        bgpstream_ipv4_addr_equal(&a4, &b4) == 0 &&
          bgpstream_ipv4_addr_equal(&a4, &a4) != 0);

  /* MASK CHECKS */
  if (
    test_v4_mask("255.255.255.255", 32, "255.255.255.255") ||
    test_v4_mask("255.255.255.255", 20, "255.255.240.0") ||
    test_v4_mask("255.255.255.255", 19, "255.255.224.0") ||
    test_v4_mask("255.255.255.255", 18, "255.255.192.0") ||
    test_v4_mask("255.255.255.255", 17, "255.255.128.0") ||
    test_v4_mask("255.255.255.255", 16, "255.255.0.0") ||
    test_v4_mask("255.255.255.255", 15, "255.254.0.0") ||
    test_v4_mask("255.255.255.255", 14, "255.252.0.0") ||
    test_v4_mask("255.255.255.255", 13, "255.248.0.0") ||
    test_v4_mask("255.255.255.255", 12, "255.240.0.0") ||
    test_v4_mask("255.255.255.255", 0, "0.0.0.0"))
      return -1;

  /* copy checks */
  bgpstream_addr_copy(&b, &a);

  CHECK("IPv4 address copy",
        bgpstream_addr_equal(&a, &b) != 0);

  return 0;
}

#define IPV6_TEST_ADDR_A "2001:500:88:200::8"
#define IPV6_TEST_ADDR_B "2001:48d0:101:501::123"

static int test_v6_mask(const char *a6str, int len, const char *m6str)
{
  char buf[80];
  bgpstream_ipv6_addr_t a6, m6;
  bgpstream_ip_addr_t a, m;

  bgpstream_str2addr(a6str, (bgpstream_ip_addr_t *)&a6);
  bgpstream_str2addr(m6str, (bgpstream_ip_addr_t *)&m6);
  bgpstream_ipv6_addr_mask(&a6, len);
  sprintf(buf, "IPv6 mask %d (ipv6)", len);
  CHECK(buf, bgpstream_ipv6_addr_equal(&a6, &m6));

  bgpstream_str2addr(a6str, (bgpstream_ip_addr_t *)&a);
  bgpstream_str2addr(m6str, (bgpstream_ip_addr_t *)&m);
  bgpstream_addr_mask(&a, len);
  sprintf(buf, "IPv6 mask %d (generic)", len);
  CHECK(buf, bgpstream_addr_equal(&a, &m));

  return 0;
}

static int test_addresses_ipv6()
{
  bgpstream_ip_addr_t a;
  bgpstream_ip_addr_t b;

  bgpstream_ipv6_addr_t a6;
  bgpstream_ipv6_addr_t b6;

  /* IPv6 */
  CHECK("IPv6 address from string",
        bgpstream_str2addr(IPV6_TEST_ADDR_A, &a) != NULL);

  /* check conversion from and to string */
  bgpstream_addr_ntop(buffer, BUFFER_LEN, &a);
  CHECK("IPv6 address to string", strcmp(buffer, IPV6_TEST_ADDR_A) == 0);

  /* STORAGE CHECKS */

  /* populate address b */
  bgpstream_str2addr(IPV6_TEST_ADDR_B, &b);

  /* check generic equal */
  CHECK("IPv6 address generic-equals",
        bgpstream_addr_equal(&a, &b) == 0 &&
          bgpstream_addr_equal(&a, &a) != 0);

  /* IPV6-SPECIFIC CHECKS */

  /* populate ipv6 a */
  bgpstream_str2addr(IPV6_TEST_ADDR_A, (bgpstream_ip_addr_t *)&a6);
  /* populate ipv6 b */
  bgpstream_str2addr(IPV6_TEST_ADDR_B, (bgpstream_ip_addr_t *)&b6);

  /* check generic equal */
  CHECK("IPv6 address generic-equals (cast from ipv6)",
        bgpstream_addr_equal((bgpstream_ip_addr_t *)&a6,
                             (bgpstream_ip_addr_t *)&b6) == 0 &&
          bgpstream_addr_equal((bgpstream_ip_addr_t *)&a6,
                               (bgpstream_ip_addr_t *)&a6) != 0);

  /* check ipv6 equal */
  CHECK("IPv6 address ipv6-equals (ipv6)",
        bgpstream_ipv6_addr_equal(&a6, &b6) == 0 &&
          bgpstream_ipv6_addr_equal(&a6, &a6) != 0);

  if (
    test_v6_mask("1:2:3:89ab:cdef:4:5:6", 128, "1:2:3:89ab:cdef:4:5:6") ||
    test_v6_mask("1:2:3:89ab:cdef:4:5:6", 72, "1:2:3:89ab:cd00::") ||
    test_v6_mask("1:2:3:89ab:cdef:4:5:6", 71, "1:2:3:89ab:cc00::") ||
    test_v6_mask("1:2:3:89ab:cdef:4:5:6", 70, "1:2:3:89ab:cc00::") ||
    test_v6_mask("1:2:3:89ab:cdef:4:5:6", 69, "1:2:3:89ab:c800::") ||
    test_v6_mask("1:2:3:89ab:cdef:4:5:6", 68, "1:2:3:89ab:c000::") ||
    test_v6_mask("1:2:3:89ab:cdef:4:5:6", 67, "1:2:3:89ab:c000::") ||
    test_v6_mask("1:2:3:89ab:cdef:4:5:6", 66, "1:2:3:89ab:c000::") ||
    test_v6_mask("1:2:3:89ab:cdef:4:5:6", 65, "1:2:3:89ab:8000::") ||
    test_v6_mask("1:2:3:89ab:cdef:4:5:6", 64, "1:2:3:89ab::") ||
    test_v6_mask("1:2:3:89ab:cdef:4:5:6", 63, "1:2:3:89aa::") ||
    test_v6_mask("1:2:3:89ab:cdef:4:5:6", 62, "1:2:3:89a8::") ||
    test_v6_mask("1:2:3:89ab:cdef:4:5:6", 61, "1:2:3:89a8::") ||
    test_v6_mask("1:2:3:89ab:cdef:4:5:6", 60, "1:2:3:89a0::") ||
    test_v6_mask("1:2:3:89ab:cdef:4:5:6", 59, "1:2:3:89a0::") ||
    test_v6_mask("1:2:3:89ab:cdef:4:5:6", 58, "1:2:3:8980::") ||
    test_v6_mask("1:2:3:89ab:cdef:4:5:6", 57, "1:2:3:8980::") ||
    test_v6_mask("1:2:3:89ab:cdef:4:5:6", 56, "1:2:3:8900::") ||
    test_v6_mask("1:2:3:89ab:cdef:4:5:6", 0, "::"))
      return -1;

  /* copy checks */
  bgpstream_addr_copy(&b, &a);

  CHECK("IPv6 address copy",
        bgpstream_addr_equal(&a, &b) != 0);

  return 0;
}

int main()
{
  CHECK_SECTION("IPv4 addresses", test_addresses_ipv4() == 0);
  CHECK_SECTION("IPv6 addresses", test_addresses_ipv6() == 0);
  ENDTEST;
  return 0;
}
