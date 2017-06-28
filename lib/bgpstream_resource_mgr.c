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
#include "bgpstream_filter.h"
#include "bgpstream_reader.h"
#include "bgpstream_resource_mgr.h"
#include "bgpstream_log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/** TODO: Fix the rib period filter to not need to build this string as this
    will fail for collectors with really long names */
#define BUFFER_LEN 1024

struct res_list_elem {
  /** The resource info */
  bgpstream_resource_t *res;

  /** Pointer to a reader instance if the resource is "open" */
  bgpstream_reader_t *reader;

  /** Is the reader open? (i.e. have we waited for it to open) */
  int open;

  /** Previous list elem */
  struct res_list_elem *prev;

  /** Next list elem */
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

  /** The number of open resources in this group */
  int res_open_cnt;

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
  int res_cnt;

  // the number of open resources
  int res_open_cnt;

  // borrowed pointer to a filter manager instance
  bgpstream_filter_mgr_t *filter_mgr;

};

static int open_batch(bgpstream_resource_mgr_t *q, struct res_group *gp);

static void res_list_destroy(struct res_list_elem *l, int destroy_resource) {
  if (l == NULL) {
    return;
  }
  struct res_list_elem *cur = l;
  struct res_list_elem *tmp = NULL;

  while (cur != NULL) {
    tmp = cur;
    cur = tmp->next; // move on
    tmp->prev = NULL;
    tmp->next = NULL;
    if (destroy_resource != 0) {
      bgpstream_reader_destroy(tmp->reader);
      tmp->open = 0;
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

static int open_res_list(bgpstream_resource_mgr_t *q,
                         struct res_group *gp,
                         struct res_list_elem *el)
{
  while (el != NULL) {
    assert(el->res != NULL);
    // it is possible that this is already open (because of re-sorting)
    if (el->reader != NULL) {
      el = el->next;
      continue;
    }
    // open this resource
    if ((el->reader =
         bgpstream_reader_create(el->res, q->filter_mgr)) == NULL) {
      bgpstream_log(BGPSTREAM_LOG_ERR,
                    "Failed to open resource: %s", el->res->uri);
      return -1;
    }
    // update stats
    q->res_open_cnt++;
    gp->res_open_cnt++;
    el = el->next;
  }

  return 0;
}

static int open_group(bgpstream_resource_mgr_t *q, struct res_group *gp)
{
  // first open RIBs
  if (open_res_list(q, gp, gp->res_list[BGPSTREAM_RIB]) != 0) {
    return -1;
  }

  // then open updates
  if (open_res_list(q, gp, gp->res_list[BGPSTREAM_UPDATE]) != 0) {
    return -1;
  }

  return 0;
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

static struct res_group *res_group_create(struct res_list_elem *el)
{
  struct res_group *gp;
  assert(el->res != NULL);

  if ((gp = malloc_zero(sizeof(struct res_group))) == NULL) {
    return NULL;
  }

  gp->time = gp->overlap_start = el->res->current_time;
  // hax since some RIBs start early
  if (el->res->record_type == BGPSTREAM_RIB) {
    gp->overlap_start -= el->res->duration;
  }
  gp->overlap_end = gp->time + el->res->duration;

  gp->res_list[el->res->record_type] = el;
  el->next = NULL; el->prev = NULL;
  gp->res_cnt = 1;

  if (el->reader != NULL) {
    gp->res_open_cnt++;
  }

  return gp;
}

static int res_group_add(bgpstream_resource_mgr_t *q,
                         struct res_group *gp,
                         struct res_list_elem *el)
{
  assert(gp->time == el->res->current_time);

  // is this our first RIB in the group?
  if (gp->res_list[BGPSTREAM_RIB] == NULL &&
      el->res->record_type == BGPSTREAM_RIB) {
    // need to fudge the time because RIBs can start early
    gp->overlap_start -= el->res->duration;
  }

  // update the max duration?
  if ((gp->time + el->res->duration) > gp->overlap_end) {
    gp->overlap_end = gp->time + el->res->duration;
  }

  // add to the appropriate list
  if (gp->res_list[el->res->record_type] == NULL) {
    gp->res_list[el->res->record_type] = el;
  } else {
    gp->res_list[el->res->record_type]->prev = el;
    el->next = gp->res_list[el->res->record_type];
    gp->res_list[el->res->record_type] = el;
  }

  gp->res_cnt++;

  if (el->reader != NULL) {
    gp->res_open_cnt++;
    // if we have just opened the first file in the group, then open the rest
    if (gp->res_cnt > gp->res_open_cnt &&
        open_batch(q, gp) != 0) {
      return -1;
    }
  }

  return 0;
}

#if 0
static void list_dump(struct res_list_elem *el, int log_level)
{
  while (el != NULL) {
    bgpstream_log(log_level, "    res->current_time: %d",
                  el->res->current_time);
    bgpstream_log(log_level, "    res->uri: %s",
                  el->res->uri);
    bgpstream_log(log_level, "    %s",
                  (el->reader == NULL) ? "not-open" : "open");
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
#endif

/** Insert a result into the queue from either end
 *
 * FROMEND = {head, tail};
 * TOEND = {tail, head};
 * CMP = {<, >};
 * TODIR = {next, prev};
 * FROMDIR = {prev, next};
 */
#define QUEUE_PUSH(el, FROMEND, TOEND, CMPOP, TODIR, FROMDIR)           \
  do {                                                                  \
    cur = q->FROMEND;                                                   \
    last = NULL;                                                        \
    while (cur != NULL && cur->time CMPOP el->res->current_time) {      \
      last = cur;                                                       \
      cur = cur->TODIR;                                                 \
    }                                                                   \
    if (cur != NULL && cur->time == el->res->current_time) {            \
      /* just add to the current group */                               \
      if (res_group_add(q, cur, el) != 0) {                             \
        goto err;                                                       \
      }                                                                 \
    } else {                                                            \
      /* we first need to create a new group */                         \
      if ((gp = res_group_create(el)) == NULL) {                        \
        goto err;                                                       \
      }                                                                 \
      gp->TODIR = cur;                                                  \
      gp->FROMDIR = last;                                               \
      if (cur == NULL) {                                                \
        /* creating a new TOEND */                                      \
        assert(last == q->TOEND);                                       \
        last->TODIR = gp;                                               \
        q->TOEND = gp;                                                  \
      } else if (last == NULL) {                                        \
        /* creating a new FROMEND */                                    \
        assert(q->FROMEND == cur);                                      \
        cur->FROMDIR = gp;                                              \
        q->FROMEND = gp;                                                \
      } else {                                                          \
        /* splicing into the list */                                    \
        cur->FROMDIR = gp;                                              \
        last->TODIR = gp;                                               \
      }                                                                 \
    }                                                                   \
  } while (0)

static int insert_resource_elem(bgpstream_resource_mgr_t *q,
                                struct res_list_elem *el)
{
  struct res_group *gp = NULL, *cur = NULL, *last = NULL;

  // is the list empty?
  if (q->head == NULL) {
    assert(q->tail == NULL);

    // create a new res group
    if ((gp = res_group_create(el)) == NULL) {
      return -1;
    }

    // and make it the head/tail
    q->head = q->tail = gp;
  } else {
    // by here head AND tail are guaranteed to be set
    assert(q->head != NULL && q->tail != NULL);

    // which end of the list is this record closer to?
    // can probably be optimized somewhat, lists are not my specialty...
    if (abs(el->res->current_time - q->head->time) <
        abs(el->res->current_time - q->tail->time)) {
      // closer to the head of the list
      QUEUE_PUSH(el, head, tail, <, next, prev);
    } else {
      // equidistant, or closer to the tail
      QUEUE_PUSH(el, tail, head, >, prev, next);
    }
  }

  // count the resource
  q->res_cnt++;
  if (el->reader != NULL) {
    q->res_open_cnt++;
  }

  // we're done!
  return 0;

 err:
  res_group_destroy(gp, 1);
  return -1;
}

static void pop_res_el(bgpstream_resource_mgr_t *q,
                       struct res_group *gp,
                       struct res_list_elem *el)
{
  // disconnect from the list
  if (el->next != NULL) {
    el->next->prev = el->prev;
  }
  if (el->prev != NULL) {
    el->prev->next = el->next;
  }

  // maybe update the group head
  if (gp->res_list[el->res->record_type] == el) {
    gp->res_list[el->res->record_type] = el->next;
  }

  // update group and queue stats
  gp->res_cnt--;
  gp->res_open_cnt--;
  q->res_cnt--;
  q->res_open_cnt--;
  assert(gp->res_cnt >= 0);
  assert(gp->res_open_cnt >= 0);
  assert(q->res_cnt >= 0);
  assert(q->res_open_cnt >= 0);

  el->prev = NULL;
  el->next = NULL;
}

static void reap_groups(bgpstream_resource_mgr_t *q)
{
  struct res_group *gp = q->head;
  struct res_group *nxt;

  while (gp != NULL) {
    nxt = gp->next;

    if (gp->res_cnt == 0) {
      if (gp->next != NULL) {
        gp->next->prev = gp->prev;
      }
      if (gp->prev != NULL) {
        gp->prev->next = gp->next;
      }

      if (q->head == gp) {
        q->head = gp->next;
      }
      if (q->tail == gp) {
        q->tail = gp->prev;
      }

      // and destroy the group
      gp->prev = NULL;
      gp->next = NULL;
      res_group_destroy(gp, 0);
    }

    gp = nxt;
  }
}

static int sort_res_list(bgpstream_resource_mgr_t *q,
                         struct res_group *gp,
                         struct res_list_elem *el)
{
  struct res_list_elem *el_nxt;

  while (el != NULL) {
    el_nxt = el->next;
    if (el->open != 0) {
      el = el_nxt;
      continue;
    }

    if (bgpstream_reader_open_wait(el->reader) != 0) {
      return -1;
    }
    if (el->res->current_time != el->res->initial_time) {
      // this needs to be popped and then re-inserted
      pop_res_el(q, gp, el);
      if (insert_resource_elem(q, el) != 0) {
        return -1;
      }
    }
    el->open = 1;
    el = el_nxt;
  }

  return 0;
}

static int sort_group(bgpstream_resource_mgr_t *q,
                      struct res_group *gp)
{
  // wait for updates
  if (sort_res_list(q, gp, gp->res_list[BGPSTREAM_UPDATE]) != 0) {
    return -1;
  }

  // wait for ribs
  if (sort_res_list(q, gp, gp->res_list[BGPSTREAM_RIB]) != 0) {
    return -1;
  }

  return 0;
}

static int sort_batch(bgpstream_resource_mgr_t *q)
{
  struct res_group *cur = q->head;
  int empty_groups = 0;

  // wait for the batch to open first
  while (cur != NULL && cur->res_open_cnt != 0) {

    if (sort_group(q, cur) != 0) {
      return -1;
    }

    if (cur->res_cnt == 0) {
      empty_groups++;
    }

    cur = cur->next;
  }

  // now reap any empty groups that our sorting has created
  if (empty_groups != 0) {
    reap_groups(q);
  }

  return 0;
}

// open all overlapping resources. does not modify the queue
static int open_batch(bgpstream_resource_mgr_t *q, struct res_group *gp)
{
  // start from the head of the queue and open resources until we
  // find a group that does not overlap with the previous ones
  struct res_group *cur = gp;
  int first = 1;
  uint32_t last_overlap_end = 0;

  while (cur != NULL &&
         (first != 0 || last_overlap_end > cur->overlap_start)) {
    // this is included in the batch

    if (open_group(q, cur) != 0) {
      return -1;
    }

    // update our overlap calculation
    if (first != 0 || cur->overlap_end > last_overlap_end) {
      first = 0;
      last_overlap_end = cur->overlap_end;
    }

    cur = cur->next;
  }

  // its possible that the timestamp of the first record in a dump file doesn't
  // match the initial time reported to us from the broker (e.g., in the case of
  // filtering), so we re-sort the batch before we read anything from it.
  return sort_batch(q);
}

// when this is called we are guaranteed to have at least one open resource, and
// if things have gone right, we should read from the first resource in the
// queue. once we have read from the resource, we should check the new time of
// the resource and see if it needs to be moved.
static int pop_record(bgpstream_resource_mgr_t *q, bgpstream_record_t *record)
{
  uint32_t prev_time;
  int rc;
  struct res_list_elem *el = NULL;
  struct res_group *gp = NULL;

  // the resource we want to read from MUST be in the first group (q->head), and
  // will either be the head of the RIBS list if there are any ribs, otherwise
  // it will be the head of the updates list
  if (q->head->res_list[BGPSTREAM_RIB] != NULL) {
    el = q->head->res_list[BGPSTREAM_RIB];
  } else {
    el = q->head->res_list[BGPSTREAM_UPDATE];
  }
  assert(el != NULL && el->res != NULL);

  // cache the current time so we can check if we need to remove and re-insert
  prev_time = el->res->current_time;

  // ask the resource to give us the next record (that it has already read). it
  // will internally grab the next record from the resource and update the time
  // of the resource.
  if ((rc = bgpstream_reader_get_next_record(el->reader, record)) < 0) {
    // some kind of error occurred
    bgpstream_log(BGPSTREAM_LOG_ERR, "Failed to get next record from reader");
    return -1;
  }

  // if the time has changed or we've reached EOS, pop from the queue
  if (el->res->current_time != prev_time || rc == 0) {
    // first, remove this list elem from the group
    pop_res_el(q, q->head, el);

    // if we have emptied the group, remove the group
    if (q->head->res_cnt == 0) {
      gp = q->head;
      q->head = gp->next;
      if (q->head != NULL) {
        q->head->prev = NULL;
      }
      if (q->tail == gp) {
        q->tail = q->head;
      }

      // and then destroy the group
      gp->next = NULL;
      res_group_destroy(gp, 0);
    }

    if (rc == 0) {
      // we're at EOS, so destroy the resource
      res_list_destroy(el, 1);
    } else if (el->res->current_time != prev_time) {
      // time has changed, so we need to re-insert
      if (insert_resource_elem(q, el) != 0) {
        return -1;
      }
    }
  }

  // all is well
  return rc;
}

static int wanted_resource(bgpstream_resource_t *res,
                           bgpstream_filter_mgr_t *filter_mgr)
{
  char buffer[BUFFER_LEN];
  khiter_t k;
  int khret;

  if (res->record_type != BGPSTREAM_RIB || filter_mgr->rib_period == 0) {
    // its an updates file or there is no rib period set
    return 1;
  }

  snprintf(buffer, BUFFER_LEN, "%s.%s", res->project, res->collector);

  if ((k = kh_get(collector_ts, filter_mgr->last_processed_ts, buffer)) ==
      kh_end(filter_mgr->last_processed_ts)) {
    // first time we've seen a rib for this collector
    k = kh_put(collector_ts, filter_mgr->last_processed_ts, strdup(buffer),
               &khret);
    kh_value(filter_mgr->last_processed_ts, k) = res->initial_time;
    return 1;
  }

  if (res->initial_time <
      kh_value(filter_mgr->last_processed_ts, k) + filter_mgr->rib_period) {
    // still within our period
    return 0;
  }

  // we've found a rib we like!
  kh_value(filter_mgr->last_processed_ts, k) = res->initial_time;
  return 1;
}

/* ========== PUBLIC METHODS BELOW HERE ========== */

bgpstream_resource_mgr_t *
bgpstream_resource_mgr_create(bgpstream_filter_mgr_t *filter_mgr)
{
  bgpstream_resource_mgr_t *q;

  if ((q = malloc_zero(sizeof(bgpstream_resource_mgr_t))) == NULL) {
    return NULL;
  }

  q->filter_mgr = filter_mgr;

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

  // filter manager is a borrowed pointer
  q->filter_mgr = NULL;

  free(q);
}

int
bgpstream_resource_mgr_push(bgpstream_resource_mgr_t *q,
                            bgpstream_resource_transport_type_t transport_type,
                            bgpstream_resource_format_type_t format_type,
                            const char *uri,
                            uint32_t initial_time,
                            uint32_t duration,
                            const char *project, const char *collector,
                            bgpstream_record_dump_type_t record_type)
{
  bgpstream_resource_t *res = NULL;
  struct res_list_elem *el = NULL;

  // first create the resource
  if ((res = bgpstream_resource_create(transport_type, format_type, uri,
                                       initial_time, duration, project,
                                       collector, record_type)) == NULL) {
    return -1;
  }

  // before we insert, lets check if it matches our RIB period filter (if we
  // have one)
  if (wanted_resource(res, q->filter_mgr) == 0) {
    bgpstream_resource_destroy(res);
    return 0;
  }

  // now create a list element to hold the resource
  if ((el = res_list_elem_create(res)) == NULL) {
    bgpstream_log(BGPSTREAM_LOG_ERR, "Could not create list element");
    goto err;
  }
  res = NULL;

  // now we know we want to keep it
  if (insert_resource_elem(q, el) != 0) {
    bgpstream_log(BGPSTREAM_LOG_ERR, "Could not insert resource into queue");
    goto err;
  }

  return 0;

 err:
  res_list_destroy(el, 1);
  bgpstream_resource_destroy(res);
  return -1;
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
  int rc = 0; // EOF

  // don't let EOF mean EOS until we have no more resources left
  while (rc == 0) {
    if (q->res_cnt == 0) {
      // we have nothing in the queue, so now we can return EOS
      return 0;
    }

    // we know we have something in the queue, but if we have nothing open, then
    // it is time to open some resources!
    if (q->res_open_cnt == 0 && open_batch(q, q->head) != 0) {
      bgpstream_log(BGPSTREAM_LOG_ERR, "Failed to open resource batch");
      goto err;
    }
    // its possible that we failed to open all the files, perhaps in that case
    // we shouldn't abort, but instead return EOS and let the caller decide what
    // to do, but for now:
    assert(q->res_open_cnt != 0);

    // we now know that we have open resources to read from, lets do it
    if ((rc = pop_record(q, record)) < 0) {
      return rc;
    }
    // if we managed to read a record, then return it
    if (rc > 0) {
      return rc;
    }
    // otherwise, keep trying
  }

 err:
  return -1;
}
