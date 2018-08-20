/*
 * Copyright (C) 2015 The Regents of the University of California.
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

#ifndef __BGPSTREAM_UTILS_AS_PATH_INT_H
#define __BGPSTREAM_UTILS_AS_PATH_INT_H

#include "bgpstream_utils_as_path.h"

/** @file
 *
 * @brief Header file that exposes the private interface of BGP Stream AS
 * objects
 *
 * @author Chiara Orsini, Alistair King
 *
 */

/**
 * @name Private Constants
 *
 * @{ */

/* @} */

/**
 * @name Private Enums
 *
 * @{ */

/** @} */

/**
 * @name Private Opaque Data Structures
 *
 * @{ */

/** @} */

/**
 * @name Private Data Structures
 *
 * @{ */

struct bgpstream_as_path {

  /* byte array of segments */
  uint8_t *data;

  /* length of the byte array in use */
  uint16_t data_len;

  /* allocated length of the byte array */
  uint16_t data_alloc_len;

  /** The number of segments in the path */
  uint16_t seg_cnt;

  /* offset of the origin segment */
  uint16_t origin_offset;
};

/** @} */

/**
 * @name Private API Functions
 *
 * @{ */

/**
 * Append an AS Path segment to the given AS Path
 *
 * Even though BGPStream splits AS_SEQ segments into multiple segments
 * internally, this function accepts multiple ASNs when using the
 * "BGPSTREAM_AS_PATH_SEG_ASN" type to optimize addition of AS_SEQ segments
 */
int bgpstream_as_path_append(bgpstream_as_path_t *path,
                             bgpstream_as_path_seg_type_t type, uint32_t *asns,
                             int asns_cnt);

/** Update the internal fields once the data array has been changed
 *
 * @param path          pointer to the AS Path to update
 */
void bgpstream_as_path_update_fields(bgpstream_as_path_t *path);

/** @} */

#endif /* __BGPSTREAM_UTILS_AS_PATH_INT_H */
