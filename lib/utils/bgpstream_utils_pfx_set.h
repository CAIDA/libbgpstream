/*
 * bgpstream
 *
 * Chiara Orsini, CAIDA, UC San Diego
 * chiara@caida.org
 *
 * Copyright (C) 2014 The Regents of the University of California.
 *
 * This file is part of bgpstream.
 *
 * bgpstream is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * bgpstream is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with bgpstream.  If not, see <http://www.gnu.org/licenses/>.
 *
 */


#ifndef _BL_PFX_SET_H
#define _BL_PFX_SET_H

#include "bgpstream_utils.h"


typedef struct bl_pfx_storage_set_t bl_pfx_storage_set_t;
	    	    
/** Allocate memory for a strucure that maintains
 *  unique set of IP prefixes.
 *
 * @return a pointer to the structure, or
 *  NULL if an error occurred
 */
bl_pfx_storage_set_t *bl_pfx_storage_set_create();

/** Insert a new prefix into the prefix set.
 *
 * @param as_set pointer to the prefix set
 * @param ip_prefix generic prefix
 * @return 1 if a prefix has been inserted, 0 if it already existed
 */
int bl_pfx_storage_set_insert(bl_pfx_storage_set_t *ip_prefix_set, bl_pfx_storage_t prefix);

/** Empty the prefix set.
 *
 * @param as_set pointer to the prefix set
 */
void bl_pfx_storage_set_reset(bl_pfx_storage_set_t *ip_prefix_set);

/** Get the size of the set.
 *
 * @param as_set pointer to the prefix set
 * @return the size of the prefix set
 */
int bl_pfx_storage_set_size(bl_pfx_storage_set_t *ip_prefix_set);

/** Get the merge of the set.
 *  @param union_set pointer to the prefix set that will include the merge
 *  @param part_set pointer to the prefix set that will be merged with the union_set
 */
void bl_pfx_storage_set_merge(bl_pfx_storage_set_t *union_set, bl_pfx_storage_set_t *part_set);

/** Deallocate memory for the IP prefix set
 *
 * @param as_set a pointer to the AS set
 */
void bl_pfx_storage_set_destroy(bl_pfx_storage_set_t *ip_prefix_set);


// same functions, ipv4 specific

typedef struct bl_ipv4_pfx_set_t bl_ipv4_pfx_set_t;


bl_ipv4_pfx_set_t *bl_ipv4_pfx_set_create(); 
int bl_ipv4_pfx_set_insert(bl_ipv4_pfx_set_t *ip_prefix_set, bl_ipv4_pfx_t prefix);
void bl_ipv4_pfx_set_reset(bl_ipv4_pfx_set_t *ip_prefix_set);
int bl_ipv4_pfx_set_size(bl_ipv4_pfx_set_t *ip_prefix_set);
void bl_ipv4_pfx_set_merge(bl_ipv4_pfx_set_t *union_set, bl_ipv4_pfx_set_t *part_set);
void bl_ipv4_pfx_set_destroy(bl_ipv4_pfx_set_t *ip_prefix_set);


// same functions, ipv6 specific

typedef struct bl_ipv6_pfx_set_t bl_ipv6_pfx_set_t;


bl_ipv6_pfx_set_t *bl_ipv6_pfx_set_create(); 
int bl_ipv6_pfx_set_insert(bl_ipv6_pfx_set_t *ip_prefix_set, bl_ipv6_pfx_t prefix);
void bl_ipv6_pfx_set_reset(bl_ipv6_pfx_set_t *ip_prefix_set);
int bl_ipv6_pfx_set_size(bl_ipv6_pfx_set_t *ip_prefix_set);
void bl_ipv6_pfx_set_merge(bl_ipv6_pfx_set_t *union_set, bl_ipv6_pfx_set_t *part_set);
void bl_ipv6_pfx_set_destroy(bl_ipv6_pfx_set_t *ip_prefix_set);


#endif /* _BL_PFX_SET_H */

