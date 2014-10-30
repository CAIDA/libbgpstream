/*
 * bgpwatcher
 *
 * Chiara Orsini, CAIDA, UC San Diego
 * corsaro-info@caida.org
 *
 * Copyright (C) 2014 The Regents of the University of California.
 *
 * This file is part of bgpwatcher.
 *
 * bgpwatcher is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * bgpwatcher is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with bgpwatcher.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef __BGPSTORE_COMMON_H
#define __BGPSTORE_COMMON_H


#include "khash.h"
#include <assert.h>



/** The client status is a structure that maintains
 *  the interests of each client, i.e.: which data
 *  is the client interested as a consumer, and
 *  which data is the client interested as a 
 *  producer.
 *  Every bit in the  array indicates whether
 *  an type of information is interesting or not.
 */
typedef struct struct_clientstatus_t {
  uint8_t producer_intents;
  uint8_t consumer_interests;
} clientstatus_t;

KHASH_INIT(strclientstatus, char*, clientstatus_t , 1,
	   kh_str_hash_func, kh_str_hash_equal);


// TODO: documentation

int compatible_intents(khash_t(strclientstatus) *active_clients, char* client_str, uint8_t mask);

int compatible_interests(khash_t(strclientstatus) *active_clients, char* client_str, uint8_t mask);


#endif /* __BGPSTORE_COMMON_H */



