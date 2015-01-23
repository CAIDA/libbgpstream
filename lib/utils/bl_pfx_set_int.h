/*
 * bgp-common
 *
 * Chiara Orsini, CAIDA, UC San Diego
 * chiara@caida.org
 *
 * Copyright (C) 2014 The Regents of the University of California.
 *
 * This file is part of bgp-common.
 *
 * bgp-common is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * bgp-common is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with bgp-common.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef _BL_PFX_SET_INT_H
#define _BL_PFX_SET_INT_H

#include "bl_bgp_utils.h"
#include <stdio.h>
#include "utils.h"
#include <assert.h>
#include <khash.h>

/** set of unique IP prefixes
 *  this structure maintains a set of unique
 *  prefixes (ipv4 and ipv6 prefixes, both hashed
 *  using a int64 type)
 */
KHASH_INIT(bl_pfx_storage_set /* name */, 
	   bl_pfx_storage_t /* khkey_t */, 
	   char /* khval_t */, 
	   0  /* kh_is_set */, 
	   bl_pfx_storage_hash_func /*__hash_func */,  
	   bl_pfx_storage_hash_equal /* __hash_equal */);


struct bl_pfx_storage_set_t {
  khash_t(bl_pfx_storage_set) *hash;
};

/* ipv4 specific set */

KHASH_INIT(bl_ipv4_pfx_set /* name */, 
	   bl_ipv4_pfx_t /* khkey_t */, 
	   char /* khval_t */, 
	   0  /* kh_is_set */, 
	   bl_ipv4_pfx_hash_func /*__hash_func */,  
	   bl_ipv4_pfx_hash_equal /* __hash_equal */);


struct bl_ipv4_pfx_set_t {
  khash_t(bl_ipv4_pfx_set) *hash;
};

/* ipv6 specific set */

KHASH_INIT(bl_ipv6_pfx_set /* name */, 
	   bl_ipv6_pfx_t /* khkey_t */, 
	   char /* khval_t */, 
	   0  /* kh_is_set */, 
	   bl_ipv6_pfx_hash_func /*__hash_func */,  
	   bl_ipv6_pfx_hash_equal /* __hash_equal */);


struct bl_ipv6_pfx_set_t {
  khash_t(bl_ipv6_pfx_set) *hash;
};


#endif /* _BL_PFX_SET_INT_H */

