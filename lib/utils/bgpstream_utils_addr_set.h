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


#ifndef _BL_ADDR_SET_H
#define _BL_ADDR_SET_H

#include "bgpstream_utils.h"


typedef struct bgpstream_addr_storage_set_t bgpstream_addr_storage_set_t;


/** Allocate memory for a strucure that maintains
 *  unique set of IP addresses.
 *
 * @return a pointer to the structure, or
 *  NULL if an error occurred
 */
bgpstream_addr_storage_set_t *bgpstream_addr_storage_set_create();

/** Insert a new address into the address set.
 *
 * @param as_set pointer to the address set
 * @param ip_address generic address
 * @return 1 if an address has been inserted, 0 if it already existed
 */
int bgpstream_addr_storage_set_insert(bgpstream_addr_storage_set_t *ip_address_set, bgpstream_addr_storage_t address);

/** Empty the address set.
 *
 * @param as_set pointer to the address set
 */
void bgpstream_addr_storage_set_reset(bgpstream_addr_storage_set_t *ip_address_set);

/** Get the size of the set.
 *
 * @param as_set pointer to the address set
 * @return the size of the address set
 */
int bgpstream_addr_storage_set_size(bgpstream_addr_storage_set_t *ip_address_set);

/** Get the merge of the set.
 *  @param union_set pointer to the address set that will include the merge
 *  @param part_set pointer to the address set that will be merged with the union_set
 */
void bgpstream_addr_storage_set_merge(bgpstream_addr_storage_set_t *union_set, bgpstream_addr_storage_set_t *part_set);

/** Deallocate memory for the IP address set
 *
 * @param as_set a pointer to the AS set
 */
void bgpstream_addr_storage_set_destroy(bgpstream_addr_storage_set_t *ip_address_set);



// same functions, ipv4 specific
typedef struct bgpstream_ipv4_addr_set_t bgpstream_ipv4_addr_set_t;


bgpstream_ipv4_addr_set_t *bgpstream_ipv4_addr_set_create();
int bgpstream_ipv4_addr_set_insert(bgpstream_ipv4_addr_set_t *ip_address_set, bgpstream_ipv4_addr_t ip_address);
void bgpstream_ipv4_addr_set_reset(bgpstream_ipv4_addr_set_t *ip_address_set);
int bgpstream_ipv4_addr_set_size(bgpstream_ipv4_addr_set_t *ip_address_set);
void bgpstream_ipv4_addr_set_merge(bgpstream_ipv4_addr_set_t *union_set, bgpstream_ipv4_addr_set_t *part_set);
void bgpstream_ipv4_addr_set_destroy(bgpstream_ipv4_addr_set_t *ip_address_set);


// same functions, ipv6 specific
typedef struct bgpstream_ipv6_addr_set_t bgpstream_ipv6_addr_set_t;


bgpstream_ipv6_addr_set_t *bgpstream_ipv6_addr_set_create();
int bgpstream_ipv6_addr_set_insert(bgpstream_ipv6_addr_set_t *ip_address_set, bgpstream_ipv6_addr_t ip_address);
void bgpstream_ipv6_addr_set_reset(bgpstream_ipv6_addr_set_t *ip_address_set);
int bgpstream_ipv6_addr_set_size(bgpstream_ipv6_addr_set_t *ip_address_set);
void bgpstream_ipv6_addr_set_merge(bgpstream_ipv6_addr_set_t *union_set, bgpstream_ipv6_addr_set_t *part_set);
void bgpstream_ipv6_addr_set_destroy(bgpstream_ipv6_addr_set_t *ip_address_set);


#endif /* _BL_ADDR_SET_H */


