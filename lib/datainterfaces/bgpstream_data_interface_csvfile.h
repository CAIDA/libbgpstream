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

#ifndef __BGPSTREAM_DATA_INTERFACE_CSVFILE_H
#define __BGPSTREAM_DATA_INTERFACE_CSVFILE_H

#include "bgpstream_filter.h"
#include "bgpstream_input.h"

/** Opaque handle that represents the CSV-file data source */
typedef struct bgpstream_di_csvfile bgpstream_di_csvfile_t;

bgpstream_di_csvfile_t *
bgpstream_di_csvfile_create(bgpstream_filter_mgr_t *filter_mgr,
                                    char *csvfile_file);

int bgpstream_di_csvfile_update_input_queue(
  bgpstream_di_csvfile_t *csvfile_ds, bgpstream_input_mgr_t *input_mgr);

void bgpstream_di_csvfile_destroy(
  bgpstream_di_csvfile_t *csvfile_ds);

#endif /* __BGPSTREAM_DATA_INTERFACE_CSVFILE_H */
