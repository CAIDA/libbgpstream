/*
 * This file is part of bgpstream
 *
 * CAIDA, UC San Diego
 * bgpstream-info@caida.org
 *
 * Copyright (C) 2012 The Regents of the University of California.
 * Authors: Alistair King, Chiara Orsini
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __BGPSTREAM_PARSEBGP_COMMON_H
#define __BGPSTREAM_PARSEBGP_COMMON_H

#include "bgpstream_elem.h"
#include "parsebgp.h"

#define COPY_IP(dst, afi, src, do_unknown)                                     \
  do {                                                                         \
    switch (afi) {                                                             \
    case PARSEBGP_BGP_AFI_IPV4:                                                \
      (dst)->version = BGPSTREAM_ADDR_VERSION_IPV4;                            \
      memcpy(&(dst)->ipv4, src, 4);                                            \
      break;                                                                   \
                                                                               \
    case PARSEBGP_BGP_AFI_IPV6:                                                \
      (dst)->version = BGPSTREAM_ADDR_VERSION_IPV6;                            \
      memcpy(&(dst)->ipv6, src, 16);                                           \
      break;                                                                   \
                                                                               \
    default:                                                                   \
      do_unknown;                                                              \
    }                                                                          \
  } while (0)

/** Process the given path attributes and populate the given elem
 *
 * @param el            pointer to the elem to populate
 * @param attrs         array of parsebgp path attributes to process
 * @return 0 if processing was successful, -1 otherwise
 *
 * @note this does not process the NEXT_HOP attribute, nor the
 * MP_REACH/MP_UNREACH attributes
 */
int bgpstream_parsebgp_process_path_attrs(
  bgpstream_elem_t *el, parsebgp_bgp_update_path_attr_t *attrs);

/** Extract the appropriate NEXT-HOP information from the given attributes
 *
 * @param el            pointer to the elem to populate
 * @param attrs         array of parsebgp path attributes to process
 * @param is_mp_pfx     flag indicating if the current prefix is from MP_REACH
 * @return 0 if processing was successful, -1 otherwise
 *
 * Note: from my reading of RFC4760, it is theoretically possible for a single
 * UPDATE to carry reachability information for both v4 and another (v6) AFI, so
 * we use the is_mp_pfx flag to direct us to either the NEXT_HOP attr, or the
 * MP_REACH attr.
 */
int bgpstream_parsebgp_process_next_hop(bgpstream_elem_t *el,
                                        parsebgp_bgp_update_path_attr_t *attrs,
                                        int is_mp_pfx);

#endif /* __BGPSTREAM_PARSEBGP_COMMON_H */
