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
 *
 * Authors:
 *   Chiara Orsini
 *   Alistair King
 *   Shane Alcock <salcock@waikato.ac.nz>
 *   Samir Al-Sheikh
 */

#include "bgpstream_utils_as_path_int.h"
#include "bgpstream_log.h"
#include "config.h"
#include "khash.h"
#include "utils.h"
#include <assert.h>
#include <inttypes.h>
#include <stdio.h>

#define SIZEOF_SEG_SET(segp)                                                   \
  (sizeof(bgpstream_as_path_seg_set_t) +                                       \
   (sizeof(uint32_t) * (segp)->asn_cnt))

#define SIZEOF_SEG(segp)                                                       \
  (((segp)->type == BGPSTREAM_AS_PATH_SEG_ASN)                                 \
     ? sizeof(bgpstream_as_path_seg_asn_t)                                     \
     : SIZEOF_SEG_SET(&(segp)->set))

#define CUR_SEG(path, iter)                                                    \
  ((bgpstream_as_path_seg_t *)((path)->data + (iter)->cur_offset))

static bgpstream_as_path_seg_asn_t *
seg_asn_dup(const bgpstream_as_path_seg_asn_t *src)
{
  bgpstream_as_path_seg_asn_t *seg = NULL;

  if ((seg = malloc(sizeof(bgpstream_as_path_seg_asn_t))) == NULL) {
    return NULL;
  }

  *seg = *src;

  return seg;
}

static bgpstream_as_path_seg_set_t *
seg_set_dup(const bgpstream_as_path_seg_set_t *src)
{
  bgpstream_as_path_seg_set_t *seg = NULL;
  int i;

  if ((seg = malloc(SIZEOF_SEG_SET(src))) == NULL) {
    return NULL;
  }

  *seg = *src;

  for (i = 0; i < src->asn_cnt; i++) {
    seg->asn[i] = src->asn[i];
  }

  return seg;
}

/* ========== PUBLIC FUNCTIONS ========== */

/* AS HOP FUNCTIONS */

#define ADD_CHAR(chr)                                                          \
  do {                                                                         \
    if (written < len) {                                                       \
      buf[written] = chr;                                                      \
    }                                                                          \
    written++;                                                                 \
  } while (0)

/*
 * WARNING: The output format of this function is documented in both
 * bgpstream_utils_as_path.h and _pybgpstream_bgpelem.c. Ensure both places are
 * updated if changing this format (and be very sure that you need to change it
 * at all since this is a well-known format also used by bgpdump).
 */
int bgpstream_as_path_seg_snprintf(char *buf, size_t len,
                                   const bgpstream_as_path_seg_t *seg)
{
  size_t written = 0;
  const char *chars;

  if (seg == NULL) {
    if (len > 0) {
      *buf = '\0';
    }
    return 0;
  }

  switch (seg->type) {
  case BGPSTREAM_AS_PATH_SEG_ASN:
    return snprintf(buf, len, "%" PRIu32, seg->asn.asn);

  case BGPSTREAM_AS_PATH_SEG_SET:
    /* {A,B,C} */
    chars = "{,}";
    break;

  case BGPSTREAM_AS_PATH_SEG_CONFED_SEQ:
    /* (A B C) */
    chars = "( )";
    break;

  case BGPSTREAM_AS_PATH_SEG_CONFED_SET:
    /* [A,B,C] */
    chars = "[,]";
    break;

  default:
    /* <A B C> */
    chars = "< >";
    break;
  }

  ADD_CHAR(chars[0]);
  for (int i = 0; i < seg->set.asn_cnt; i++) {
    if (i > 0) {
      ADD_CHAR(chars[1]);
    }
    size_t remain = (len <= written) ? 0 : len - written;
    written += snprintf(buf + written, remain, "%" PRIu32, seg->set.asn[i]);
  }
  ADD_CHAR(chars[2]);
  if (written < len) {
    buf[written] = '\0';
  } else if (len > 0) {
    buf[len - 1] = '\0';
  }

  return written;
}

bgpstream_as_path_seg_t *bgpstream_as_path_seg_dup(
    const bgpstream_as_path_seg_t *src)
{
  assert(src != NULL);

  assert(src->type != BGPSTREAM_AS_PATH_SEG_INVALID);

  if (src->type == BGPSTREAM_AS_PATH_SEG_ASN) {
    return (bgpstream_as_path_seg_t *)seg_asn_dup(&src->asn);
  } else {
    return (bgpstream_as_path_seg_t *)seg_set_dup(&src->set);
  }
}

void bgpstream_as_path_seg_destroy(bgpstream_as_path_seg_t *seg)
{
  free(seg);
  return;
}

