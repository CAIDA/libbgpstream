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
 *
 * Authors:
 *   Chiara Orsini
 *   Alistair King
 *   Shane Alcock <salcock@waikato.ac.nz>
 *   Samir Al-Sheikh
 */

#ifndef __BGPSTREAM_UTILS_AS_PATH_H
#define __BGPSTREAM_UTILS_AS_PATH_H

#include <limits.h>
#include <stddef.h>
#include <stdint.h>

/** @file
 *
 * @brief Header file that exposes the public interface of BGP Stream AS
 * objects
 *
 * @author Chiara Orsini, Alistair King
 *
 */

/**
 * @name Public Constants
 *
 * @{ */

/** @} */

/**
 * @name Public Enums
 *
 * @{ */

/** The type of an segment
 *
 */
typedef enum {

  /** Invalid Segment Type */
  BGPSTREAM_AS_PATH_SEG_INVALID = 0,

  /** Simple ASN AS Path Segment */
  BGPSTREAM_AS_PATH_SEG_ASN = 1,

  /** AS Path Segment Set */
  BGPSTREAM_AS_PATH_SEG_SET = 2,

  /** AS Path Segment Confederation Sequence (RFC 5065) */
  BGPSTREAM_AS_PATH_SEG_CONFED_SEQ = 3,

  /** AS Path Segment Confederation Set (RFC 5065) */
  BGPSTREAM_AS_PATH_SEG_CONFED_SET = 4,

} bgpstream_as_path_seg_type_t;

/** @} */

/**
 * @name Public Opaque Data Structures
 *
 * @{ */

/** Opaque pointer to an AS Path object */
typedef struct bgpstream_as_path bgpstream_as_path_t;

/** @} */

/**
 * @name Public Data Structures
 *
 * @{ */

/** Simple ASN AS Path Segment */
typedef struct bgpstream_as_path_seg_asn {

  /** Type of the AS Path Segment: BGPSTREAM_AS_PATH_SEG_ASN */
  uint8_t type;

  /** ASN value for this segment */
  uint32_t asn;

} __attribute__((packed)) bgpstream_as_path_seg_asn_t;

/** AS Path Segment Set */
typedef struct bgpstream_as_path_seg_set {

  /** Type of the AS Path Segment:
   *    BGPSTREAM_AS_PATH_SEG_SET,
   *    BGPSTREAM_AS_PATH_SEG_CONFED_SEQ,
   *    BGPSTREAM_AS_PATH_SEG_CONFED_SET
   */
  uint8_t type;

  /** Number of ASNs in the set */
  uint8_t asn_cnt;

  /** Array of ASNs in the set */
  uint32_t asn[]; // C99 flexible array member

} __attribute__((packed)) bgpstream_as_path_seg_set_t;

/** Generic AS Path Segment.
 *
 * A pointer to this type is used to point to either a
 * bgpstream_as_path_seg_asn_t or a bgpstream_as_path_seg_set_t.
 * Choose the appropriate union member based on the type, e.g.
 *
 *     if (seg->type == BGPSTREAM_AS_PATH_SEG_ASN) {
 *       // do something with seg->asn;
 *     } else {
 *       // do something with seg->set;
 *     }
 *
 * (Casting also works, but is less convenient and error prone).
 */
typedef struct bgpstream_as_path_seg {

  union {
    /** Type of the AS Path Segment (bgpstream_as_path_seg_type_t) */
    uint8_t type;

    /** Simple ASN Path Segment, if type == BGPSTREAM_AS_PATH_SEG_ASN */
    bgpstream_as_path_seg_asn_t asn;

    /** AS Path Segment Set, if type != BGPSTREAM_AS_PATH_SEG_ASN */
    bgpstream_as_path_seg_set_t set;
  };

} bgpstream_as_path_seg_t;

/** Path iterator structure */
typedef struct bgpstream_as_path_iter {

  /* current offset into the data buffer */
  uint16_t cur_offset;

} bgpstream_as_path_iter_t;

/** @} */

/**
 * @name Public API Functions
 *
 * @{ */

/* AS PATH SEGMENT FUNCTIONS */

