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

#include "config.h"
#include "utils.h"
#include "bgpstream_resource_mgr.h"
#include "bgpstream_log.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

struct res_list_elem {
  bgpstream_resource_t *res;
  struct res_list_elem *next;
};

struct res_group {
  /** The common "intial_time" of these resources */
  uint32_t time;

  /** The start time for overlap calculations (`time` for updates,
      `time-duration` for RIBs) */
  uint32_t overlap_start;
  /** The end time for overlap calculations */
  uint32_t overlap_end;

  /** List of RIBs/updates at this timestamp
      (use BGPSTREAM_UPDATE / BGPSTREAM_RIB to index */
  struct res_list_elem *res_list[_BGPSTREAM_RECORD_DUMP_TYPE_CNT];

  /** Total number of resources in this group */
  int res_cnt;

  /** Previous group (newer timestamp) */
  struct res_group *prev;

  /** Next group (older timestamp) */
  struct res_group *next;
};

struct bgpstream_resource_mgr {

  /** Ordered queue of resources, grouped by timestamp (i.e. group by second).
   * The oldest timestamp is at the head, newest at the tail */
  struct res_group *head;

  struct res_group *tail;

  // the number of resources in the queue
  int queue_len;

  // the number of open resources
  int open_len;

};

static void res_list_destroy(struct res_list_elem *l, int destroy_resource) {
  if (l == NULL) {
    return;
  }
  struct res_list_elem *cur = l;
  struct res_list_elem *tmp = NULL;

  while (cur != NULL) {
    tmp = cur;
    cur = tmp->next; // move on
    tmp->next = NULL;
    if (destroy_resource != 0) {
      bgpstream_resource_destroy(tmp->res);
    }
    tmp->res = NULL;
    free(tmp);
  }
}

static struct res_list_elem *res_list_elem_create(bgpstream_resource_t *res)
{
  struct res_list_elem *el;

  if ((el = malloc_zero(sizeof(struct res_list_elem))) == NULL) {
    return NULL;
  }

  el->res = res;

  // its up the caller to connect it to something...

  return el;
}

static void res_group_destroy(struct res_group *g, int destroy_resource) {
  if (g == NULL) {
    return;
  }
  g->prev = NULL;
  g->next = NULL;
  int i;
  for (i=0; i<_BGPSTREAM_RECORD_DUMP_TYPE_CNT; i++) {
    res_list_destroy(g->res_list[i], destroy_resource);
    g->res_list[i] = NULL;
  }
  free(g);
}

static struct res_group *res_group_create(bgpstream_resource_t *res)
{
  struct res_group *gp;

  if ((gp = malloc_zero(sizeof(struct res_group))) == NULL) {
    return NULL;
  }

  gp->time = gp->overlap_start = res->initial_time;
  // hax since some RIBs start early
  if (res->record_type == BGPSTREAM_RIB) {
    gp->overlap_start -= res->duration;
  }
  gp->overlap_end = gp->time + res->duration;

  if ((gp->res_list[res->record_type] = res_list_elem_create(res)) == NULL) {
    res_group_destroy(gp, 1);
    return NULL;
  }
  gp->res_cnt = 1;

  return gp;
}

static int res_group_add(struct res_group *gp,
                         bgpstream_resource_t *res)
{
  struct res_list_elem *el;

  assert(gp->time == res->initial_time);

  // is this our first RIB in the group?
  if (gp->res_list[BGPSTREAM_RIB] == NULL &&
      res->record_type == BGPSTREAM_RIB) {
    // need to fudge the time because RIBs can start early
    gp->overlap_start -= res->duration;
  }

  // update the max duration?
  if ((gp->time + res->duration) > gp->overlap_end) {
    gp->overlap_end = gp->time + res->duration;
  }

  // create a new list element
  if ((el = res_list_elem_create(res)) == NULL) {
    return -1;
  }

  // add to the appropriate list
  if (gp->res_list[res->record_type] == NULL) {
    gp->res_list[res->record_type] = el;
  } else {
    el->next = gp->res_list[res->record_type];
    gp->res_list[res->record_type] = el;
  }

  gp->res_cnt++;

  return 0;
}

