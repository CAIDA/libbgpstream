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


#ifndef _BL_BGP_UTILS_H
#define _BL_BGP_UTILS_H

#include <time.h>

#include <bgpstream_utils_addr.h>
#include <bgpstream_utils_pfx.h>

/** The maximum number of characters allowed in a collector name */
#define BGPSTREAM_UTILS_COLLECTOR_NAME_LEN 128

typedef enum {BL_AS_TYPE_UNKNOWN = 0,
	      BL_AS_NUMERIC      = 1,
	      BL_AS_STRING       = 2
} bl_as_type_t;

#define BL_AS_TYPE_MAX 3

typedef struct struct_bl_aspath_storage_t {
  /** aspath type (numeric or string) */
  bl_as_type_t type;
  /** number of hops in the AS path */
  uint8_t hop_count; // number of hops in the AS path
  /** aspath */
  union {
    // if the path contains sets or confederations
    // we maintain the string structure
    char * str_aspath; 
    // otherwise we maintain the as path as a vector of uint_32
    uint32_t * numeric_aspath;
  };
} bl_aspath_storage_t;


/** generic as representation 
 *  actually, generic aspath hop
 *  description */
typedef struct struct_bl_as_storage_t {
  /** as type (numeric or string) */
  bl_as_type_t type;
  /** as number */
  union {
    // it could be a confederation or a set
    // so we cannot set a length
    char    *as_string; 
    uint32_t as_number;
  };
} bl_as_storage_t;



/** Print functions */

char *bl_print_as(bl_as_storage_t *as);
char *bl_print_aspath(bl_aspath_storage_t *aspath);


/** as-path utility functions */

bl_as_storage_t bl_get_origin_as(bl_aspath_storage_t *aspath);
bl_as_storage_t bl_copy_origin_as(bl_as_storage_t *as);
void bl_origin_as_freedynmem(bl_as_storage_t *as);
bl_aspath_storage_t bl_copy_aspath(bl_aspath_storage_t *aspath);
void bl_aspath_freedynmem(bl_aspath_storage_t *aspath);


/** khash utility functions */

/** addresses */


#if UINT_MAX == 0xffffffffu
unsigned int bl_as_storage_hash_func(bl_as_storage_t as);
#elif ULONG_MAX == 0xffffffffu
unsigned long bl_as_storage_hash_func(bl_as_storage_t as);
#endif


/** as numbers */
int bl_as_storage_hash_equal(bl_as_storage_t as1, bl_as_storage_t as2);



#endif /* _BL_BGP_UTILS_H */

