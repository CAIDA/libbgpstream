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


#ifndef __BGPSTREAM_UTILS_AS_PATH_H
#define __BGPSTREAM_UTILS_AS_PATH_H

#include <limits.h>

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

/* @} */

/**
 * @name Public Enums
 *
 * @{ */

/** The type of an segment
 *
 */
typedef enum {

  /** Invalid Segment Type */
  BGPSTREAM_AS_PATH_SEG_INVALID      = 0,

  /** Simple ASN AS Path Segment */
  BGPSTREAM_AS_PATH_SEG_ASN          = 1,

  /** AS Path Segment Set */
  BGPSTREAM_AS_PATH_SEG_SET          = 2,

  /** AS Path Segment Confederation Set */
  BGPSTREAM_AS_PATH_SEG_CONFED_SET   = 3,

  /** AS Path Segment Confederation Sequence */
  BGPSTREAM_AS_PATH_SEG_CONFED_SEQ   = 4,

  /** @todo etc */

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


/** Generic AS Path Segment.
 *
 * This must be cast to the appropriate structure based on the type
 */
typedef struct bgpstream_as_path_seg {

  /** Type of the AS Path Segment (bgpstream_as_path_seg_type_t) */
  uint8_t type;

} bgpstream_as_path_seg_t;

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
   *    BGPSTREAM_AS_PATH_SEG_CONFED_SET,
   *    BGPSTREAM_AS_PATH_SEG_CONFED_SEQ
   */
  uint8_t type;

  /** Number of ASNs in the set */
  uint8_t asn_cnt;

  /** Array of ASNs in the set */
  uint32_t asn[0];

} __attribute__((packed)) bgpstream_as_path_seg_set_t;

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
 */
int bgpstream_as_path_seg_snprintf(char *buf, size_t len,
                                   bgpstream_as_path_seg_t *seg);

/** Duplicate the given AS Path Segment
 *
 * @param src           pointer to the AS path segment to duplicate
 * @return a pointer to the created segment if successful, NULL otherwise
 *
 * @note the returned segment must be destroyed using
 * bgpstream_as_path_seg_destroy
 */
bgpstream_as_path_seg_t *bgpstream_as_path_seg_dup(bgpstream_as_path_seg_t *src);

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
#if UINT_MAX == 0xffffffffu
unsigned int
#elif ULONG_MAX == 0xffffffffu
unsigned long
#endif
bgpstream_as_path_seg_hash(bgpstream_as_path_seg_t *seg);


/** Compare two AS path segments for equality
 *
 * @param seg1          pointer to the first AS path segment to compare
 * @param seg2          pointer to the second AS path segment to compare
 * @return 0 if the segments are not equal, non-zero if they are equal
 */
int bgpstream_as_path_seg_equal(bgpstream_as_path_seg_t *seg1,
                                bgpstream_as_path_seg_t *seg2);


/* AS PATH FUNCTIONS */

/** Write the string representation of the given AS path into the given character
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
                               bgpstream_as_path_t *as_path);

/** Create the given AS path structure.
 *
 * @return pointer to the created AS path object if successful, NULL otherwise
 */
bgpstream_as_path_t *bgpstream_as_path_create();

/** Clear the given AS path structure
 *
 * @param               pointer to the AS path to clear
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
int bgpstream_as_path_copy(bgpstream_as_path_t *dst, bgpstream_as_path_t *src);

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
bgpstream_as_path_get_origin_as(bgpstream_as_path_t *path);

/** Reset the segment iterator for the given path
 *
 * @param path          pointer to the AS path to reset segment iterator for
 */
void bgpstream_as_path_reset_iter(bgpstream_as_path_t *path);

/** Get the next segment from the given path
 *
 * @param path          pointer to the AS path to get the segment from
 * @return **borrowed** pointer to the next segment, NULL if the path has no
 *         more segments
 *
 * @note the returned pointer is owned **by the path**. It MUST NOT be destroyed
 * using bgpstream_as_path_seg_destroy. Also, it is only valid as long as the
 * path is valid.
 */
bgpstream_as_path_seg_t *
bgpstream_as_path_get_next_seg(bgpstream_as_path_t *path);

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


/** @} */


#endif /* __BGPSTREAM_UTILS_AS_H */

