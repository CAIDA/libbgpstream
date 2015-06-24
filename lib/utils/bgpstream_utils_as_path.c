/*
 * This file is part of bgpstream
 *
 * Copyright (C) 2015 The Regents of the University of California.
 * Authors: Alistair King, Chiara Orsini
 *
 * All rights reserved.
 *
 * This code has been developed by CAIDA at UC San Diego.
 * For more information, contact bgpstream-info@caida.org
 *
 * This source code is proprietary to the CAIDA group at UC San Diego and may
 * not be redistributed, published or disclosed without prior permission from
 * CAIDA.
 *
 * Report any bugs, questions or comments to bgpstream-info@caida.org
 *
 */

#include "config.h"

#include <assert.h>
#include <inttypes.h>
#include <stdio.h>

#include "bgpdump_lib.h"
#include "khash.h"
#include "utils.h"

#include "bgpstream_utils_as_path.h"

#define SIZEOF_SEG_SET(segp)                                            \
  (sizeof(bgpstream_as_path_seg_set_t) +                                \
   (sizeof(uint32_t)*((bgpstream_as_path_seg_set_t*)(segp))->asn_cnt))

#define SIZEOF_SEG(segp)                        \
  (((segp)->type == BGPSTREAM_AS_PATH_SEG_ASN) ?        \
   sizeof(bgpstream_as_path_seg_asn_t) :                \
   SIZEOF_SEG_SET(segp))

#define CUR_SEG(path)                           \
  ((bgpstream_as_path_seg_t *)(path->data+path->cur_offset))

struct bgpstream_as_path {

  /* byte array of segments */
  uint8_t *data;

  /* length of the byte array in use */
  size_t data_len;

  /* allocated length of the byte array */
  size_t data_alloc_len;

  /** The number of segments in the path */
  int seg_cnt;

  /* current offset into the data buffer (for iterating) */
  size_t cur_offset;

  /* offset of the origin segment */
  ssize_t origin_offset;

};

static bgpstream_as_path_seg_asn_t *seg_asn_dup(bgpstream_as_path_seg_asn_t *src)
{
  bgpstream_as_path_seg_asn_t *seg = NULL;

  if((seg = malloc(sizeof(bgpstream_as_path_seg_asn_t))) == NULL)
    {
      return NULL;
    }

  *seg = *src;

  return seg;
}

static bgpstream_as_path_seg_set_t *seg_set_dup(bgpstream_as_path_seg_set_t *src)
{
  bgpstream_as_path_seg_set_t *seg = NULL;
  int i;

  if((seg = malloc(SIZEOF_SEG_SET(src))) == NULL)
    {
      return NULL;
    }

  *seg = *src;

  for(i=0; i<src->asn_cnt; i++)
    {
      seg->asn[i] = src->asn[i];
    }

  return seg;
}

/* ========== PUBLIC FUNCTIONS ========== */

/* AS HOP FUNCTIONS */

#define ADD_CHAR(chr)                           \
  do {                                          \
    if((len-written) > 0)                       \
      {                                         \
        *bufp = chr;                            \
        bufp++;                                 \
      }                                         \
    written++;                                  \
  } while(0)

#define SET_SNPRINTF(fchr, lchr, schr)          \
  do {                                          \
    char *bufp = buf;                                                   \
    bgpstream_as_path_seg_set_t *segset = (bgpstream_as_path_seg_set_t*)seg; \
    ADD_CHAR(fchr);                                                     \
    int i;                                                              \
    for(i=0; i<segset->asn_cnt; i++)                                    \
      {                                                                 \
        written += snprintf(bufp, (len-written), "%"PRIu32, segset->asn[i]); \
        bufp = buf + written;                                           \
        if(i < segset->asn_cnt-1)                                       \
          {                                                             \
            ADD_CHAR(schr);                                             \
          }                                                             \
      }                                                                 \
    ADD_CHAR(lchr);                                                     \
    if((len-written) > 0) *bufp = '\0';                                 \
  } while(0)

