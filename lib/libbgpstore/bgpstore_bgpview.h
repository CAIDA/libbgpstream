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

#ifndef __BGPSTORE_BGPVIEW_H
#define __BGPSTORE_BGPVIEW_H

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include "utils.h"
#include <khash.h>
#include <assert.h>




typedef struct struct_bgpview_t {
  // something
  int test;
} bgpview_t;


/** Allocate memory for a strucure that maintains
 *  the bgp information collected for a single timestamp
 *  (ts = table_time received in peer and pfx records).
 *
 * @return a pointer to the structure, or
 *  NULL if an error occurred
 */
bgpview_t *bgpview_create();


/** Deallocate memory for the bgpview structure
 *
 * @param bgp_view a pointer to the bgpview memory
 */
void bgpview_destroy(bgpview_t *bgp_view);


#endif /* __BGPSTORE_BGPVIEW_H */
