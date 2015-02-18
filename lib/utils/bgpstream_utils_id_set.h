/*
 * This file is part of bgpstream
 *
 * Copyright (C) 2015 The Regents of the University of California.
 * Authors: Alistair King, Chiara Orsini
 *
 * All rights reserved.
 *
 * This code has been developed by CAIDA at UC San Diego.
 * For more information, contact bgpstream-info@caida.org
 *
 * This source code is proprietary to the CAIDA group at UC San Diego and may
 * not be redistributed, published or disclosed without prior permission from
 * CAIDA.
 *
 * Report any bugs, questions or comments to bgpstream-info@caida.org
 *
 */


#ifndef __BGPSTREAM_UTILS_ID_SET_H
#define __BGPSTREAM_UTILS_ID_SET_H

/** @file
 *
 * @brief Header file that exposes the public interface of the BGP Stream ID
 * Set.
 *
 * @author Chiara Orsini
 *
 */

/**
 * @name Opaque Data Structures
 *
 * @{ */

/** Opaque structure containing a ID set instance */
typedef struct bgpstream_id_set bgpstream_id_set_t;

/** @} */

/**
 * @name Public API Functions
 *
 * @{ */

/** Create a new ID set instance
 *
 * @return a pointer to the structure, or NULL if an error occurred
 */
bgpstream_id_set_t *bgpstream_id_set_create();

/** Insert a new ID into the given set.
 *
 * @param set           pointer to the id set
 * @param id            id to insert in the set
 * @return 1 if the id was inserted, 0 if it already existed, -1 if an error
 * occurred
 */
int bgpstream_id_set_insert(bgpstream_id_set_t *set, uint32_t id);

/** Check whether an ID exists in the set
 *
 * @param set           pointer to the ID set
 * @param id            the ID to check
 * @return 0 if the ID is not in the set, 1 if it is in the set
 */
int bgpstream_id_set_exists(bgpstream_id_set_t *set, uint32_t id);

/** Get the number of IDs in the given set
 *
 * @param set           pointer to the id set
 * @return the size of the id set
 */
int bgpstream_id_set_size(bgpstream_id_set_t *set);

/** Merge two ID sets
 *
 * @param dst_set      pointer to the set to merge src into
 * @param src_set      pointer to the set to merge into dst
 * @return 0 if the sets were merged succsessfully, -1 otherwise
 */
int bgpstream_id_set_merge(bgpstream_id_set_t *dst_set,
                           bgpstream_id_set_t *src_set);

/** Destroy the given ID set
 *
 * @param set           pointer to the ID set to destroy
 */
void bgpstream_id_set_destroy(bgpstream_id_set_t *set);

/** Empty the id set.
 *
 * @param set           a pointer to the id set to clear
 */
void bgpstream_id_set_clear(bgpstream_id_set_t *set);

#endif /* __BGPSTREAM_UTILS_ID_SET_H */