inline
#if UINT_MAX == 0xffffffffu
  unsigned int
#elif ULONG_MAX == 0xffffffffu
  unsigned long
#endif
bgpstream_as_path_seg_hash(const bgpstream_as_path_seg_t *seg)
{
  if (seg == NULL) {
    return -1;
  }

  return seg->type == BGPSTREAM_AS_PATH_SEG_ASN ? seg->asn.asn :
    seg->set.asn[0];
}

int bgpstream_as_path_seg_equal(const bgpstream_as_path_seg_t *seg1,
                                const bgpstream_as_path_seg_t *seg2)
{
  if (seg1->type != seg2->type) {
    return 0;
  }

  assert(seg1->type != BGPSTREAM_AS_PATH_SEG_INVALID);

  if (seg1->type == BGPSTREAM_AS_PATH_SEG_ASN) {
    return seg1->asn.asn == seg2->asn.asn;
  } else {
    if (seg1->set.asn_cnt != seg2->set.asn_cnt) {
      return 0;
    }
    return memcmp(seg1->set.asn, seg2->set.asn,
                  sizeof(uint32_t) * seg2->set.asn_cnt) == 0;
  }
}

/* AS PATH FUNCTIONS */
int bgpstream_as_path_snprintf(char *buf, size_t len,
                               const bgpstream_as_path_t *path)
{
  bgpstream_as_path_iter_t iter;
  size_t written = 0;
  bgpstream_as_path_seg_t *seg;
  int need_sep = 0;

  /* iterate through the path and print each segment */
  bgpstream_as_path_iter_reset(&iter);
  while ((seg = bgpstream_as_path_get_next_seg(path, &iter)) != NULL) {
    if (need_sep != 0) {
      ADD_CHAR(' ');
    }
    need_sep = 1;
    size_t remain = (len <= written) ? 0 : len - written;
    written += bgpstream_as_path_seg_snprintf(buf + written, remain, seg);
  }
  if (len > 0) {
    if (written == 0) {
      buf[0] = '\0';
    } else if (written >= len) {
      buf[len - 1] = '\0';
    }
  }
  return written;
}

bgpstream_as_path_t *bgpstream_as_path_create()
{
  bgpstream_as_path_t *path;

  if ((path = malloc_zero(sizeof(bgpstream_as_path_t))) == NULL) {
    return NULL;
  }

  path->origin_offset = UINT16_MAX;

  return path;
}

void bgpstream_as_path_clear(bgpstream_as_path_t *path)
{
  path->data_len = 0;
  path->seg_cnt = 0;
  path->origin_offset = UINT16_MAX;
}

void bgpstream_as_path_destroy(bgpstream_as_path_t *path)
{
  if (path->data_alloc_len != UINT16_MAX) {
    free(path->data);
  }
  path->data = NULL;
  path->data_alloc_len = 0;
  bgpstream_as_path_clear(path);
  free(path);
}

int bgpstream_as_path_copy(bgpstream_as_path_t *dst,
    const bgpstream_as_path_t *src)
{
  if (dst->data_alloc_len == UINT16_MAX) {
    /* no longer points to external memory */
    dst->data_alloc_len = 0;
  }
  if (dst->data_alloc_len < src->data_len) {
    if ((dst->data = realloc(dst->data, src->data_len)) == NULL) {
      return -1;
    }
    dst->data_alloc_len = src->data_len;
  }

  memcpy(dst->data, src->data, src->data_len);
  dst->data_len = src->data_len;

  dst->seg_cnt = src->seg_cnt;
  dst->origin_offset = src->origin_offset;

  return 0;
}

bgpstream_as_path_seg_t *
bgpstream_as_path_get_origin_seg(bgpstream_as_path_t *path)
{
  /*assert(path != NULL);*/

  if (path->data_len == 0) {
    return NULL;
  }

  /*assert(path->data != NULL);*/
  /*assert(path->origin_offset != UINT16_MAX);*/

  return (bgpstream_as_path_seg_t *)(path->data + path->origin_offset);
}

int bgpstream_as_path_get_origin_val(bgpstream_as_path_t *path, uint32_t *asn)
{
  bgpstream_as_path_seg_t *origin_seg = bgpstream_as_path_get_origin_seg(path);
  if (origin_seg == NULL || origin_seg->type != BGPSTREAM_AS_PATH_SEG_ASN) {
    return -1;
  } else {
    *asn = origin_seg->asn.asn;
    return 0;
  }
}

void bgpstream_as_path_iter_reset(bgpstream_as_path_iter_t *iter)
{
  iter->cur_offset = 0;
}

