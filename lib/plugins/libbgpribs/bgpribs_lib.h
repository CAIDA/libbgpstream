/*
 * bgpcorsaro
 *
 * Chiara Orsini, CAIDA, UC San Diego
 * corsaro-info@caida.org
 *
 * Copyright (C) 2012 The Regents of the University of California.
 *
 * This file is part of bgpcorsaro.
 *
 * bgpcorsaro is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * bgpcorsaro is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with bgpcorsaro.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef __BGPRIBS_LIB_H
#define __BGPRIBS_LIB_H

#include "bgpstream_lib.h"

typedef struct collectors_table_wrapper collectors_table_wrapper_t;


collectors_table_wrapper_t *collectors_table_create();
int collectors_table_process_record(collectors_table_wrapper_t *collectors_table,
				    bgpstream_record_t * bs_record);
void collectors_table_interval_end(collectors_table_wrapper_t *collectors_table,
				   int interval_processing_start, int interval_start);
void collectors_table_destroy(collectors_table_wrapper_t *collectors_table);



#endif /* __BGPRIBS_LIB_H */
