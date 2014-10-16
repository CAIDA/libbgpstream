/*
 * bgpcorsaro
 *
 * Chiara Orsini, CAIDA, UC San Diego
 * corsaro-info@caida.org
 *
 * Copyright (C) 2014 The Regents of the University of California.
 *
 * This file is part of bgpcorsaro.
 *
 * bgpcorsaro is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * bgpcorsaro is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with bgpcorsaro.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef __BGPRIBS_PREFIXES_TABLE_H
#define __BGPRIBS_PREFIXES_TABLE_H

#include <assert.h>
#include "khash.h"
#include "utils.h"
#include "bgpribs_khash.h"

/** @file
 *
 * @brief Header file that exposes the structures needed
 * to manage a unique set of IP prefixes.
 *
 * @author Chiara Orsini
 *
 */


/** prefixes table (set of unique prefixes)
 *  this structure contains two separate sets: 
 *  the set  of unique ipv4 prefixes and the set
 *  of unique ipv6 prefixes
 */

KHASH_INIT(ipv4_prefixes_table_t /* name */, 
	   bgpstream_prefix_t /* khkey_t */, 
	   char /* khval_t */, 
	   0  /* kh_is_set */, 
	   bgpstream_prefix_ipv4_hash_func /*__hash_func */,  
	   bgpstream_prefix_ipv4_hash_equal /* __hash_equal */);


KHASH_INIT(ipv6_prefixes_table_t /* name */, 
	   bgpstream_prefix_t /* khkey_t */, 
	   char /* khval_t */, 
	   0  /* kh_is_map */, 
	   bgpstream_prefix_ipv6_hash_func /*__hash_func */,  
	   bgpstream_prefix_ipv6_hash_equal /* __hash_equal */);

/** Structure that manage a set of unique IP prefixes 
 *  one hash is for ipv4 prefixes, the other one is for
 *  ipv6 prefixes.
 */
typedef struct struct_prefixes_table_t {
  khash_t(ipv4_prefixes_table_t) * ipv4_prefixes_table;
  khash_t(ipv6_prefixes_table_t) * ipv6_prefixes_table;
} prefixes_table_t;

/** Allocate memory for a strucure that maintains
 *  unique IP prefixes.
 *
 * @return a pointer to the structure, or
 *  NULL if an error occurred
 */
prefixes_table_t *prefixes_table_create();

/** Insert a new prefix into the prefix table.
 *
 * @param prefixes_table a pointer to the prefixes table
 * @param prefix a prefix to insert in the table
 */
void prefixes_table_insert(prefixes_table_t *prefixes_table,
			   bgpstream_prefix_t prefix);

/** Empty the prefix table.
 *
 * @param prefixes_table a pointer to the prefix table
 */
void prefixes_table_reset(prefixes_table_t *prefixes_table);

/** Deallocate memory for the prefix table.
 *
 * @param prefixes_table a pointer to the prefix table
 */
void prefixes_table_destroy(prefixes_table_t *prefixes_table);


#endif /* __BGPRIBS_PREFIXES_TABLE_H */
