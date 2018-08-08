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
 *
 * Authors:
 *   Samir Al-Sheikh (s.al-sheikh@fu-berlin.de)
 */

#include <stdio.h>
#include <string.h>
#include <wandio.h>

#define RESULT_LEN 40

#define sep "--------------------------------------------------------"         \

#define CHECK_RPKI_SECTION(name, check)                                        \
  do {                                                                         \
    int s = RESULT_LEN - strlen("Test-Section: ") - strlen(name);              \
    int r = RESULT_LEN - strlen("Result for section ") - strlen(name);         \
    fprintf(stderr, "* " sep "\n");                                            \
    fprintf(stderr, "* %*c Test-Section: " name "\n", s/2, ' ');               \
    fprintf(stderr, "* " sep "\n");                                            \
    if (!(check)) {                                                            \
      fprintf(stderr, "* " sep "\n");                                          \
      fprintf(stderr, "* %*c Result for section " name ": FAILED\n", r/2, ' ');\
      fprintf(stderr, "* " sep "\n");                                          \
      return -1;                                                               \
    } else {                                                                   \
      fprintf(stderr, "* " sep "\n");                                          \
      fprintf(stderr, "* %*c Result for section " name ": OK\n", r/2, ' ');    \
      fprintf(stderr, "* " sep "\n\n");                                        \
    }                                                                          \
  } while (0)

#define CHECK_RPKI_RESULT(test, check)                                         \
  do {                                                                         \
    int s = RESULT_LEN - strlen(test);                                         \
    if (!(check)) {                                                            \
      fprintf(stderr, "*   Test: %s ... %*c FAILED\n", test, s, ' ');          \
      return -1;                                                               \
    }                                                                          \
    fprintf(stderr, "*   Test: %s ... %*c OK\n", test, s, ' ');                \
  } while (0)

#define SETUP                                                                  \
  do {                                                                         \
    bs = bgpstream_create();                                                   \
  } while (0)

#define CHECK_SET_INTERFACE(interface)                                         \
  do {                                                                         \
    di_id = bgpstream_get_data_interface_id_by_name(bs, STR(interface));       \
    bgpstream_set_data_interface(bs, di_id);                                   \
  } while (0)

#define VALIDATION_BUF 2048

/** Test-Section: RPKI Parsing */
#define PARSING_SSH_TESTCASE_1 "user,host_key,private_key"
#define PARSING_PCC_TESTCASE_1 "FU-Berlin:*;HAW:*"

/** Test-Section: RPKI Window Parsing */
#define PARSING_WND_TESTCASE_1                                                 \
  (const uint32_t[6]) {                                                        \
    1506816000, 1506816000, 1506817000, 1506817100, 1506818000, 1506818100     \
  }

#define PARSING_WND_TESTCASE_1_RST                                             \
  "1506816000-1506816000,1506817000-1506817100,"                               \
  "1506818000-1506818100"

#define PARSING_WND_TESTCASE_2                                                 \
  (const uint32_t[24]) {                                                       \
    1506816000, 1506816100, 1506817000, 1506817100, 1506818000, 1506818100,    \
        1506819000, 1506819100, 1506820000, 1506812100, 1506821000,            \
        1506821100, 1506822000, 1506822100, 1506823000, 1506823100,            \
        1506824000, 1506824100, 1506825000, 1506825100, 1506826000,            \
        1506826100, 1506827000, 1506827100                                     \
  }

#define PARSING_WND_TESTCASE_2_RST                                             \
  "1506816000-1506816100,1506817000-1506817100,"                               \
  "1506818000-1506818100,1506819000-1506819100,"                               \
  "1506820000-1506812100,1506821000-1506821100,"                               \
  "1506822000-1506822100,1506823000-1506823100,"                               \
  "1506824000-1506824100,1506825000-1506825100,"                               \
  "1506826000-1506826100,1506827000-1506827100"

