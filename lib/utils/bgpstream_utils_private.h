/*
 * Copyright (C) 2019 The Regents of the University of California.
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

#ifndef __BGPSTREAM_UTILS_PRIVATE_H
#define __BGPSTREAM_UTILS_PRIVATE_H

/* Convert a network-order 16 bit integer pointed to by p to host order.
 * Safe even if value is unaligned, unlike ntohs(*(uint16_t*)p). */
#define nptohs(p)                                                              \
  ((uint16_t)                                                                  \
  (((uint16_t)((const uint8_t*)(p))[0] << 8) |                                 \
   ((uint16_t)((const uint8_t*)(p))[1])))

/* Convert a network-order 32 bit integer pointed to by p to host order.
 * Safe even if value is unaligned, unlike ntohl(*(uint32_t*)p). */
#define nptohl(p)                                                              \
  ((uint32_t)                                                                  \
  (((uint32_t)((const uint8_t*)(p))[0] << 24) |                                \
   ((uint32_t)((const uint8_t*)(p))[1] << 16) |                                \
   ((uint32_t)((const uint8_t*)(p))[2] << 8) |                                 \
   ((uint32_t)((const uint8_t*)(p))[3])))

/* Convert a network-order 64 bit integer pointed to by p to host order.
 * Safe even if value is unaligned, unlike ntohll(*(uint64_t*)p). */
#define nptohll(p)                                                             \
  ((uint64_t)                                                                  \
  (((uint64_t)((const uint8_t*)(p))[0] << 56) |                                \
   ((uint64_t)((const uint8_t*)(p))[1] << 48) |                                \
   ((uint64_t)((const uint8_t*)(p))[2] << 40) |                                \
   ((uint64_t)((const uint8_t*)(p))[3] << 32) |                                \
   ((uint64_t)((const uint8_t*)(p))[4] << 24) |                                \
   ((uint64_t)((const uint8_t*)(p))[5] << 16) |                                \
   ((uint64_t)((const uint8_t*)(p))[6] << 8) |                                 \
   ((uint64_t)((const uint8_t*)(p))[7])))

#endif // __BGPSTREAM_UTILS_PRIVATE_H
