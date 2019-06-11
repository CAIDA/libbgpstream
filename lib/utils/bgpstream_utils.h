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

#ifndef __BGPSTREAM_UTILS_H
#define __BGPSTREAM_UTILS_H

/** @file
 *
 * @brief Header file that exposes all BGPStream utility types and functions.
 *
 * @author Chiara Orsini
 *
 */

/**
 * @name Public Constants
 *
 * @{ */

/** The maximum number of characters allowed in a name string */
#define BGPSTREAM_UTILS_STR_NAME_LEN 256

/** @} */

/* Include all utility headers */
#include "bgpstream_utils_addr.h"          /* IP Address utilities */
#include "bgpstream_utils_addr_set.h"      /* IP Address Set utilities */
#include "bgpstream_utils_as_path.h"       /* AS Path utilities */
#include "bgpstream_utils_as_path_store.h" /* AS Path Store utilities */
#include "bgpstream_utils_community.h"     /* Community utilities */
#include "bgpstream_utils_id_set.h"        /* ID Set utilities */
#include "bgpstream_utils_ip_counter.h"    /* IP Overlap Counter */
#include "bgpstream_utils_patricia.h"      /* Patricia Tree utilities */
#include "bgpstream_utils_peer_sig_map.h"  /* Peer Signature utilities */
#include "bgpstream_utils_pfx.h"           /* Prefix utilities */
#include "bgpstream_utils_pfx_set.h"       /* Prefix Set utilities */
#include "bgpstream_utils_str_set.h"       /* String Set utilities */
#include "bgpstream_utils_time.h"          /* Time management utilities */

#endif /* __BGPSTREAM_UTILS_H */
