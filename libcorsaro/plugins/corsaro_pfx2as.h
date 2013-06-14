/* 
 * corsaro
 *
 * Alistair King, CAIDA, UC San Diego
 * alistair@caida.org
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


#ifndef __CORSARO_PFX2AS_H
#define __CORSARO_PFX2AS_H

#include "corsaro_plugin.h"

CORSARO_PLUGIN_GENERATE_PROTOS(corsaro_pfx2as)

#ifdef WITH_PLUGIN_SIXT
CORSARO_PLUGIN_GENERATE_FT_PROTO(corsaro_pfx2as)
#endif

#endif /* __CORSARO_PFX2AS_H */