/** Test-Section: RPKI Validation */
#define VALIDATE_TESTCASE_1 "FU-Berlin:CC01"
#define VALIDATE_TESTCASE_1_RST                                                \
  (const char * [517]) {                                                       \
    "FU-Berlin,CC01,notfound;",                                                \
        "FU-Berlin,CC01,valid,35226,2a02:2158::/32-32;",                       \
        "FU-Berlin,CC01,valid,35226,2a02:2158::/32-32;",                       \
        "FU-Berlin,CC01,notfound;",                                            \
        "FU-Berlin,CC01,valid,35226,2a02:2158::/32-32;",                       \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;",                                            \
        "FU-Berlin,CC01,valid,12654,84.205.73.0/24-24;",                       \
        "FU-Berlin,CC01,valid,12654,2001:7fb:fe10::/48-48;",                   \
        "FU-Berlin,CC01,valid,12654,84.205.66.0/24-24;",                       \
        "FU-Berlin,CC01,valid,12654,2001:7fb:ff02::/48-48;",                   \
        "FU-Berlin,CC01,valid,12654,2001:7fb:ff02::/48-48;",                   \
        "FU-Berlin,CC01,notfound;",                                            \
        "FU-Berlin,CC01,valid,35226,2a02:2158::/32-32;",                       \
        "FU-Berlin,CC01,notfound;",                                            \
        "FU-Berlin,CC01,valid,12654,2001:7fb:ff02::/48-48;",                   \
        "FU-Berlin,CC01,notfound;",                                            \
        "FU-Berlin,CC01,valid,50530,2a00:1ce0::/32-48;",                       \
        "FU-Berlin,CC01,notfound;",                                            \
        "FU-Berlin,CC01,valid,47524,176.240.0.0/16-24;",                       \
        "FU-Berlin,CC01,valid,12654,84.205.78.0/24-24;",                       \
        "FU-Berlin,CC01,valid,12654,2001:7fb:fe0e::/48-48;",                   \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;",                                            \
        "FU-Berlin,CC01,valid,12654,84.205.77.0/24-24;",                       \
        "FU-Berlin,CC01,valid,12654,84.205.77.0/24-24;",                       \
        "FU-Berlin,CC01,valid,47524,176.240.0.0/16-24;",                       \
        "FU-Berlin,CC01,valid,12654,84.205.67.0/24-24;",                       \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,valid,47524,176.240.0.0/16-24;",                       \
        "FU-Berlin,CC01,notfound;",                                            \
        "FU-Berlin,CC01,valid,12654,84.205.79.0/24-24;",                       \
        "FU-Berlin,CC01,valid,27891,2800:a020::/32-32;",                       \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;",                                            \
        "FU-Berlin,CC01,valid,12654,2001:7fb:fe05::/48-48;",                   \
        "FU-Berlin,CC01,valid,12654,84.205.69.0/24-24;",                       \
        "FU-Berlin,CC01,valid,47524,176.240.0.0/16-24;",                       \
        "FU-Berlin,CC01,notfound;",                                            \
        "FU-Berlin,CC01,valid,47524,176.240.0.0/16-24;",                       \
        "FU-Berlin,CC01,valid,12654,2001:7fb:fe0a::/48-48;",                   \
        "FU-Berlin,CC01,valid,12654,84.205.74.0/24-24;",                       \
        "FU-Berlin,CC01,valid,35226,2a02:2158::/32-32;",                       \
        "FU-Berlin,CC01,valid,35226,2a02:2158::/32-32;",                       \
        "FU-Berlin,CC01,notfound;",                                            \
        "FU-Berlin,CC01,valid,35226,2a02:2158::/32-32;",                       \
        "FU-Berlin,CC01,valid,20312,150.186.0.0/15-19;"                        \
        "FU-Berlin,CC01,valid,27807,150.186.0.0/15-16;"                        \
        "FU-Berlin,CC01,valid,27891,150.187.178.0/24-24;",                     \
        "FU-Berlin,CC01,valid,20312,150.186.0.0/15-19;"                        \
        "FU-Berlin,CC01,valid,27686,150.186.112.0/20-20;"                      \
        "FU-Berlin,CC01,valid,27807,150.186.0.0/15-16;",                       \
        "FU-Berlin,CC01,valid,20312,150.185.0.0/16-20;"                        \
        "FU-Berlin,CC01,valid,27807,150.185.0.0/16-16;"                        \
        "FU-Berlin,CC01,valid,27892,150.185.192.0/24-24;",                     \
        "FU-Berlin,CC01,valid,20312,150.185.0.0/16-20;"                        \
        "FU-Berlin,CC01,valid,27807,150.185.0.0/16-16;"                        \
        "FU-Berlin,CC01,valid,27892,150.185.222.0/24-24;",                     \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;",                                            \
        "FU-Berlin,CC01,invalid,17287,150.186.32.0/19-19;"                     \
        "FU-Berlin,CC01,invalid,20312,150.186.0.0/15-19;"                      \
        "FU-Berlin,CC01,invalid,27807,150.186.0.0/15-16;",                     \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,valid,47524,176.240.0.0/16-24;",                       \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,valid,20312,150.186.0.0/15-19;"                        \
        "FU-Berlin,CC01,valid,27807,150.186.0.0/15-16;"                        \
        "FU-Berlin,CC01,valid,27890,150.186.64.0/19-19;",                      \
        "FU-Berlin,CC01,notfound;",                                            \
        "FU-Berlin,CC01,valid,20312,150.186.0.0/15-19;"                        \
        "FU-Berlin,CC01,valid,27807,150.186.0.0/15-16;"                        \
        "FU-Berlin,CC01,valid,27891,150.187.142.0/24-24;",                     \
        "FU-Berlin,CC01,valid,20312,150.186.0.0/15-19;"                        \
        "FU-Berlin,CC01,valid,27807,150.186.0.0/15-16;"                        \
        "FU-Berlin,CC01,valid,27891,150.187.145.0/24-24;",                     \
        "FU-Berlin,CC01,valid,20312,150.186.0.0/15-19;"                        \
        "FU-Berlin,CC01,valid,27807,150.186.0.0/15-16;"                        \
        "FU-Berlin,CC01,valid,27891,150.187.141.0/24-24;",                     \
        "FU-Berlin,CC01,valid,27891,190.168.192.0/18-18;",                     \
        "FU-Berlin,CC01,valid,20312,150.186.0.0/15-19;"                        \
        "FU-Berlin,CC01,valid,27807,150.186.0.0/15-16;"                        \
        "FU-Berlin,CC01,valid,27891,150.187.148.0/24-24;",                     \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;",                                            \
        "FU-Berlin,CC01,valid,20312,150.185.0.0/16-20;"                        \
        "FU-Berlin,CC01,valid,23007,150.185.128.0/18-18;"                      \
        "FU-Berlin,CC01,valid,27807,150.185.0.0/16-16;",                       \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;",                                            \
        "FU-Berlin,CC01,valid,47524,176.240.0.0/16-24;",                       \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;",                                            \
        "FU-Berlin,CC01,valid,31078,2a00:1328::/32-36;",                       \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;",                                            \
        "FU-Berlin,CC01,valid,12654,2001:7fb:fe03::/48-48;",                   \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,valid,12654,2001:7fb:fe03::/48-48;",                   \
        "FU-Berlin,CC01,valid,12654,84.205.67.0/24-24;",                       \
        "FU-Berlin,CC01,notfound;",                                            \
        "FU-Berlin,CC01,valid,12654,84.205.67.0/24-24;",                       \
        "FU-Berlin,CC01,valid,12654,2001:7fb:fe03::/48-48;",                   \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,valid,27820,2800:130::/32-32;",                        \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;",                                            \
        "FU-Berlin,CC01,valid,12654,2001:7fb:ff02::/48-48;",                   \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,valid,12654,84.205.64.0/24-24;",                       \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,valid,35226,2a02:2158::/32-32;",                       \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;",                                            \
        "FU-Berlin,CC01,valid,60822,46.23.192.0/21-21;",                       \
        "FU-Berlin,CC01,valid,60822,195.137.144.0/22-22;",                     \
        "FU-Berlin,CC01,valid,60822,185.85.212.0/22-22;",                      \
        "FU-Berlin,CC01,valid,60822,46.23.204.0/22-22;",                       \
        "FU-Berlin,CC01,valid,60822,46.23.200.0/22-22;",                       \
        "FU-Berlin,CC01,valid,60822,46.23.192.0/21-21;",                       \
        "FU-Berlin,CC01,valid,60822,195.137.144.0/22-22;",                     \
        "FU-Berlin,CC01,valid,60822,185.85.212.0/22-22;",                      \
        "FU-Berlin,CC01,valid,60822,46.23.204.0/22-22;",                       \
        "FU-Berlin,CC01,valid,60822,46.23.200.0/22-22;",                       \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;",                                            \
        "FU-Berlin,CC01,valid,60822,46.23.192.0/21-21;",                       \
        "FU-Berlin,CC01,valid,60822,195.137.144.0/22-22;",                     \
        "FU-Berlin,CC01,valid,60822,185.85.212.0/22-22;",                      \
        "FU-Berlin,CC01,valid,60822,46.23.204.0/22-22;",                       \
        "FU-Berlin,CC01,valid,60822,46.23.200.0/22-22;",                       \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,valid,27820,2800:130::/32-32;",                        \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;",                                            \
        "FU-Berlin,CC01,valid,20312,150.185.0.0/16-20;"                        \
        "FU-Berlin,CC01,valid,27807,150.185.0.0/16-16;",                       \
        "FU-Berlin,CC01,valid,20312,150.188.0.0/15-24;"                        \
        "FU-Berlin,CC01,valid,27807,150.188.0.0/15-16;",                       \
        "FU-Berlin,CC01,valid,20312,150.186.0.0/15-19;"                        \
        "FU-Berlin,CC01,valid,27807,150.186.0.0/15-16;",                       \
        "FU-Berlin,CC01,valid,20312,150.186.0.0/15-19;"                        \
        "FU-Berlin,CC01,valid,27807,150.186.0.0/15-16;",                       \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,valid,47524,176.240.0.0/16-24;",                       \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;",                                            \
        "FU-Berlin,CC01,valid,31078,2a00:1328::/32-36;",                       \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,valid,31078,2a00:1328::/32-36;",                       \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,valid,31078,2a00:1328::/32-36;",                       \
        "FU-Berlin,CC01,valid,12654,2001:7fb:fe00::/48-48;",                   \
        "FU-Berlin,CC01,valid,10091,2404:e800::/31-64;",                       \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;",                                            \
        "FU-Berlin,CC01,valid,18747,190.60.0.0/15-24;",                        \
        "FU-Berlin,CC01,valid,18747,190.60.0.0/15-24;",                        \
        "FU-Berlin,CC01,notfound;",                                            \
        "FU-Berlin,CC01,valid,12654,2001:7fb:fe00::/48-48;",                   \
        "FU-Berlin,CC01,notfound;",                                            \
        "FU-Berlin,CC01,valid,12654,2001:7fb:ff02::/48-48;",                   \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;",                                            \
        "FU-Berlin,CC01,valid,60822,46.23.192.0/21-21;",                       \
        "FU-Berlin,CC01,valid,60822,195.137.144.0/22-22;",                     \
        "FU-Berlin,CC01,valid,60822,185.85.212.0/22-22;",                      \
        "FU-Berlin,CC01,valid,60822,46.23.204.0/22-22;",                       \
        "FU-Berlin,CC01,valid,60822,46.23.200.0/22-22;",                       \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,valid,12654,84.205.64.0/24-24;",                       \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,valid,10091,2404:e800::/31-64;",                       \
        "FU-Berlin,CC01,valid,12654,2001:7fb:ff02::/48-48;",                   \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,valid,12654,84.205.64.0/24-24;",                       \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,valid,27820,2800:130::/32-32;",                        \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,valid,35226,2a02:2158::/32-32;",                       \
        "FU-Berlin,CC01,valid,31078,2a00:1328::/32-36;",                       \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,valid,201565,185.11.232.0/22-22;",                     \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;",                                            \
        "FU-Berlin,CC01,invalid,9050,188.214.141.0/24-24;",                    \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;",                \
        "FU-Berlin,CC01,notfound;", "FU-Berlin,CC01,notfound;"                 \
  }
