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

#ifndef __BGPRIBS_RIBS_TABLE_H
#define __BGPRIBS_RIBS_TABLE_H

#include <assert.h>
#include "khash.h"
#include "utils.h"
#include "bl_bgp_utils.h"
#include "bgpstream_elem.h"

/** @file
 *
 * @brief Header file that exposes the structures needed
 * to manage a RIB table of a peer (both ipv4 and ipv6).
 *
 * @author Chiara Orsini
 *
 */


/** prefixdata
 *  this structure contains information about a given prefix.
 *  If a prefix is active it means that it is currently visible
 *  in the peer RIBS table, if it is inactive it means that it
 *  has been withdrawn.
 *  The timestamp (that indicates the last time these prefix information
 *  were updated) enables to merge out of order information in the 
 *  current RIB table smoothly.
 *
 */
typedef struct struct_prefixdata_t {
  /** flag that tells if a prefix is currently
   *  visible on the peer RIB table or not:
   *  1 if the prefix is active,
   *  0 if it is not active */
  uint8_t is_active;  
  /** last time (bgp time) this entry was updated */
  long int ts;        
  /** AS path associated with the prefix */
  bgpstream_aspath_t aspath;
  /** AS number that originated this prefix, 
   *  0 if it is an IBGP announced prefix, 
   *  or if there is an AS_SET or AS_confederation
   *  in the AS path */
  uint32_t origin_as;
} prefixdata_t;



KHASH_INIT(ipv4_rib_map /* name */, 
	   bl_ipv4_pfx_t /* khkey_t */, 
	   prefixdata_t /* khval_t */, 
	   1  /* kh_is_map */, 
	   bl_ipv4_pfx_hash_func /*__hash_func */,  
	   bl_ipv4_pfx_hash_equal /* __hash_equal */);

typedef khash_t(ipv4_rib_map) ipv4_rib_map_t;


KHASH_INIT(ipv6_rib_map /* name */, 
	   bl_ipv6_pfx_t /* khkey_t */, 
	   prefixdata_t /* khval_t */, 
	   1  /* kh_is_map */, 
	   bl_ipv6_pfx_hash_func /*__hash_func */,  
	   bl_ipv6_pfx_hash_equal /* __hash_equal */);

typedef khash_t(ipv6_rib_map) ipv6_rib_map_t;


/** ribs table
 *  this structure contains two separate RIBs (one for ipv4 prefixes
 *  and one for ipv6 prefixes), each entry of the RIB associate a:
 *  bgpstream_prefix_t -- (to) --> a prefixdata_t
 */
typedef struct struct_ribs_table_t {
  /** ipv4 rib table */
  ipv4_rib_map_t *ipv4_rib;
  /** number of active ipv4 prefixes in table */
  uint32_t ipv4_size;
  /** ipv6 rib table */
  ipv6_rib_map_t *ipv6_rib;
  /** number of active ipv6 prefixes in table */
  uint32_t ipv6_size;
  // reference rib = last rib applied to this ribs_table
  long int reference_rib_start; // when the reference rib starts
  long int reference_rib_end;   // when the reference rib ends
  long int reference_dump_time; // dump_time associated with the reference rib
} ribs_table_t;

/** Allocate memory for a strucure that maintains
 *  the RIBs table of a peer.
 *
 * @return a pointer to the structure, or
 *  NULL if an error occurred
 */
ribs_table_t *ribs_table_create();

/** Apply a new BGP information to the RIB table.
 *  It could be a RIB row, an announcement, or a
 *  withdrawal message.
 *
 * @param ribs_table a pointer to the RIBs table
 * @param bs_elem a BGP info to apply to the RIBs table
 */
void ribs_table_apply_elem(ribs_table_t *ribs_table, bgpstream_elem_t *bs_elem);

/** Empty the RIBs table.
 *
 * @param ribs_table a pointer to the RIBs table
 */
void ribs_table_reset(ribs_table_t *ribs_table);

/** Deallocate memory for the RIBs table.
 *
 * @param ribs_table a pointer to the RIBs table
 */
void ribs_table_destroy(ribs_table_t *ribs_table);

#endif /* __BGPRIBS_RIBS_TABLE_H */
