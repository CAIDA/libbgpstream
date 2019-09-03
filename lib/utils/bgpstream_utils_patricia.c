/*
 * BGPStream-specific modifications and improvements are Copyright (C) 2015 The
 * Regents of the University of California.
 *
 * This software is heavily based on software developed by
 * Dave Plonka <plonka@doit.wisc.edu> and released under the following license:
 *
 * Copyright (c) 1997, 1998, 1999
 *
 * The Regents of the University of Michigan ("The Regents") and Merit Network,
 * Inc.  All rights reserved.
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 1.  Redistributions of source code must retain the above
 *     copyright notice, this list of conditions and the
 *     following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the
 *     following disclaimer in the documentation and/or other
 *     materials provided with the distribution.
 * 3.  All advertising materials mentioning features or use of
 *     this software must display the following acknowledgement:
 * This product includes software developed by the University of Michigan, Merit
 * Network, Inc., and their contributors.
 * 4.  Neither the name of the University, Merit Network, nor the
 *     names of their contributors may be used to endorse or
 *     promote products derived from this software without
 *     specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "khash.h" /* << kroundup32 */
#include <assert.h>
#include <limits.h>
#include <netdb.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "bgpstream_log.h"
#include "bgpstream_utils_patricia.h"
#include "bgpstream_utils_pfx.h"
#include "utils.h"

/* for debug purposes */
/* DEBUG static char buffer[1024]; */

#define BGPSTREAM_PATRICIA_MAXBITS 128

// Test the n'th bit in the array of bytes starting at *p.
// In byte 0, most significant bit is 0, least is 7.
#define BIT_ARRAY_TEST(p, n) (((p)[(n) >> 3]) & (0x80 >> ((n) & 0x07)))

enum {
  BGPSTREAM_PATRICIA_SELF,
  BGPSTREAM_PATRICIA_PARENT,
  BGPSTREAM_PATRICIA_CHILD,
  BGPSTREAM_PATRICIA_SIBLING
};

static int comp_with_mask(const void *addr, const void *dest, u_int mask)
{

  if (/* mask/8 == 0 || */ memcmp(addr, dest, mask / 8) == 0) {
    int n = mask / 8;
    int m = ((-1) << (8 - (mask % 8)));

    if (mask % 8 == 0 ||
        (((const u_char *)addr)[n] & m) == (((const u_char *)dest)[n] & m))
      return (1);
  }
  return (0);
}

struct bgpstream_patricia_node {

  /* flag: 0 = glue node, 1 = actual prefix */
  uint8_t actual;

  /* who we are in patricia tree */
  bgpstream_pfx_t prefix;

  /* left and right children */
  bgpstream_patricia_node_t *l;
  bgpstream_patricia_node_t *r;

  /* parent node */
  bgpstream_patricia_node_t *parent;

  /* pointer to user data */
  void *user;
};

struct bgpstream_patricia_tree {

  /* IPv4 tree */
  bgpstream_patricia_node_t *head4;

  /* IPv6 tree */
  bgpstream_patricia_node_t *head6;

  /* Number of nodes per tree */
  uint64_t ipv4_active_nodes;
  uint64_t ipv6_active_nodes;

  /** Pointer to a function that destroys the user structure
   *  in the bgpstream_patricia_node_t structure */
  bgpstream_patricia_tree_destroy_user_t *node_user_destructor;
};

/** Data structure containing a list of pointers to Patricia Tree nodes
 *  that are returned as the result of a computation */
struct bgpstream_patricia_tree_result_set {
  /* resizable array of node pointers */
  bgpstream_patricia_node_t **result_nodes;
  /* number of result nodes*/
  int n_recs;
  /* iterator position */
  int _cursor;
  /* current size of the result nodes array */
  int _alloc_size;
};

/* ======================= UTILITY FUNCTIONS ======================= */

static inline const unsigned char *bgpstream_pfx_get_first_byte(
    const bgpstream_pfx_t *pfx)
{
  return (const unsigned char *)&pfx->address.addr;
}

/* ======================= RESULT SET FUNCTIONS  ======================= */

static int bgpstream_patricia_tree_result_set_add_node(
  bgpstream_patricia_tree_result_set_t *set, bgpstream_patricia_node_t *node)
{
  set->n_recs++;
  /* Realloc if necessary */
  if (set->_alloc_size < set->n_recs) {
    /* round n_recs up to next pow 2 */
    set->_alloc_size = set->n_recs;
    kroundup32(set->_alloc_size);

    if ((set->result_nodes =
           realloc(set->result_nodes, sizeof(bgpstream_patricia_node_t *) *
                                        set->_alloc_size)) == NULL) {
      bgpstream_log(BGPSTREAM_LOG_ERR,
                    "could not realloc result_nodes in result set");
      return -1;
    }
  }
  set->result_nodes[set->n_recs - 1] = node;
  return 0;
}

static inline void bgpstream_patricia_tree_result_set_clear(
  bgpstream_patricia_tree_result_set_t *set)
{
  set->n_recs = 0;
  set->_cursor = 0;
}

/* ======================= PATRICIA NODE FUNCTIONS ======================= */