bgpstream_as_path_seg_t *
bgpstream_as_path_get_next_seg(const bgpstream_as_path_t *path,
                               bgpstream_as_path_iter_t *iter)
{
  bgpstream_as_path_seg_t *cur_seg;

  /* end of path */
  if (path->data_len == 0 || iter->cur_offset >= path->data_len) {
    return NULL;
  }

  cur_seg = CUR_SEG(path, iter);

  /* be sure that this segment is not corrupt */
  /*assert((iter->cur_offset + SIZEOF_SEG(cur_seg)) <= path->data_len);*/
  /*assert(cur_seg->type != BGPSTREAM_AS_PATH_SEG_INVALID);*/

  /* calculate the size of the current segment and skip over it*/
  iter->cur_offset += SIZEOF_SEG(cur_seg);

  return cur_seg;
}

int bgpstream_as_path_get_len(bgpstream_as_path_t *path)
{
  return path->seg_cnt;
}

uint16_t bgpstream_as_path_get_data(bgpstream_as_path_t *path, uint8_t **data)
{
  assert(data != NULL);
  if (path == NULL) {
    *data = NULL;
    return 0;
  } else {
    *data = path->data;
    return path->data_len;
  }
}

int bgpstream_as_path_populate_from_data(bgpstream_as_path_t *path,
                                         uint8_t *data, uint16_t data_len)
{
  /*assert(path != NULL);*/

  bgpstream_as_path_clear(path);

  if (path->data_alloc_len == UINT16_MAX) {
    /* the data is not owned by us */
    path->data_alloc_len = 0;
    path->data = NULL;
  }

  if (path->data_alloc_len < data_len) {
    if ((path->data = realloc(path->data, data_len)) == NULL) {
      return -1;
    }
    path->data_alloc_len = data_len;
  }

  memcpy(path->data, data, data_len);
  path->data_len = data_len;

  bgpstream_as_path_update_fields(path);

  return 0;
}

int bgpstream_as_path_populate_from_data_zc(bgpstream_as_path_t *path,
                                            uint8_t *data, uint16_t data_len)
{
  /*assert(path != NULL);*/

  bgpstream_as_path_clear(path);

  /* signal that this is external data */
  path->data_alloc_len = UINT16_MAX;
  path->data = data;
  path->data_len = data_len;

  bgpstream_as_path_update_fields(path);

  return 0;
}

/* from http://burtleburtle.net/bob/hash/integer.html */
static inline uint32_t mixbits(uint32_t a)
{
  a = a ^ (a >> 4);
  a = (a ^ 0xdeadbeef) + (a << 5);
  a = a ^ (a >> 11);
  return a;
}

#if UINT_MAX == 0xffffffffu
unsigned int
#elif ULONG_MAX == 0xffffffffu
unsigned long
#endif
bgpstream_as_path_hash(const bgpstream_as_path_t *path)
{
  if (path->data_len > 0) {
    /* put the peer (ish) hash into the top bits */
    /* and put the origin hash into the bottom bits */
    return mixbits(
      ((bgpstream_as_path_seg_hash((const bgpstream_as_path_seg_t *)path->data) &
        0xFFFF)
       << 8) |
      (bgpstream_as_path_seg_hash(
         (const bgpstream_as_path_seg_t *)(path->data + path->origin_offset)) &
       0xFFFF));
  } else {
    return 0;
  }
}

inline int bgpstream_as_path_equal(const bgpstream_as_path_t *path1,
                                   const bgpstream_as_path_t *path2)
{
  return (path1->data_len == path2->data_len) &&
         !memcmp(path1->data, path2->data, path1->data_len);
}

/* ========== PRIVATE FUNCTIONS ========== */

