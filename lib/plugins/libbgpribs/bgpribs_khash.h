/*
 * bgpcorsaro
 *
 * Chiara Orsini, CAIDA, UC San Diego
 * corsaro-info@caida.org
 *
 * Copyright (C) 2012 The Regents of the University of California.
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

#ifndef __BGPRIBS_KHASH_H
#define __BGPRIBS_KHASH_H

#include "khash.h"
#include "bgpstream_lib.h"

/** @file
 *
 * @brief Header file that exposes the hash and hash-comparison
 * functions that can be used to insert ipv4/6 addresses and prefixes
 * in a khash map or set.
 *
 * @author Chiara Orsini
 *
 */


/** Transform an ipv4 address into a 32 bit
 *  unsigned integer. Each ipv4 address correspond
 *  to a different hash, thus there are no collisions.
 *
 * @param ip a pointer to an ipv4 address
 * @return a 32 bit unsigned integer
 *
 */
khint32_t bgpstream_ipv4_address_hash_func(bgpstream_ip_address_t ip);

/** Check whether two ipv4 addresses are equal.
 *  
 * @param ip1 a pointer to an ipv4 address
 * @param ip2 a pointer to an ipv4 address
 * @return 1 if ip1 and ip2 are equal, 0 otherwise
 *
 */
int bgpstream_ipv4_address_hash_equal(bgpstream_ip_address_t ip1, bgpstream_ip_address_t ip2);


/** Transform an ipv6 address into a 64 bit
 *  unsigned integer. We consider only the first 64 bits
 *  of the IPv6 address (which is 128), thus ther could
 *  be collisions
 *
 * @param ip a pointer to an ipv6 address
 * @return a 64 bit unsigned integer
 *
 */
khint64_t bgpstream_ipv6_address_hash_func(bgpstream_ip_address_t ip); 

/** Check whether two ipv6 addresses are equal.
 *  
 * @param ip1 a pointer to an ipv6 address
 * @param ip2 a pointer to an ipv6 address
 * @return 1 if ip1 and ip2 are equal, 0 otherwise
 *
 */
int bgpstream_ipv6_address_hash_equal(bgpstream_ip_address_t ip1, bgpstream_ip_address_t ip2);


/** Transform an ipv4 prefix into a 32 bit
 *  unsigned integer. Each ipv4 prefix whose len is
 *  shorter than 24 correspond to a different hash.
 *  Collisions occur when networks smaller than a
 *  /24 are inserted.
 *
 * @param ip a pointer to an ipv4 prefix
 * @return a 32 bit unsigned integer
 *
 */
khint32_t bgpstream_prefix_ipv4_hash_func(bgpstream_prefix_t prefix);


/** Check whether two ipv4 prefixes are equal.
 *  
 * @param prefix1 a pointer to an ipv4 prefix
 * @param prefix2 a pointer to an ipv4 prefix
 * @return 1 if prefix1 and prefix2 are equal, 0 otherwise
 *
 */
int bgpstream_prefix_ipv4_hash_equal(bgpstream_prefix_t prefix1, bgpstream_prefix_t prefix2);


/** Transform an ipv6 prefix into a 64 bit
 *  unsigned integer. Each ipv6 prefix whose len is
 *  shorter than 56 correspond to a different hash.
 *  Collisions occur when networks smaller than a
 *  /56 are inserted.
 *
 * @param ip a pointer to an ipv6 prefix
 * @return a 64 bit unsigned integer
 *
 */
khint64_t bgpstream_prefix_ipv6_hash_func(bgpstream_prefix_t prefix);

/** Check whether two ipv6 prefixes are equal.
 *  
 * @param prefix1 a pointer to an ipv6 prefix
 * @param prefix2 a pointer to an ipv6 prefix
 * @return 1 if prefix1 and prefix2 are equal, 0 otherwise
 *
 */
int bgpstream_prefix_ipv6_hash_equal(bgpstream_prefix_t prefix1, bgpstream_prefix_t prefix2);


#endif /* __BGPRIBS_KHASH_H */