static bgpstream_patricia_node_t *
bgpstream_patricia_node_create(bgpstream_patricia_tree_t *pt,
                               const bgpstream_pfx_t *pfx)
{
  bgpstream_patricia_node_t *node;

  assert(pfx);
  assert(pfx->mask_len <= BGPSTREAM_PATRICIA_MAXBITS);
  assert(pfx->address.version != BGPSTREAM_ADDR_VERSION_UNKNOWN);

  if ((node = malloc_zero(sizeof(bgpstream_patricia_node_t))) == NULL) {
    return NULL;
  }

  if (pfx->address.version == BGPSTREAM_ADDR_VERSION_IPV4) {
    pt->ipv4_active_nodes++;
  } else {
    pt->ipv6_active_nodes++;
  }

  bgpstream_pfx_copy(&node->prefix, pfx);

  node->parent = NULL;
  node->actual = 1;
  node->l = NULL;
  node->r = NULL;
  node->user = NULL;
  return node;
}

static bgpstream_patricia_node_t *bgpstream_patricia_gluenode_create(
  const bgpstream_pfx_t *pfx, uint8_t mask_len)
{
  bgpstream_patricia_node_t *node;

  if ((node = malloc_zero(sizeof(bgpstream_patricia_node_t))) == NULL) {
    return NULL;
  }
  bgpstream_addr_copy(&node->prefix.address, &pfx->address);
  bgpstream_addr_mask(&node->prefix.address, mask_len);
  node->prefix.mask_len = mask_len;
  node->parent = NULL;
  node->actual = 0;
  node->l = NULL;
  node->r = NULL;
  return node;
}

/* ======================= PATRICIA TREE FUNCTIONS ======================= */

#define bgpstream_patricia_get_head(pt, v)          \
  ((v) == BGPSTREAM_ADDR_VERSION_IPV4 ? pt->head4 : \
   (v) == BGPSTREAM_ADDR_VERSION_IPV6 ? pt->head6 : \
   NULL)

static void bgpstream_patricia_set_head(bgpstream_patricia_tree_t *pt,
                                        bgpstream_addr_version_t v,
                                        bgpstream_patricia_node_t *n)
{
  switch (v) {
  case BGPSTREAM_ADDR_VERSION_IPV4:
    pt->head4 = n;
    break;
  case BGPSTREAM_ADDR_VERSION_IPV6:
    pt->head6 = n;
    break;
  default:
    assert(0);
  }
}

static uint64_t
bgpstream_patricia_tree_count_subnets(const bgpstream_patricia_node_t *node,
                                      uint64_t subnet_size)
{
  if (node == NULL) {
    return 0;
  }
  /* if the node is a glue node, then the /subnet_size subnets are the sum of
   * the
   * /24 subnets contained in its left and right subtrees */
  if (!node->actual) {
    /* if the glue node is already a /subnet_size, then just return 1 (even
     * though
     * the subnetworks below could be a non complete /subnet_size */
    if (node->prefix.mask_len >= subnet_size) {
      return 1;
    } else {
      return bgpstream_patricia_tree_count_subnets(node->l, subnet_size) +
             bgpstream_patricia_tree_count_subnets(node->r, subnet_size);
    }
  } else {
    /* otherwise we just count the subnet for the given network and return
     * we don't need to go deeper in the tree (everything else beyond this
     * point is covered */

    /* compute how many /subnet_size are in this prefix */
    if (node->prefix.mask_len >= subnet_size) {
      return 1;
    } else {
      uint8_t diff = subnet_size - node->prefix.mask_len;
      if (diff == 64) {
        return UINT64_MAX;
      } else {
        return (uint64_t)1 << diff;
      }
    }
  }
}

/* depth pecifies how many "children" to explore for each node */
static int bgpstream_patricia_tree_add_more_specifics(
  bgpstream_patricia_tree_result_set_t *set, bgpstream_patricia_node_t *node,
  const uint8_t depth)
{
  if (node == NULL || depth == 0) {
    return 0;
  }
  uint8_t d = depth;
  /* if it is a node containing a real prefix, then copy the address to a new
   * result node */
  if (node->actual) {
    if (bgpstream_patricia_tree_result_set_add_node(set, node) != 0) {
      return -1;
    }
    d--;
  }

  /* using pre-order R - Left - Right */
  if (bgpstream_patricia_tree_add_more_specifics(set, node->l, d) != 0) {
    return -1;
  }
  if (bgpstream_patricia_tree_add_more_specifics(set, node->r, d) != 0) {
    return -1;
  }
  return 0;
}

/* depth pecifies how many "children" to explore for each node */
static int bgpstream_patricia_tree_add_less_specifics(
  bgpstream_patricia_tree_result_set_t *set, bgpstream_patricia_node_t *node,
  const uint8_t depth)
{
  if (node == NULL) {
    return 0;
  }
  uint8_t d = depth;
  while (node != NULL && d > 0) {
    /* if it is a node containing a real prefix, then copy the address to a new
     * result node */
    if (node->actual) {
      if (bgpstream_patricia_tree_result_set_add_node(set, node) != 0) {
        return -1;
      }
      d--;
    }
    node = node->parent;
  }
  return 0;
}

