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


khint32_t bgpstream_ipv4_address_hash_func(bgpstream_ip_address_t ip);
int bgpstream_ipv4_address_hash_equal(bgpstream_ip_address_t ip1, bgpstream_ip_address_t ip2);

khint64_t bgpstream_ipv6_address_hash_func(bgpstream_ip_address_t ip); 
int bgpstream_ipv6_address_hash_equal(bgpstream_ip_address_t ip1, bgpstream_ip_address_t ip2);


khint32_t bgpstream_prefix_ipv4_hash_func(bgpstream_prefix_t prefix);
int bgpstream_prefix_ipv4_hash_equal(bgpstream_prefix_t prefix1, bgpstream_prefix_t prefix2);

khint64_t bgpstream_prefix_ipv6_hash_func(bgpstream_prefix_t prefix);
int bgpstream_prefix_ipv6_hash_equal(bgpstream_prefix_t prefix1, bgpstream_prefix_t prefix2);


#endif /* __BGPRIBS_KHASH_H */
