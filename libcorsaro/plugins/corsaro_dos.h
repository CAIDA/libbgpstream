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
 */

#ifndef __CORSARO_DOS_H
#define __CORSARO_DOS_H

#include "corsaro_plugin.h"

CORSARO_PLUGIN_GENERATE_PROTOS(corsaro_dos)

/**
 * @name DoS Structures
 *
 * These data structures are used when reading DoS files.
 */

/** Structure to hold the dos record in the global output file
 *
 * All values are in HOST byte order
 */
typedef struct corsaro_dos_global_header
{
  /** The number of packets which had mismatched IP addresses in the header 
   * 
   * This is specific to ICMP packets which have a quoted source IP address
   * which does not match the destination address.
   */
  uint32_t mismatched_pkt_cnt;

  /** The number of attack vectors in this interval */
  uint32_t attack_vector_cnt;
  
  /** The number of potential attack vectors which were not classified as being
   * part of an attack
   */
  uint32_t non_attack_vector_cnt;
} PACKED corsaro_dos_global_header_t;

/** Structure to hold the dos header details for an interval
 *
 * All values are in HOST byte order
 */
typedef struct corsaro_dos_header
{
  /** The number of attack vectors in this interval */
  uint32_t attack_vector_cnt;
} PACKED corsaro_dos_header_t;

/** Structure to hold a dos attack vector
 *
 * This structure is only used when READING the dos record from a file
 * The output vector structure (internal to the plugin) contains much
 * more state
 *
 * All values are in HOST byte order.
 */
typedef struct corsaro_dos_attack_vector_in
{
  /** The IP address of the alleged target of the attack */
  uint32_t target_ip;

  /** Number of IP addresses the alleged attack has originated from */
  uint32_t attacker_ip_cnt;

  /** Number of IP addresses the alleged attack has originated from in the
      current interval */
  uint32_t interval_attacker_ip_cnt;

  /** Number of ports that alleged attack packets have originated from */
  uint32_t attack_port_cnt;

  /** Number of ports that alleged attack packets were directed to */
  uint32_t target_port_cnt;

  /** The number of packets that comprise this vector */
  uint64_t packet_cnt;

  /** The number of packets added to this vector in the current interval */
  uint32_t interval_packet_cnt;

  /** The number of bytes that comprise this vector */
  uint64_t byte_cnt;

  /** The number of bytes added to this vector in the current interval */
  uint32_t interval_byte_cnt;

  /** The maximum packet rate observed thus far */
  uint64_t max_ppm;

  /** The time of the initial packet (seconds) */
  uint32_t start_time_sec;

  /** The time of the initial packet (usec) */
  uint32_t start_time_usec;

  /** The time of the last packet (seconds) */
  uint32_t latest_time_sec;

  /** The time of the last packet (usec) */
  uint32_t latest_time_usec;

  uint32_t initial_packet_len;

  /** A copy of the packet that caused the vector to be created 
   *
   * Can be reconstituted into a libtrace packet buffer using
   * corsaro_dos_attack_vector_get_packet
   *
   * We don't store an actual libtrace packet in here because the
   * libtrace_packet_t structure is very inefficient (64k per packet).
   */
  uint8_t *initial_packet;
} PACKED corsaro_dos_attack_vector_in_t;

/** @} */

/**
 * @name DoS Convenience Functions
 *
 * These functions can be used to do some higher-level manipulation with
 * dos records that have been read from a file. They are 'class'
 * functions that can be used without needing an instance of the actual
 * plugin. Note that writing to a file always requires an corsaro output
 * object however.
 */

void corsaro_dos_attack_vector_get_packet(
			    corsaro_dos_attack_vector_in_t *attack_vector, 
			    libtrace_packet_t *packet);

/** Write a global dos header record to the given corsaro file in ascii
 *
 * @param corsaro     The corsaro object associated with the file
 * @param file        The corsaro file to write to
 * @param header      The global header record to write out
 * @return the number of bytes written, -1 if an error occurs
 */
off_t corsaro_dos_global_header_fprint(corsaro_t *corsaro, 
				       corsaro_file_t *file, 
				       corsaro_dos_global_header_t *header);

/** Write a global dos header record to stdout in ascii format
 *
 * @param header     The global header record to write out
 */
void corsaro_dos_global_header_print(corsaro_dos_global_header_t *header);

/** Write a dos attack vector to the given corsaro file in ascii
 *
 * @param corsaro      The corsaro object associated with the file
 * @param file         The corsaro file to write to
 * @param av           The attack vector to write out
 * @return the number of bytes written, -1 if an error occurs
 */
off_t corsaro_dos_attack_vector_fprint(corsaro_t *corsaro, 
				       corsaro_file_t *file, 
				       corsaro_dos_attack_vector_in_t *av);

/** Write a dos attack vector to stdout in ascii format
 *
 * @param av          The attack vector to write out
 */
void corsaro_dos_attack_vector_print(corsaro_dos_attack_vector_in_t *av);

/** Write a dos header record to the given corsaro file in ascii
 *
 * @param corsaro     The corsaro object associated with the file
 * @param file        The corsaro file to write to
 * @param header      The header record to write out
 * @return the number of bytes written, -1 if an error occurs
 */
off_t corsaro_dos_header_fprint(corsaro_t *corsaro, 
				corsaro_file_t *file, 
				corsaro_dos_header_t *header);

/** Write a dos header record to stdout in ascii format
 *
 * @param header     The header record to write out
 */
void corsaro_dos_header_print(corsaro_dos_header_t *header);

/** Write a generic dos record to the given corsaro file in 
 *  ascii
 *
 * @param corsaro      The corsaro object associated with the file
 * @param file         The corsaro file to write to
 * @param record_type  The type of the record
 * @param record       The record to write out
 * @return the number of bytes written, -1 if an error occurs
 */
off_t corsaro_dos_record_fprint(corsaro_t *corsaro, 
				corsaro_file_t *file, 
				corsaro_in_record_type_t record_type,
				corsaro_in_record_t *record);

/** Write a generic dos record to stdout in ascii format
 *
 * @param record_type  The type of the record
 * @param record       The record to write out
 * @return 0 if successful, -1 if an error occurs
 */
int corsaro_dos_record_print(corsaro_in_record_type_t record_type,
			     corsaro_in_record_t *record);

/** @} */

#endif /* __CORSARO_DOS_H */