#ifdef PATH_COPY_DEBUG
static void test_path_copy(bgpstream_as_path_t *path)
{
  /* DEBUG */
  bgpstream_as_path_t *newpath = bgpstream_as_path_create();
  assert(newpath != NULL);
  bgpstream_as_path_t *corepath = bgpstream_as_path_create();
  assert(corepath != NULL);

  assert(bgpstream_as_path_equal(newpath, corepath));

  bgpstream_as_path_copy(newpath, path, 0, 0);
  assert(bgpstream_as_path_equal(path, newpath));

  char buf1[8000];
  bgpstream_as_path_snprintf(buf1, 8000, path);
  char buf2[8000];
  bgpstream_as_path_snprintf(buf2, 8000, newpath);
  assert(strcmp(buf1, buf2) == 0);

  // core path
  bgpstream_as_path_copy(corepath, path, 1, 1);
  char buf3[8000];
  bgpstream_as_path_snprintf(buf3, 8000, corepath);

  if (bgpstream_as_path_get_len(path) > 0 &&
      bgpstream_as_path_equal(path, corepath) != 0) {
    bgpstream_log(BGPSTREAM_LOG_ERR, "%s|%s", buf1, buf3);
    assert(0);
  }

  // orig, copy, core
  fprintf(stdout, "PATH_COPY|%s|%s|%s\n", buf1, buf2, buf3);

  // core origin
  char buf4[8000];
  bgpstream_as_path_seg_snprintf(buf4, 8000,
                                 bgpstream_as_path_get_origin_seg(path));
  char buf5[8000];
  bgpstream_as_path_seg_snprintf(buf5, 8000,
                                 bgpstream_as_path_get_origin_seg(newpath));
  char buf6[8000];
  bgpstream_as_path_seg_snprintf(buf6, 8000,
                                 bgpstream_as_path_get_origin_seg(corepath));
  fprintf(stdout, "PATH_ORIGIN|%s:%s|%s:%s|%s:%s\n", buf1, buf4, buf2, buf5,
          buf3, buf6);

#if 0
  // hashing
  fprintf(stdout, "PATH_HASH|%s:%"PRIu32"|%s:%"PRIu32"\n",
          buf1, bgpstream_as_path_hash(path),
          buf3, bgpstream_as_path_hash(corepath));
#endif

  bgpstream_as_path_destroy(newpath);
  bgpstream_as_path_destroy(corepath);
}
#endif

int bgpstream_as_path_append(bgpstream_as_path_t *path,
                             bgpstream_as_path_seg_type_t type, uint32_t *asns,
                             int asns_cnt)
{
  bgpstream_as_path_seg_t *seg;
  bgpstream_as_path_iter_t iter;

  /* careful, this is stored into a uint16_t */
  size_t new_len;

  int i;

  // seek the iterator to the end of the path where we'll add our new segment
  iter.cur_offset = path->data_len;

  /* ensure that the path data buffer is long enough */
  if (type == BGPSTREAM_AS_PATH_SEG_ASN) {
    new_len = path->data_len + (sizeof(bgpstream_as_path_seg_asn_t) * asns_cnt);
    assert(new_len < UINT16_MAX);
  } else {
    /* a set */
    new_len = path->data_len + sizeof(bgpstream_as_path_seg_set_t) +
              (sizeof(uint32_t) * asns_cnt);
    assert(new_len < UINT16_MAX);
  }

  if (path->data_alloc_len < new_len) {
    if ((path->data = realloc(path->data, new_len)) == NULL) {
      return -1;
    }
    path->data_alloc_len = new_len;
  }
  path->data_len = new_len;

  // get a pointer to the newly added segment
  seg = CUR_SEG(path, &iter);

  seg->type = type;

  // TODO: this code will allow adjacent segments of the same type to be added
  // which is technically illegal.

  // if all the ASes we've been given will go in a single path segment, update
  // the path origin pointer, and set the number of ASes now
  if (type != BGPSTREAM_AS_PATH_SEG_ASN) {
    path->origin_offset = iter.cur_offset;
    seg->set.asn_cnt = asns_cnt;
  }

  // loop through the ASNs and add them
  for (i = 0; i < asns_cnt; i++) {
    // if this is a normal "sequence" segment, split it into multiple
    if (type == BGPSTREAM_AS_PATH_SEG_ASN) {
      seg->type = BGPSTREAM_AS_PATH_SEG_ASN;
      seg->asn.asn = asns[i];

      /* move on to the next segment (already alloc'd) */
      path->origin_offset = iter.cur_offset;
      iter.cur_offset += sizeof(bgpstream_as_path_seg_asn_t);
      path->seg_cnt++;
      seg = CUR_SEG(path, &iter);
    } else {
      // just add this ASN to the segment
      seg->set.asn[i] = asns[i];
    }
  }

  if (type != BGPSTREAM_AS_PATH_SEG_ASN) {
    iter.cur_offset = new_len;
    path->seg_cnt++;
  }

  return 0;
}

void bgpstream_as_path_update_fields(bgpstream_as_path_t *path)
{
  bgpstream_as_path_iter_t iter;
  uint16_t offset = 0;
  bgpstream_as_path_seg_t *seg;

  /* walk the path to find the seg_cnt and origin_offset */
  bgpstream_as_path_iter_reset(&iter);
  path->seg_cnt = 0;

  while ((seg = bgpstream_as_path_get_next_seg(path, &iter)) != NULL) {
    path->origin_offset = offset;
    path->seg_cnt++;
    offset += SIZEOF_SEG(seg);
  }
}
