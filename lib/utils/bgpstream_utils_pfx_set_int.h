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

#ifndef _BL_PFX_SET_INT_H
#define _BL_PFX_SET_INT_H

#include <bgpstream_utils_pfx_set.h>
#include "khash.h"

#define STORAGE_HASH_VAL(arg) bgpstream_pfx_storage_hash(&(arg))
#define STORAGE_EQUAL_VAL(arg1, arg2) \
  bgpstream_pfx_storage_equal(&(arg1), &(arg2))

#define V4_HASH_VAL(arg) bgpstream_ipv4_pfx_hash(&(arg))
#define V4_EQUAL_VAL(arg1, arg2) \
  bgpstream_ipv4_pfx_equal(&(arg1), &(arg2))

#define V6_HASH_VAL(arg) bgpstream_ipv6_pfx_hash(&(arg))
#define V6_EQUAL_VAL(arg1, arg2) \
  bgpstream_ipv6_pfx_equal(&(arg1), &(arg2))

/** set of unique IP prefixes
 *  this structure maintains a set of unique
 *  prefixes (ipv4 and ipv6 prefixes, both hashed
 *  using a int64 type)
 */
KHASH_INIT(bgpstream_pfx_storage_set /* name */,
	   bgpstream_pfx_storage_t /* khkey_t */,
	   char /* khval_t */,
	   0  /* kh_is_set */,
	   STORAGE_HASH_VAL /*__hash_func */,
	   STORAGE_EQUAL_VAL /* __hash_equal */);


struct bgpstream_pfx_storage_set_t {
  khash_t(bgpstream_pfx_storage_set) *hash;
};

/* ipv4 specific set */

KHASH_INIT(bgpstream_ipv4_pfx_set /* name */,
	   bgpstream_ipv4_pfx_t /* khkey_t */,
	   char /* khval_t */,
	   0  /* kh_is_set */,
	   V4_HASH_VAL /*__hash_func */,
	   V4_EQUAL_VAL /* __hash_equal */);


struct bgpstream_ipv4_pfx_set_t {
  khash_t(bgpstream_ipv4_pfx_set) *hash;
};

/* ipv6 specific set */

KHASH_INIT(bgpstream_ipv6_pfx_set /* name */,
	   bgpstream_ipv6_pfx_t /* khkey_t */,
	   char /* khval_t */,
	   0  /* kh_is_set */,
	   V6_HASH_VAL /*__hash_func */,
	   V6_EQUAL_VAL /* __hash_equal */);


struct bgpstream_ipv6_pfx_set_t {
  khash_t(bgpstream_ipv6_pfx_set) *hash;
};


#endif /* _BL_PFX_SET_INT_H */
