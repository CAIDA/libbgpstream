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
