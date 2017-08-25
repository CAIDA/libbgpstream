/*
 * This file is part of bgpstream
 *
 * CAIDA, UC San Diego
 * bgpstream-info@caida.org
 *
 * Copyright (C) 2012 The Regents of the University of California.
 * Authors: Alistair King, Chiara Orsini
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "utils.h"
#include "bgpstream_elem_generator.h"
#include <assert.h>

struct bgpstream_elem_generator {

  /** Array of elems */
  bgpstream_elem_t **elems;

  /** Number of elems that are active in the elems list */
  int elems_cnt;

  /** Number of allocated elems in the elems list */
  int elems_alloc_cnt;

  /* Current iterator position (iter == cnt means end-of-list) */
  int iter;
};

/* ==================== PRIVATE FUNCTIONS ==================== */

/* ==================== PROTECTED FUNCTIONS ==================== */

bgpstream_elem_generator_t *bgpstream_elem_generator_create()
{
  bgpstream_elem_generator_t *self;

  if ((self = malloc_zero(sizeof(bgpstream_elem_generator_t))) == NULL) {
    return NULL;
  }

  /* indicates not populated */
  self->elems_cnt = -1;

  return self;
}

void bgpstream_elem_generator_destroy(bgpstream_elem_generator_t *self)
{
  int i;
  if (self == NULL) {
    return;
  }

  /* free all the alloc'd elems */
  for (i = 0; i < self->elems_alloc_cnt; i++) {
    bgpstream_elem_destroy(self->elems[i]);
    self->elems[i] = NULL;
  }

  free(self->elems);

  self->elems_cnt = self->elems_alloc_cnt = self->iter = 0;

  free(self);
}

void bgpstream_elem_generator_clear(bgpstream_elem_generator_t *self)
{
  /* explicit clear is done by get_next_elem */

  self->elems_cnt = -1;
  self->iter = 0;
}

void bgpstream_elem_generator_empty(bgpstream_elem_generator_t *self)
{
  self->elems_cnt = 0;
  self->iter = 0;
}

int bgpstream_elem_generator_is_populated(bgpstream_elem_generator_t *self)
{
  return self->elems_cnt != -1;
}

bgpstream_elem_t *
bgpstream_elem_generator_get_new_elem(bgpstream_elem_generator_t *self)
{
  bgpstream_elem_t *elem = NULL;

  /* check if we need to alloc more elems */
  if (self->elems_cnt + 1 >= self->elems_alloc_cnt) {

    /* alloc more memory */
    if ((self->elems = realloc(self->elems, sizeof(bgpstream_elem_t *) *
                                              (self->elems_alloc_cnt + 1))) ==
        NULL) {
      return NULL;
    }

    /* create an elem */
    if ((self->elems[self->elems_alloc_cnt] = bgpstream_elem_create()) ==
        NULL) {
      return NULL;
    }

    self->elems_alloc_cnt++;
  }

  elem = self->elems[self->elems_cnt];
  bgpstream_elem_clear(elem);
  return elem;
}

void bgpstream_elem_generator_commit_elem(bgpstream_elem_generator_t *self,
                                          bgpstream_elem_t *el)
{
  assert(self->elems[self->elems_cnt] == el);
  self->elems_cnt++;
}

bgpstream_elem_t *
bgpstream_elem_generator_get_next_elem(bgpstream_elem_generator_t *self)
{
  bgpstream_elem_t *elem = NULL;

  if (self->iter < self->elems_cnt) {
    elem = self->elems[self->iter];
    self->iter++;
  }

  return elem;
}
