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

#ifndef __BGPSTREAM_ELEM_GENERATOR_H
#define __BGPSTREAM_ELEM_GENERATOR_H

#include "bgpstream_record.h"

/** @file
 *
 * @brief Header file that exposes the protected interface of the bgpstream elem
 * generator.
 *
 * @author Alistair King
 *
 */

/**
 * @name Public Opaque Data Structures
 *
 * @{ */

typedef struct bgpstream_elem_generator bgpstream_elem_generator_t;

/** @} */

/**
 * @name Protected API Functions
 *
 * @{ */

/** Create a new Elem generator
 *
 * @return pointer to a new Elem generator instance
 *
 * Currently the elem generator is simply a reusable container for elem records
 * (populated by a call to bgpstream_elem_populate_generator), but it is
 * possible that at some point in the future it may actually generate elem
 * records on the fly.
 */
bgpstream_elem_generator_t *bgpstream_elem_generator_create(void);

/** Destroy the given generator
 *
 * @param generator     pointer to the generator to destroy
 */
void bgpstream_elem_generator_destroy(bgpstream_elem_generator_t *generator);

/** Clear the generator ready for re-use
 *
 * @param generator     pointer to the generator to clear
 */
void bgpstream_elem_generator_clear(bgpstream_elem_generator_t *generator);

/** Mark the generator as having no elems
 *
 * @param generator     pointer to the generator to empty
 *
 * This is slightly different to _clear in that it will leave the generator in a
 * "populated" state, but with zero elems.
 */
void bgpstream_elem_generator_empty(bgpstream_elem_generator_t *self);

/** Check if the generator has been populated
 *
 * @param generator     pointer to the generator
 * @return 1 if the generator is ready for use, 0 otherwise
 */
int bgpstream_elem_generator_is_populated(
  bgpstream_elem_generator_t *generator);

/** Get a "new" elem structure from the generator
 *
 * @param generator     pointer to the generator to get the elem from
 * @return pointer to a fresh elem structure if successful, NULL otherwise
 */
bgpstream_elem_t *
bgpstream_elem_generator_get_new_elem(bgpstream_elem_generator_t *self);

/** "Commit" the given elem to the generator
 *
 * @param generator     pointer to the generator the elem was obtained from
 * @param elem          pointer to the elem to commit
 *
 * Note: this function must be called at most once per call to _get_new_elem. If
 * _get_new_elem is called again before _commit_elem is called, then the *same*
 * elem will be returned.
 */
void bgpstream_elem_generator_commit_elem(bgpstream_elem_generator_t *generator,
                                          bgpstream_elem_t *elem);

/** Get the next elem from the generator
 *
 * @param generator     pointer to the generator to retrieve an elem from
 * @return borrowed pointer to the next elem
 *
 * @note the memory for the returned elem belongs to the generator.
 */
bgpstream_elem_t *
bgpstream_elem_generator_get_next_elem(bgpstream_elem_generator_t *generator);

/** @} */

#endif /* __BGPSTREAM_ELEM_GENERATOR_H */
