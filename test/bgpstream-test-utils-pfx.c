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

#define IPV4_TEST_PFX_A "192.0.43.0/24"
#define IPV4_TEST_PFX_B "130.217.0.0/16"
#define IPV4_TEST_PFX_B_CHILD "130.217.250.0/24"

static int test_prefixes_ipv4()
{
  bgpstream_pfx_t a;
  bgpstream_pfx_t b;

  // using heap instead of stack can reveal certain types of memory access
  // errors, especially when run under valgrind
  bgpstream_ipv4_pfx_t *a4 = malloc(sizeof(bgpstream_ipv4_pfx_t));
  bgpstream_ipv4_pfx_t *b4 = malloc(sizeof(bgpstream_ipv4_pfx_t));

  /* build a prefix from a string */
  CHECK("IPv4 prefix from string",
        bgpstream_str2pfx(IPV4_TEST_PFX_A, &a) != NULL);

  /* convert prefix to string */
  CHECK_SNPRINTF("IPv4 prefix to string", IPV4_TEST_PFX_A, BUFFER_LEN,
    CHAR_P, bgpstream_pfx_snprintf(cs_buf, cs_len, &a));

  /* STORAGE CHECKS */

  /* populate pfx b (storage) */
  bgpstream_str2pfx(IPV4_TEST_PFX_B, &b);

  /* check generic equal */
  CHECK("IPv4 prefix generic-equals",
    !bgpstream_pfx_equal(&a, &b) && bgpstream_pfx_equal(&a, &a));

  /* IPV4-SPECIFIC CHECKS */

  /* populate ipv4 a */
  bgpstream_str2pfx(IPV4_TEST_PFX_A, (bgpstream_pfx_t *)a4);
  /* populate ipv4 b */
  bgpstream_str2pfx(IPV4_TEST_PFX_B, (bgpstream_pfx_t *)b4);

  /* check generic equal */
  CHECK(
    "IPv4 prefix generic-equals (cast from ipv4)",
    !bgpstream_pfx_equal((bgpstream_pfx_t *)a4, (bgpstream_pfx_t *)b4) &&
      bgpstream_pfx_equal((bgpstream_pfx_t *)a4, (bgpstream_pfx_t *)a4));

  /* check ipv4 equal */
  CHECK("IPv4 prefix ipv4-equals (ipv4)",
        !bgpstream_ipv4_pfx_equal(a4, b4) &&
          bgpstream_ipv4_pfx_equal(a4, a4));

  /* prefix contains (i.e. more specifics) */
  bgpstream_str2pfx(IPV4_TEST_PFX_B, &a);
  bgpstream_str2pfx(IPV4_TEST_PFX_B_CHILD, &b);
  /* b is a child of a BUT a is NOT a child of b */
  CHECK("IPv4 prefix contains",
    bgpstream_pfx_contains(&a, &b) && !bgpstream_pfx_contains(&b, &a));

  /* prefix set checks */
  bgpstream_pfx_set_t *set = bgpstream_pfx_set_create();
  CHECK("IPv4 pfx_set_insert",
      bgpstream_pfx_set_insert(set, (bgpstream_pfx_t*)a4) == 1);
  CHECK("IPv4 pfx_set_exists",
      bgpstream_pfx_set_exists(set, (bgpstream_pfx_t*)a4) &&
      !bgpstream_pfx_set_exists(set, (bgpstream_pfx_t*)b4));
  CHECK("IPv4 pfx_set_insert",
      bgpstream_pfx_set_insert(set, (bgpstream_pfx_t*)b4) == 1);
  CHECK("IPv4 pfx_set_exists",
      bgpstream_pfx_set_exists(set, (bgpstream_pfx_t*)a4) &&
      bgpstream_pfx_set_exists(set, (bgpstream_pfx_t*)b4));
  CHECK("IPv4 pfx_set_insert dup",
      bgpstream_pfx_set_insert(set, (bgpstream_pfx_t*)a4) == 0);
  CHECK("IPv4 pfx_set_version_size",
      bgpstream_pfx_set_version_size(set, BGPSTREAM_ADDR_VERSION_IPV4) == 2);
  bgpstream_pfx_set_destroy(set);

  free(a4);
  free(b4);
  return 0;
}

#define IPV6_TEST_PFX_A "2001:500:88::/48"
#define IPV6_TEST_PFX_A_CHILD "2001:500:88:beef::/64"
#define IPV6_TEST_PFX_B "2001:48d0:101:501::/64"
#define IPV6_TEST_PFX_B_CHILD "2001:48d0:101:501:beef::/96"