static int
bgpstream_patricia_tree_find_more_specific(const bgpstream_patricia_node_t *node)
{
  if (node == NULL) {
    return 0;
  }

  /* Does this node or one of its descendants contains a real prefix? */
  return node->actual ||
    bgpstream_patricia_tree_find_more_specific(node->l) ||
    bgpstream_patricia_tree_find_more_specific(node->r);
}

static void bgpstream_patricia_tree_merge_tree(bgpstream_patricia_tree_t *dst,
    const bgpstream_patricia_node_t *node)
{
  if (node == NULL) {
    return;
  }
  /* Add the current node, if it is not a glue node */
  if (node->actual) {
    bgpstream_patricia_tree_insert(dst, &node->prefix);
  }
  /* Recursively add left and right node */
  bgpstream_patricia_tree_merge_tree(dst, node->l);
  bgpstream_patricia_tree_merge_tree(dst, node->r);
}

static bgpstream_patricia_walk_cb_result_t bpt_walk_children(
  const bgpstream_patricia_tree_t *pt, const bgpstream_patricia_node_t *node,
  bgpstream_patricia_tree_process_node_t *fun, void *data)
{
  bgpstream_patricia_walk_cb_result_t rc;

  if (node == NULL)
    return BGPSTREAM_PATRICIA_WALK_CONTINUE;

  /* In order traversal: Left - Node - Right */

  /* Left */
  rc = bpt_walk_children(pt, node->l, fun, data);
  if (rc != BGPSTREAM_PATRICIA_WALK_CONTINUE) return rc;

  /* Node */
  if (node->actual) {
    rc = fun(pt, node, data);
    if (rc != BGPSTREAM_PATRICIA_WALK_CONTINUE) return rc;
  }

  /* Right */
  rc = bpt_walk_children(pt, node->r, fun, data);
  if (rc != BGPSTREAM_PATRICIA_WALK_CONTINUE) return rc;

  return BGPSTREAM_PATRICIA_WALK_CONTINUE;
}

static bgpstream_patricia_walk_cb_result_t bpt_walk_parents(
  const bgpstream_patricia_tree_t *pt, const bgpstream_patricia_node_t *node,
  bgpstream_patricia_tree_process_node_t *fun, void *data)
{
  bgpstream_patricia_walk_cb_result_t rc;
  for ( ; node; node = node->parent) {
    if (node->actual) {
      rc = fun(pt, node, data);
      if (rc != BGPSTREAM_PATRICIA_WALK_CONTINUE) return rc;
    }
  }
  return BGPSTREAM_PATRICIA_WALK_CONTINUE;
}

static void bgpstream_patricia_tree_print_tree(
    const bgpstream_patricia_node_t *node)
{
  if (node == NULL) {
    return;
  }
  bgpstream_patricia_tree_print_tree(node->l);

  char buffer[INET6_ADDRSTRLEN+4];

  /* if node is not a glue node, print the prefix */
  if (node->actual) {
    bgpstream_pfx_snprintf(buffer, sizeof(buffer), &node->prefix);
    fprintf(stdout, "%*s%s\n", node->prefix.mask_len, "", buffer);
  }

  bgpstream_patricia_tree_print_tree(node->r);
}

static void
bgpstream_patricia_tree_destroy_tree(bgpstream_patricia_tree_t *pt,
                                     bgpstream_patricia_node_t *head)
{
  if (head != NULL) {
    bgpstream_patricia_node_t *l = head->l;
    bgpstream_patricia_node_t *r = head->r;
    bgpstream_patricia_tree_destroy_tree(pt, l);
    bgpstream_patricia_tree_destroy_tree(pt, r);
    if (head->user != NULL && pt->node_user_destructor != NULL) {
      pt->node_user_destructor(head->user);
    }
    free(head);
  }
}

/* ======================= PUBLIC API FUNCTIONS ======================= */

bgpstream_patricia_tree_result_set_t *
bgpstream_patricia_tree_result_set_create()
{
  bgpstream_patricia_tree_result_set_t *set;

  if ((set = malloc_zero(sizeof(bgpstream_patricia_tree_result_set_t))) ==
      NULL) {
    fprintf(stderr,
            "Error: could not create bgpstream_patricia_tree_result_set\n");
    return NULL;
  }

  /* always have space for a single node  */
  if (bgpstream_patricia_tree_result_set_add_node(set, NULL) != 0) {
    free(set);
    return NULL;
  }

  bgpstream_patricia_tree_result_set_clear(set);
  return set;
}

void bgpstream_patricia_tree_result_set_destroy(
  bgpstream_patricia_tree_result_set_t **set_p)
{
  assert(set_p);
  bgpstream_patricia_tree_result_set_t *set = *set_p;
  if (set != NULL) {
    free(set->result_nodes);
    set->result_nodes = NULL;
    set->n_recs = 0;
    set->_cursor = 0;
    set->_alloc_size = 0;
    free(set);
    *set_p = NULL;
  }
}

void bgpstream_patricia_tree_result_set_rewind(
  bgpstream_patricia_tree_result_set_t *set)
{
  set->_cursor = 0;
}

bgpstream_patricia_node_t *bgpstream_patricia_tree_result_set_next(
  bgpstream_patricia_tree_result_set_t *set)
{
  if (set->n_recs <= set->_cursor) {
    /* No more nodes */
    return NULL;
  }
  return set->result_nodes[set->_cursor++]; /* Advance head */
}

