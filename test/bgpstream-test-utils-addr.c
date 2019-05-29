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
#define IPV4_TEST_ADDR_A_MASKED "192.0.43.0"
#define IPV4_TEST_ADDR_A_MASKLEN 24
#define IPV4_TEST_ADDR_B "192.172.226.3"

static int test_addresses_ipv4()
{
  bgpstream_ip_addr_t a;
  bgpstream_ip_addr_t a_masked;
  bgpstream_ip_addr_t b;

  bgpstream_ipv4_addr_t a4;
  bgpstream_ipv4_addr_t a4_masked;
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
  /* generic mask */
  bgpstream_str2addr(IPV4_TEST_ADDR_A, &a);
  bgpstream_str2addr(IPV4_TEST_ADDR_A_MASKED, &a_masked);

  bgpstream_addr_mask(&a, IPV4_TEST_ADDR_A_MASKLEN);

  CHECK("IPv4 address generic-mask",
        bgpstream_addr_equal(&a, &a_masked) != 0);

  /* ipv4-specific */
  bgpstream_str2addr(IPV4_TEST_ADDR_A, (bgpstream_ip_addr_t *)&a4);
  bgpstream_str2addr(IPV4_TEST_ADDR_A_MASKED, (bgpstream_ip_addr_t *)&a4_masked);

  bgpstream_ipv4_addr_mask(&a4, IPV4_TEST_ADDR_A_MASKLEN);

  CHECK("IPv4 address ipv4-mask (ipv4)",
        bgpstream_ipv4_addr_equal(&a4, &a4_masked) != 0);

  /* copy checks */
  bgpstream_addr_copy(&b, &a);

  CHECK("IPv4 address copy",
        bgpstream_addr_equal(&a, &b) != 0);

  return 0;
}

#define IPV6_TEST_ADDR_A "2001:500:88:200::8"
#define IPV6_TEST_ADDR_A_MASKED "2001:500:88::"
#define IPV6_TEST_ADDR_A_MASKLEN 48
#define IPV6_TEST_ADDR_B "2001:48d0:101:501::123"
#define IPV6_TEST_ADDR_B_MASKED "2001:48d0:101:501::"
#define IPV6_TEST_ADDR_B_MASKLEN 96

static int test_addresses_ipv6()
{
  bgpstream_ip_addr_t a;
  bgpstream_ip_addr_t a_masked;
  bgpstream_ip_addr_t b;
  bgpstream_ip_addr_t b_masked;

  bgpstream_ipv6_addr_t a6;
  bgpstream_ipv6_addr_t a6_masked;
  bgpstream_ipv6_addr_t b6;
  bgpstream_ipv6_addr_t b6_masked;

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

  /* MASK CHECKS (addr a checks len < 64, addr b checks len > 64) */
  /* generic mask */
  bgpstream_str2addr(IPV6_TEST_ADDR_A, &a);
  bgpstream_str2addr(IPV6_TEST_ADDR_A_MASKED, &a_masked);
  bgpstream_addr_mask(&a, IPV6_TEST_ADDR_A_MASKLEN);

  bgpstream_str2addr(IPV6_TEST_ADDR_B, &b);
  bgpstream_str2addr(IPV6_TEST_ADDR_B_MASKED, &b_masked);
  bgpstream_addr_mask(&b, IPV6_TEST_ADDR_B_MASKLEN);

  CHECK("IPv6 address generic-mask",
        bgpstream_addr_equal(&a, &a_masked) != 0 &&
          bgpstream_addr_equal(&b, &b_masked) != 0);

  /* ipv6-specific */
  bgpstream_str2addr(IPV6_TEST_ADDR_A, (bgpstream_ip_addr_t *)&a6);
  bgpstream_str2addr(IPV6_TEST_ADDR_A_MASKED, (bgpstream_ip_addr_t *)&a6_masked);
  bgpstream_ipv6_addr_mask(&a6, IPV6_TEST_ADDR_A_MASKLEN);

  bgpstream_str2addr(IPV6_TEST_ADDR_B, (bgpstream_ip_addr_t *)&b6);
  bgpstream_str2addr(IPV6_TEST_ADDR_B_MASKED, (bgpstream_ip_addr_t *)&b6_masked);
  bgpstream_ipv6_addr_mask(&b6, IPV6_TEST_ADDR_B_MASKLEN);

  CHECK("IPv6 address ipv6-mask (ipv6)",
        bgpstream_ipv6_addr_equal(&a6, &a6_masked) != 0 &&
          bgpstream_ipv6_addr_equal(&b6, &b6_masked) != 0);

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
