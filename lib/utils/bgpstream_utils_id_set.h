/*
 * Copyright (C) 2014 The Regents of the University of California.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
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

/** Reset the internal iterator
 *
 * @param set           pointer to the id set
 */
void bgpstream_id_set_rewind(bgpstream_id_set_t *set);

/** Returns a pointer to the next id
 *
 * @param set           pointer to the string set
 * @return a pointer to the next id in the set
 *         (borrowed pointer), NULL if the end of the set
 *         has been reached
 */
uint32_t *bgpstream_id_set_next(bgpstream_id_set_t *set);

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