int bgpstream_patricia_tree_result_set_count(
  const bgpstream_patricia_tree_result_set_t *set)
{
  return set->n_recs;
}

void bgpstream_patricia_tree_result_set_print(
  bgpstream_patricia_tree_result_set_t *set)
{
  bgpstream_patricia_tree_result_set_rewind(set);
  bgpstream_patricia_node_t *next;
  char buffer[1024];
  while ((next = bgpstream_patricia_tree_result_set_next(set)) != NULL) {
    bgpstream_pfx_snprintf(buffer, 1024, &next->prefix);
    fprintf(stdout, "%s\n", buffer);
  }
}

bgpstream_patricia_tree_t *bgpstream_patricia_tree_create(
  bgpstream_patricia_tree_destroy_user_t *bspt_user_destructor)
{
  bgpstream_patricia_tree_t *pt = NULL;
  if ((pt = malloc_zero(sizeof(bgpstream_patricia_tree_t))) == NULL) {
    return NULL;
  }
  pt->head4 = NULL;
  pt->head6 = NULL;
  pt->ipv4_active_nodes = 0;
  pt->ipv6_active_nodes = 0;
  pt->node_user_destructor = bspt_user_destructor;
  return pt;
}

/* Search below node for another node with the same branching bits as pfx, and
 * return
 *   - a node with the same len, if one exists
 *   - or, a node with a longer len, if one exists
 *   - or, a node with a shorter len
 * The bit length of the returned node can be used to determine which type was
 * returned.
 */
static const bgpstream_patricia_node_t *
bpt_search_node(const bgpstream_patricia_node_t *node,
                const bgpstream_pfx_t *pfx)
{
  const unsigned char *addr = bgpstream_pfx_get_first_byte(pfx);
  while (node->prefix.mask_len < pfx->mask_len) {
    if (BIT_ARRAY_TEST(addr, node->prefix.mask_len)) {
      /* patricia_lookup: take right at node */
      if (!node->r) return node;
      node = node->r;
    } else {
      /* patricia_lookup: take left at node */
      if (!node->l) return node;
      node = node->l;
    }
  }
  return node;
}

static const bgpstream_patricia_node_t *
bpt_find_insert_point_const(const bgpstream_patricia_node_t *node_it,
                            const bgpstream_pfx_t *pfx,
                            int *relation,
                            uint8_t *differ_bit_p)
{
  node_it = bpt_search_node(node_it, pfx);

  uint8_t bitlen = pfx->mask_len;
  const unsigned char *paddr = bgpstream_pfx_get_first_byte(pfx);
  const unsigned char *naddr = bgpstream_pfx_get_first_byte(&node_it->prefix);

  /* find the first bit different */
  int i, j, r;
  uint8_t check_bit = (node_it->prefix.mask_len < bitlen) ?
    node_it->prefix.mask_len : bitlen;
  uint8_t differ_bit = 0;
  for (i = 0; i * 8 < check_bit; i++) {
    if ((r = (paddr[i] ^ naddr[i])) == 0) {
      differ_bit = (i + 1) * 8;
      continue;
    }
    /* I know the better way, but for now */
    for (j = 0; j < 8; j++) {
      if (r & (0x80 >> j)) {
        break;
      }
    }
    /* must be found */
    assert(j < 8);
    differ_bit = i * 8 + j;
    break;
  }

  if (differ_bit > check_bit) {
    differ_bit = check_bit;
  }

  /* go back up until we find the parent with all the same leading bits */
  while (node_it->parent && node_it->parent->prefix.mask_len >= differ_bit) {
    node_it = node_it->parent;
  }

  if (differ_bit == bitlen && node_it->prefix.mask_len == bitlen) {
    /* pfx should be AT node_it */
    *relation = BGPSTREAM_PATRICIA_SELF;

  } else if (node_it->prefix.mask_len == differ_bit) {
    /* pfx should be a CHILD of node_it (and have no children of its own) */
    *relation = BGPSTREAM_PATRICIA_PARENT;

  } else if (bitlen == differ_bit) {
    /* pfx should be a PARENT of node_it */
    *relation = BGPSTREAM_PATRICIA_CHILD;

  } else {
    /* pfx should be a SIBLING of node_it, under a new GLUE NODE */
    *relation = BGPSTREAM_PATRICIA_SIBLING;
  }
  *differ_bit_p = differ_bit;
  return node_it;
}

static inline bgpstream_patricia_node_t *
bpt_find_insert_point(bgpstream_patricia_node_t *node_it,
                      const bgpstream_pfx_t *pfx,
                      int *relation,
                      uint8_t *differ_bit_p)
{
  return bgpstream_nonconst_node(bpt_find_insert_point_const(
    node_it, pfx, relation, differ_bit_p));
}

