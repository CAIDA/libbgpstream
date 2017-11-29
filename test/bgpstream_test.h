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

#include "bgpstream.h"
#include "config.h"

#define CHECK_MSG(name, err_msg, check)                                        \
  do {                                                                         \
    if (!(check)) {                                                            \
      fprintf(stderr, " * " name ": FAILED\n");                                \
      fprintf(stderr, " ! Failed check was: '" #check "'\n");                  \
      fprintf(stderr, err_msg "\n\n");                                         \
      return -1;                                                               \
    } else {                                                                   \
      fprintf(stderr, " * " name ": OK\n");                                    \
    }                                                                          \
  } while (0)

#define CHECK(name, check)                                                     \
  do {                                                                         \
    if (!(check)) {                                                            \
      fprintf(stderr, " * " name ": FAILED\n");                                \
      fprintf(stderr, " ! Failed check was: '" #check "'\n");                  \
      return -1;                                                               \
    } else {                                                                   \
      fprintf(stderr, " * " name ": OK\n");                                    \
    }                                                                          \
  } while (0)

#define CHECK_SECTION(name, check)                                             \
  do {                                                                         \
    fprintf(stderr, "Checking " name "...\n");                                 \
    if (!(check)) {                                                            \
      fprintf(stderr, name ": FAILED\n\n");                                    \
      return -1;                                                               \
    } else {                                                                   \
      fprintf(stderr, name ": OK\n\n");                                        \
    }                                                                          \
  } while (0)

#define SKIPPED(name)                                                          \
  do {                                                                         \
    fprintf(stderr, " * " name ": SKIPPED\n");                                 \
  } while (0)

#define SKIPPED_SECTION(name)                                                  \
  do {                                                                         \
    fprintf(stderr, name ": SKIPPED\n");                                       \
  } while (0)
