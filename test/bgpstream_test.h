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

#if defined(__GNUC__)
 #define UNUSED  __attribute__((unused))
#else
 #define UNUSED  /* empty */
#endif

/*
 * Test messages are formatted according to the TAP protocol, specifically for
 * use with automake's test harness.  See:
 * https://www.gnu.org/software/automake/manual/automake.html#Custom-Test-Drivers
 * https://metacpan.org/pod/release/PETDANCE/Test-Harness-2.65_02/lib/Test/Harness/TAP.pod
 */
static int tap_test_num = 0;
static int test_failures = 0;
static int test_section_result UNUSED = 1;

#define CHECK_MSG(name, err_msg, check)                                        \
  do {                                                                         \
    if (!(check)) {                                                            \
      test_failures++;                                                         \
      const char *_p = err_msg;                                                \
      int _i;                                                                  \
      printf("not ok %d - %s\n", ++tap_test_num, name);                        \
      printf("# Failed check was: '" #check "'\n");                            \
      while (1) {                                                              \
        _i = (int)strcspn(_p, "\n");                                           \
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
      test_failures++;                                                         \
      printf("not ok %d - %s\n", ++tap_test_num, name);                        \
      printf("# Failed check was: '" #check "'\n");                            \
      test_section_result = 0;                                                 \
      /* return EXIT_TEST_ERROR; */                                            \
    } else {                                                                   \
      printf("ok     %d - %s\n", ++tap_test_num, name);                        \
    }                                                                          \
  } while (0)

#define _CHECK_SNPRINTF_RETVAL_CHAR_P()                                        \
  if (cs_len < explen + 1) {                                                   \
    if (retval != NULL) {                                                      \
      snprintf(msg, sizeof(msg), "retval == NULL");                            \
      break;                                                                   \
    }                                                                          \
  } else {                                                                     \
    if (retval != cs_buf) {                                                    \
      snprintf(msg, sizeof(msg), "retval (%p) == cs_buf (%p)",                 \
        (void*)retval, (void*)cs_buf);                                         \
      break;                                                                   \
    }                                                                          \
  }                                                                            \
  if (retval == NULL)                                                          \
    continue;

#define _CHECK_SNPRINTF_RETVAL_int()                                           \
  if (retval != explen) {                                                      \
    snprintf(msg, sizeof(msg), "retval == explen");                            \
    break;                                                                     \
  }

typedef char *CHAR_P;

// Check that an snprintf-style function has the expected return value,
// expected buffer value, and does not write past the allowed size.
// `rettype` should be one of the following:
//     `int` if function returns an int number of bytes that *would* be written
//     `CHAR_P` if function returns char* equal to the write buffer on
//          success, or NULL on buffer overflow
// `call` is the snprintf-style function call, using cs_buf and cs_len.
#define CHECK_SNPRINTF(name, expected, bufsize, rettype, call)                 \
  do {                                                                         \
    char cs_buf_plus1[bufsize+2];                                              \
    char *cs_buf = &cs_buf_plus1[1];                                           \
    int explen = strlen(expected);                                             \
    if (bufsize <= explen) {                                                   \
      printf("# bad test at %s:%d: bufsize shorter than expected\n",           \
        __FILE__, __LINE__);                                                   \
      explen = bufsize - 1;                                                    \
    }                                                                          \
    char msg[80] = "";                                                         \
    int cs_len;                                                                \
    for (cs_len = explen + 1; !*msg && cs_len >= 0; cs_len--) {                \
      char filler = '@';                                                       \
      memset(cs_buf_plus1, filler, bufsize+2);                                 \
      rettype retval = call;                                                   \
      _CHECK_SNPRINTF_RETVAL_##rettype();                                      \
      if (cs_len > 0 && (strncmp(cs_buf, expected, cs_len-1) != 0 ||           \
        cs_buf[cs_len-1] != '\0'))                                             \
      {                                                                        \
        snprintf(msg, sizeof(msg), "output comparison");                       \
        break;                                                                 \
      }                                                                        \
      if (cs_buf[-1] != filler) {                                              \
        snprintf(msg, sizeof(msg), "write before start of buffer)");           \
        break;                                                                 \
      }                                                                        \
      for (int _i = cs_len; _i < bufsize; _i++) {                              \
        if (cs_buf[_i] != filler) {                                            \
          snprintf(msg, sizeof(msg),                                           \
            "no write past end of buffer (offset=%d)", _i);                    \
          break;                                                               \
        }                                                                      \
      }                                                                        \
    }                                                                          \
    if (*msg) {                                                                \
      test_failures++;                                                         \
      printf("not ok %d - %s\n", ++tap_test_num, name);                        \
      printf("# Failed check: %s (with cs_len=%d, exp_len=%lu)\n",             \
          msg, cs_len, strlen(expected));                                      \
      printf("# expected:  \"%.*s\"\n", cs_len>0 ? cs_len-1 : 0, expected);    \
      printf("# result  :  \"%.*s\"\n", bufsize, cs_buf);                      \
      test_section_result = 0;                                                 \
      /* return EXIT_TEST_ERROR; */                                            \
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
    if (test_failures > 0)                                                     \
      exit(EXIT_TEST_ERROR);                                                   \
  } while (0)

// Exit status for automake script-based tests
#define EXIT_TEST_SKIPPED 77
#define EXIT_TEST_ERROR   99
