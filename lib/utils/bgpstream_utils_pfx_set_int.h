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


/** set of unique IP prefixes
 *  this structure maintains a set of unique
 *  prefixes (ipv4 and ipv6 prefixes, both hashed
 *  using a int64 type)
 */
KHASH_INIT(bgpstream_pfx_storage_set /* name */,
	   bgpstream_pfx_storage_t /* khkey_t */,
	   char /* khval_t */,
	   0  /* kh_is_set */,
	   bgpstream_pfx_storage_hash_val /*__hash_func */,
	   bgpstream_pfx_storage_equal_val /* __hash_equal */);


struct bgpstream_pfx_storage_set {
  khash_t(bgpstream_pfx_storage_set) *hash;
};

/* ipv4 specific set */

KHASH_INIT(bgpstream_ipv4_pfx_set /* name */,
	   bgpstream_ipv4_pfx_t /* khkey_t */,
	   char /* khval_t */,
	   0  /* kh_is_set */,
	   bgpstream_ipv4_pfx_storage_hash_val /*__hash_func */,
	   bgpstream_ipv4_pfx_storage_equal_val /* __hash_equal */);


struct bgpstream_ipv4_pfx_set {
  khash_t(bgpstream_ipv4_pfx_set) *hash;
};

/* ipv6 specific set */

KHASH_INIT(bgpstream_ipv6_pfx_set /* name */,
	   bgpstream_ipv6_pfx_t /* khkey_t */,
	   char /* khval_t */,
	   0  /* kh_is_set */,
	   bgpstream_ipv6_pfx_storage_hash_val /*__hash_func */,
	   bgpstream_ipv6_pfx_storage_equal_val /* __hash_equal */);


struct bgpstream_ipv6_pfx_set {
  khash_t(bgpstream_ipv6_pfx_set) *hash;
};


#endif /* __BGPSTREAM_UTILS_PFX_SET_INT_H */
