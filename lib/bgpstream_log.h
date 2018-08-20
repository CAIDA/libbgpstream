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

#ifndef _BGPSTREAM_LOG_H
#define _BGPSTREAM_LOG_H

#include <stdarg.h>

#define BGPSTREAM_LOG_ERR 0
#define BGPSTREAM_LOG_WARN 10
#define BGPSTREAM_LOG_INFO 20
#define BGPSTREAM_LOG_CONFIG 30
#define BGPSTREAM_LOG_FINE 40
#define BGPSTREAM_LOG_VFINE 50
#define BGPSTREAM_LOG_FINEST 60

// TODO: move this to configure (or even an API call)
#define BGPSTREAM_LOG_LEVEL BGPSTREAM_LOG_INFO

#define bgpstream_log(level, ...)                                              \
  do {                                                                         \
    if ((level) <= BGPSTREAM_LOG_LEVEL) {                                      \
      bgpstream_log_func((level), __FILE__, __LINE__, __VA_ARGS__);            \
    }                                                                          \
  } while (0)

void bgpstream_log_func(int level, const char *file, int line,
                        const char *format, ...)
  __attribute__((format(printf, 4, 5)));

#endif /* _BGPSTREAM_DEBUG_H */