int bgpstream_as_path_seg_snprintf(char *buf, size_t len,
                                   bgpstream_as_path_seg_t *seg)
{
  size_t written = 0;

  if(seg == NULL)
    {
      if(len > 0)
        {
          *buf = '\0';
        }
      return 0;
    }

  switch(seg->type)
    {
    case BGPSTREAM_AS_PATH_SEG_ASN:
      written = snprintf(buf, len, "%"PRIu32,
                         ((bgpstream_as_path_seg_asn_t*)seg)->asn);
      break;

    case BGPSTREAM_AS_PATH_SEG_SET:
      /* {A,B,C} */
      SET_SNPRINTF('{', '}', ',');
      break;

    case BGPSTREAM_AS_PATH_SEG_CONFED_SET:
      /* [A,B,C] */
      SET_SNPRINTF('[', ']', ',');
      break;

    case BGPSTREAM_AS_PATH_SEG_CONFED_SEQ:
      /* (A B C) */
      SET_SNPRINTF('(', ')', ' ');
      break;

    default:
      written = 0;
      if(len > 0)
        *buf = '\0';
      break;
    }

  if(written > len)
    {
      buf[len-1] = '\0';
    }

  return written;
}

bgpstream_as_path_seg_t *bgpstream_as_path_seg_dup(bgpstream_as_path_seg_t *src)
{
  assert(src != NULL);

  assert(src->type != BGPSTREAM_AS_PATH_SEG_INVALID);

  if(src->type == BGPSTREAM_AS_PATH_SEG_ASN)
    {
      return (bgpstream_as_path_seg_t*)
        seg_asn_dup((bgpstream_as_path_seg_asn_t*)src);
    }
  else
    {
      return (bgpstream_as_path_seg_t*)
        seg_set_dup((bgpstream_as_path_seg_set_t*)src);
    }
}

void bgpstream_as_path_seg_destroy(bgpstream_as_path_seg_t *seg)
{
  free(seg);
  return;
}

#if UINT_MAX == 0xffffffffu
unsigned int
#elif ULONG_MAX == 0xffffffffu
unsigned long
#endif
bgpstream_as_path_seg_hash(bgpstream_as_path_seg_t *seg)
{
  if(seg == NULL)
    {
      return -1;
    }

  if(seg->type == BGPSTREAM_AS_PATH_SEG_ASN)
    {
      return ((bgpstream_as_path_seg_asn_t*)seg)->asn;
    }

  return ((bgpstream_as_path_seg_set_t*)seg)->asn[0];
}

int bgpstream_as_path_seg_equal(bgpstream_as_path_seg_t *seg1,
                                bgpstream_as_path_seg_t *seg2)
{
  if(seg1->type != seg2->type)
    {
      return 0;
    }

  assert(seg1->type != BGPSTREAM_AS_PATH_SEG_INVALID);

  if(seg1->type == BGPSTREAM_AS_PATH_SEG_ASN)
    {
      return ((bgpstream_as_path_seg_asn_t*)seg1)->asn ==
        ((bgpstream_as_path_seg_asn_t*)seg2)->asn;
    }
  else
    {
      if(((bgpstream_as_path_seg_set_t*)seg1)->asn_cnt !=
         ((bgpstream_as_path_seg_set_t*)seg2)->asn_cnt)
        {
          return 0;
        }
      return bcmp(((bgpstream_as_path_seg_set_t*)seg1)->asn,
                  ((bgpstream_as_path_seg_set_t*)seg2)->asn,
                  sizeof(uint32_t) *
                  ((bgpstream_as_path_seg_set_t*)seg2)->asn_cnt) == 0;
    }
}

/* AS PATH FUNCTIONS */

int bgpstream_as_path_snprintf(char *buf, size_t len,
                               bgpstream_as_path_t *path)
{
  size_t written = 0;
  bgpstream_as_path_seg_t *seg;
  int need_sep = 0;
  char *bufp = buf;

  /* iterate through the path and print each segment */
  bgpstream_as_path_reset_iter(path);
  while((seg = bgpstream_as_path_get_next_seg(path)) != NULL)
    {
      if(need_sep != 0)
        {
          ADD_CHAR(' ');
        }
      need_sep = 1;
      written += bgpstream_as_path_seg_snprintf(bufp, (len-written), seg);
      bufp = buf+written;
    }
  *bufp = '\0';

  if(written > len)
    {
      buf[len-1] = '\0';
    }
  return written;
}

