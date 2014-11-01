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


#include "bgpstore_interests_dispatcher.h"
#include "bgpstore_common.h"

#include "bgpwatcher_common.h" // interests masks

#include "bl_bgp_utils.h"
#include "bl_str_set.h"
#include "bl_id_set.h"

#include "khash.h"
#include <assert.h>


int bgpstore_interests_dispatcher_run(clientinfo_map_t *active_clients, bgpview_t *bgp_view, uint32_t ts) {

  // TODO:
  // create an empty interests dispatcher structure
  // for each active client:
  // if the interest does not exists in the structure, compute that aggregation
  // send the structure to the requesting client
  // destroy the interests dispatcher structure
  
  return 0;
}
