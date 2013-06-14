/* 
 * corsaro
 *
 * Alistair King, CAIDA, UC San Diego
 * corsaro-info@caida.org
 * 
 * Copyright (C) 2012 The Regents of the University of California.
 * 
 * This file is part of corsaro.
 *
 * corsaro is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * corsaro is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with corsaro.  If not, see <http://www.gnu.org/licenses/>.
 *
 */


#ifndef __CORSARO_GEODB_H
#define __CORSARO_GEODB_H

#include "corsaro_plugin.h"

CORSARO_PLUGIN_GENERATE_PROTOS(corsaro_geodb)

#ifdef WITH_PLUGIN_SIXT
CORSARO_PLUGIN_GENERATE_FT_PROTO(corsaro_geodb)
#endif

/** Structure which contains a Maxmind GeoLite City record */
struct corsaro_maxmind_record
{
  /** A unique ID for this record 
   * (used to join the Blocks and Locations Files) 
   */
  uint32_t id;

  /** 16bit value which represents the ISO2 country code treat each byte as a
   * character to convert to ASCII
   */
  uint16_t country_code;
  
  /** 2 character string which represents the region the city is in */
  char region[3];

  /** String which contains the city name */
  char *city; 
  
  /** String which contains the postal code 
   * Note, this cannot be an int as some countries (I'm looking at you, Canada)
   * use characters
   */
  char *post_code;

  /** Latitude of the city */
  double latitude;

  /** Longitude of the city */
  double longitude;

  /** Metro code */
  uint32_t metro_code;

  /** Area code */
  uint32_t area_code;
};

#endif /* __CORSARO_GEODB_H */