bgpstream_as_path_t *bgpstream_as_path_create()
{
  bgpstream_as_path_t *path;

  if((path = malloc_zero(sizeof(bgpstream_as_path_t))) == NULL)
    {
      return NULL;
    }

  path->origin_offset = -1;

  return path;
}

void bgpstream_as_path_clear(bgpstream_as_path_t *path)
{
  path->data_len = 0;
  path->seg_cnt = 0;
  path->cur_offset = 0;
  path->origin_offset = -1;
}

void bgpstream_as_path_destroy(bgpstream_as_path_t *path)
{
  free(path->data);
  path->data = NULL;
  path->data_alloc_len = 0;
  bgpstream_as_path_clear(path);
  free(path);
}

int bgpstream_as_path_copy(bgpstream_as_path_t *dst, bgpstream_as_path_t *src,
                           int first_seg_idx, int excl_last_seg)
{
  size_t new_len = src->data_len;
  int new_seg_cnt = src->seg_cnt;
  int i;
  bgpstream_as_path_seg_t *seg;
  size_t seg_size = 0;
  size_t new_begin_offset = 0;
  size_t new_origin_offset = 0;

  /* set dst to the empty path */
  bgpstream_as_path_clear(dst);

  if(new_len == 0 || new_seg_cnt == 0)
    {
      /* copy nothing, dst is already empty */
      return 0;
    }

  assert(src->origin_offset >= 0);

  if(excl_last_seg != 0)
    {
      /* subtract the origin from the end of the path */
      new_len = src->origin_offset;
      new_seg_cnt--;

      /* is the path now empty? */
      if(new_len == 0 || new_seg_cnt == 0)
        {
          /* copy nothing */
          return 0;
        }
    }

  /* is there enough segments to satisfy the first_seg_idx requirement? */
  if(new_seg_cnt < (first_seg_idx+1))
    {
      /* copy nothing */
      return 0;
    }

  for(i=0; i<new_seg_cnt; i++)
    {
      seg = (bgpstream_as_path_seg_t*)(src->data+new_begin_offset);
      seg_size = SIZEOF_SEG(seg);
      if(i < first_seg_idx)
        {
          new_begin_offset += seg_size;
        }

      /* we're also looking for the new origin offset */
      if(i < new_seg_cnt-1)
        {
          new_origin_offset += seg_size;
        }
    }
  new_len -= new_begin_offset;
  new_seg_cnt -= first_seg_idx;

  /* ensure we have enough space */
  if(dst->data_alloc_len < new_len)
    {
      if((dst->data = realloc(dst->data, new_len)) == NULL)
        {
          return -1;
        }
      dst->data_alloc_len = new_len;
    }

  /* copy the subpath into dst */
  memcpy(dst->data, src->data+new_begin_offset, new_len);

  /* update dst info */
  dst->data_len = new_len;
  dst->seg_cnt = new_seg_cnt;

  bgpstream_as_path_reset_iter(dst);

  dst->origin_offset = new_origin_offset - new_begin_offset;

  return 0;
}

bgpstream_as_path_seg_t *
bgpstream_as_path_get_origin_seg(bgpstream_as_path_t *path)
{
  assert(path != NULL);

  if(path->data_len == 0)
    {
      return NULL;
    }

  assert(path->data != NULL);

  return (bgpstream_as_path_seg_t*)(path->data+path->origin_offset);
}

void bgpstream_as_path_reset_iter(bgpstream_as_path_t *path)
{
  path->cur_offset = 0;
}

bgpstream_as_path_seg_t *
bgpstream_as_path_get_next_seg(bgpstream_as_path_t *path)
{
  bgpstream_as_path_seg_t *cur_seg;

  /* end of path */
  if(path->data_len == 0 || path->cur_offset >= path->data_len)
    {
      return NULL;
    }

  cur_seg = CUR_SEG(path);

  /* be sure that this segment is not corrupt */
  assert((path->cur_offset + SIZEOF_SEG(cur_seg)) <= path->data_len);
  assert(cur_seg->type != BGPSTREAM_AS_PATH_SEG_INVALID);

  /* calculate the size of the current segment and skip over it*/
  path->cur_offset += SIZEOF_SEG(cur_seg);

  return cur_seg;
}

int bgpstream_as_path_get_len(bgpstream_as_path_t *path)
{
  return path->seg_cnt;
}