static int test_prefixes_ipv6()
{
  bgpstream_pfx_t a;
  bgpstream_pfx_t a_child;
  bgpstream_pfx_t b;
  bgpstream_pfx_t b_child;

  // using heap instead of stack can reveal certain types of memory access
  // errors, especially when run under valgrind
  bgpstream_ipv6_pfx_t *a6 = malloc(sizeof(bgpstream_ipv6_pfx_t));
  bgpstream_ipv6_pfx_t *b6 = malloc(sizeof(bgpstream_ipv6_pfx_t));

  /* build a prefix from a string */
  CHECK("IPv6 prefix from string",
        bgpstream_str2pfx(IPV6_TEST_PFX_A, &a) != NULL);

  /* convert prefix to string */
  CHECK_SNPRINTF("IPv6 prefix to string", IPV6_TEST_PFX_A, BUFFER_LEN,
    CHAR_P, bgpstream_pfx_snprintf(cs_buf, cs_len, &a));

  /* STORAGE CHECKS */

  /* populate pfx b (storage) */
  bgpstream_str2pfx(IPV6_TEST_PFX_B, &b);

  /* check generic equal */
  CHECK("IPv6 prefix generic-equals",
    !bgpstream_pfx_equal(&a, &b) && bgpstream_pfx_equal(&a, &a));

  /* IPV6-SPECIFIC CHECKS */

  /* populate ipv6 a */
  bgpstream_str2pfx(IPV6_TEST_PFX_A, (bgpstream_pfx_t *)a6);
  /* populate ipv6 b */
  bgpstream_str2pfx(IPV6_TEST_PFX_B, (bgpstream_pfx_t *)b6);

  /* check generic equal */
  CHECK(
    "IPv6 prefix generic-equals (cast from ipv6)",
    !bgpstream_pfx_equal((bgpstream_pfx_t *)a6, (bgpstream_pfx_t *)b6) &&
      bgpstream_pfx_equal((bgpstream_pfx_t *)a6, (bgpstream_pfx_t *)a6));

  /* check ipv6 equal */
  CHECK("IPv6 prefix ipv6-equals (ipv6)",
        !bgpstream_ipv6_pfx_equal(a6, b6) &&
          bgpstream_ipv6_pfx_equal(a6, a6));

  /* prefix contains (i.e. more specifics) */
  bgpstream_str2pfx(IPV6_TEST_PFX_A, &a);
  bgpstream_str2pfx(IPV6_TEST_PFX_A_CHILD, &a_child);
  bgpstream_str2pfx(IPV6_TEST_PFX_B, &b);
  bgpstream_str2pfx(IPV6_TEST_PFX_B_CHILD, &b_child);
  /* b is a child of a BUT a is NOT a child of b */
  CHECK("IPv6 prefix contains",
        bgpstream_pfx_contains(&a, &a_child) &&
        !bgpstream_pfx_contains(&a_child, &a) &&
        bgpstream_pfx_contains(&b, &b_child) &&
        !bgpstream_pfx_contains(&b_child, &b));

  /* prefix set checks */
  bgpstream_pfx_set_t *set = bgpstream_pfx_set_create();
  CHECK("IPv6 pfx_set_insert",
      bgpstream_pfx_set_insert(set, (bgpstream_pfx_t*)a6) == 1);
  CHECK("IPv6 pfx_set_exists",
      bgpstream_pfx_set_exists(set, (bgpstream_pfx_t*)a6) &&
      !bgpstream_pfx_set_exists(set, (bgpstream_pfx_t*)b6));
  CHECK("IPv6 pfx_set_insert",
      bgpstream_pfx_set_insert(set, (bgpstream_pfx_t*)b6) == 1);
  CHECK("IPv6 pfx_set_exists",
      bgpstream_pfx_set_exists(set, (bgpstream_pfx_t*)a6) &&
      bgpstream_pfx_set_exists(set, (bgpstream_pfx_t*)b6));
  CHECK("IPv6 pfx_set_insert dup",
      bgpstream_pfx_set_insert(set, (bgpstream_pfx_t*)a6) == 0);
  CHECK("IPv6 pfx_set_version_size",
      bgpstream_pfx_set_version_size(set, BGPSTREAM_ADDR_VERSION_IPV6) == 2);
  bgpstream_pfx_set_destroy(set);

  free(a6);
  free(b6);

  return 0;
}

int main()
{
  CHECK_SECTION("IPv4 prefixes", test_prefixes_ipv4() == 0);
  CHECK_SECTION("IPv6 prefixes", test_prefixes_ipv6() == 0);

  ENDTEST;
  return 0;
}
