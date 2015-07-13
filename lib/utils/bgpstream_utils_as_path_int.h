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


#ifndef __BGPSTREAM_UTILS_AS_PATH_INT_H
#define __BGPSTREAM_UTILS_AS_PATH_INT_H

#include "bgpstream_utils_as_path.h"

#include "bgpdump_lib.h"

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

/** Populate an AS Path structure based on a BGP Dump AS Path Attribute
 *
 * @param path          pointer to the AS Path to populate
 * @param bd_path       pointer to a BGP Dump AS Path attribute structure
 * @return 0 if the path was populated successfully, -1 otherwise
 */
int bgpstream_as_path_populate(bgpstream_as_path_t *path,
                               struct aspath *bd_path);

/** Update the internal fields once the data array has been changed
 *
 * @param path          pointer to the AS Path to update
 */
void bgpstream_as_path_update_fields(bgpstream_as_path_t *path);

/** @} */


#endif /* __BGPSTREAM_UTILS_AS_PATH_INT_H */