bgpstream_patricia_node_t *
bgpstream_patricia_tree_insert(bgpstream_patricia_tree_t *pt,
                               const bgpstream_pfx_t *pfx)
{
  assert(pt);
  assert(pfx);
  assert(pfx->mask_len <= BGPSTREAM_PATRICIA_MAXBITS);
  assert(pfx->address.version != BGPSTREAM_ADDR_VERSION_UNKNOWN);

  /* DEBUG   char buffer[1024];
   * bgpstream_pfx_snprintf(buffer, 1024, pfx); */

  bgpstream_patricia_node_t *new_node = NULL;
  bgpstream_addr_version_t v = pfx->address.version;
  bgpstream_patricia_node_t *node_it = bgpstream_patricia_get_head(pt, v);

  /* if Patricia Tree is empty, then insert new node */
  if (node_it == NULL) {
    if ((new_node = bgpstream_patricia_node_create(pt, pfx)) == NULL) {
      bgpstream_log(BGPSTREAM_LOG_ERR, "Error creating pt node");
      return NULL;
    }
    /* attach first node in Tree */
    bgpstream_patricia_set_head(pt, v, new_node);
    /* DEBUG       fprintf(stderr, "Adding %s to HEAD\n", buffer); */
    return new_node;
  }

  /* Find insertion point */
  int relation;
  uint8_t differ_bit;

  node_it = bpt_find_insert_point(node_it, pfx, &relation, &differ_bit);

  uint8_t bitlen = pfx->mask_len;
  if (relation == BGPSTREAM_PATRICIA_SELF) {
    /* check the node contains an actual prefix,
     * i.e. it is not a glue node */
    if (node_it->actual) {
      /* Exact node found */
      /* DEBUG  fprintf(stderr, "Prefix %s already in tree\n", buffer); */
      return node_it;
    }
    /* otherwise replace the info in the glue node with proper
     * prefix information and increment the right counter*/
    assert(bgpstream_pfx_equal(&node_it->prefix, pfx));
    node_it->actual = 1;
    if (pfx->address.version == BGPSTREAM_ADDR_VERSION_IPV4) {
      pt->ipv4_active_nodes++;
    } else {
      pt->ipv6_active_nodes++;
    }

    /* patricia_lookup: new node #1 (glue mod) */
    /* DEBUG fprintf(stderr, "Using %s to replace a GLUE node\n", buffer); */
    return node_it;
  }

  /* Create a new node */
  if ((new_node = bgpstream_patricia_node_create(pt, pfx)) == NULL) {
    bgpstream_log(BGPSTREAM_LOG_ERR, "Error creating pt node");
    return NULL;
  }

  /* Insert the new node in the Patricia Tree: CHILD */
  if (relation == BGPSTREAM_PATRICIA_PARENT) {
    /* appending the new node as a child of node_it */
    const unsigned char *paddr = bgpstream_pfx_get_first_byte(pfx);
    new_node->parent = node_it;
    if (node_it->prefix.mask_len < BGPSTREAM_PATRICIA_MAXBITS &&
        BIT_ARRAY_TEST(paddr, node_it->prefix.mask_len)) {
      assert(node_it->r == NULL);
      node_it->r = new_node;
    } else {
      assert(node_it->l == NULL);
      node_it->l = new_node;
    }
    /* patricia_lookup: new_node #2 (child) */
    /* DEBUG  fprintf(stderr, "Adding %s as a CHILD node\n", buffer); */
    return new_node;
  }

  /* Insert the new node in the Patricia Tree: PARENT */
  if (relation == BGPSTREAM_PATRICIA_CHILD) {
    /* attaching the new node as a parent of node_it */
    const unsigned char *naddr = bgpstream_pfx_get_first_byte(&node_it->prefix);
    if (bitlen < BGPSTREAM_PATRICIA_MAXBITS &&
        BIT_ARRAY_TEST(naddr, bitlen)) {
      new_node->r = node_it;
    } else {
      new_node->l = node_it;
    }
    new_node->parent = node_it->parent;
    if (node_it->parent == NULL) {
      assert(bgpstream_patricia_get_head(pt, v) == node_it);
      bgpstream_patricia_set_head(pt, v, new_node);
    } else {
      if (node_it->parent->r == node_it) {
        node_it->parent->r = new_node;
      } else {
        node_it->parent->l = new_node;
      }
    }
    node_it->parent = new_node;
    /* patricia_lookup: new_node #3 (parent) */
    /* DEBUG fprintf(stderr, "Adding %s as a PARENT node\n", buffer); */
    return new_node;

  } else /* BGPSTREAM_PATRICIA_SIBLING */ {
    /* Insert the new node in the Patricia Tree: CREATE A GLUE NODE AND APPEND
     * TO IT*/

    bgpstream_patricia_node_t *glue_node =
      bgpstream_patricia_gluenode_create(pfx, differ_bit);

    glue_node->parent = node_it->parent;

    const unsigned char *paddr = bgpstream_pfx_get_first_byte(pfx);
    if (differ_bit < BGPSTREAM_PATRICIA_MAXBITS &&
        BIT_ARRAY_TEST(paddr, differ_bit)) {
      glue_node->r = new_node;
      glue_node->l = node_it;
    } else {
      glue_node->r = node_it;
      glue_node->l = new_node;
    }
    new_node->parent = glue_node;

    if (node_it->parent == NULL) {
      assert(bgpstream_patricia_get_head(pt, v) == node_it);
      bgpstream_patricia_set_head(pt, v, glue_node);
    } else {
      if (node_it->parent->r == node_it) {
        node_it->parent->r = glue_node;
      } else {
        node_it->parent->l = glue_node;
      }
    }
    node_it->parent = glue_node;
    /* "patricia_lookup: new_node #4 (glue+node) */
    /* DEBUG fprintf(stderr, "Adding %s as a CHILD of a NEW GLUE node\n",
     * buffer); */
    return new_node;
  }

  /* DEBUG   fprintf(stderr, "Adding %s as a ??\n", buffer); */
  /* return new_node; */
}

