/*
 * bgp-common
 *
 * Chiara Orsini, CAIDA, UC San Diego
 * chiara@caida.org
 *
 * Copyright (C) 2014 The Regents of the University of California.
 *
 * This file is part of bgp-common.
 *
 * bgp-common is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * bgp-common is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with bgp-common.  If not, see <http://www.gnu.org/licenses/>.
 *
 */


#ifndef _BL_PEERSIGN_MAP_INT_H
#define _BL_PEERSIGN_MAP_INT_H

#include "khash.h"
#include "bl_peersign_map.h"
#include "bl_bgp_utils.h"


khint64_t bl_peer_signature_hash_func(bl_peer_signature_t *ps);
int bl_peer_signature_hash_equal(bl_peer_signature_t *ps1,bl_peer_signature_t *ps2);


KHASH_INIT(bl_peersign_bsid_map, bl_peer_signature_t*, bl_peerid_t, 1,
	   bl_peer_signature_hash_func, bl_peer_signature_hash_equal);

KHASH_INIT(bl_bsid_peersign_map, bl_peerid_t, bl_peer_signature_t*, 1,
	   kh_int_hash_func, kh_int_hash_equal);


struct bl_peersign_map {
  khash_t(bl_peersign_bsid_map) * ps_id;
  khash_t(bl_bsid_peersign_map) * id_ps;
};


#endif /* _PEERSIGN_MAP_H */

