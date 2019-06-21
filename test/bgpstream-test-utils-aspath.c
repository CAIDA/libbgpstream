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

#include "bgpstream_test.h"
#include "bgpstream_utils_as_path_int.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char buffer[1024];


static struct {
  bgpstream_as_path_seg_type_t type;
  int cnt;
  uint32_t asns[10];
  const char *expected;
} testsegs[] = {
  { BGPSTREAM_AS_PATH_SEG_ASN,        4, { 11, 12, 13, 14 }, "11 12 13 14"},
  { BGPSTREAM_AS_PATH_SEG_SET,        3, { 21, 22, 23 },     "{21,22,23}"},
  { BGPSTREAM_AS_PATH_SEG_CONFED_SEQ, 4, { 31, 32, 33, 34 }, "(31 32 33 34)"},
  { BGPSTREAM_AS_PATH_SEG_CONFED_SET, 3, { 41, 42, 43 },     "[41,42,43]"},
  { 99 /* invalid */,                 2, { 991, 992 },       "<991 992>"},
  { -1,                              -1, { -1 },             NULL },
};

int main(int argc, char *argv[])
{
  int test_cnt = 0;
  char expected[1024] = "";
  bgpstream_as_path_seg_t *seg;
  bgpstream_as_path_t *path1 = bgpstream_as_path_create();
  bgpstream_as_path_t *path2 = bgpstream_as_path_create();
  CHECK("as_path create", path1 && path2);

  for (int i = 0; testsegs[i].cnt >= 0; i++) {
    CHECK("as_path append segment", bgpstream_as_path_append(path1,
          testsegs[i].type, testsegs[i].asns, testsegs[i].cnt) == 0);
    CHECK("as_path unequal/copy/equal",
        !bgpstream_as_path_equal(path1, path2) &&
        bgpstream_as_path_copy(path2, path1) == 0 &&
        bgpstream_as_path_equal(path1, path2));
    seg = bgpstream_as_path_get_origin_seg(path1);
    CHECK("as_path get_origin_seg", seg && seg->type == testsegs[i].type);
    if (seg->type == BGPSTREAM_AS_PATH_SEG_ASN) {
      // origin should be the single last ASN in the testseg
      CHECK("as_path origin asn",
          seg->asn.asn == testsegs[i].asns[testsegs[i].cnt - 1]);
      test_cnt += testsegs[i].cnt;
    } else {
      // origin should be the entire last testseg
      CHECK("as_path origin set",
          seg->set.asn_cnt == testsegs[i].cnt &&
          memcmp(seg->set.asn, testsegs[i].asns, testsegs[i].cnt) == 0);
      test_cnt++;
    }
    if (i > 0) strcat(expected, " ");
    strcat(expected, testsegs[i].expected);
    bgpstream_as_path_snprintf(buffer, sizeof(buffer), path1);
    CHECK("as_path print", strcmp(buffer, expected) == 0);
    // printf("# output:  %s\n", buffer);
  }

  CHECK("as_path len", bgpstream_as_path_get_len(path1) == test_cnt);

  ENDTEST;
  return 0;
}

