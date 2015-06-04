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


#ifndef __BGPSTREAM_UTILS_AS_H
#define __BGPSTREAM_UTILS_AS_H

/** @file
 *
 * @brief Header file that exposes the public interface of BGP Stream AS
 * objects
 *
 * WARNING: This API is still under active development and **will** change.
 *
 * @author Chiara Orsini
 *
 */

/**
 * @name Public Enums
 *
 * @{ */

/** The type of an AS hop or path
 *
 * @note this API is still under development. These fields **will** change.
 */
typedef enum {

  /** AS Hop type unknown */
  BGPSTREAM_AS_TYPE_UNKNOWN      = 0,

  /** AS Hop type numeric (for regular ASNs) */
  BGPSTREAM_AS_TYPE_NUMERIC      = 1,

  /** AS Hop type string (for unusual AS hops: sets, confederations, etc) */
  BGPSTREAM_AS_TYPE_STRING       = 2

} bgpstream_as_type_t;

/** @} */

/**
 * @name Public Data Structures
 *
 * @{ */

/** Generic storage for a BGP Stream AS Path object */
typedef struct struct_bgpstream_as_path_t {

  /** AS Path type (numeric or string) */
  bgpstream_as_type_t type;

  /** Number of hops in the AS path */
  uint32_t hop_count;

  /** AS Path
   *
   * If the type is NUMERIC, the numeric_aspath field is an array of ASNs,
   * otherwise, str_aspath is a string representation of the path.
   */
  union {
    /** String representation of the path.
     * Only meaningful when type is STRING.
     */
    char *str_aspath;

    /** Path represented as a vector of uint32_t's.
     * Only meaningful when type is NUMERIC.
     */
    uint32_t *numeric_aspath;
  };

} bgpstream_as_path_t;


/** Generic storage for an AS hop */
typedef struct struct_bgpstream_as_hop_t {

  /** AS Hop type (numeric or string) */
  bgpstream_as_type_t type;

  /** AS number, if type is NUMERIC, otherwise, a string representation of the
      hop (set, confederation, etc) */
  union {
    /** String representation of the hop.
     * Only meaningful when type is STRING.
     */
    char    *as_string;

    /** Numeric representation of the hop
     * Only meaningful when type is NUMERIC.
     */
    uint32_t as_number;
  };

} bgpstream_as_hop_t;

/** @} */



/**
 * @name Public API Functions
 *
 * @{ */

/* AS HOP FUNCTIONS */

/** Write the string representation of the given AS hop into the given character
 *  buffer.
 *
 * @param buf           pointer to a character buffer at least len bytes long
 * @param len           length of the given character buffer
 * @param as_hop        pointer to the bgpstream AS hop to convert to string
 * @return the number of characters written given an infinite len (not including
 * the trailing nul). If this value is greater than or equal to len, then the
 * output was truncated.
 */
int bgpstream_as_hop_snprintf(char *buf, size_t len,
                             bgpstream_as_hop_t *as_hop);

/** Initialize the given AS hop structure.
 *
 * @param as_hop        pointer to the AS hop structure to initialize
 */
void bgpstream_as_hop_init(bgpstream_as_hop_t *as_hop);

/** Reset the given AS hop structure and free any dynamically allocated memory.
 *
 * @param as_hop        pointer to the AS hop structure to clear
 */
void bgpstream_as_hop_clear(bgpstream_as_hop_t *as_hop);

/** Copy one AS Hop structure into another
 *
 * @param dst           pointer to the AS hop structure to copy into
 * @param src           pointer to the AS hop structure to copy from
 * @return 0 if the copy was successful, -1 otherwise
 *
 * @note this function assumes that the destination hop structure has either
 * never been used and has been initialized using bgpstream_as_hop_init, or it
 * has been cleared using bgpstream_as_hop_clear.
 */
int bgpstream_as_hop_copy(bgpstream_as_hop_t *dst, bgpstream_as_hop_t *src);

/** Hash the given AS hop into a 32bit number
 *
 * @param as_hop        pointer to the AS hop to hash
 * @return 32bit hash of the AS hop
 */
#if UINT_MAX == 0xffffffffu
unsigned int
#elif ULONG_MAX == 0xffffffffu
unsigned long
#endif
bgpstream_as_hop_hash(bgpstream_as_hop_t *as_hop);


/** Compare two AS hops for equality
 *
 * @param as_hop1       pointer to the first AS hop to compare
 * @param as_hop2       pointer to the second AS hop to compare
 * @return 0 if the AS hops are not equal, non-zero if they are equal
 */
int bgpstream_as_hop_equal(bgpstream_as_hop_t *as_hop1,
                           bgpstream_as_hop_t *as_hop2);


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

/** Initialize the given AS path structure.
 *
 * @param as_path        pointer to the AS path structure to initialize
 */
void bgpstream_as_path_init(bgpstream_as_path_t *as_path);

/** Reset the given AS path structure and free any dynamically allocated memory.
 *
 * @param as_path        pointer to the AS path structure to clear
 */
void bgpstream_as_path_clear(bgpstream_as_path_t *as_path);

/** Copy one AS Path structure into another
 *
 * @param dst           pointer to the AS path structure to copy into
 * @param src           pointer to the AS path structure to copy from
 * @return 0 if the copy was successful, -1 otherwise
 *
 * @note this function assumes that the destination path structure has either
 * never been used and has been initialized using bgpstream_as_path_init, or it
 * has been cleared using bgpstream_as_path_clear.
 */
int bgpstream_as_path_copy(bgpstream_as_path_t *dst, bgpstream_as_path_t *src);

/** Store information about the origin AS hop from the given path into the
 * provided AS hop structure.
 *
 * @param as_path       pointer to the AS path to extract the origin AS for
 * @param[out] as_hop   pointer to an AS hop structure to store the origin AS in
 * @return 0 if the origin AS was extracted successfully, -1 otherwise
 *
 * @note the AS hop structure may contain dynamically allocated memory so must
 * be cleared using bgpstream_as_hop_clear before reuse or discard.
 */
int bgpstream_as_path_get_origin_as(bgpstream_as_path_t *as_path,
                                    bgpstream_as_hop_t *as_hop);


/** @} */


#endif /* __BGPSTREAM_UTILS_AS_H */