void bgpstream_patricia_tree_walk_up_down(
    const bgpstream_patricia_tree_t *pt,
    const bgpstream_pfx_t *pfx,
    bgpstream_patricia_tree_process_node_t *exact_fun,
    bgpstream_patricia_tree_process_node_t *parent_fun,
    bgpstream_patricia_tree_process_node_t *child_fun,
    void *data)
{
  bgpstream_addr_version_t v = pfx->address.version;
  const bgpstream_patricia_node_t *node_it = bgpstream_patricia_get_head(pt, v);

  if (!node_it) {
    // Tree is empty
    return;
  }

  // Find insertion point
  int relation;
  uint8_t differ_bit; // unused
  node_it = bpt_find_insert_point_const(node_it, pfx, &relation, &differ_bit);
  bgpstream_patricia_walk_cb_result_t rc;

  // Walk parents and/or children of the insertion point
  if (relation == BGPSTREAM_PATRICIA_SELF) {
    if (node_it->actual) {
      if (exact_fun) {
        rc = exact_fun(pt, node_it, data);
        if (rc == BGPSTREAM_PATRICIA_WALK_END_ALL) return;
      }
    }
    if (parent_fun) {
      rc = bpt_walk_parents(pt, node_it->parent, parent_fun, data);
      if (rc == BGPSTREAM_PATRICIA_WALK_END_ALL) return;
    }
    if (child_fun) {
      rc = bpt_walk_children(pt, node_it->l, child_fun, data);
      if (rc != BGPSTREAM_PATRICIA_WALK_CONTINUE) return;
      rc = bpt_walk_children(pt, node_it->r, child_fun, data);
    }

  } else if (relation == BGPSTREAM_PATRICIA_PARENT) {
    if (parent_fun) {
      bpt_walk_parents(pt, node_it, parent_fun, data);
    }

  } else if (relation == BGPSTREAM_PATRICIA_CHILD) {
    if (parent_fun) {
      rc = bpt_walk_parents(pt, node_it->parent, parent_fun, data);
      if (rc == BGPSTREAM_PATRICIA_WALK_END_ALL) return;
    }
    if (child_fun) {
      bpt_walk_children(pt, node_it, child_fun, data);
    }

  } else if (relation == BGPSTREAM_PATRICIA_SIBLING) {
    if (parent_fun) {
      bpt_walk_parents(pt, node_it->parent, parent_fun, data);
    }
  }
}

void *bgpstream_patricia_tree_get_user(bgpstream_patricia_node_t *node)
{
  return node->user;
}

int bgpstream_patricia_tree_set_user(bgpstream_patricia_tree_t *pt,
                                     bgpstream_patricia_node_t *node,
                                     void *user)
{
  if (node->user == user) {
    return 0;
  }
  if (node->user != NULL && pt->node_user_destructor != NULL) {
    pt->node_user_destructor(node->user);
  }
  node->user = user;
  return 1;
}

static bgpstream_patricia_walk_cb_result_t set_exact(
    const bgpstream_patricia_tree_t *pt, const bgpstream_patricia_node_t *node,
    void *data)
{
  *(uint8_t *)data |= BGPSTREAM_PATRICIA_EXACT_MATCH;
  return BGPSTREAM_PATRICIA_WALK_END_DIRECTION;
}

static bgpstream_patricia_walk_cb_result_t set_less_specific(
    const bgpstream_patricia_tree_t *pt, const bgpstream_patricia_node_t *node,
    void *data)
{
  *(uint8_t *)data |= BGPSTREAM_PATRICIA_LESS_SPECIFICS;
  return BGPSTREAM_PATRICIA_WALK_END_DIRECTION;
}

static bgpstream_patricia_walk_cb_result_t set_more_specific(
    const bgpstream_patricia_tree_t *pt, const bgpstream_patricia_node_t *node,
    void *data)
{
  *(uint8_t *)data |= BGPSTREAM_PATRICIA_MORE_SPECIFICS;
  return BGPSTREAM_PATRICIA_WALK_END_DIRECTION;
}

uint8_t
bgpstream_patricia_tree_get_pfx_overlap_info(
    const bgpstream_patricia_tree_t *pt, const bgpstream_pfx_t *pfx)
{
  uint8_t result = 0;
  bgpstream_patricia_tree_walk_up_down(pt, pfx, set_exact, set_less_specific,
      set_more_specific, &result);
  return result;
}

void bgpstream_patricia_tree_remove(bgpstream_patricia_tree_t *pt,
                                    const bgpstream_pfx_t *pfx)
{
  bgpstream_patricia_tree_remove_node(pt,
    bgpstream_patricia_tree_search_exact(pt, pfx));
}

