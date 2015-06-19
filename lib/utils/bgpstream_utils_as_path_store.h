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


#ifndef __BGPSTREAM_UTILS_AS_PATH_STORE_H
#define __BGPSTREAM_UTILS_AS_PATH_STORE_H

#include "bgpstream_utils_as_path.h"

/** @file
 *
 * @brief Header file that exposes the public interface of the BGPStream AS Path
 * Store
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


/** @} */

/**
 * @name Public Opaque Data Structures
 *
 * @{ */

/** Opaque pointer to an AS Path Store object */
typedef struct bgpstream_as_path_store bgpstream_as_path_store_t;

/** Opaque pointer to an AS Path Store Path object */
typedef struct bgpstream_as_path_store_path bgpstream_as_path_store_path_t;

/** @} */

/**
 * @name Public Data Structures
 *
 * @{ */

/** Represents a single path in the store
 *
 * A path ID should be treated as an opaque identifier.
 */
typedef struct bgpstream_as_path_store_path_id {

  /** An internal hash of the origin ASN segment of the path */
  uint32_t origin_id;

  /** ID of the path within the origin pathset */
  uint16_t path_id;

} __attribute__((packed)) bgpstream_as_path_store_path_id_t;

/** @} */

/**
 * @name Public API Functions
 *
 * @{ */

/** Create a new AS Path Store
 *
 * @return pointer to the created store if successful, NULL otherwise
 */
bgpstream_as_path_store_t *bgpstream_as_path_store_create();

/** Destroy the given AS Path Store
 *
 * @param store         pointer to the store to destroy
 */
void bgpstream_as_path_store_destroy(bgpstream_as_path_store_t *store);

/** Get the number of paths in the store
 *
 * @param store         pointer to the store
 * @return the number of paths in the store
 */
uint32_t bgpstream_as_path_store_get_size(bgpstream_as_path_store_t *store);

/** Get the ID of the given path from the store
 *
 * @param store         pointer to the store
 * @param path          pointer to the path to get the ID for
 * @param[out] path_id  pointer to a path ID structure to store the ID into
 * @return 0 if the ID was populated correctly, -1 otherwise
 *
 * If the path is not already in the store, it will be added
 */
int
bgpstream_as_path_store_get_path_id(bgpstream_as_path_store_t *store,
                                    bgpstream_as_path_t *path,
                                    bgpstream_as_path_store_path_id_t *id);

/** Get a (borrowed) pointer to the Store Path for the given Path ID
 *
 * @param store         pointer to the store
 * @param path_id       ID of the path to retrieve
 * @return borrowed pointer to the Store Path, NULL if no path exists
 *
 * If a native BGPStream path is required, use the
 * bgpstream_as_path_store_path_get_path function.
 */
bgpstream_as_path_store_path_t *
bgpstream_as_path_store_get_store_path(bgpstream_as_path_store_t *store,
                                       bgpstream_as_path_store_path_id_t id);

/* STORE PATH FUNCTIONS */

/** Convert the given store path to a native BGPStream AS Path
 *
 * @param store_path    pointer to a store path to convert
 * @return a borrowed pointer to an AS Path object
 *
 * The returned pointer is valid as long as the store path is valid.
 */
bgpstream_as_path_t *
bgpstream_as_path_store_path_get_path(bgpstream_as_path_store_path_t *store_path);

/** @} */


#endif /* __BGPSTREAM_UTILS_AS_PATH_STORE_H */

