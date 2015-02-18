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

#ifndef __BGPSTREAM_ELEM_INT_H
#define __BGPSTREAM_ELEM_INT_H

#include <bgpstream_elem.h>
#include <bgpstream_utils.h>

/** @file
 *
 * @brief Header file that exposes the protected interface of a bgpstream elem.
 *
 * @author Chiara Orsini
 *
 */

/**
 * @name Protected API Functions
 *
 * @{ */

/** Create a new BGP Stream Elem instance
 *
 * @return a pointer to an Elem instance if successful, NULL otherwise
 */
bgpstream_elem_t *bgpstream_elem_create();

/** Destroy the given BGP Stream Elem instance
 *
 * @param elem        pointer to a BGP Stream Elem instance to destroy
 */
void bgpstream_elem_destroy(bgpstream_elem_t *elem);

/** Clear the given BGP Stream Elem instance
 *
 * @param elem        pointer to a BGP Stream Elem instance to clear
 */
void bgpstream_elem_clear(bgpstream_elem_t *elem);

/** @} */

#endif /* __BGPSTREAM_ELEM_INT_H */