size_t bgpstream_as_path_get_data(bgpstream_as_path_t *path, uint8_t **data)
{
  assert(data != NULL);
  if(path == NULL)
    {
      *data = NULL;
      return 0;
    }
  else
    {
      *data = path->data;
      return path->data_len;
    }
}

/** @todo consider making a populate_zero_copy function that refs external
    memory */
int bgpstream_as_path_populate_from_data(bgpstream_as_path_t *path,
                                         uint8_t *data, size_t data_len)
{
  size_t offset = 0;
  bgpstream_as_path_seg_t *seg;

  assert(path != NULL);

  bgpstream_as_path_clear(path);

  if(path->data_alloc_len < data_len)
    {
      if((path->data = realloc(path->data, data_len)) == NULL)
        {
          return -1;
        }
      path->data_alloc_len = data_len;
    }

  memcpy(path->data, data, data_len);
  path->data_len = data_len;

  /* walk the path to find the seg_cnt and origin_offset */
  bgpstream_as_path_reset_iter(path);
  path->seg_cnt = 0;

  while((seg = bgpstream_as_path_get_next_seg(path)) != NULL)
    {
      path->origin_offset = offset;
      path->seg_cnt++;
      offset += SIZEOF_SEG(seg);
    }

  return 0;
}

#if UINT_MAX == 0xffffffffu
unsigned int
#elif ULONG_MAX == 0xffffffffu
unsigned long
#endif
bgpstream_as_path_hash(bgpstream_as_path_t *path)
{
  uint32_t hash;
  bgpstream_as_path_seg_t *seg;

  if(path->data_len > 0)
    {
      /* put the peer (ish) hash into the top bits */
      seg = (bgpstream_as_path_seg_t*)path->data;
      hash = (bgpstream_as_path_seg_hash(seg) & 0xFFFF) << 16;

      /* put the origin hash into the bottom bits */
      seg = bgpstream_as_path_get_origin_seg(path);
      hash |= bgpstream_as_path_seg_hash(seg) & 0xFFFF;
    }
  else
    {
      hash = 0;
    }

  return hash;
}

