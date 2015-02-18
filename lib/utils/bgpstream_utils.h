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

/** The maximum number of characters allowed in a collector name */
#define BGPSTREAM_UTILS_STR_NAME_LEN 128

/** @} */

/* Include all utility headers */
#include <bgpstream_utils_addr.h>         /** < IP Address utilities */
#include <bgpstream_utils_addr_set.h>     /** < IP Address Set utilities */
#include <bgpstream_utils_as.h>           /** < AS/AS Path utilities */
#include <bgpstream_utils_id_set.h>       /** < ID Set utilities */
#include <bgpstream_utils_peer_sig_map.h> /** < Peer Signature utilities */
#include <bgpstream_utils_pfx.h>          /** < Prefix utilities */
#include <bgpstream_utils_pfx_set.h>      /** < Prefix Set utilities */
#include <bgpstream_utils_str_set.h>      /** < String Set utilities */

#endif /* __BGPSTREAM_UTILS_H */

