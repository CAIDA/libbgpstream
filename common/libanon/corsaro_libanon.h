/* 
 * corsaro
 *
 * Alistair King, CAIDA, UC San Diego
 * corsaro-info@caida.org
 * 
 * Copyright (C) 2012 The Regents of the University of California.
 * 
 * This file is part of corsaro.
 *
 * corsaro is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * corsaro is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with corsaro.  If not, see <http://www.gnu.org/licenses/>.
 *
 * This file is a modified version of the 'ipanon.h' file included with 
 * libtrace (http://research.wand.net.nz/software/libtrace.php)
 *
 */

#ifndef __CORSARO_LIBANON_H
#define __CORSARO_LIBANON_H

#include <inttypes.h>

/** The encryption algorithm used */
typedef enum corsaro_anon_enc_type {
	CORSARO_ANON_ENC_NONE,			         /**< No encryption */
	CORSARO_ANON_ENC_PREFIX_SUBSTITUTION,	   /**< Substitute a prefix */
	CORSARO_ANON_ENC_CRYPTOPAN	  /**< Prefix preserving encryption */
} corsaro_anon_enc_type_t;

/** Initialize the anonymization module 
 *
 * @param type      The encryption type to use
 * @param key       The encryption key to use
 * @return 0 if the plugin was successfully enabled, -1 otherwise
 */
void corsaro_anon_init(corsaro_anon_enc_type_t type, char *key);

/** Anonymize an IP address
 *
 * @param orig_addr  The IP address to anonymize
 * @return the anonymized IP address
 */
uint32_t corsaro_anon_ip(uint32_t orig_addr);

/** Anonymize the source/destination addresses in an IP header
 *
 * @param ip               A pointer to the IP header
 * @param enc_source       Should the source address be anonymized
 * @param enc_dest         Should the destination address be anonymized
 *
 * This function will attempt to anonymize addresses in returned ICMP packets
 * also. It will also update the checksums.
 */
void corsaro_anon_ip_header(struct libtrace_ip *ip, 
			    int enc_source,
			    int enc_dest);

#endif /* __CORSARO_LIBANON_H */