int bgpstream_as_path_equal(bgpstream_as_path_t *path1,
                            bgpstream_as_path_t *path2)
{
  if(path1->data_len != path2->data_len)
    {
      return 0;
    }
  if(path1->seg_cnt != path2->seg_cnt)
    {
      return 0;
    }
  if(bcmp(path1->data, path2->data, path1->data_len) != 0)
    {
      return 0;
    }
  return 1;
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

  if(bgpstream_as_path_get_len(path) > 0 &&
     bgpstream_as_path_equal(path, corepath) != 0)
    {
      fprintf(stderr, "ERROR: %s|%s\n", buf1, buf3);
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
  fprintf(stdout, "PATH_ORIGIN|%s:%s|%s:%s|%s:%s\n",
          buf1, buf4, buf2, buf5, buf3, buf6);

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

int bgpstream_as_path_populate(bgpstream_as_path_t *path,
                               struct aspath *bd_path)
{
  uint8_t *ptr;
  uint8_t *end;

  struct assegment *bd_seg;
  bgpstream_as_path_seg_t *seg;

  size_t new_len;

  int i;

  uint16_t *ptr16;
  uint32_t *ptr32;
  uint32_t asn;

  assert(path != NULL);
  assert(bd_path != NULL);

  ptr = (uint8_t*)bd_path->data;
  end = ptr + bd_path->length;

  /* we are going to overwrite the current path */
  bgpstream_as_path_clear(path);

  bgpstream_as_path_seg_type_t last_type = BGPSTREAM_AS_PATH_SEG_ASN;

  while(ptr < end)
    {
      bd_seg = (struct assegment *)ptr;

      /* Check AS type validity. */
      if((bd_seg->type != AS_SET) &&
         (bd_seg->type != AS_SEQUENCE) &&
         (bd_seg->type != AS_CONFED_SET) &&
         (bd_seg->type != AS_CONFED_SEQUENCE))
        {
          fprintf(stderr, "WARN: AS_PATH with segment type %d\n", bd_seg->type);
          return -1;
        }

      /* Check AS length. */
      if ((ptr + (bd_seg->length * bd_path->asn_len) + AS_HEADER_SIZE) > end)
      {
        fprintf(stderr, "ERROR: Corrupted AS_PATH\n");
        return -1;
      }

      /* ensure that the data buffer is long enough */
      if(bd_seg->type == AS_SEQUENCE)
        {
          new_len = path->data_len +
            (sizeof(bgpstream_as_path_seg_asn_t) * bd_seg->length);
        }
      else
        {
          /* a set */
          new_len = path->data_len + sizeof(bgpstream_as_path_seg_set_t) +
            (sizeof(uint32_t) * bd_seg->length);
        }

      if(path->data_alloc_len < new_len)
        {
          if((path->data = realloc(path->data, new_len)) == NULL)
            {
              return -1;
            }
          path->data_alloc_len = new_len;
        }
      path->data_len = new_len;

      seg = CUR_SEG(path);
      switch(bd_seg->type)
        {
        case AS_SET:
          seg->type = BGPSTREAM_AS_PATH_SEG_SET;
          break;

        case AS_SEQUENCE:
          /* we break sequences into many segments */
          break;

        case AS_CONFED_SET:
          seg->type = BGPSTREAM_AS_PATH_SEG_CONFED_SET;
          break;

        case AS_CONFED_SEQUENCE:
          seg->type = BGPSTREAM_AS_PATH_SEG_CONFED_SEQ;
          break;
        }

      /** @todo consider merging consecutive segments of the same type? */
      if(last_type != BGPSTREAM_AS_PATH_SEG_ASN && seg->type == last_type)
        {
          fprintf(stderr, "ERROR: Consecutive segments of identical type\n");
          fprintf(stderr, "ERROR: This is an unhandled error.\n");
          fprintf(stderr, "ERROR: Contact bgpstream-info@caida.org\n");
          assert(0);
          return -1;
        }

      if(bd_seg->type != AS_SEQUENCE)
        {
          path->origin_offset = path->cur_offset;
          ((bgpstream_as_path_seg_set_t*)seg)->asn_cnt = bd_seg->length;
        }

      for(i=0; i<bd_seg->length; i++)
        {
          switch(bd_path->asn_len)
            {
            case ASN16_LEN:
              ptr16 = (uint16_t*) (bd_seg->data+(i*bd_path->asn_len));
              asn = ntohs(*ptr16);
              break;
            case ASN32_LEN:
              ptr32 = (uint32_t*) (bd_seg->data+(i*bd_path->asn_len));
              asn = ntohl(*ptr32);
              break;
            default:
              asn = 0;
              assert(0);
              return -1;
            }

          if(bd_seg->type == AS_SEQUENCE)
            {
              seg->type = BGPSTREAM_AS_PATH_SEG_ASN;
              ((bgpstream_as_path_seg_asn_t*)seg)->asn = asn;

              /* move on */
              path->origin_offset = path->cur_offset;
              path->cur_offset += sizeof(bgpstream_as_path_seg_asn_t);
              path->seg_cnt++;
              seg = CUR_SEG(path);
            }
          else
            {
              ((bgpstream_as_path_seg_set_t*)seg)->asn[i] = asn;
            }
        }

      if(bd_seg->type != AS_SEQUENCE)
        {
          path->cur_offset = new_len;
          path->seg_cnt++;
        }

      ptr += (bd_seg->length * bd_path->asn_len) + AS_HEADER_SIZE;
    }

  bgpstream_as_path_reset_iter(path);

  /* the following will cause a performance hit. only enable if debugging path
     parsing */
#ifdef PATH_DEBUG
  char buffer[8000];
  size_t written = bgpstream_as_path_snprintf(buffer, 8000, path);
  if(bd_path->str != NULL)
    {
      fprintf(stdout, "warn: as path string populated\n");
    }
  process_attr_aspath_string(bd_path);
  if(written >= 8000 || strcmp(buffer, bd_path->str) != 0)
    {
      fprintf(stderr, "Written: %lu\n", written);
      fprintf(stderr, "BS Path: >>%s<<\n", buffer);
      fprintf(stderr, "BD Path: %s\n", bd_path->str);
      assert(0);
      return -1;
    }
#endif

#ifdef PATH_COPY_DEBUG
  test_path_copy(path);
#endif

  return 0;
}
