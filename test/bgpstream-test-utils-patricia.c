/*
 * Copyright (C) 2016 The Regents of the University of California.
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

#define IPV4_TEST_PFX_A "192.0.43.0/24"
#define IPV4_TEST_PFX_B "130.217.0.0/16"
#define IPV4_TEST_PFX_B_CHILD "130.217.250.0/24"
#define IPV4_TEST_24_CNT 257
#define IPV4_TEST_PFX_OVERLAP "130.217.240.0/20"

#define IPV6_TEST_PFX_A "2001:500:88::/48"
#define IPV6_TEST_PFX_A_CHILD "2001:500:88:beef::/64"
#define IPV6_TEST_PFX_B "2001:48d0:101:501::/64"
#define IPV6_TEST_PFX_B_CHILD "2001:48d0:101:501:beef::/96"
#define IPV6_TEST_64_CNT 65537

static int test_patricia()
{
  bgpstream_patricia_tree_t *pt;
  bgpstream_patricia_tree_result_set_t *res;
  bgpstream_patricia_node_t *node;
  bgpstream_pfx_t pfx;
  const bgpstream_pfx_t *pfxp;
  int count4 = 0;
  int count6 = 0;

// Convenience macros
#define s2p(str) bgpstream_str2pfx((str), &pfx)
#define BPT_insert               bgpstream_patricia_tree_insert
#define BPT_search_exact         bgpstream_patricia_tree_search_exact
#define BPT_get_pfx_overlap_info bgpstream_patricia_tree_get_pfx_overlap_info
#define BPT_get_minimum_coverage bgpstream_patricia_tree_get_minimum_coverage
#define BPT_get_less_specifics   bgpstream_patricia_tree_get_less_specifics
#define BPT_get_pfx              bgpstream_patricia_tree_get_pfx
#define BPT_get_mincovering_pfx  bgpstream_patricia_tree_get_mincovering_prefix
#define BPT_pfx_count            bgpstream_patricia_prefix_count

#define INSERT(ipv, str, count) \
  do { \
    char namebuf[1024]; \
    snprintf(namebuf, sizeof(namebuf), "Insert into Patricia Tree v%d: %s", \
      ipv, str); \
    CHECK(namebuf, \
      BPT_insert(pt, s2p(str)) && \
      BPT_pfx_count(pt, BGPSTREAM_ADDR_VERSION_IPV##ipv) == count); \
  } while (0)

  /* Create a Patricia Tree */
  CHECK("Create Patricia Tree",
        (pt = bgpstream_patricia_tree_create(NULL)) != NULL);

  /* Create a Patricia Tree */
  CHECK("Create Patricia Tree Result",
        (res = bgpstream_patricia_tree_result_set_create()) != NULL);

  /* Insert into Patricia Tree */

  INSERT(4, IPV4_TEST_PFX_A,       ++count4);
  INSERT(4, IPV4_TEST_PFX_B,       ++count4);
  INSERT(4, IPV4_TEST_PFX_B_CHILD, ++count4);
  INSERT(4, IPV4_TEST_PFX_B,       count4); // duplicate; don't increment counter

  INSERT(6, IPV6_TEST_PFX_A,       ++count6);
  INSERT(6, IPV6_TEST_PFX_A_CHILD, ++count6);
  INSERT(6, IPV6_TEST_PFX_B,       ++count6);
  INSERT(6, IPV6_TEST_PFX_B_CHILD, ++count6);
  INSERT(6, IPV6_TEST_PFX_A_CHILD, count6); // duplicate; don't increment counter

  /* Search prefixes */
  CHECK("Patricia Tree v4 search exact",
        BPT_search_exact(pt, s2p(IPV4_TEST_PFX_A)) != NULL);
  CHECK("Patricia Tree v6 search exact",
        BPT_search_exact(pt, s2p(IPV6_TEST_PFX_A)) != NULL);

  /* Overlap info */
  CHECK("Patricia Tree v4 overlap info",
    BPT_get_pfx_overlap_info(pt, s2p(IPV4_TEST_PFX_OVERLAP)) ==
        (BGPSTREAM_PATRICIA_LESS_SPECIFICS | BGPSTREAM_PATRICIA_MORE_SPECIFICS));
  CHECK("Patricia Tree v6 overlap info",
    BPT_get_pfx_overlap_info(pt, s2p(IPV6_TEST_PFX_B)) ==
        (BGPSTREAM_PATRICIA_EXACT_MATCH | BGPSTREAM_PATRICIA_MORE_SPECIFICS));

  /* Count minimum coverage prefixes */
  CHECK("Patricia Tree v4 minimum coverage",
        BPT_get_minimum_coverage(pt, BGPSTREAM_ADDR_VERSION_IPV4, res) == 0 &&
        bgpstream_patricia_tree_result_set_count(res) == 2);
  CHECK("Patricia Tree v6 minimum coverage",
        BPT_get_minimum_coverage(pt, BGPSTREAM_ADDR_VERSION_IPV6, res) == 0 &&
        bgpstream_patricia_tree_result_set_count(res) == 2);

  /* Count prefixes subnets */
  CHECK("Patricia Tree v4 /24 subnets",
        bgpstream_patricia_tree_count_24subnets(pt) == IPV4_TEST_24_CNT);
  CHECK("Patricia Tree v6 /64 subnets",
        bgpstream_patricia_tree_count_64subnets(pt) == IPV6_TEST_64_CNT);

  /* Less specifics */
  CHECK("Patricia Tree v4 less specific",
        (node = BPT_search_exact(pt, s2p(IPV4_TEST_PFX_B_CHILD))) != NULL &&
        BPT_get_less_specifics(pt, node, res) == 0 &&
        bgpstream_patricia_tree_result_set_count(res) == 1 &&
        (node = bgpstream_patricia_tree_result_set_next(res)) != NULL &&
        (pfxp = BPT_get_pfx(node)) != NULL &&
        bgpstream_pfx_equal(pfxp, s2p(IPV4_TEST_PFX_B)) != 0);

  /* Min covering */
  CHECK("Patricia Tree v4 min covering pfx",
        (node = BPT_search_exact(pt, s2p(IPV4_TEST_PFX_B_CHILD))) != NULL &&
        BPT_get_mincovering_pfx(pt, node, res) == 0 &&
        bgpstream_patricia_tree_result_set_count(res) == 1 &&
        (node = bgpstream_patricia_tree_result_set_next(res)) != NULL &&
        (pfxp = BPT_get_pfx(node)) != NULL &&
        bgpstream_pfx_equal(pfxp, s2p(IPV4_TEST_PFX_B)) != 0);

  bgpstream_patricia_tree_destroy(pt);
  bgpstream_patricia_tree_result_set_destroy(&res);

  // This sequence of inserts caused an assertion failure in @6959441
  const char *pfxs[] = {
    "1.0.0.0/24",
    "1.0.4.0/22",
    "1.0.64.0/18",
    "1.0.128.0/24",
    "1.0.129.0/24",
    "1.0.132.0/22", // failed assert
    "2.158.48.15/21",
    "2.158.57.0/24",
    "2.158.48.0/20", // different failed assert
    NULL
  };
  pt = bgpstream_patricia_tree_create(NULL);
  for (int i = 0; pfxs[i]; i++){
    INSERT(4, pfxs[i], i+1);
  }
  bgpstream_patricia_tree_destroy(pt);

  return 0;
}

int main()
{
  CHECK_SECTION("Patricia Tree", test_patricia() == 0);
  ENDTEST;
  return 0;
}
