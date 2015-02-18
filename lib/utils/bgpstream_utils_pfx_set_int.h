/*
 * This file is part of bgpstream
 *
 * Copyright (C) 2015 The Regents of the University of California.
 * Authors: Alistair King, Chiara Orsini
 *
 * All rights reserved.
 *
 * This code has been developed by CAIDA at UC San Diego.
 * For more information, contact bgpstream-info@caida.org
 *
 * This source code is proprietary to the CAIDA group at UC San Diego and may
 * not be redistributed, published or disclosed without prior permission from
 * CAIDA.
 *
 * Report any bugs, questions or comments to bgpstream-info@caida.org
 *
 */

#ifndef __BGPSTREAM_UTILS_PFX_SET_INT_H
#define __BGPSTREAM_UTILS_PFX_SET_INT_H

#include "khash.h"

#include <bgpstream_utils_pfx_set.h>

/** @file
 *
 * @brief Header file that exposes the private interface of the BGP Stream
 * Prefix Sets.
 *
 * @author Chiara Orsini
 *
 * @note this interface MUST NOT be used. It will be removed in the next version
 * of BGP Stream.
 *
 */

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


struct bgpstream_pfx_storage_set {
  khash_t(bgpstream_pfx_storage_set) *hash;
};

/* ipv4 specific set */

KHASH_INIT(bgpstream_ipv4_pfx_set /* name */,
	   bgpstream_ipv4_pfx_t /* khkey_t */,
	   char /* khval_t */,
	   0  /* kh_is_set */,
	   V4_HASH_VAL /*__hash_func */,
	   V4_EQUAL_VAL /* __hash_equal */);


struct bgpstream_ipv4_pfx_set {
  khash_t(bgpstream_ipv4_pfx_set) *hash;
};

/* ipv6 specific set */

KHASH_INIT(bgpstream_ipv6_pfx_set /* name */,
	   bgpstream_ipv6_pfx_t /* khkey_t */,
	   char /* khval_t */,
	   0  /* kh_is_set */,
	   V6_HASH_VAL /*__hash_func */,
	   V6_EQUAL_VAL /* __hash_equal */);


struct bgpstream_ipv6_pfx_set {
  khash_t(bgpstream_ipv6_pfx_set) *hash;
};


#endif /* __BGPSTREAM_UTILS_PFX_SET_INT_H */