void bgpstream_patricia_tree_remove_node(bgpstream_patricia_tree_t *pt,
                                         bgpstream_patricia_node_t *node)
{
  assert(pt);
  if (node == NULL) {
    return;
  }

  bgpstream_addr_version_t v = node->prefix.address.version;
  bgpstream_patricia_node_t *parent;
  bgpstream_patricia_node_t *child;

  uint64_t *num_active_node = (v == BGPSTREAM_ADDR_VERSION_IPV6) ?
    &pt->ipv6_active_nodes : &pt->ipv4_active_nodes;

  /* we do not allow for explicit removal of glue nodes */
  if (!node->actual) {
    return;
  }

  if (node->user != NULL) {
    if (pt->node_user_destructor != NULL) {
      pt->node_user_destructor(node->user);
    }
    node->user = NULL;
  }

  /* if node has both children */
  if (node->r != NULL && node->l != NULL) {
    /* if it is a glue node, there is nothing to remove,
     * if it is node with a valid prefix, then it becomes a glue node
     */
    node->actual = 0;
    /* node data remains, unless we decide to pass a destroy function somewehere
     */
    /* node->user = NULL; */
    /* DEBUG fprintf(stderr, "Removing node with both children\n"); */
    return;
  }

  /* if node has no children */
  if (node->r == NULL && node->l == NULL) {
    parent = node->parent;
    free(node);
    (*num_active_node) = (*num_active_node) - 1;

    /* removing head of tree */
    if (parent == NULL) {
      assert(node == bgpstream_patricia_get_head(pt, v));
      bgpstream_patricia_set_head(pt, v, NULL);
      /* DEBUG fprintf(stderr, "Removing head (that had no children)\n"); */
      return;
    }

    /* check if the node was the right or the left child */
    if (parent->r == node) {
      parent->r = NULL;
      child = parent->l;
    } else {
      assert(parent->l == node);
      parent->l = NULL;
      child = parent->r;
    }

    /* if the current parent was a valid prefix, return */
    if (parent->actual) {
      /* DEBUG fprintf(stderr, "Removing node with no children\n"); */
      return;
    }

    /* otherwise it makes no sense to have a glue node
     * with only one child, the parent has to be removed */

    if (parent->parent == NULL) { /* if the parent parent is the head, then
                                   * attach the only child directly */
      assert(parent == bgpstream_patricia_get_head(pt, v));
      bgpstream_patricia_set_head(pt, v, child);
    } else {
      if (parent->parent->r == parent) { /* if the parent is a right child */
        parent->parent->r = child;
      } else { /* if the parent is a left child */
        assert(parent->parent->l == parent);
        parent->parent->l = child;
      }
    }
    /* the child parent, is now the grand-parent */
    child->parent = parent->parent;
    free(parent);
    return;
  }

  /* if node has only one child */
  if (node->r) {
    child = node->r;
  } else {
    assert(node->l);
    child = node->l;
  }
  /* the child parent, is now the grand-parent */
  parent = node->parent;
  child->parent = parent;

  free(node);
  (*num_active_node) = (*num_active_node) - 1;

  if (parent == NULL) { /* if the parent is the head, then attach
                         * the only child directly */
    assert(node == bgpstream_patricia_get_head(pt, v));
    bgpstream_patricia_set_head(pt, v, child);
    return;
  } else {
    /* attach child node to the correct parent child pointer */
    if (parent->r == node) { /* if node was a right child */
      parent->r = child;
    } else { /* if node was a left child */
      assert(parent->l == node);
      parent->l = child;
    }
  }
}

const bgpstream_patricia_node_t *
bgpstream_patricia_tree_search_exact_const(const bgpstream_patricia_tree_t *pt,
                                           const bgpstream_pfx_t *pfx)
{
  assert(pt);
  assert(pfx);
  assert(pfx->mask_len <= BGPSTREAM_PATRICIA_MAXBITS);
  assert(pfx->address.version != BGPSTREAM_ADDR_VERSION_UNKNOWN);

  bgpstream_addr_version_t v = pfx->address.version;
  const bgpstream_patricia_node_t *node = bgpstream_patricia_get_head(pt, v);

  /* if Patricia Tree is empty*/
  if (node == NULL) {
    return NULL;
  }
  uint8_t bitlen = pfx->mask_len;

  node = bpt_search_node(node, pfx);

  // if node has the wrong length, or is a glue node, then no exact match
  if (node->prefix.mask_len != bitlen || !node->actual) {
    return NULL;
  }

  /* compare the prefixes bit by bit */
  if (comp_with_mask(
        bgpstream_pfx_get_first_byte(&node->prefix),
        bgpstream_pfx_get_first_byte(pfx), bitlen)) {
    /* exact match found */
    return node;
  }
  return NULL;
}

uint64_t bgpstream_patricia_prefix_count(const bgpstream_patricia_tree_t *pt,
                                         bgpstream_addr_version_t v)
{
  switch (v) {
  case BGPSTREAM_ADDR_VERSION_IPV4:
    return pt->ipv4_active_nodes;
  case BGPSTREAM_ADDR_VERSION_IPV6:
    return pt->ipv6_active_nodes;
  default:
    return 0;
  }
}

uint64_t bgpstream_patricia_tree_count_24subnets(
    const bgpstream_patricia_tree_t *pt)
{
  return bgpstream_patricia_tree_count_subnets(pt->head4, 24);
}

