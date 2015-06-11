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

static void path_clear(bgpstream_as_path_t *path)
{
  path->data_len = 0;
  path->seg_cnt = 0;
  path->cur_offset = 0;
  path->origin_offset = -1;
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
  if(seg->type == BGPSTREAM_AS_PATH_SEG_ASN)
    {
      return __ac_Wang_hash(((bgpstream_as_path_seg_asn_t*)seg)->asn);
    }

  return __ac_Wang_hash(((bgpstream_as_path_seg_set_t*)seg)->asn[0]);
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
      return memcmp(((bgpstream_as_path_seg_set_t*)seg1)->asn,
                    ((bgpstream_as_path_seg_set_t*)seg2)->asn,
                    sizeof(uint32_t) *
                    ((bgpstream_as_path_seg_set_t*)seg2)->asn_cnt);
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

void bgpstream_as_path_destroy(bgpstream_as_path_t *path)
{
  free(path->data);
  path->data = NULL;
  path->data_alloc_len = 0;
  path_clear(path);
  free(path);
}

int bgpstream_as_path_copy(bgpstream_as_path_t *dst, bgpstream_as_path_t *src)
{

  /* check that there is enough space in the destination data array */
  if(dst->data_alloc_len < src->data_len)
    {
      if((dst->data = malloc(src->data_len)) == NULL)
        {
          return -1;
        }
      dst->data_alloc_len = src->data_alloc_len;
    }

  /* copy the data */
  memcpy(dst->data, src->data, src->data_len);

  dst->data_len = src->data_len;
  dst->seg_cnt = src->seg_cnt;
  bgpstream_as_path_reset_iter(dst);

  return 0;
}

bgpstream_as_path_seg_t *
bgpstream_as_path_get_origin_as(bgpstream_as_path_t *path)
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

/* ========== PRIVATE FUNCTIONS ========== */

int bgpstream_as_path_populate(bgpstream_as_path_t *path,
                               struct aspath *bd_path)
{
  uint8_t *ptr;
  uint8_t *end;

  struct assegment *bd_seg;
  bgpstream_as_path_seg_t *seg;

  size_t new_len;

  int i;

  uint16_t tmp16;
  uint32_t asn;

  assert(path != NULL);
  assert(bd_path != NULL);

  ptr = (uint8_t*)bd_path->data;
  end = ptr + bd_path->length;

  /* we are going to overwrite the current path */
  path_clear(path);

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
          return -1;
        }

      /* Check AS length. */
      if ((ptr + (bd_seg->length * bd_path->asn_len) + AS_HEADER_SIZE) > end)
      {
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
              memcpy(&tmp16, &bd_seg->data+(i*bd_path->asn_len),
                     sizeof(uint16_t));
              asn = ntohs(tmp16);
              break;
            case ASN32_LEN:
              memcpy(&asn, &bd_seg->data+(i*bd_path->asn_len),
                     sizeof(uint32_t));
              asn = ntohl(asn);
              break;
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
  if(written >= 8000 || strcmp(buffer, bd_path->str) != 0)
    {
      fprintf(stderr, "Written: %lu\n", written);
      fprintf(stderr, "BS Path: >>%s<<\n", buffer);
      fprintf(stderr, "BD Path: %s\n", bd_path->str);
      assert(0);
      return -1;
    }
#endif

  return 0;
}
