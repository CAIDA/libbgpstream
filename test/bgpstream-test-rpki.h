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

#define ELEMS(x) (sizeof(x) / sizeof((x)[0]))

#define ERROR_START                                                            \
  " * Intended Error-Check:\n"                                                 \
  " * ------------------------------\n * "
#define ERROR_END " * ------------------------------\n"

#define PRINT_START                                                            \
  do {                                                                         \
    fprintf(stderr, ERROR_START);                                              \
  } while (0)

#define PRINT_MID                                                              \
  do {                                                                         \
    fprintf(stderr, " *\n");                                                   \
  } while (0)

#define PRINT_END                                                              \
  do {                                                                         \
    fprintf(stderr, ERROR_END);                                                \
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

#define PARSING_SIZE 2048
#define VALIDATION_BUF 2048

#define PARSING_BIN_TESTCASE_1 "1,0,0,FU-Berlin,CC01"
#define PARSING_BIN_TESTCASE_2 "0,1,0,FU-Berlin,CC01"
#define PARSING_BIN_TESTCASE_3 "0,0,0,FU-Berlin,CC01"
#define PARSING_BIN_TESTCASE_4 "q,0,0,FU-Berlin,CC01"
#define PARSING_SSH_TESTCASE_1                                                 \
  "0,0,1,user,host_key,private_key,FU-Berlin,CC06(RTR)"
#define PARSING_SSH_TESTCASE_2 "0,0,1,FU-Berlin,CC06(RTR)"
#define PARSING_SSH_TESTCASE_3 "0,0,1"
#define PARSING_PCC_TESTCASE_1                                                 \
  "1,0,0,FU-Berlin,CC01,FU-Berlin,CC02,FU-Berlin,CC03,"                        \
  "FU-Berlin,CC04,FU-Berlin,CC05(RTR),FU-Berlin,CC06(RTR),"                    \
  "FU-Berlin,CC07,FU-Berlin,CC08(RTR),FU-Berlin,CC09(RTR),"

#define PARSING_PCC_TESTCASE_2                                                 \
  "1,0,0,FU-Berlin,CC01,FU-Berlin,CC02,FU-Berlin,CC03,"                        \
  "FU-Berlin,CC04,FU-Berlin,CC05(RTR),FU-Berlin,CC06(RTR),"                    \
  "FU-Berlin,CC07,FU-Berlin,CC08(RTR),FU-Berlin,CC09(RTR),"                    \
  "FU-Berlin,CC10,FU-Berlin,CC11,FU-Berlin,CC12,"                              \
  "FU-Berlin,CC13,FU-Berlin,CC14(RTR),FU-Berlin,CC15(RTR),"                    \
  "FU-Berlin,CC16,FU-Berlin,CC17(RTR),FU-Berlin,CC18(RTR),"                    \
  "FU-Berlin,CC19(RTR),FU-Berlin,CC20,FU-Berlin,CC21,"                         \
  "FU-Berlin,CC22,FU-Berlin,CC23,FU-Berlin,CC24(RTR),"                         \
  "FU-Berlin,CC25(RTR),FU-Berlin,CC26,FU-Berlin,CC27(RTR)"

#define PARSING_WND_TESTCASE_1                                                 \
  (const int[6]) {                                                             \
    1506816000, 1506816000, 1506817000, 1506817100, 1506818000, 1506818100     \
  }

#define PARSING_WND_TESTCASE_2                                                 \
  (const int[24]) {                                                            \
    1506816000, 1506816100, 1506817000, 1506817100, 1506818000, 1506818100,    \
        1506819000, 1506819100, 1506820000, 1506812100, 1506821000,            \
        1506821100, 1506822000, 1506822100, 1506823000, 1506823100,            \
        1506824000, 1506824100, 1506825000, 1506825100, 1506826000,            \
        1506826100, 1506827000, 1506827100                                     \
  }

#define PARSING_WND_TESTCASE_1_RST                                             \
  "1506816000-1506816000,1506817000-1506817100,"                               \
  "1506818000-1506818100"
#define PARSING_WND_TESTCASE_2_RST                                             \
  "1506816000-1506816100,1506817000-1506817100,"                               \
  "1506818000-1506818100,1506819000-1506819100,"                               \
  "1506820000-1506812100,1506821000-1506821100,"                               \
  "1506822000-1506822100,1506823000-1506823100,"                               \
  "1506824000-1506824100,1506825000-1506825100,"                               \
  "1506826000-1506826100,1506827000-1506827100"

#define VALIDATE_TESTCASE_1 "1,0,0,FU-Berlin,CC06(RTR)"
#define VALIDATE_TESTCASE_RST2                                                 \
  (const char * [517]) {                                                       \
    "FU-Berlin,CC06(RTR),notfound;",                                           \
        "FU-Berlin,CC06(RTR),valid,35226,2a02:2158::/32-32;",                  \
        "FU-Berlin,CC06(RTR),valid,35226,2a02:2158::/32-32;",                  \
        "FU-Berlin,CC06(RTR),notfound;",                                       \
        "FU-Berlin,CC06(RTR),valid,35226,2a02:2158::/32-32;",                  \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;",                                       \
        "FU-Berlin,CC06(RTR),valid,12654,84.205.73.0/24-24;",                  \
        "FU-Berlin,CC06(RTR),valid,12654,2001:7fb:fe10::/48-48;",              \
        "FU-Berlin,CC06(RTR),valid,12654,84.205.66.0/24-24;",                  \
        "FU-Berlin,CC06(RTR),valid,12654,2001:7fb:ff02::/48-48;",              \
        "FU-Berlin,CC06(RTR),valid,12654,2001:7fb:ff02::/48-48;",              \
        "FU-Berlin,CC06(RTR),notfound;",                                       \
        "FU-Berlin,CC06(RTR),valid,35226,2a02:2158::/32-32;",                  \
        "FU-Berlin,CC06(RTR),notfound;",                                       \
        "FU-Berlin,CC06(RTR),valid,12654,2001:7fb:ff02::/48-48;",              \
        "FU-Berlin,CC06(RTR),notfound;",                                       \
        "FU-Berlin,CC06(RTR),valid,50530,2a00:1ce0::/32-48;",                  \
        "FU-Berlin,CC06(RTR),notfound;",                                       \
        "FU-Berlin,CC06(RTR),valid,47524,176.240.0.0/16-24;",                  \
        "FU-Berlin,CC06(RTR),valid,12654,84.205.78.0/24-24;",                  \
        "FU-Berlin,CC06(RTR),valid,12654,2001:7fb:fe0e::/48-48;",              \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;",                                       \
        "FU-Berlin,CC06(RTR),valid,12654,84.205.77.0/24-24;",                  \
        "FU-Berlin,CC06(RTR),valid,12654,84.205.77.0/24-24;",                  \
        "FU-Berlin,CC06(RTR),valid,47524,176.240.0.0/16-24;",                  \
        "FU-Berlin,CC06(RTR),valid,12654,84.205.67.0/24-24;",                  \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),valid,47524,176.240.0.0/16-24;",                  \
        "FU-Berlin,CC06(RTR),notfound;",                                       \
        "FU-Berlin,CC06(RTR),valid,12654,84.205.79.0/24-24;",                  \
        "FU-Berlin,CC06(RTR),valid,27891,2800:a020::/32-32;",                  \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;",                                       \
        "FU-Berlin,CC06(RTR),valid,12654,2001:7fb:fe05::/48-48;",              \
        "FU-Berlin,CC06(RTR),valid,12654,84.205.69.0/24-24;",                  \
        "FU-Berlin,CC06(RTR),valid,47524,176.240.0.0/16-24;",                  \
        "FU-Berlin,CC06(RTR),notfound;",                                       \
        "FU-Berlin,CC06(RTR),valid,47524,176.240.0.0/16-24;",                  \
        "FU-Berlin,CC06(RTR),valid,12654,2001:7fb:fe0a::/48-48;",              \
        "FU-Berlin,CC06(RTR),valid,12654,84.205.74.0/24-24;",                  \
        "FU-Berlin,CC06(RTR),valid,35226,2a02:2158::/32-32;",                  \
        "FU-Berlin,CC06(RTR),valid,35226,2a02:2158::/32-32;",                  \
        "FU-Berlin,CC06(RTR),notfound;",                                       \
        "FU-Berlin,CC06(RTR),valid,35226,2a02:2158::/32-32;",                  \
        "FU-Berlin,CC06(RTR),valid,20312,150.186.0.0/"                         \
        "15-19;FU-Berlin,CC06(RTR),valid,27807,150.186.0.0/"                   \
        "15-16;FU-Berlin,CC06(RTR),valid,27891,150.187.178.0/24-24;",          \
        "FU-Berlin,CC06(RTR),valid,20312,150.186.0.0/"                         \
        "15-19;FU-Berlin,CC06(RTR),valid,27807,150.186.0.0/"                   \
        "15-16;FU-Berlin,CC06(RTR),valid,27686,150.186.112.0/20-20;",          \
        "FU-Berlin,CC06(RTR),valid,27807,150.185.0.0/"                         \
        "16-16;FU-Berlin,CC06(RTR),valid,20312,150.185.0.0/"                   \
        "16-20;FU-Berlin,CC06(RTR),valid,27892,150.185.192.0/24-24;",          \
        "FU-Berlin,CC06(RTR),valid,27807,150.185.0.0/"                         \
        "16-16;FU-Berlin,CC06(RTR),valid,20312,150.185.0.0/"                   \
        "16-20;FU-Berlin,CC06(RTR),valid,27892,150.185.222.0/24-24;",          \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;",                                       \
        "FU-Berlin,CC06(RTR),invalid,20312,150.186.0.0/"                       \
        "15-19;FU-Berlin,CC06(RTR),invalid,27807,150.186.0.0/"                 \
        "15-16;FU-Berlin,CC06(RTR),invalid,17287,150.186.32.0/19-19;",         \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),valid,47524,176.240.0.0/16-24;",                  \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),valid,20312,150.186.0.0/"                         \
        "15-19;FU-Berlin,CC06(RTR),valid,27807,150.186.0.0/"                   \
        "15-16;FU-Berlin,CC06(RTR),valid,27890,150.186.64.0/19-19;",           \
        "FU-Berlin,CC06(RTR),notfound;",                                       \
        "FU-Berlin,CC06(RTR),valid,20312,150.186.0.0/"                         \
        "15-19;FU-Berlin,CC06(RTR),valid,27807,150.186.0.0/"                   \
        "15-16;FU-Berlin,CC06(RTR),valid,27891,150.187.142.0/24-24;",          \
        "FU-Berlin,CC06(RTR),valid,20312,150.186.0.0/"                         \
        "15-19;FU-Berlin,CC06(RTR),valid,27807,150.186.0.0/"                   \
        "15-16;FU-Berlin,CC06(RTR),valid,27891,150.187.145.0/24-24;",          \
        "FU-Berlin,CC06(RTR),valid,20312,150.186.0.0/"                         \
        "15-19;FU-Berlin,CC06(RTR),valid,27807,150.186.0.0/"                   \
        "15-16;FU-Berlin,CC06(RTR),valid,27891,150.187.141.0/24-24;",          \
        "FU-Berlin,CC06(RTR),valid,27891,190.168.192.0/18-18;",                \
        "FU-Berlin,CC06(RTR),valid,20312,150.186.0.0/"                         \
        "15-19;FU-Berlin,CC06(RTR),valid,27807,150.186.0.0/"                   \
        "15-16;FU-Berlin,CC06(RTR),valid,27891,150.187.148.0/24-24;",          \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;",                                       \
        "FU-Berlin,CC06(RTR),valid,27807,150.185.0.0/"                         \
        "16-16;FU-Berlin,CC06(RTR),valid,20312,150.185.0.0/"                   \
        "16-20;FU-Berlin,CC06(RTR),valid,23007,150.185.128.0/18-18;",          \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;",                                       \
        "FU-Berlin,CC06(RTR),valid,47524,176.240.0.0/16-24;",                  \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;",                                       \
        "FU-Berlin,CC06(RTR),valid,31078,2a00:1328::/32-36;",                  \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;",                                       \
        "FU-Berlin,CC06(RTR),valid,12654,2001:7fb:fe03::/48-48;",              \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),valid,12654,2001:7fb:fe03::/48-48;",              \
        "FU-Berlin,CC06(RTR),valid,12654,84.205.67.0/24-24;",                  \
        "FU-Berlin,CC06(RTR),notfound;",                                       \
        "FU-Berlin,CC06(RTR),valid,12654,84.205.67.0/24-24;",                  \
        "FU-Berlin,CC06(RTR),valid,12654,2001:7fb:fe03::/48-48;",              \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),valid,27820,2800:130::/32-32;",                   \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;",                                       \
        "FU-Berlin,CC06(RTR),valid,12654,2001:7fb:ff02::/48-48;",              \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),valid,12654,84.205.64.0/24-24;",                  \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),valid,35226,2a02:2158::/32-32;",                  \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;",                                       \
        "FU-Berlin,CC06(RTR),valid,60822,46.23.192.0/21-21;",                  \
        "FU-Berlin,CC06(RTR),valid,60822,195.137.144.0/22-22;",                \
        "FU-Berlin,CC06(RTR),valid,60822,185.85.212.0/22-22;",                 \
        "FU-Berlin,CC06(RTR),valid,60822,46.23.204.0/22-22;",                  \
        "FU-Berlin,CC06(RTR),valid,60822,46.23.200.0/22-22;",                  \
        "FU-Berlin,CC06(RTR),valid,60822,46.23.192.0/21-21;",                  \
        "FU-Berlin,CC06(RTR),valid,60822,195.137.144.0/22-22;",                \
        "FU-Berlin,CC06(RTR),valid,60822,185.85.212.0/22-22;",                 \
        "FU-Berlin,CC06(RTR),valid,60822,46.23.204.0/22-22;",                  \
        "FU-Berlin,CC06(RTR),valid,60822,46.23.200.0/22-22;",                  \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;",                                       \
        "FU-Berlin,CC06(RTR),valid,60822,46.23.192.0/21-21;",                  \
        "FU-Berlin,CC06(RTR),valid,60822,195.137.144.0/22-22;",                \
        "FU-Berlin,CC06(RTR),valid,60822,185.85.212.0/22-22;",                 \
        "FU-Berlin,CC06(RTR),valid,60822,46.23.204.0/22-22;",                  \
        "FU-Berlin,CC06(RTR),valid,60822,46.23.200.0/22-22;",                  \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),valid,27820,2800:130::/32-32;",                   \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;",                                       \
        "FU-Berlin,CC06(RTR),valid,27807,150.185.0.0/"                         \
        "16-16;FU-Berlin,CC06(RTR),valid,20312,150.185.0.0/16-20;",            \
        "FU-Berlin,CC06(RTR),valid,20312,150.188.0.0/"                         \
        "15-24;FU-Berlin,CC06(RTR),valid,27807,150.188.0.0/15-16;",            \
        "FU-Berlin,CC06(RTR),valid,20312,150.186.0.0/"                         \
        "15-19;FU-Berlin,CC06(RTR),valid,27807,150.186.0.0/15-16;",            \
        "FU-Berlin,CC06(RTR),valid,20312,150.186.0.0/"                         \
        "15-19;FU-Berlin,CC06(RTR),valid,27807,150.186.0.0/15-16;",            \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),valid,47524,176.240.0.0/16-24;",                  \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;",                                       \
        "FU-Berlin,CC06(RTR),valid,31078,2a00:1328::/32-36;",                  \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),valid,31078,2a00:1328::/32-36;",                  \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),valid,31078,2a00:1328::/32-36;",                  \
        "FU-Berlin,CC06(RTR),valid,12654,2001:7fb:fe00::/48-48;",              \
        "FU-Berlin,CC06(RTR),valid,10091,2404:e800::/31-64;",                  \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;",                                       \
        "FU-Berlin,CC06(RTR),valid,18747,190.60.0.0/15-24;",                   \
        "FU-Berlin,CC06(RTR),valid,18747,190.60.0.0/15-24;",                   \
        "FU-Berlin,CC06(RTR),notfound;",                                       \
        "FU-Berlin,CC06(RTR),valid,12654,2001:7fb:fe00::/48-48;",              \
        "FU-Berlin,CC06(RTR),notfound;",                                       \
        "FU-Berlin,CC06(RTR),valid,12654,2001:7fb:ff02::/48-48;",              \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;",                                       \
        "FU-Berlin,CC06(RTR),valid,60822,46.23.192.0/21-21;",                  \
        "FU-Berlin,CC06(RTR),valid,60822,195.137.144.0/22-22;",                \
        "FU-Berlin,CC06(RTR),valid,60822,185.85.212.0/22-22;",                 \
        "FU-Berlin,CC06(RTR),valid,60822,46.23.204.0/22-22;",                  \
        "FU-Berlin,CC06(RTR),valid,60822,46.23.200.0/22-22;",                  \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),valid,12654,84.205.64.0/24-24;",                  \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),valid,10091,2404:e800::/31-64;",                  \
        "FU-Berlin,CC06(RTR),valid,12654,2001:7fb:ff02::/48-48;",              \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),valid,12654,84.205.64.0/24-24;",                  \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),valid,27820,2800:130::/32-32;",                   \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),valid,35226,2a02:2158::/32-32;",                  \
        "FU-Berlin,CC06(RTR),valid,31078,2a00:1328::/32-36;",                  \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),valid,201565,185.11.232.0/22-22;",                \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;",                                       \
        "FU-Berlin,CC06(RTR),invalid,9050,188.214.141.0/24-24;",               \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;",      \
        "FU-Berlin,CC06(RTR),notfound;", "FU-Berlin,CC06(RTR),notfound;"       \
  }
