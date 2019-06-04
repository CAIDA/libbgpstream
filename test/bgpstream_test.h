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
#include <string.h>

/*
 * Test messages are formatted according to the TAP protocol, specifically for
 * use with automake's test harness.  See:
 * https://www.gnu.org/software/automake/manual/automake.html#Custom-Test-Drivers
 * https://metacpan.org/pod/release/PETDANCE/Test-Harness-2.65_02/lib/Test/Harness/TAP.pod
 */
static int tap_test_num = 0;
static int test_section_result = 1;

#define CHECK_MSG(name, err_msg, check)                                        \
  do {                                                                         \
    if (!(check)) {                                                            \
      const char *_p = err_msg;                                                \
      int _i;                                                                  \
      printf("not ok %d - %s\n", ++tap_test_num, name);                        \
      printf("# Failed check was: '" #check "'\n");                            \
      while (1) {                                                              \
        _i = strcspn(_p, "\n");                                                \
        printf("# %.*s\n", _i, _p);                                            \
        if (_p[_i] == '\0') break;                                             \
        _p += _i + 1;                                                          \
      }                                                                        \
      return -1;                                                               \
    } else {                                                                   \
      printf("ok     %d - %s\n", ++tap_test_num, name);                        \
    }                                                                          \
  } while (0)

#define CHECK(name, check)                                                     \
  do {                                                                         \
    if (!(check)) {                                                            \
      printf("not ok %d - %s\n", ++tap_test_num, name);                        \
      printf("# Failed check was: '" #check "'\n");                            \
      test_section_result = 0;                                                 \
      /* return -1; */                                                         \
    } else {                                                                   \
      printf("ok     %d - %s\n", ++tap_test_num, name);                        \
    }                                                                          \
  } while (0)

#define CHECK_SECTION(name, check)                                             \
  do {                                                                         \
    test_section_result = 1;                                                   \
    printf("# Checking section: " name "...\n");                               \
    if (!(check) || !test_section_result) {                                    \
      printf("# Section %s: FAIL\n", name);                                    \
      /* return -1; */                                                         \
    } else {                                                                   \
      printf("# Section %s: PASS\n", name);                                    \
    }                                                                          \
  } while (0)

#define SKIPPED(name)                                                          \
  do {                                                                         \
    printf("ok     %d # SKIP test %s\n", ++tap_test_num, name);                \
  } while (0)

#define SKIPPED_SECTION(name)                                                  \
  do {                                                                         \
    printf("ok     %d # SKIP section: %s\n", ++tap_test_num, name);            \
  } while (0)

// Print the TAP test plan line "1..N"
#define ENDTEST                                                                \
  do {                                                                         \
    printf("1..%d\n", tap_test_num);                                           \
  } while (0)

#define EXIT_TEST_SKIPPED 77
#define EXIT_TEST_ERROR   99
