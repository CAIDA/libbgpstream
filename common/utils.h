/* 
 * corsaro
 *
 * Alistair King, CAIDA, UC San Diego
 * corsaro-info@caida.org
 *
 * ntholl and htonll macros from 
 *   http://www.codeproject.com/KB/cpp/endianness.aspx
 *
 * Other functions from scamper as noted in utils.c
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

#ifndef __UTILS_H
#define __UTILS_H

/** @file
 *
 * @brief Header file for common utility functions
 *
 * @author Alistair King
 *
 */

/** Internal to the STR macro */
#define XSTR(a) #a

/** Stringify a macro value */
#define STR(a) XSTR(a)

#define ARR_CNT(a) (sizeof(a) / sizeof(a[0]))

/* ntholl and htonll macros from 
   http://www.codeproject.com/KB/cpp/endianness.aspx */
/** Byte-swap a 64-bit integer */
#define ntohll(x) (((uint64_t)(ntohl((int)((x << 32) >> 32))) << 32) |	\
		   (uint32_t)ntohl(((int)(x >> 32))))

/** Byte-swap a 64-bit integer */
#define htonll(x) ntohll(x)

/** Convert a host ordered short to a network ordered byte array 
 *
 * @param[out] bytes        The converted byte array
 * @param u16               The host-ordered short 
 */
void bytes_htons(uint8_t *bytes, uint16_t u16);

/** Convert a host ordered long to a network ordered byte array
 *
 * @param[out] bytes        The converted byte array
 * @param u32               The host-ordered long
 */
void bytes_htonl(uint8_t *bytes, uint32_t u32);

/** Convert a host ordered long-long (64 bit) to a network ordered byte array
 *
 * @param[out] bytes        The converted byte array
 * @param u64               The host-ordered long-long (64 bit)
 */
void bytes_htonll(uint8_t *bytes, uint64_t u64);

/** Convenience function to get the current time of day 
 *
 * @param[out] tv           A pointer to a timeval containing the time of day
 */
void gettimeofday_wrap(struct timeval *tv);

/** Allocate memory and set it to zero 
 *
 * @param size              The size of memory to allocate
 * @return a pointer to zeroed memory if successful, -1 if an error occurs
 */
void *malloc_zero(const size_t size);

/** Find the delta between two timevals
 *
 * @param[out] result      A pointer to a timeval containing the delta
 * @param x                The start timeval
 * @param y                The end timeval
 * @return 1 if the result is negative, 0 otherwise
 */
int timeval_subtract (struct timeval *result, 
		      const struct timeval *x, const struct timeval *y);

/** Remove a newline from the given string
* @param line              A pointer to the string to be chomped
*
* @note This function replaces the first occurance of '\n' with '\0'.
* Therefore it is only useful for strings where the newline is at the end.
* e.g. those returned by fgets (or similar)
*/
void chomp(char *line);

#endif /* __UTILS_H */