/** Write the string representation of the given AS Path Segment into the given
 *  character buffer.
 *
 * @param buf           pointer to a character buffer at least len bytes long
 * @param len           length of the given character buffer
 * @param seg           pointer to the segment to convert to string
 * @return the number of characters written given an infinite len (not including
 * the trailing nul). If this value is greater than or equal to len, then the
 * output was truncated.
 *
 * String representation format:
 * - If the segment is a simple ASN (BGPSTREAM_AS_PATH_SEG_ASN), then the string
 *   will be the decimal representation of the ASN (not dotted-decimal).
 * - If the segment is an AS Set (BGPSTREAM_AS_PATH_SEG_SET), then the string
 *   will be a comma-separated list of ASNs, enclosed in braces. E.g.,
 *   "{12345,6789}".
 * - If the segment is an AS Confederation Set
 *   (BGPSTREAM_AS_PATH_SEG_CONFED_SET), then the string will be a
 *   comma-separated list of ASNs, enclosed in brackets. E.g., "[12345,6789]".
 * - If the segment is an AS Confederation Sequence
 *   (BGPSTREAM_AS_PATH_SEG_CONFED_SEQ), then the string will be a
 *   space-separated list of ASNs, enclosed in parentheses.
 *   E.g., "(12345 6789)".
 * - If the segment is an unknown type (this should not happen), then the
 *   string will be a space-separated list of ASNs, enclosed in angle
 *   brackets.  E.g., "<12345 6789>".
 * Note that it is possible to have a set/sequence with only a single element.
 */
int bgpstream_as_path_seg_snprintf(char *buf, size_t len,
                                   const bgpstream_as_path_seg_t *seg);

/** Duplicate the given AS Path Segment
 *
 * @param src           pointer to the AS path segment to duplicate
 * @return a pointer to the created segment if successful, NULL otherwise
 *
 * @note the returned segment must be destroyed using
 * bgpstream_as_path_seg_destroy
 */
bgpstream_as_path_seg_t *
bgpstream_as_path_seg_dup(const bgpstream_as_path_seg_t *src);

/** Destroy the given AS Path Segment
 *
 * @param seg           pointer to the AS path segment to destroy
 */
void bgpstream_as_path_seg_destroy(bgpstream_as_path_seg_t *seg);

/** Hash the given AS path segment into a 32bit number
 *
 * @param seg        pointer to the AS path segment to hash
 * @return 32bit hash of the AS path segment
 */
uint32_t
bgpstream_as_path_seg_hash(const bgpstream_as_path_seg_t *seg);

/** Compare two AS path segments for equality
 *
 * @param seg1          pointer to the first AS path segment to compare
 * @param seg2          pointer to the second AS path segment to compare
 * @return 0 if the segments are not equal, non-zero if they are equal
 */
int bgpstream_as_path_seg_equal(const bgpstream_as_path_seg_t *seg1,
                                const bgpstream_as_path_seg_t *seg2);

/* AS PATH FUNCTIONS */

/** Write the string representation of the given AS path into the given
 * character
 *  buffer.
 *
 * @param buf           pointer to a character buffer at least len bytes long
 * @param len           length of the given character buffer
 * @param as_path       pointer to the bgpstream AS path to convert to string
 * @return the number of characters written given an infinite len (not including
 * the trailing nul). If this value is greater than or equal to len, then the
 * output was truncated.
 */
int bgpstream_as_path_snprintf(char *buf, size_t len,
                               const bgpstream_as_path_t *as_path);

/** Create an empty AS path structure.
 *
 * @return pointer to the created AS path object if successful, NULL otherwise
 */
bgpstream_as_path_t *bgpstream_as_path_create(void);

/** Clear the given AS path structure
 *
 * @param path        pointer to the AS path to clear
 */
void bgpstream_as_path_clear(bgpstream_as_path_t *path);

/** Destroy the given AS path structure
 *
 * @param path        pointer to the AS path structure to destroy
 */
void bgpstream_as_path_destroy(bgpstream_as_path_t *path);

/** Copy one AS Path structure into another
 *
 * @param dst           pointer to the AS path structure to copy into
 * @param src           pointer to the AS path structure to copy from
 * @return 0 if the copy was successful, -1 otherwise
 *
 * @note this function will overwrite any data currently in dst. If there are
 * existing borrowed segment pointers into the path they will become garbage.
 */
int bgpstream_as_path_copy(bgpstream_as_path_t *dst,
    const bgpstream_as_path_t *src);

