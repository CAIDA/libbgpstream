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

#ifndef __BGPSTREAM_UTILS_IPCNT_H
#define __BGPSTREAM_UTILS_IPCNT_H

#include <stdint.h>

#include "bgpstream_utils_pfx.h"

/** @file
 *
 * @brief Header file that exposes the public interface of BGP Stream IP
 * Counter objects
 *
 * @author Chiara Orsini
 *
 */

/**
 * @name Opaque Data Structures
 *
 * @{ */

/** Opaque structure containing an IP Counter instance */
typedef struct bgpstream_ip_counter bgpstream_ip_counter_t;

/** @} */

/**
 * @name Public API Functions
 *
 * @{ */

/** Create a new IP Counter instance
 *
 * @return a pointer to the structure, or NULL if an error occurred
 */
bgpstream_ip_counter_t *bgpstream_ip_counter_create(void);

/** Add a prefix to the IP Counter
 *
 * @param ipc          pointer to the IP Counter
 * @param pfx          prefix to insert in IP Counter
 * @return             0 if a prefix was added correctly, -1 otherwise
 */
int bgpstream_ip_counter_add(bgpstream_ip_counter_t *ipc, bgpstream_pfx_t *pfx);

/** Get the number of unique IPs in the IP Counter
 *
 * @param ipc            pointer to the IP Counter
 * @param v              IP version
 * @return               number of unique IPs in the IP Counter
 *                       (unique /32 in IPv4, unique /64 in IPv6)
 */
uint64_t bgpstream_ip_counter_get_ipcount(bgpstream_ip_counter_t *ipc,
                                          bgpstream_addr_version_t v);

/** Return the number of unique IPs in the IP Counter instance that
 *  overlap with the provided prefix
 *
 * @param ipc            pointer to the IP Counter
 * @param pfx            prefix to compare
 * @param more_specific  it is set to 1 if the prefix is a more specific
 * @return               number of unique IPs in the IP Counter that
 *                       overlap with pfx
 */
uint64_t bgpstream_ip_counter_is_overlapping(bgpstream_ip_counter_t *ipc,
                                             bgpstream_pfx_t *pfx,
                                             uint8_t *more_specific);

/** Empty the IP Counter
 *
 * @param ipc            pointer to the IP Counter to clear
 */
void bgpstream_ip_counter_clear(bgpstream_ip_counter_t *ipc);

/** Destroy the given IP Counter
 *
 * @param ipc            pointer to the IP Counter to destroy
 */
void bgpstream_ip_counter_destroy(bgpstream_ip_counter_t *ipc);

#endif /* __BGPSTREAM_UTILS_IPCNT_H */
