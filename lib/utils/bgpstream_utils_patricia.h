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

#ifndef __BGPSTREAM_UTILS_PATRICIA_H
#define __BGPSTREAM_UTILS_PATRICIA_H

#include <arpa/inet.h>
#include <limits.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <sys/socket.h>

#include "bgpstream_utils_pfx.h"

/** @file
 *
 * @brief Header file that exposes the public interface of BGP Stream Patricia
 * Tree
 * objects
 *
 * @author Chiara Orsini
 *
 */

#define BGPSTREAM_PATRICIA_LESS_SPECIFICS 0b0100
#define BGPSTREAM_PATRICIA_EXACT_MATCH 0b0010
#define BGPSTREAM_PATRICIA_MORE_SPECIFICS 0b0001

/**
 * @name Opaque Data Structures
 *
 * @{ */

/** Opaque structure containing a Patricia Tree node instance */
typedef struct bgpstream_patricia_node bgpstream_patricia_node_t;

/** Opaque structure containing a Patricia Tree instance */
typedef struct bgpstream_patricia_tree bgpstream_patricia_tree_t;

/** Opaque structure containing a Patricia Tree results set */
typedef struct bgpstream_patricia_tree_result_set
  bgpstream_patricia_tree_result_set_t;

/** @} */

/**
 * @name Public Data Structures
 *
 * @{ */

/** Callback for destroying a custom user structure associated with a
 *  patricia tree node
 * @param user    user pointer to destroy
 */
typedef void(bgpstream_patricia_tree_destroy_user_t)(void *user);

/** Callback for custom processing of the bgpstream_patricia_node structures
 *  when traversing a patricia tree
 *
 * @param node    pointer to the node
 * @param data    user pointer to a data structure that can be used by the
 *                user to store a state
 */
typedef void(bgpstream_patricia_tree_process_node_t)(
  bgpstream_patricia_tree_t *pt, bgpstream_patricia_node_t *node, void *data);

/** @} */

/**
 * @name Public API Functions
 *
 * @{ */

/** Initialize a new result set instance
 *
 * @return the result set instance created, NULL if an error occurs
 */
bgpstream_patricia_tree_result_set_t *
bgpstream_patricia_tree_result_set_create(void);

/** Free a result set instance
 *
 * @param this_p        The pointer to the result set instance to free
 */
void bgpstream_patricia_tree_result_set_destroy(
  bgpstream_patricia_tree_result_set_t **set_p);

/** Move the result set iterator pointer to the the beginning
 *  (so that next returns the first element)
 *
 * @param this          The result set instance
 */
void bgpstream_patricia_tree_result_set_rewind(
  bgpstream_patricia_tree_result_set_t *set);

/** Get the next result in the result set iterator
 *
 * @param this          The result set instance
 * @return a pointer to the result
 */
bgpstream_patricia_node_t *bgpstream_patricia_tree_result_set_next(
  bgpstream_patricia_tree_result_set_t *set);

/** Count the number of results in the list
 *
 * @param result      pointer to the patricia tree result list to print
 * @return the number of nodes in the result set
 */
int bgpstream_patricia_tree_result_set_count(
  bgpstream_patricia_tree_result_set_t *set);

/** Print the result list
 *
 * @param result      pointer to the patricia tree result list to print
 */
void bgpstream_patricia_tree_result_set_print(
  bgpstream_patricia_tree_result_set_t *set);

/** Create a new Patricia Tree instance
 *
 * @param bspt_user_destructor          a function that destroys the user
 * structure
 *                                      in the Patricia Tree Node structure
 * @return a pointer to the structure, or NULL if an error occurred
 */
bgpstream_patricia_tree_t *bgpstream_patricia_tree_create(
  bgpstream_patricia_tree_destroy_user_t *bspt_user_destructor);

/** Insert a new prefix, if it does not exist
 *
 * @param pt           pointer to the patricia tree to lookup in
 * @param pfx          pointer to the prefix to insert
 * @return a pointer to the prefix in the Patricia Tree, or NULL if an error
 * occurred
 */
bgpstream_patricia_node_t *
bgpstream_patricia_tree_insert(bgpstream_patricia_tree_t *pt,
                               bgpstream_pfx_t *pfx);

/** Get the user pointer associated with the node
 *
 * @param node        pointer to a node
 * @return the user pointer associated with the node
 */
void *bgpstream_patricia_tree_get_user(bgpstream_patricia_node_t *node);

/** Set the user pointer associated with the node
 *
 * @param pt           pointer to the patricia tree
 * @param node         pointer to a node
 * @param user         user pointer to associate with the view structure
 * @return 1 if a new user pointer is set, 0 if the user pointer was already
 *         set to the address provided.
 */
int bgpstream_patricia_tree_set_user(bgpstream_patricia_tree_t *pt,
                                     bgpstream_patricia_node_t *node,
                                     void *user);

/** Merge the information of two Patricia Trees
 *
 * @param dst        pointer to the patricia tree to modify
 * @param src        pointer to the patricia tree to merge into dest
 */
void bgpstream_patricia_tree_merge(bgpstream_patricia_tree_t *dst,
                                   const bgpstream_patricia_tree_t *src);

/** Remove a prefix from the Patricia Tree (if it exists)
 *
 * @param pt           pointer to the patricia tree to lookup in
 * @param pfx          pointer to the prefix to remove
 */
void bgpstream_patricia_tree_remove(bgpstream_patricia_tree_t *pt,
                                    bgpstream_pfx_t *pfx);

/** Remove a node from the Patricia Tree
 *
 * @param pt           pointer to the patricia tree to lookup in
 * @param node         pointer to the node to remove
 */