#if 0
static void list_dump(struct res_list_elem *el, int log_level)
{
  while (el != NULL) {
    bgpstream_log(log_level, "    res->initial_time: %d",
                  el->res->initial_time);
    bgpstream_log(log_level, "    res->uri: %s",
                  el->res->uri);
    el = el->next;
  }
}

static void queue_dump(struct res_group *head, int log_level)
{
  while (head != NULL) {
    bgpstream_log(log_level,
                  "res_group: time: %d, overlap_start: %d, "
                  "overlap_end: %d, cnt: %d",
                  head->time, head->overlap_start, head->overlap_end,
                  head->res_cnt);

    int i;
    for (i=0; i<_BGPSTREAM_RECORD_DUMP_TYPE_CNT; i++) {
      bgpstream_log(log_level, "  records (type %d):", i);
      list_dump(head->res_list[i], log_level);
    }

    head = head->next;
  }
}

static void batch_dump(bgpstream_resource_t **batch, int batch_cnt,
                       int log_level)
{
  int i;
  for (i=0; i<batch_cnt; i++) {
    bgpstream_log(log_level, "[%d] res->initial_time: %d", i,
                  batch[i]->initial_time);
    bgpstream_log(log_level, "[%d] res->uri: %s", i, batch[i]->uri);
  }
}
#endif

// HERE: fix queue_len, open_len

// open all overlapping resources. does not modify the queue
static int open_batch(bgpstream_resource_mgr_t *q)
{
  // start from the head of the queue and open resources until we
  // find a group that does not overlap with the previous ones
  struct res_group *cur = q->head;
  struct res_list_elem *el;
  int first = 1;
  uint32_t last_overlap_end = 0;

  while (cur != NULL &&
         (first != 0 || last_overlap_end > cur->overlap_start)) {
    // this is included in the batch

    // first open RIBs
    el = cur->res_list[BGPSTREAM_RIB];
    while (el != NULL) {
      assert(el->res != NULL);
      if (bgpstream_resource_open(el->res) != 0) {
        bgpstream_log(BGPSTREAM_LOG_ERR,
                      "Failed to open RIB: %s", el->res->uri);
        return -1;
      }
      el = el->next;
    }
    // then open updates
    el = cur->res_list[BGPSTREAM_UPDATE];
    while (el != NULL) {
      assert(el->res != NULL);
      if (bgpstream_resource_open(el->res) != 0) {
        bgpstream_log(BGPSTREAM_LOG_ERR,
                      "Failed to open RIB: %s", el->res->uri);
        return -1;
      }
      el = el->next;
    }

    // update our overlap calculation
    if (first != 0 || cur->overlap_end > last_overlap_end) {
      first = 0;
      last_overlap_end = cur->overlap_end;
    }

    cur = cur->next;
  }

  return 0;
}

static int pop_record(bgpstream_resource_mgr_t *q, bgpstream_record_t *record)
{
  return -1;
}

/* ========== PUBLIC METHODS BELOW HERE ========== */

bgpstream_resource_mgr_t *
bgpstream_resource_mgr_create()
{
  bgpstream_resource_mgr_t *q;

  if ((q = malloc_zero(sizeof(bgpstream_resource_mgr_t))) == NULL) {
    return NULL;
  }

  return q;
}

void
bgpstream_resource_mgr_destroy(bgpstream_resource_mgr_t *q)
{
  if (q == NULL) {
    return;
  }
  struct res_group *cur = q->head;

  while (cur != NULL) {
    q->head = cur->next;
    res_group_destroy(cur, 1);
    cur = q->head;
  }
  q->tail = NULL;

  free(q);
}

/** Insert a result into the queue from either end
 *
 * FROMEND = {head, tail};
 * TOEND = {tail, head};
 * CMP = {<, >};
 * TODIR = {next, prev};
 * FROMDIR = {prev, next};
 */