/** Get the origin AS segment from the given path
 *
 * @param path          pointer to the AS path to extract the origin AS for
 * @return **borrowed** pointer to the origin AS segment
 *
 * @note the returned pointer is owned **by the path**. It MUST NOT be destroyed
 * using bgpstream_as_path_seg_destroy. Also, it is only valid as long as the
 * path is valid.
 */
bgpstream_as_path_seg_t *
bgpstream_as_path_get_origin_seg(bgpstream_as_path_t *path);

/** Get the origin ASN value from the given path if the origin segment is a
 * simple ASN value (i.e. not a set or confederation).
 *
 * @param path          pointer to the AS path to extract the origin AS for
 * @param asn           pointer to a 32-bit field in which the ASN value will be
 *                      stored
 * @return 0 if the asn value is valid, -1 otherwise
 *
 * The return value of this function **must** be checked. If the return value is
 * not 0, then the value of the `asn` parameter is undefined.
 */
int bgpstream_as_path_get_origin_val(bgpstream_as_path_t *path, uint32_t *asn);

/** Reset the segment iterator for the given path
 *
 * @param iter          pointer to the AS path iterator to reset
 */
void bgpstream_as_path_iter_reset(bgpstream_as_path_iter_t *iter);

/** Get the next segment from the given path
 *
 * @param path          pointer to the AS path to get the segment from
 * @param iter          pointer to an AS path iterator
 * @return **borrowed** pointer to the next segment, NULL if the path has no
 *         more segments
 *
 * @note the returned pointer is owned **by the path**. It MUST NOT be destroyed
 * using bgpstream_as_path_seg_destroy. Also, it is only valid as long as the
 * path is valid.
 */
bgpstream_as_path_seg_t *
bgpstream_as_path_get_next_seg(const bgpstream_as_path_t *path,
                               bgpstream_as_path_iter_t *iter);

/** Get the number of segments in the AS Path
 *
 * @param path          pointer to the path to get the length of
 * @return the number of segments in the given path
 *
 * @note that this returns the number of BGPStream segments. This may not be the
 * same as the number of segments in the original MRT message as BGPStream
 * expands AS_SEQUENCE segments into a series of individual ASN segments.
 */
int bgpstream_as_path_get_len(bgpstream_as_path_t *path);

/** Provides access to the internal byte array that stores the path segments
 *
 * @param path          pointer to the path
 * @param[out] data     set to point to the path's internal byte array
 * @return the number of bytes in the data array
 *
 * This function is to be used when serializing a path. The returned data array
 * belongs to the path and must not be modified or freed.
 */
uint16_t bgpstream_as_path_get_data(bgpstream_as_path_t *path, uint8_t **data);

/** Populate the given AS Path from the given byte array
 *
 * @param path          pointer to the path to populate
 * @param data          pointer to the data arrayg
 * @param data_len      number of bytes in the data array
 * @return 0 if the path was populated successfully, -1 otherwise
 */
int bgpstream_as_path_populate_from_data(bgpstream_as_path_t *path,
                                         uint8_t *data, uint16_t data_len);

/** Populate the given AS Path from the given byte array (Zero Copy)
 *
 * @param path          pointer to the path to populate
 * @param data          pointer to the data array
 * @param data_len      number of bytes in the data array
 * @return 0 if the path was populated successfully, -1 otherwise
 *
 * @note this function **does not** copy the data into the path. The path is
 * only valid as long as the data array passed to this function is valid.
 */
int bgpstream_as_path_populate_from_data_zc(bgpstream_as_path_t *path,
                                            uint8_t *data, uint16_t data_len);

/** Hash the given AS path into a 32bit number
 *
 * @param path          pointer to the AS path to hash
 * @return 32bit hash of the AS path
 */
uint32_t
bgpstream_as_path_hash(const bgpstream_as_path_t *path);

/** Compare two AS path for equality
 *
 * @param path1          pointer to the first AS path to compare
 * @param path2          pointer to the second AS path to compare
 * @return 0 if the paths are not equal, non-zero if they are equal
 *
 * @note for this function to return true, the paths must be identical. If ASN
 * ordering within sets is not consistent, this will return false.
 */
int bgpstream_as_path_equal(const bgpstream_as_path_t *path1,
                            const bgpstream_as_path_t *path2);

/** @} */

#endif /* __BGPSTREAM_UTILS_AS_PATH_H */
