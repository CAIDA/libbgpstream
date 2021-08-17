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
#include <errno.h>
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

// does NOT mask off trailing bits in the resulting prefix
// copied from bgpstream_str2pfx
static bgpstream_pfx_t *str2pfx_raw(const char *pfx_str, bgpstream_pfx_t *pfx)
{
  if (pfx_str == NULL || pfx == NULL) {
    return NULL;
  }

  char pfx_copy[INET6_ADDRSTRLEN + 3];
  char *endptr = NULL;

  /* strncpy() functions copy at most len characters from src into
   * dst.  If src is less than len characters long, the remainder of
   * dst is filled with `\0' characters.  Otherwise, dst is not
   * terminated. */
  strncpy(pfx_copy, pfx_str, INET6_ADDRSTRLEN + 3);
  if (pfx_copy[INET6_ADDRSTRLEN + 3 - 1] != '\0') {
    return NULL;
  }

  /* get pointer to ip/mask divisor */
  char *found = strchr(pfx_copy, '/');
  if (found == NULL) {
    return NULL;
  }

  *found = '\0';
  /* get the ip address */
  if (bgpstream_str2addr(pfx_copy, &pfx->address) == NULL) {
    return NULL;
  }

  /* get the mask len */
  errno = 0;
  unsigned long int r = strtoul(found + 1, &endptr, 10);
  int ret = errno;
  if (!(endptr != NULL && *endptr == '\0') || ret != 0 ||
      (pfx->address.version == BGPSTREAM_ADDR_VERSION_IPV4 && r > 32) ||
      (pfx->address.version == BGPSTREAM_ADDR_VERSION_IPV6 && r > 128)) {
    return NULL;
  }
  pfx->mask_len = (uint8_t)r;
  pfx->allowed_matches = BGPSTREAM_PREFIX_MATCH_ANY;

  return pfx;
}


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
#define s2p(str) str2pfx_raw((str), &pfx)
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

  /* Default route */
  pt = bgpstream_patricia_tree_create(NULL);
  res = bgpstream_patricia_tree_result_set_create();
  count4 = 0;
  INSERT(4, "10.0.0.0/8", ++count4);
  INSERT(4, "10.1.2.3/32", ++count4);
  INSERT(4, "0.0.0.0/0", ++count4);
  INSERT(4, "192.172.226.78/32", ++count4);

  CHECK("Patricia Tree v4 default route - non-default case",
        (node = BPT_search_exact(pt, s2p("10.1.2.3/32"))) != NULL &&
        BPT_get_mincovering_pfx(pt, node, res) == 0 &&
        bgpstream_patricia_tree_result_set_count(res) == 1 &&
        (node = bgpstream_patricia_tree_result_set_next(res)) != NULL &&
        (pfxp = BPT_get_pfx(node)) != NULL &&
        bgpstream_pfx_equal(pfxp, s2p("10.0.0.0/8")) != 0);

  CHECK("Patricia Tree v4 default route - default case",
        (node = BPT_search_exact(pt, s2p("192.172.226.78/32"))) != NULL &&
        BPT_get_mincovering_pfx(pt, node, res) == 0 &&
        bgpstream_patricia_tree_result_set_count(res) == 1 &&
        (node = bgpstream_patricia_tree_result_set_next(res)) != NULL &&
        (pfxp = BPT_get_pfx(node)) != NULL &&
        bgpstream_pfx_equal(pfxp, s2p("0.0.0.0/0")) != 0);
  bgpstream_patricia_tree_destroy(pt);
  bgpstream_patricia_tree_result_set_destroy(&res);

  /* Incorrectly masked prefixes */
  pt = bgpstream_patricia_tree_create(NULL);
  res = bgpstream_patricia_tree_result_set_create();
  count4 = 0;

  // simple case: we expect the parent of 10.1.2.3/32 to be 10.1.0.0/16
  INSERT(4, "10.1.2.3/16", ++count4); // should be masked off during insertion
  INSERT(4, "10.1.2.3/32", ++count4);
  // insert a node so that a glue node is created at the root
  INSERT(4, "192.172.226.77/32", ++count4);
  // insert our incorrectly masked "default" route, it should replace the root glue node
  INSERT(4, "192.172.226.78/0", ++count4);
  INSERT(4, "192.172.226.78/32", ++count4);

  CHECK("Patricia Tree v4 - unmasked prefixes",
        (node = BPT_search_exact(pt, s2p("10.1.2.3/32"))) != NULL &&
        BPT_get_mincovering_pfx(pt, node, res) == 0 &&
        bgpstream_patricia_tree_result_set_count(res) == 1 &&
        (node = bgpstream_patricia_tree_result_set_next(res)) != NULL &&
        (pfxp = BPT_get_pfx(node)) != NULL &&
        bgpstream_pfx_equal(pfxp, s2p("10.1.0.0/16")) != 0);

  CHECK("Patricia Tree v4 - unmasked prefixes; replace glue",
        (node = BPT_search_exact(pt, s2p("192.172.226.78/32"))) != NULL &&
        BPT_get_mincovering_pfx(pt, node, res) == 0 &&
        bgpstream_patricia_tree_result_set_count(res) == 1 &&
        (node = bgpstream_patricia_tree_result_set_next(res)) != NULL &&
        (pfxp = BPT_get_pfx(node)) != NULL &&
        bgpstream_pfx_equal(pfxp, s2p("0.0.0.0/0")) != 0);

  bgpstream_patricia_tree_destroy(pt);
  bgpstream_patricia_tree_result_set_destroy(&res);

  return 0;
}

int main()
{
  CHECK_SECTION("Patricia Tree", test_patricia() == 0);
  ENDTEST;
  return 0;
}
