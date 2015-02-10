/*
 * bgpstream
 *
 * Chiara Orsini, CAIDA, UC San Diego
 * chiara@caida.org
 *
 * Copyright (C) 2014 The Regents of the University of California.
 *
 * This file is part of bgpstream.
 *
 * bgpstream is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * bgpstream is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with bgpstream.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <assert.h>
#include <inttypes.h>
#include <stdio.h>


#include "khash.h"
#include "utils.h"

#include "bgpstream_utils_as.h"

/* AS HOP FUNCTIONS */

int bgpstream_as_hop_snprintf(char *buf, size_t len,
                             bgpstream_as_hop_t *as_hop)
{
  size_t written;

  switch(as_hop->type)
    {
    case BGPSTREAM_AS_TYPE_NUMERIC:
      written = snprintf(buf, len, "%"PRIu32, as_hop->as_number);
      break;

    case BGPSTREAM_AS_TYPE_STRING:
      written = snprintf(buf, len, "%s", as_hop->as_string);
      break;

    default:
      written = 0;
      if(len > 0)
        *buf = '\0';
      break;
    }

  return written;
}

void bgpstream_as_hop_init(bgpstream_as_hop_t *as_hop)
{
  as_hop->type = BGPSTREAM_AS_TYPE_UNKNOWN;
  as_hop->as_string = NULL;
  return;
}

void bgpstream_as_hop_clear(bgpstream_as_hop_t *as_hop)
{
  if(as_hop->type == BGPSTREAM_AS_TYPE_STRING)
    {
      free(as_hop->as_string);
      as_hop->as_string = NULL;
    }

  bgpstream_as_hop_init(as_hop);
  return;
}

int bgpstream_as_hop_copy(bgpstream_as_hop_t *dst, bgpstream_as_hop_t *src)
{
  /** check that they reset or init'd the hop */
  assert(dst->as_string == NULL);

  dst->type = src->type;

  switch(src->type)
    {
    case BGPSTREAM_AS_TYPE_NUMERIC:
      dst->as_number = src->as_number;
      break;

    case BGPSTREAM_AS_TYPE_STRING:
      if((dst->as_string = strdup(src->as_string)) == NULL)
        {
          return -1;
        }
      break;

    default:
      return -1;
      break;
    }

  return 0;
}

#if UINT_MAX == 0xffffffffu
unsigned int
#elif ULONG_MAX == 0xffffffffu
unsigned long
#endif
bgpstream_as_hop_hash(bgpstream_as_hop_t *as_hop)
{
  switch(as_hop->type)
    {
    case BGPSTREAM_AS_TYPE_NUMERIC:
      return __ac_Wang_hash(as_hop->as_number);
      break;

    case BGPSTREAM_AS_TYPE_STRING:
      return kh_str_hash_func(as_hop->as_string);
      break;

    default:
      return 0;
      break;
    }
}

int bgpstream_as_hop_equal(bgpstream_as_hop_t *as_hop1,
                           bgpstream_as_hop_t *as_hop2)
{
  if(as_hop1->type == BGPSTREAM_AS_TYPE_NUMERIC &&
     as_hop2->type == BGPSTREAM_AS_TYPE_NUMERIC)
    {
      return (as_hop1->as_number == as_hop2->as_number);
    }
  if(as_hop1->type == BGPSTREAM_AS_TYPE_STRING &&
     as_hop2->type == BGPSTREAM_AS_TYPE_STRING)
    {
      return (strcmp(as_hop1->as_string, as_hop2->as_string) == 0);
    }
  return 0;
}

/* AS PATH FUNCTIONS */

int bgpstream_as_path_snprintf(char *buf, size_t len,
                              bgpstream_as_path_t *as_path)
{
  size_t written = 0;
  char *p;
  int i;

  switch(as_path->type)
    {
    case BGPSTREAM_AS_TYPE_NUMERIC:
      p = buf;

      if(len > 0)
        *p = '\0';

      for(i=0; i<as_path->hop_count; i++)
        {
          written += snprintf(p, (len-written),
                              "%"PRIu32, as_path->numeric_aspath[i]);

          /* may seek of end of array, but won't be dereferenced */
          p = buf + written;

          if(i < as_path->hop_count-1)
            {
              if(written < len)
                {
                  *p = ' ';
                }
              written++;
              p++;
            }
        }
      break;

    case BGPSTREAM_AS_TYPE_STRING:
      written = snprintf(buf, len, "%s", as_path->str_aspath);
      break;

    default:
      /* written = 0 */
      if(len > 0)
        *buf = '\0';
      break;
    }

  return written;
}

void bgpstream_as_path_init(bgpstream_as_path_t *as_path)
{
  as_path->type = BGPSTREAM_AS_TYPE_UNKNOWN;
  as_path->hop_count = 0;
  as_path->str_aspath = NULL;
  as_path->numeric_aspath = NULL;
  return;
}

void bgpstream_as_path_clear(bgpstream_as_path_t *as_path)
{
  switch(as_path->type)
    {
    case BGPSTREAM_AS_TYPE_NUMERIC:
      free(as_path->numeric_aspath);
      break;

    case BGPSTREAM_AS_TYPE_STRING:
      free(as_path->str_aspath);
      break;

    default:
      break;
    }

  bgpstream_as_path_init(as_path);
}

int bgpstream_as_path_copy(bgpstream_as_path_t *dst, bgpstream_as_path_t *src)
{
  int i;

  /** check that they reset or init'd the path */
  assert(dst->str_aspath == NULL);

  dst->type = src->type;
  dst->hop_count = src->hop_count;

  if(src->hop_count == 0)
    {
      return 0;
    }

  switch(src->type)
    {
    case BGPSTREAM_AS_TYPE_NUMERIC:
      if((dst->numeric_aspath =
          (uint32_t *)malloc(src->hop_count * sizeof(uint32_t))) == NULL)
        {
          return -1;
        }
      for(i=0; i<src->hop_count; i++)
	{
	  dst->numeric_aspath[i] = src->numeric_aspath[i];
	}
      break;

    case BGPSTREAM_AS_TYPE_STRING:
      if((dst->str_aspath = strdup(src->str_aspath)) == NULL)
        {
          return -1;
        }
      break;

    default:
      return -1;
      break;
    }

  return 0;
}

int bgpstream_as_path_get_origin_as(bgpstream_as_path_t *as_path,
                                    bgpstream_as_hop_t *as_hop)
{
  char *p;

  /* if there are no hops, return successful, but with ASN 0 */
  if(as_path->hop_count == 0)
    {
      as_hop->type = BGPSTREAM_AS_TYPE_NUMERIC;
      as_hop->as_number = 0;
      return 0;
    }

  switch(as_path->type)
    {
    case BGPSTREAM_AS_TYPE_NUMERIC:
      as_hop->type = BGPSTREAM_AS_TYPE_NUMERIC;
      as_hop->as_number = as_path->numeric_aspath[as_path->hop_count-1];
      return 0;
      break;

    case BGPSTREAM_AS_TYPE_STRING:
      as_hop->type = BGPSTREAM_AS_TYPE_STRING;

      /* we look for the last space in the string and return what remains */
      if((p = strrchr(as_path->str_aspath, ' ')) == NULL)
        {
          /* use the entire aspath as the last hop */
          p = as_path->str_aspath;
        }

      if((as_hop->as_string = strdup(p)) == NULL)
        {
          return -1;
        }
      return 0;
      break;

    default:
      return -1;
      break;
    }
}
