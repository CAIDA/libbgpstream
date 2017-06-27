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

#ifndef _BGPSTREAM_INT_H
#define _BGPSTREAM_INT_H

#include "bgpstream.h"
#include "bgpstream_filter.h"
#include "bgpstream_record_int.h"

/** Get a borrowed pointer to the filter manager currently in use */
bgpstream_filter_mgr_t *bgpstream_int_get_filter_mgr(bgpstream_t *bs);

#endif /* _BGPSTREAM_INT_H */
