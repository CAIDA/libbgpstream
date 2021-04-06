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

#include "bgpstream_resource_mgr.h"
#include "bgpstream_filter.h"
#include "bgpstream_log.h"
#include "bgpstream_reader.h"
#include "config.h"
#include "utils.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define BUFFER_LEN 1024

/** Approximately how frequently should stream resources that return AGAIN be
    polled? (in msec) */
#define AGAIN_POLL_INTERVAL 100
#define MSEC_TO_NSEC 1000000

struct res_list_elem {
  /** The resource info */
  bgpstream_resource_t *res;

  /** Pointer to a reader instance if the resource is "open" */
  bgpstream_reader_t *reader;

  /** Is the reader open? (i.e. have we waited for it to open) */
  int open;

  /** Time when this resource should next be polled (if 0 then poll
      immediately) */
  uint32_t next_poll;

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
  struct res_list_elem *res_list[_BGPSTREAM_RECORD_TYPE_CNT];

  /** Total number of resources in this group */
  int res_cnt;

  /** The number of open resources in this group */
  int res_open_cnt;

  /** The number of open resources that have been checked and re-sorted */
  int res_open_checked_cnt;

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

  // the number of stream resources
  int res_stream_cnt;

  // borrowed pointer to a filter manager instance
  bgpstream_filter_mgr_t *filter_mgr;
};

static int open_batch(bgpstream_resource_mgr_t *q, struct res_group *gp);