void bgpstream_patricia_tree_remove_node(bgpstream_patricia_tree_t *pt,
                                         bgpstream_patricia_node_t *node);

/** Search exact prefix in Patricia Tree
 *
 * @param pt           pointer to the patricia tree to lookup in
 * @param pfx          pointer to the prefix to search
 * @return a pointer to the prefix in the Patricia Tree, or NULL if an error
 * occurred
 */
bgpstream_patricia_node_t *
bgpstream_patricia_tree_search_exact(bgpstream_patricia_tree_t *pt,
                                     bgpstream_pfx_t *pfx);

/** Count the number of prefixes in the Patricia Tree
 *
 * @param pt         pointer to the patricia tree
 * @param v          IP version
 * @return the number of prefixes
 */
uint64_t bgpstream_patricia_prefix_count(bgpstream_patricia_tree_t *pt,
                                         bgpstream_addr_version_t v);

/** Count the number of unique /24 IPv4 prefixes in the Patricia Tree
 *
 * @param pt           pointer to the patricia tree
 */
uint64_t bgpstream_patricia_tree_count_24subnets(bgpstream_patricia_tree_t *pt);

/** Count the number of unique /64 IPv6 prefixes in the Patricia Tree
 *
 * @param pt           pointer to the patricia tree
 */
uint64_t bgpstream_patricia_tree_count_64subnets(bgpstream_patricia_tree_t *pt);

/** Return more specific prefixes
 *
 * @param pt           pointer to the patricia tree
 * @param node         pointer to the node
 * @param results      pointer to the results structure to fill
 * @return 0 if the computation finished correctly, -1 if an error occurred
 */
int bgpstream_patricia_tree_get_more_specifics(
  bgpstream_patricia_tree_t *pt, bgpstream_patricia_node_t *node,
  bgpstream_patricia_tree_result_set_t *results);

/** Return the smallest less specific prefix
 *
 * @param pt           pointer to the patricia tree
 * @param node         pointer to the node
 * @param results      pointer to the results structure to fill
 * @return 0 if the computation finished correctly, -1 if an error occurred
 */
int bgpstream_patricia_tree_get_mincovering_prefix(
  bgpstream_patricia_tree_t *pt, bgpstream_patricia_node_t *node,
  bgpstream_patricia_tree_result_set_t *results);

/** Return less specific prefixes
 *
 * @param pt           pointer to the patricia tree
 * @param node         pointer to the node
 * @param results      pointer to the results structure to fill
 * @return 0 if the computation finished correctly, -1 if an error occurred
 */
int bgpstream_patricia_tree_get_less_specifics(
  bgpstream_patricia_tree_t *pt, bgpstream_patricia_node_t *node,
  bgpstream_patricia_tree_result_set_t *results);

/** Return minimum coverage (the minimum list of prefixes in the Patricia Tree
 * that
 *  cover the entire IP space)
 *
 * @param pt           pointer to the patricia tree
 * @param v          IP version
 * @param results      pointer to the results structure to fill
 * @return 0 if the computation finished correctly, -1 if an error occurred
 */
int bgpstream_patricia_tree_get_minimum_coverage(
  bgpstream_patricia_tree_t *pt, bgpstream_addr_version_t v,
  bgpstream_patricia_tree_result_set_t *results);

/** Check whether a node overlaps with other prefixes in the tree
 *
 * @param pt           pointer to the patricia tree
 * @param node         pointer to the node in the tree
 * @return a mask: all zeroes if no overlap, 1 on first bit if more specifics
 * are present
 *                 1 on the second bit if less specifics are present
 */
uint8_t
bgpstream_patricia_tree_get_node_overlap_info(bgpstream_patricia_tree_t *pt,
                                              bgpstream_patricia_node_t *node);

/** Check whether a prefix would overlap with the prefixes already in the tree
 *
 * @param pt           pointer to the patricia tree
 * @param node         pointer to the prefix to check
 * @return a mask: all zeroes if no overlap, 1 on first bit if more specifics
 * are present
 *                 1 on the second bit if less specifics are present
 */
uint8_t
bgpstream_patricia_tree_get_pfx_overlap_info(bgpstream_patricia_tree_t *pt,
                                             bgpstream_pfx_t *pfx);

/** Get node's prefix
 *
 * @param node         pointer to the node
 * @return a pointer to the node's prefix , or NULL if an error occurred
 */
bgpstream_pfx_t *
bgpstream_patricia_tree_get_pfx(bgpstream_patricia_node_t *node);

/** Process the nodes while walking the Patricia Tree in order
 *
 * @param pt           pointer to the patricia tree
 * @param fun          pointer to a function to process the nodes in the
 * patricia tree
 * @param data         pointer to a custom structure that can be used to update
 * a state
 *                     during the fun call
 */
void bgpstream_patricia_tree_walk(bgpstream_patricia_tree_t *pt,
                                  bgpstream_patricia_tree_process_node_t *fun,
                                  void *data);

/** Print the prefixes in the Patricia Tree
 *
 * @param pt           pointer to the patricia tree
 */
void bgpstream_patricia_tree_print(bgpstream_patricia_tree_t *pt);

/** Clear the given Patricia Tree (i.e. remove all prefixes)
 *
 * @param pt           pointer to the patricia tree to clear
 */
void bgpstream_patricia_tree_clear(bgpstream_patricia_tree_t *pt);

/** Destroy the given Patricia Tree
 *
 * @param pt           pointer to the patricia tree to destroy
 */
void bgpstream_patricia_tree_destroy(bgpstream_patricia_tree_t *pt);

/** @} */

#endif /* __BGPSTREAM_UTILS_PATRICIA_H */
