/*
 * bgpcorsaro
 *
 * Chiara Orsini, CAIDA, UC San Diego
 * corsaro-info@caida.org
 *
 * Copyright (C) 2014 The Regents of the University of California.
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

#ifndef __BGPRIBS_ASES_TABLE_H
#define __BGPRIBS_ASES_TABLE_H

#include <assert.h>
#include "khash.h"
#include "utils.h"
#include "bgpribs_khash.h"

/** @file
 *
 * @brief Header file that exposes the structures needed
 * to manage a unique set of AS numbers.
 *
 * @author Chiara Orsini
 *
 */


/** ases table (set of unique ases)
 *  this structure maintains a set of unique
 *  AS numbers (16/32 bits AS numbers are hashed
 *  using a uint32 type)
 */
KHASH_INIT(ases_table_t /* name */, 
	   uint32_t /* khkey_t */, 
	   char /* khval_t */, 
	   0  /* kh_is_set */, 
	   kh_int_hash_func /*__hash_func */,  
	   kh_int_hash_equal /* __hash_equal */);

/** Wrapper around the uint32 khash set */
typedef struct struct_ases_table_wrapper_t {
  khash_t(ases_table_t) * table;
} ases_table_wrapper_t;

/** Allocate memory for a strucure that maintains
 *  unique AS numbers.
 *
 * @return a pointer to the structure, or
 *  NULL if an error occurred
 */
ases_table_wrapper_t *ases_table_create();

/** Insert a new AS into the AS table.
 *
 * @param ases_table a pointer to the AS table
 * @param as AS number to insert in the table
 */
void ases_table_insert(ases_table_wrapper_t *ases_table, uint32_t as);

/** Empty the AS table.
 *
 * @param ases_table a pointer to the AS table
 */
void ases_table_reset(ases_table_wrapper_t *ases_table);

/** Deallocate memory for the AS table.
 *
 * @param ases_table a pointer to the AS table
 */
void ases_table_destroy(ases_table_wrapper_t *ases_table);


#endif /* __BGPRIBS_ASES_TABLE_H */
