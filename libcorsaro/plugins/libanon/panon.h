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
 * This file is a modified version of the 'panon.h' file included with 
 * libtrace (http://research.wand.net.nz/software/libtrace.php)
 *
 */

#ifndef _PANON_H_
#define _PANON_H_

/* $Id: panon.h 1448 2009-10-15 05:25:10Z perry $ */

#include "rijndael.h"
#include <inttypes.h>

uint32_t anonymize( const uint32_t orig_addr );
uint32_t pp_anonymize( const uint32_t orig_addr );
uint32_t cpp_anonymize( const uint32_t orig_addr );
void panon_init_decrypt(const uint8_t * key);
void panon_init(const char * key);
void panon_init_cache(void); 

#endif 
