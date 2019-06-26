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

#ifndef __BGPSTREAM_UTILS_COMMUNITY_INT_H
#define __BGPSTREAM_UTILS_COMMUNITY_INT_H

#include "bgpstream_utils_community.h"
#include <stdlib.h>

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

/** @} */

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

/** @} */

/**
 * @name Private API Functions
 *
 * @{ */

/** Populate a community set structure based on the raw data from a BGP
 * COMMUNITIES attribute
 *
 * @param set           pointer to the community set to populate
 * @param buf           pointer to the raw COMMUNITIES attribute data
 * @param len           length of the raw COMMUNITIES attribute
 * @return 0 if the set was populated successfully, -1 otherwise
 */
int bgpstream_community_set_populate(bgpstream_community_set_t *set,
                                     uint8_t *buf, size_t len);

/** @} */

#endif /* __BGPSTREAM_UTILS_COMMUNITY_INT_H */