#define QUEUE_PUSH(res, FROMEND, TOEND, CMPOP, TODIR, FROMDIR)  \
  do {                                                          \
    cur = q->FROMEND;                                           \
    last = NULL;                                                \
    while (cur != NULL && cur->time CMPOP res->initial_time) {  \
      last = cur;                                               \
      cur = cur->TODIR;                                         \
    }                                                           \
    if (cur->time == res->initial_time) {                       \
      /* just add to the current group */                       \
      if (res_group_add(cur, res) != 0) {                       \
        goto err;                                               \
      }                                                         \
    } else {                                                    \
      /* we first need to create a new group */                 \
      if ((gp = res_group_create(res)) == NULL) {               \
        goto err;                                               \
      }                                                         \
      gp->TODIR = cur;                                          \
      gp->FROMDIR = last;                                       \
      if (cur == NULL) {                                        \
        /* creating a new TOEND */                              \
        assert(last == q->TOEND);                               \
        last->TODIR = gp;                                       \
        q->TOEND = gp;                                          \
      } else if (last == NULL) {                                \
        /* creating a new FROMEND */                            \
        assert(q->FROMEND == cur);                              \
        cur->FROMDIR = gp;                                      \
        q->FROMEND = gp;                                        \
      } else {                                                  \
        /* splicing into the list */                            \
        cur->FROMDIR = gp;                                      \
        last->TODIR = gp;                                       \
      }                                                         \
    }                                                           \
  } while (0)

bgpstream_resource_t *
bgpstream_resource_mgr_push(bgpstream_resource_mgr_t *q,
                            bgpstream_transport_type_t transport_type,
                            bgpstream_format_type_t format_type,
                            const char *uri,
                            uint32_t initial_time,
                            uint32_t duration,
                            const char *project, const char *collector,
                            bgpstream_record_dump_type_t record_type)
{
  bgpstream_resource_t *res;
  struct res_group *gp = NULL, *cur = NULL, *last = NULL;

  // first create the resource
  if ((res = bgpstream_resource_create(transport_type, format_type, uri,
                                       initial_time, duration, project,
                                       collector, record_type)) == NULL) {
    return NULL;
  }
  // now figure out where to put it

  // is the list empty?
  if (q->head == NULL) {
    assert(q->tail == NULL);

    // create a new res group
    if ((gp = res_group_create(res)) == NULL) {
      goto err;
    }

    // and make it the head/tail
    q->head = q->tail = gp;

    // we're done!
    return res;
  }

  // by here head AND tail are guaranteed to be set
  assert(q->head != NULL && q->tail != NULL);

  // which end of the list is this record closer to?
  // can probably be optimized somewhat, lists are not my specialty...
  if (abs(res->initial_time - q->head->time) <
      abs(res->initial_time - q->tail->time)) {
    // closer to the head of the list
    QUEUE_PUSH(res, head, tail, <, next, prev);
  } else {
    // equidistant, or closer to the tail
    QUEUE_PUSH(res, tail, head, >, prev, next);
  }

  return res;

 err:
  bgpstream_resource_destroy(res);
  res_group_destroy(gp, 1);
  return NULL;
}

int
bgpstream_resource_mgr_empty(bgpstream_resource_mgr_t *q)
{
  return (q->head == NULL);
}

int
bgpstream_resource_mgr_get_record(bgpstream_resource_mgr_t *q,
                                  bgpstream_record_t *record)
{
  if (q->queue_len == 0) {
    // we have nothing in the queue, so return EOS
    return 0;
  }

  // we know we have something in the queue, but if we have nothing open, then
  // it is time to open some resources!
  if (q->open_len == 0 && open_batch(q) != 0) {
    bgpstream_log(BGPSTREAM_LOG_ERR, "Failed to open resource batch");
    goto err;
  }

  // we now know that we have open resources to read from, lets do it
  return pop_record(q, record);

 err:
  return -1;
}