uint64_t bgpstream_patricia_tree_count_64subnets(
    const bgpstream_patricia_tree_t *pt)
{
  return bgpstream_patricia_tree_count_subnets(pt->head6, 64);
}

int bgpstream_patricia_tree_get_more_specifics(
  bgpstream_patricia_tree_t *pt, bgpstream_patricia_node_t *node,
  bgpstream_patricia_tree_result_set_t *results)
{
  bgpstream_patricia_tree_result_set_clear(results);

  if (node != NULL) { /* we do not return the node itself */
    if (bgpstream_patricia_tree_add_more_specifics(
          results, node->l, BGPSTREAM_PATRICIA_MAXBITS + 1) != 0) {
      return -1;
    }
    if (bgpstream_patricia_tree_add_more_specifics(
          results, node->r, BGPSTREAM_PATRICIA_MAXBITS + 1) != 0) {
      return -1;
    }
  }
  return 0;
}

int bgpstream_patricia_tree_get_mincovering_prefix(
  bgpstream_patricia_tree_t *pt, bgpstream_patricia_node_t *node,
  bgpstream_patricia_tree_result_set_t *results)
{
  bgpstream_patricia_tree_result_set_clear(results);

  if (node == NULL) {
    return 0;
  }
  /* we do not return the node itself (that's why we pass the parent node) */
  return bgpstream_patricia_tree_add_less_specifics(results, node->parent, 1);
}

int bgpstream_patricia_tree_get_less_specifics(
  bgpstream_patricia_tree_t *pt, bgpstream_patricia_node_t *node,
  bgpstream_patricia_tree_result_set_t *results)
{
  bgpstream_patricia_tree_result_set_clear(results);

  if (node == NULL) {
    return 0;
  }
  /* we do not return the node itself (that's why we pass the parent node) */
  return bgpstream_patricia_tree_add_less_specifics(
    results, node->parent, BGPSTREAM_PATRICIA_MAXBITS + 1);
}

int bgpstream_patricia_tree_get_minimum_coverage(
  bgpstream_patricia_tree_t *pt, bgpstream_addr_version_t v,
  bgpstream_patricia_tree_result_set_t *results)
{
  bgpstream_patricia_tree_result_set_clear(results);
  bgpstream_patricia_node_t *head = bgpstream_patricia_get_head(pt, v);
  /* we stop at the first layer, hence depth = 1 */
  return bgpstream_patricia_tree_add_more_specifics(results, head, 1);
}

uint8_t
bgpstream_patricia_tree_get_node_overlap_info(
    const bgpstream_patricia_tree_t *pt, const bgpstream_patricia_node_t *node)
{
  uint8_t mask = BGPSTREAM_PATRICIA_EXACT_MATCH;

  const bgpstream_patricia_node_t *node_it = node->parent;
  while (node_it != NULL) {
    if (node_it->actual) {
      /* one less specific found */
      mask = mask | BGPSTREAM_PATRICIA_LESS_SPECIFICS;
      break;
    }
    node_it = node_it->parent;
  }

  node_it = node;
  if (node_it != NULL) { /* we do not consider the node itself */
    if (bgpstream_patricia_tree_find_more_specific(node->l) ||
        bgpstream_patricia_tree_find_more_specific(node->r)) {
        mask = mask | BGPSTREAM_PATRICIA_MORE_SPECIFICS;
    }
  }
  return mask;
}

void bgpstream_patricia_tree_merge(bgpstream_patricia_tree_t *dst,
                                   const bgpstream_patricia_tree_t *src)
{
  assert(dst);
  if (src == NULL) {
    return;
  }
  /* Merge IPv4 */
  bgpstream_patricia_tree_merge_tree(dst, src->head4);
  /* Merge IPv6 */
  bgpstream_patricia_tree_merge_tree(dst, src->head6);
}

void bgpstream_patricia_tree_walk(const bgpstream_patricia_tree_t *pt,
                                  bgpstream_patricia_tree_process_node_t *fun,
                                  void *data)
{
  bpt_walk_children(pt, pt->head4, fun, data);
  bpt_walk_children(pt, pt->head6, fun, data);
}

void bgpstream_patricia_tree_print(const bgpstream_patricia_tree_t *pt)
{
  bgpstream_patricia_tree_print_tree(pt->head4);
  bgpstream_patricia_tree_print_tree(pt->head6);
}

const bgpstream_pfx_t *
bgpstream_patricia_tree_get_pfx(const bgpstream_patricia_node_t *node)
{
  assert(node);
  if (node->actual) {
    return &node->prefix;
  }
  return NULL;
}

void bgpstream_patricia_tree_clear(bgpstream_patricia_tree_t *pt)
{
  assert(pt);

  bgpstream_patricia_tree_destroy_tree(pt, pt->head4);
  pt->ipv4_active_nodes = 0;
  pt->head4 = NULL;

  bgpstream_patricia_tree_destroy_tree(pt, pt->head6);
  pt->ipv6_active_nodes = 0;
  pt->head6 = NULL;
}

void bgpstream_patricia_tree_destroy(bgpstream_patricia_tree_t *pt)
{
  if (pt != NULL) {
    bgpstream_patricia_tree_clear(pt);
    free(pt);
  }
}