static void res_list_destroy(struct res_list_elem *l, int destroy_resource)
{
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

static int open_res_list(bgpstream_resource_mgr_t *q, struct res_group *gp,
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
    if ((el->reader = bgpstream_reader_create(el->res, q->filter_mgr)) ==
        NULL) {
      bgpstream_log(BGPSTREAM_LOG_ERR, "Failed to open resource: %s",
                    el->res->url);
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
  // do nothing if everything is open
  if (gp->res_open_cnt == gp->res_cnt) {
    return 0;
  }

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

static void res_group_destroy(struct res_group *g, int destroy_resource)
{
  if (g == NULL) {
    return;
  }
  g->prev = NULL;
  g->next = NULL;
  int i;
  for (i = 0; i < _BGPSTREAM_RECORD_TYPE_CNT; i++) {
    res_list_destroy(g->res_list[i], destroy_resource);
    g->res_list[i] = NULL;
  }
  free(g);
}

static uint32_t get_next_time(struct res_list_elem *el)
{
  if (el->reader != NULL) {
    return bgpstream_reader_get_next_time(el->reader);
  } else {
    // our best guess
    //
    // this will be 0 for most stream resources, which will force them into the
    // first group, we will then open the resource, and then re-sort
    return el->res->initial_time;
  }
}

static void update_overlap(struct res_group *gp, struct res_list_elem *el)
{
  // if this is a "stream", the duration is 0 (BGPSTREAM_FOREVER), and so will
  // not affect other items in the group

  // is this a RIB, and is this our first RIB in the group, and are we safe to
  // subtract without wrapping overlap_start?
  if (el->res->record_type == BGPSTREAM_RIB &&
      gp->res_list[BGPSTREAM_RIB] == NULL &&
      gp->overlap_start > el->res->duration) {
    // need to fudge the time because RIBs can start early
    gp->overlap_start -= el->res->duration;
  }

  // update the max duration?
  // if this is a new group, overlap_end will be 0, so this will always be true,
  // except when dealing with a stream (then it will be left as 0)
  if ((gp->time + el->res->duration) > gp->overlap_end) {
    gp->overlap_end = gp->time + el->res->duration;
  }
}

static struct res_group *res_group_create(struct res_list_elem *el)
{
  struct res_group *gp;
  assert(el->res != NULL);

  if ((gp = malloc_zero(sizeof(struct res_group))) == NULL) {
    return NULL;
  }

  gp->time = gp->overlap_start = get_next_time(el);
  // set the overlap start and end times
  update_overlap(gp, el);

  gp->res_list[el->res->record_type] = el;
  el->next = NULL;
  el->prev = NULL;
  gp->res_cnt = 1;

  if (el->reader != NULL) {
    gp->res_open_cnt++;
    if (el->open != 0) {
      gp->res_open_checked_cnt++;
    }
  } else {
    assert(el->open == 0);
  }

  return gp;
}

static int res_group_add(bgpstream_resource_mgr_t *q, struct res_group *gp,
                         struct res_list_elem *el)
{
  int is_dirty = 0;

  assert(gp->time == get_next_time(el));

  // potentially extend the overlap window based on this new element
  update_overlap(gp, el);

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
    if (gp->res_cnt > gp->res_open_cnt) {
      is_dirty = 1;
    }
    if (el->open != 0) {
      gp->res_open_checked_cnt++;
    }
  } else {
    assert(el->open == 0);
  }

  return is_dirty;
}

#if 0
static void list_dump(struct res_list_elem *el)
{
  while (el != NULL) {
    fprintf(stderr, "    next_time: %d\n",
                  get_next_time(el));
    fprintf(stderr, "    res->url: %s\n",
                  el->res->url);
    fprintf(stderr, "    %s\n",
                  (el->reader == NULL) ? "not-open" : "open");
    el = el->next;
  }
}

static void queue_dump(struct res_group *head)
{
  while (head != NULL) {
    fprintf(stderr,
            "res_group: time: %d, overlap_start: %d, "
            "overlap_end: %d, "
            "cnt: %d, open_cnt: %d, open_checked_cnt: %d\n",
            head->time, head->overlap_start, head->overlap_end,
            head->res_cnt, head->res_open_cnt, head->res_open_checked_cnt);

    int i;
    for (i=0; i<_BGPSTREAM_RECORD_TYPE_CNT; i++) {
      fprintf(stderr, "  records (type %d):\n", i);
      list_dump(head->res_list[i]);
    }

    head = head->next;
  }
  fprintf(stderr, "\n");
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
#define QUEUE_PUSH(el, FROMEND, TOEND, CMPOP, TODIR, FROMDIR)                  \
  do {                                                                         \
    cur = q->FROMEND;                                                          \
    last = NULL;                                                               \
    while (cur != NULL && cur->time CMPOP get_next_time(el)) {                 \
      last = cur;                                                              \
      cur = cur->TODIR;                                                        \
    }                                                                          \
    if (cur != NULL && cur->time == get_next_time(el)) {                       \
      /* just add to the current group */                                      \
      if ((is_dirty = res_group_add(q, cur, el)) < 0) {                        \
        goto err;                                                              \
      }                                                                        \
      dirty_cnt += is_dirty;                                                   \
    } else {                                                                   \
      /* we first need to create a new group */                                \
      if ((gp = res_group_create(el)) == NULL) {                               \
        goto err;                                                              \
      }                                                                        \
      gp->TODIR = cur;                                                         \
      gp->FROMDIR = last;                                                      \
      if (cur == NULL) {                                                       \
        /* creating a new TOEND */                                             \
        assert(last == q->TOEND);                                              \
        last->TODIR = gp;                                                      \
        q->TOEND = gp;                                                         \
      } else if (last == NULL) {                                               \
        /* creating a new FROMEND */                                           \
        assert(q->FROMEND == cur);                                             \
        cur->FROMDIR = gp;                                                     \
        q->FROMEND = gp;                                                       \
      } else {                                                                 \
        /* splicing into the list */                                           \
        cur->FROMDIR = gp;                                                     \
        last->TODIR = gp;                                                      \
      }                                                                        \
    }                                                                          \
  } while (0)

static int insert_resource_elem(bgpstream_resource_mgr_t *q,
                                struct res_list_elem *el)
{
  struct res_group *gp = NULL, *cur = NULL, *last = NULL;
  int dirty_cnt = 0, is_dirty = 0;

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
    if (abs((int)get_next_time(el) - (int)q->head->time) <
        abs((int)get_next_time(el) - (int)q->tail->time)) {
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
  if (el->res->duration == BGPSTREAM_FOREVER) {
    q->res_stream_cnt++;
  }

  // we're done!
  return dirty_cnt;

err:
  res_group_destroy(gp, 1);
  return -1;
}

static void pop_res_el(bgpstream_resource_mgr_t *q, struct res_group *gp,
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
  q->res_cnt--;
  if (el->open != 0) {
    gp->res_open_cnt--;
    gp->res_open_checked_cnt--;
    q->res_open_cnt--;
  }
  if (el->res->duration == BGPSTREAM_FOREVER) {
    q->res_stream_cnt--;
  }
  assert(gp->res_cnt >= 0);
  assert(gp->res_open_cnt >= 0);
  assert(gp->res_open_checked_cnt >= 0);
  assert(q->res_cnt >= 0);
  assert(q->res_open_cnt >= 0);
  assert(q->res_stream_cnt >= 0);
  assert(q->res_stream_cnt <= q->res_cnt);
  assert(gp->res_open_checked_cnt <= gp->res_open_cnt);
  assert(gp->res_open_cnt >= gp->res_cnt);
  assert(gp->res_cnt <= q->res_cnt);
  assert(gp->res_open_cnt <= q->res_open_cnt);

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

static int sort_res_list(bgpstream_resource_mgr_t *q, struct res_group *gp,
                         struct res_list_elem *el)
{
  struct res_list_elem *el_nxt;
  int dirty_cnt = 0;

  while (el != NULL) {
    el_nxt = el->next;
    if (el->open != 0 || el->reader == NULL) {
      el = el_nxt;
      continue;
    }

    if (bgpstream_reader_open_wait(el->reader) != 0) {
      return -1;
    }
    el->open = 1;
    gp->res_open_checked_cnt++;
    if (get_next_time(el) != el->res->initial_time) {
      // this needs to be popped and then re-inserted
      pop_res_el(q, gp, el);
      if ((dirty_cnt = insert_resource_elem(q, el)) < 0) {
        return -1;
      }
    }
    el = el_nxt;
  }

  return dirty_cnt;
}

static int sort_group(bgpstream_resource_mgr_t *q, struct res_group *gp)
{
  int dirty_up = 0, dirty_rib = 0;

  // wait for updates
  if ((dirty_up = sort_res_list(q, gp, gp->res_list[BGPSTREAM_UPDATE])) < 0) {
    return -1;
  }

  // wait for ribs
  if ((dirty_rib = sort_res_list(q, gp, gp->res_list[BGPSTREAM_RIB])) < 0) {
    return -1;
  }

  return dirty_up + dirty_rib;
}

/* returns the number of "dirty" groups. i.e. the number of groups that were not
   open but now have an open resource added (since this will require another
   call to open_batch) */
static int sort_batch(bgpstream_resource_mgr_t *q)
{
  struct res_group *cur = q->head;
  int empty_groups = 0;
  int dirty_cnt_total = 0, dirty_cnt = 0;

  // wait for the batch to open first
  while (cur != NULL && cur->res_open_cnt != 0) {

    if (cur->res_open_checked_cnt == cur->res_open_cnt) {
      cur = cur->next;
      continue;
    }

    if ((dirty_cnt = sort_group(q, cur)) < 0) {
      return -1;
    }
    dirty_cnt_total += dirty_cnt;

    if (cur->res_cnt == 0) {
      empty_groups++;
    }

    cur = cur->next;
  }

  // now reap any empty groups that our sorting has created
  if (empty_groups != 0) {
    reap_groups(q);
  }

  return dirty_cnt_total;
}

// open all overlapping resources. does not modify the queue
static int open_batch(bgpstream_resource_mgr_t *q, struct res_group *gp)
{
  // start from the head of the queue and open resources until we
  // find a group that does not overlap with the previous ones
  struct res_group *cur = gp;
  int first = 1;
  uint32_t last_overlap_end = 0;

  while (cur != NULL && (first != 0 || last_overlap_end > cur->overlap_start)) {
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

  return 0;
}

// when this is called we are guaranteed to have at least one open resource, and
// if things have gone right, we should read from the first resource in the
// queue. once we have read from the resource, we should check the new time of
// the resource and see if it needs to be moved.
static bgpstream_reader_status_t pop_record(bgpstream_resource_mgr_t *q,
                                            bgpstream_record_t **record)
{
  uint32_t prev_time;
  bgpstream_reader_status_t rs;
  struct res_list_elem *el = NULL;
  struct res_group *gp = NULL;
  uint32_t now;
  uint64_t sleep_nsec;
  struct timespec rqtp;

  // the resource we want to read from MUST be in the first group (q->head), and
  // will either be the head of the RIBS list if there are any ribs, otherwise
  // it will be the head of the updates list
  if (q->head->res_list[BGPSTREAM_RIB] != NULL) {
    el = q->head->res_list[BGPSTREAM_RIB];
  } else {
    el = q->head->res_list[BGPSTREAM_UPDATE];
  }
  assert(el != NULL && el->res != NULL);
  assert(el->prev == NULL);
  assert(el->open != 0);

  // we assume that if this resource has a poll timer set that has not expired
  // then since it would have been pushed to the end of the group and as such
  // all other resources already polled.
  if (el->next_poll > 0) {
    now = epoch_msec();
    if (el->next_poll > now) {
      sleep_nsec = (el->next_poll - now) * MSEC_TO_NSEC;
      rqtp.tv_sec = sleep_nsec / 1000000000;
      rqtp.tv_nsec = sleep_nsec % 1000000000;
      if (nanosleep(&rqtp, NULL) != 0) {
        // interrupted
        return -1;
      }
    }
    el->next_poll = 0;
  }

  // cache the current time so we can check if we need to remove and re-insert
  prev_time = get_next_time(el);

  // ask the resource to give us the next record (that it has already read). it
  // will internally grab the next record from the resource and update the time
  // of the resource.
  if ((rs = bgpstream_reader_get_next_record(el->reader, record)) ==
      BGPSTREAM_READER_STATUS_ERROR) {
    // some kind of error occurred
    bgpstream_log(BGPSTREAM_LOG_ERR, "Failed to get next record from reader");
    return rs;
  }

  // if we got AGAIN, then move ourselves to the end of our group to give others
  // a fair shake
  if (rs == BGPSTREAM_READER_STATUS_AGAIN) {
    assert(el->prev == NULL);
    assert(q->head->res_list[el->res->record_type] == el);
    if (el->next != NULL) {
      // grab the next element
      struct res_list_elem *next_el = el->next;
      // move the next element to the head
      q->head->res_list[el->res->record_type] = next_el;

      // disconnect ourselves from the list
      next_el->prev = NULL;
      el->next = NULL;
      el->prev = NULL; // not strictly necessary

      // and move us to the end of the list
      while (next_el->next != NULL) {
        next_el = next_el->next;
      }
      assert(el != next_el);
      // append ourselves to the end
      next_el->next = el;
      el->prev = next_el;
    }
    // and then tell the caller that while we didn't get anything useful, they
    // should try again soon
    el->next_poll = epoch_msec() + AGAIN_POLL_INTERVAL;
    assert(q->head->res_list[el->res->record_type]->prev == NULL);
    return rs;
  }

  // otherwise we must valid, or EOS
  assert(rs == BGPSTREAM_READER_STATUS_EOS || rs == BGPSTREAM_READER_STATUS_OK);

  // if the time has changed or we've reached EOS, pop from the queue
  if (get_next_time(el) != prev_time || rs == BGPSTREAM_READER_STATUS_EOS) {
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

    if (rs == BGPSTREAM_READER_STATUS_EOS) {
      // we're at EOS, so destroy the resource
      res_list_destroy(el, 1);
    } else if (get_next_time(el) != prev_time) {
      // time has changed, so we need to re-insert
      if (insert_resource_elem(q, el) < 0) {
        return -1;
      }
    }
  }

  // all is well
  return rs;
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

void bgpstream_resource_mgr_destroy(bgpstream_resource_mgr_t *q)
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

int bgpstream_resource_mgr_push(
  bgpstream_resource_mgr_t *q,
  bgpstream_resource_transport_type_t transport_type,
  bgpstream_resource_format_type_t format_type, const char *url,
  uint32_t initial_time, uint32_t duration, const char *project,
  const char *collector, bgpstream_record_type_t record_type,
  bgpstream_resource_t **resp)
{
  bgpstream_resource_t *res = NULL;
  struct res_list_elem *el = NULL;
  if (resp != NULL) {
    *resp = NULL;
  }

  // first create the resource
  if ((res = bgpstream_resource_create(transport_type, format_type, url,
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

  // now we know we want to keep it
  if (insert_resource_elem(q, el) < 0) {
    bgpstream_log(BGPSTREAM_LOG_ERR, "Could not insert resource into queue");
    goto err;
  }

  if (resp != NULL) {
    *resp = res;
  }
  return 1;

err:
  res_list_destroy(el, 1);
  bgpstream_resource_destroy(res);
  return -1;
}

int bgpstream_resource_mgr_empty(bgpstream_resource_mgr_t *q)
{
  return (q->head == NULL);
}

int bgpstream_resource_mgr_stream_only(bgpstream_resource_mgr_t *q)
{
  return (q->res_stream_cnt == q->res_cnt);
}

int bgpstream_resource_mgr_get_record(bgpstream_resource_mgr_t *q,
                                      bgpstream_record_t **record)
{
  int rs = BGPSTREAM_READER_STATUS_EOS;
  int dirty_cnt = 0;

  // don't let EOF mean EOS until we have no more resources left
  while (rs == BGPSTREAM_READER_STATUS_EOS ||
         rs == BGPSTREAM_READER_STATUS_AGAIN) {
    if (q->res_cnt == 0) {
      // we have nothing in the queue, so now we can return EOS
      return 0;
    }

    // we know we have something in the queue, but if we have nothing open, then
    // it is time to open some resources!
    // we do this inside a loop since in some cases the first batch we open get
    // sorted elsewhere in the queue, leaving the head still unopened.
    dirty_cnt = 0;
    while (q->head->res_open_cnt != q->head->res_cnt || dirty_cnt > 0) {
      if (open_batch(q, q->head) != 0) {
        goto err;
      }
      // its possible that the timestamp of the first record in a dump file
      // doesn't match the initial time reported to us from the broker (e.g., in
      // the case of filtering), so we re-sort the batch before we read anything
      // from it.
      if ((dirty_cnt = sort_batch(q)) < 0) {
        goto err;
      }
    }
    // its possible that we failed to open all the files, perhaps in that case
    // we shouldn't abort, but instead return EOS and let the caller decide what
    // to do, but for now:
    assert(q->res_open_cnt != 0);

    // we now know that we have open resources to read from, lets do it
    if ((rs = pop_record(q, record)) == BGPSTREAM_READER_STATUS_ERROR) {
      return -1;
    } else if (rs == BGPSTREAM_READER_STATUS_OK) {
      return 1;
    }
    // otherwise, could be EOS or AGAIN, so keep trying (from other resources in
    // the case of EOS)
  }

err:
  return -1;
}
