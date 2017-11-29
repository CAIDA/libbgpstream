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

#ifndef __BGPSTREAM_UTILS_STR_SET_H
#define __BGPSTREAM_UTILS_STR_SET_H

/** @file
 *
 * @brief Header file that exposes the public interface of the BGP Stream String
 * Set.
 *
 * @author Chiara Orsini
 *
 */

/**
 * @name Opaque Data Structures
 *
 * @{ */

/** Opaque structure containing a string set instance */
typedef struct bgpstream_str_set_t bgpstream_str_set_t;

/** @} */

/**
 * @name Public API Functions
 *
 * @{ */

/** Create a new string set instance
 *
 * @return a pointer to the structure, or NULL if an error occurred
 */
bgpstream_str_set_t *bgpstream_str_set_create();

/** Insert a new string into the string set.
 *
 * @param set           pointer to the string set
 * @param val           the string to insert
 * @return 1 if a string has been inserted, 0 if it already existed, -1 if an
 * error occurred
 *
 * @note this function copies the provided string
 */
int bgpstream_str_set_insert(bgpstream_str_set_t *set, const char *val);

/** Remove a string from the set
 *
 * @param set           pointer to the string set
 * @param val           pointer to the string to remove
 * @return 1 if the string was removed, 0 if the string was not in the set
 */
int bgpstream_str_set_remove(bgpstream_str_set_t *set, char *val);

/** Check whether a string exists in the set
 *
 * @param set           pointer to the string set
 * @param val           the string to check
 * @return 0 if the string is not in the set, 1 if it is in the set
 */
int bgpstream_str_set_exists(bgpstream_str_set_t *set, char *val);

/** Returns the number of unique strings in the set
 *
 * @param set           pointer to the string set
 * @return the size of the string set
 */
int bgpstream_str_set_size(bgpstream_str_set_t *set);

/** Merge two string sets
 *
 * @param dst_set      pointer to the set to merge src into
 * @param src_set      pointer to the set to merge into dst
 * @return 0 if the sets were merged succsessfully, -1 otherwise
 */
int bgpstream_str_set_merge(bgpstream_str_set_t *dst_set,
                            bgpstream_str_set_t *src_set);

/** Reset the internal iterator
 *
 * @param set           pointer to the string set
 */
void bgpstream_str_set_rewind(bgpstream_str_set_t *set);

/** Returns a pointer to the next string
 *
 * @param set           pointer to the string set
 * @return a pointer to the next string in the set
 *         (borrowed pointer), NULL if the end of the set
 *         has been reached
 */
char *bgpstream_str_set_next(bgpstream_str_set_t *set);

/** Empty the string set.
 *
 * @param set           pointer to the string set to empty
 */
void bgpstream_str_set_clear(bgpstream_str_set_t *set);

/** Destroy the given string set
 *
 * @param set           pointer to the string set to destroy
 */
void bgpstream_str_set_destroy(bgpstream_str_set_t *set);

/** @} */

#endif /* __BGPSTREAM_UTILS_STR_SET_H */
