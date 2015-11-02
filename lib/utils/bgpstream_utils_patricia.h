/*
 * This file is part of bgpstream
 *
 * CAIDA, UC San Diego
 * bgpstream-info@caida.org
 *
 * Copyright (C) 2012 The Regents of the University of California.
 * Authors: Alistair King, Chiara Orsini
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/* This software is heavily based on software developed by 
 * 
 * Dave Plonka <plonka@doit.wisc.edu>
 *
 * This product includes software developed by the University of Michigan,
 * Merit Network, Inc., and their contributors.
 *
 * This file had been called "radix.c" in the MRT sources.
 *
 * I renamed it to "patricia.c" since it's not an implementation of a general
 * radix trie.  Also I pulled in various requirements from "prefix.c" and
 * "demo.c" so that it could be used as a standalone API.
 */



#ifndef __BGPSTREAM_UTILS_PATRICIA_H
#define __BGPSTREAM_UTILS_PATRICIA_H

#include <limits.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "bgpstream_utils_pfx.h"


/** @file
 *
 * @brief Header file that exposes the public interface of BGP Stream Patricia Tree
 * objects
 *
 * @author Chiara Orsini
 *
 */


#define BGPSTREAM_PATRICIA_MORE_SPECIFICS 0b0001
#define BGPSTREAM_PATRICIA_LESS_SPECIFICS 0b0010


/**
 * @name Opaque Data Structures
 *
 * @{ */

/** Opaque structure containing a Patricia Tree node instance */
typedef struct bgpstream_patricia_node bgpstream_patricia_node_t;

/** Opaque structure containing a Patricia Tree instance */
typedef struct bgpstream_patricia_tree bgpstream_patricia_tree_t;

/** @} */


/**
 * @name Public Data Structures
 *
 * @{ */

/** Data structure containing a list of pointers to Patricia Tree nodes
 *  that are returned as the result of a computation */
typedef struct bgpstream_patricia_tree_result {

  /* pointer to a node in the Patricia Treee (borrowed memory) */
  bgpstream_patricia_node_t *node;

  /* pointer to the next result node*/
  struct bgpstream_patricia_tree_result *next;

} bgpstream_patricia_tree_result_t;


/** @} */

/**
 * @name Public API Functions
 *
 * @{ */

/** Create a new Patricia Tree instance
 *
 * @return a pointer to the structure, or NULL if an error occurred
 */
bgpstream_patricia_tree_t *bgpstream_patricia_tree_create();


/** Insert a new prefix, if it does not exist
 *
 * @param pt           pointer to the patricia tree to lookup in
 * @param pfx          pointer to the prefix to insert
 * @return a pointer to the prefix in the Patricia Tree, or NULL if an error occurred
 */
bgpstream_patricia_node_t *bgpstream_patricia_tree_insert(bgpstream_patricia_tree_t *pt, bgpstream_pfx_t *pfx);


/** Merge the information of two Patricia Trees
 *
 * @param dst        pointer to the patricia tree to modify
 * @param src        pointer to the patricia tree to merge into dest
 */
void bgpstream_patricia_tree_merge(bgpstream_patricia_tree_t *dst, const bgpstream_patricia_tree_t *src);


/** Remove a prefix from the Patricia Tree (if it exists)
 *
 * @param pt           pointer to the patricia tree to lookup in
 * @param pfx          pointer to the prefix to remove
 */
void bgpstream_patricia_tree_remove(bgpstream_patricia_tree_t *pt, bgpstream_pfx_t *pfx);


/** Remove a node from the Patricia Tree
 *
 * @param pt           pointer to the patricia tree to lookup in
 * @param node         pointer to the node to remove
 */
void bgpstream_patricia_tree_remove_node(bgpstream_patricia_tree_t *pt, bgpstream_patricia_node_t *node);


/** Search exact prefix in Patricia Tree
 *
 * @param pt           pointer to the patricia tree to lookup in
 * @param pfx          pointer to the prefix to search
 * @return a pointer to the prefix in the Patricia Tree, or NULL if an error occurred
 */
bgpstream_patricia_node_t *bgpstream_patricia_tree_search_exact(bgpstream_patricia_tree_t *pt, bgpstream_pfx_t *pfx);


/** Count the number of prefixes in the Patricia Tree
 *
 * @param pt         pointer to the patricia tree
 * @param v          IP version
 * @return the number of prefixes
 */
uint64_t bgpstream_patricia_prefix_count(bgpstream_patricia_tree_t *pt, bgpstream_addr_version_t v);


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
 * @return a pointer to the list of nodes in the Patricia Tree that are more specifics,
 * or NULL if there are none or an error occurred
 */
bgpstream_patricia_tree_result_t *bgpstream_patricia_tree_get_more_specifics(bgpstream_patricia_tree_t *pt,
                                                                             bgpstream_patricia_node_t *node);


/** Return less specific prefixes
 *
 * @param pt           pointer to the patricia tree
 * @param node         pointer to the node
 * @return a pointer to the list of nodes in the Patricia Tree that are less specifics,
 * or NULL if there are none or an error occurred
 */
bgpstream_patricia_tree_result_t *bgpstream_patricia_tree_get_less_specifics(bgpstream_patricia_tree_t *pt,
                                                                             bgpstream_patricia_node_t *node);


/** Return minimum coverage
 *
 * @param pt           pointer to the patricia tree
 * @param v          IP version
 * @return a pointer to the minimum list of prefixes in the Patricia Tree that cover the entire IP space
 * or NULL if there are none or an error occurred
 */
bgpstream_patricia_tree_result_t *bgpstream_patricia_tree_get_minimum_coverage(bgpstream_patricia_tree_t *pt,
                                                                               bgpstream_addr_version_t v);


/** Print the result list
 *
 * @param result      pointer to the patricia tree result list to print
 */
void bgpstream_patricia_tree_print_results(bgpstream_patricia_tree_result_t *result);


/** Check whether a node overlaps with other prefixes in the tree
 *
 * @param pt           pointer to the patricia tree
 * @param node         pointer to the node in the tree
 * @return a mask: all zeroes if no overlap, 1 on first bit if more specifics are present
 *                 1 on the second bit if less specifics are present
 */
uint8_t bgpstream_patricia_tree_get_node_overlap_info(bgpstream_patricia_tree_t *pt,
                                                      bgpstream_patricia_node_t *node);


/** Check whether a prefix would overlap with the prefixes already in the tree
 *
 * @param pt           pointer to the patricia tree
 * @param node         pointer to the prefix to check
 * @return a mask: all zeroes if no overlap, 1 on first bit if more specifics are present
 *                 1 on the second bit if less specifics are present
 */
uint8_t bgpstream_patricia_tree_get_pfx_overlap_info(bgpstream_patricia_tree_t *pt,
                                                      bgpstream_pfx_t *pfx);


/** Get node's prefix
 *
 * @param node         pointer to the node
 * @return a pointer to the node's prefix , or NULL if an error occurred
 */
bgpstream_pfx_t *bgpstream_patricia_tree_get_pfx(bgpstream_patricia_node_t *node);


/** Destroy the result list (not the nodes)
 *
 * @param result      pointer to the patricia tree result list to destroy
 */
void bgpstream_patricia_tree_result_destroy(bgpstream_patricia_tree_result_t **result);

                                                                             
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

