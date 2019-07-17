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

#ifndef __BGPSTREAM_RECORD_INT_H
#define __BGPSTREAM_RECORD_INT_H

#include "bgpstream_elem.h"
#include "bgpstream_format.h"
#include "bgpstream_record.h"
#include "bgpstream_utils.h"

/** @file
 *
 * @brief Header file that exposes the private interface of a bgpstream record.
 *
 * @author Alistair King
 *
 */

/**
 * @name Private Data Structures
 *
 * @{ */

struct bgpstream_record_internal {

  /** Pointer to the format module that created this data */
  bgpstream_format_t *format;

  /** Private data-structure (optionally) populated by the format module */
  void *data;
};

/** @} */

/**
 * @name Private API Functions
 *
 * @{ */

/** Create a new BGP Stream Record instance for passing to
 * bgpstream_get_next_record.
 *
 * @return a pointer to a Record instance if successful, NULL otherwise
 *
 * A Record may be reused for successive calls to bgpstream_get_next_record if
 * records are processed independently of each other
 */
bgpstream_record_t *bgpstream_record_create(bgpstream_format_t *format);

/** Destroy the given BGP Stream Record instance
 *
 * @param record        pointer to a BGP Stream Record instance to destroy
 */
void bgpstream_record_destroy(bgpstream_record_t *record);

/** Clear the given BGP Stream Record instance
 *
 * @param record        pointer to a BGP Stream Record instance to clear
 *
 * @note the record passed to bgpstream_get_next_record is automatically
 * cleaned.
 */
void bgpstream_record_clear(bgpstream_record_t *record);

/** @} */

#endif /* __BGPSTREAM_RECORD_INT_H */
