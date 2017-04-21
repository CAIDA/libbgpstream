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

#include "bgpstream_reader.h"
#include "bgpstream_log.h"
#include "utils.h"
#include "bgpdump_lib.h"
#include <assert.h>
#include <unistd.h>
#include <pthread.h>

// TODO: refactor this into transport and format modules

#define DUMP_OPEN_MAX_RETRIES 5
#define DUMP_OPEN_MIN_RETRY_WAIT 10

#define PREFETCH_IDX (reader->rec_buf_prefetch_idx)
#define EXPORTED_IDX ((reader->rec_buf_prefetch_idx + 1) % 2)

typedef enum {
  OK,
  FILTERED_DUMP,
  EMPTY_DUMP,
  CANT_OPEN_DUMP,
  CORRUPTED_DUMP,
  END_OF_DUMP,
} status_t;

struct bgpstream_reader {

  // borrowed pointer to the resource that we have opened
  bgpstream_resource_t *res;

  // borrowed pointer to a filter manager instance
  bgpstream_filter_mgr_t *filter_mgr;

  // internal flip-flop buffers for storing records
  bgpstream_record_t *rec_buf[2];
  int rec_buf_filled[2];

  // which of the flip-flop buffers is currently holding the "prefetch" record
  // the other ((this+1)%2) is holding the "exported" record
  int rec_buf_prefetch_idx;

  // the total number of successful (filtered and not) reads
  uint64_t successful_read_cnt;

  // the number of non-filtered reads (i.e. "useful")
  uint64_t valid_read_cnt;

  // XXX following fields belong in TRANSPORT or FORMAT
  // where does thread belong? maybe here, maybe not
  status_t status;
  BGPDUMP *bgpdump;
  pthread_t opener_thread;
  /* has the thread opened the dump? */
  int dump_ready;
  pthread_cond_t dump_ready_cond;
  pthread_mutex_t mutex;
  /* have we already checked that the dump is ready? */
  int skip_dump_check;
};

static int wanted_bd_entry(BGPDUMP_ENTRY *bd_entry,
                           bgpstream_filter_mgr_t *filter_mgr)
{
  bgpstream_interval_filter_t *tif;
  assert(bd_entry != NULL && filter_mgr != NULL);

  if (filter_mgr->time_intervals == NULL) {
    // no time filtering
    return 1;
  }

  uint32_t time = bd_entry->time;
  tif = filter_mgr->time_intervals;

  while (tif != NULL) {
    if (time >= tif->begin_time && (tif->end_time == BGPSTREAM_FOREVER ||
                                    time <= tif->end_time)) {
      // matches a filter interval
      return 1;
    }
    tif = tif->next;
  }

  return 0;
}

// ALL this does is read a BGPdump record into a waiting bgpstream record and
// populate all the info needed for the record.
// it assumes that the record is prepopulated with reader metadata
static int prefetch_record(bgpstream_reader_t *reader)
{
  bgpstream_record_t *record;
  int skipped_cnt = 0;
  assert(reader->status == OK);
  assert(reader->rec_buf_filled[PREFETCH_IDX] == 0);

  record = reader->rec_buf[PREFETCH_IDX];
  assert(record->bd_entry == NULL);

  while (1) {
    // try and get the next entry from the file
    record->bd_entry = bgpdump_read_next(reader->bgpdump);

    // did we read a record?
    if (record->bd_entry != NULL) {
      // we did (the normal case)
      reader->successful_read_cnt++;

      // check the filters
      if (wanted_bd_entry(record->bd_entry, reader->filter_mgr) != 0) {
        // we want this entry
        reader->valid_read_cnt++;
        reader->status = OK;
        record->attributes.record_time = record->bd_entry->time;
        break;
      } else {
        // we dont want this entry, destroy it
        bgpdump_free_mem(record->bd_entry);
        record->bd_entry = NULL;
        skipped_cnt++;
        continue;
      }
      assert(0);
    }

    // by here we did not read an entry from bgpdump... why?
    assert(record->bd_entry == NULL);

    // did bgpdump signal a corrupted read?
    if (reader->bgpdump->corrupted_read) {
      reader->status = CORRUPTED_DUMP;
      break;
    }

    // ok, so an empty read, but not corrupted...

    // end of file?
    if (reader->bgpdump->eof == 0) {
      // not EOF, so probably an MRT record that bgpdump kindly doesn't export
      // (e.g. peer table)
      // keep trying...
      continue;
    }

    // ok, yep, we got EOF

    // was this the first thing we tried to read?
    if (reader->successful_read_cnt == 0) {
      // then it is an empty file
      reader->status = EMPTY_DUMP;
      break;
    }

    // so we managed to read some things, but did we get anything useful from
    // this file?
    if (reader->valid_read_cnt == 0) {
      // dump contained data, but we filtered all of them out
      reader->status = FILTERED_DUMP;
      break;
    }

    // well, then we must have reached the end of the file happily (also update
    // the record that is ready to be exported to indicate that it was end of
    // dump)
    reader->status = END_OF_DUMP;
    break;
  }

  // now "export" the bgpdump info into the record

  // project, collector, dump type, dump time are already populated

  // if this is the first record we read and no previous
  // valid record has been discarded because of time
  if (reader->valid_read_cnt == 1 && reader->successful_read_cnt == 1) {
    record->dump_pos = BGPSTREAM_DUMP_START;
  } else {
    record->dump_pos = BGPSTREAM_DUMP_MIDDLE;
    // NB when the *next* record is pre-fetched, this may be changed to
    // end-of-dump (since we'll discover that there are no more records)
  }

  // TODO: do we REALLY need this to be visible to users?
  switch (reader->status) {
  case OK:
    record->status = BGPSTREAM_RECORD_STATUS_VALID_RECORD;
    // update our current time
    reader->res->current_time = record->attributes.record_time;
    break;
  case FILTERED_DUMP:
    record->status = BGPSTREAM_RECORD_STATUS_FILTERED_SOURCE;
    break;
  case EMPTY_DUMP:
    record->status = BGPSTREAM_RECORD_STATUS_EMPTY_SOURCE;
    break;
  case CANT_OPEN_DUMP:
    record->status = BGPSTREAM_RECORD_STATUS_CORRUPTED_SOURCE;
    break;
  case CORRUPTED_DUMP:
    record->status = BGPSTREAM_RECORD_STATUS_CORRUPTED_RECORD;
    break;
  case END_OF_DUMP:
    // update the PREVIOUS record iff we didn't skip any records in this read
    if (skipped_cnt == 0) {
      reader->rec_buf[EXPORTED_IDX]->dump_pos = BGPSTREAM_DUMP_END;
    }
  }

  reader->rec_buf_filled[PREFETCH_IDX] = 1;
  return 0;
}

static void *threaded_opener(void *user)
{
  bgpstream_reader_t *reader = (bgpstream_reader_t *)user;
  int retries = 0;
  int delay = DUMP_OPEN_MIN_RETRY_WAIT;

  /* all we do is open the dump */
  /* but try a few times in case there is a transient failure */
  while (retries < DUMP_OPEN_MAX_RETRIES && reader->bgpdump == NULL) {
    if ((reader->bgpdump = bgpdump_open_dump(reader->res->uri)) == NULL) {
      bgpstream_log(BGPSTREAM_LOG_WARN,
                    "Could not open dumpfile (%s). Attempt %d of %d",
                    reader->res->uri, retries + 1, DUMP_OPEN_MAX_RETRIES);
      retries++;
      if (retries < DUMP_OPEN_MAX_RETRIES) {
        sleep(delay);
        delay *= 2;
      }
    }
  }

  pthread_mutex_lock(&reader->mutex);
  if (reader->bgpdump == NULL) {
    bgpstream_log(BGPSTREAM_LOG_ERR,
      "Could not open dumpfile (%s) after %d attempts. Giving up.",
      reader->res->uri, DUMP_OPEN_MAX_RETRIES);
    reader->status = CANT_OPEN_DUMP;
  } else {
    // prefetch the first record (will set reader->status to error if needed)
    prefetch_record(reader);
  }
  reader->dump_ready = 1;
  pthread_cond_signal(&reader->dump_ready_cond);
  pthread_mutex_unlock(&reader->mutex);

  return NULL;
}

// does NOT copy the BS pointer or elem generator
static int record_copy(bgpstream_record_t *dst, bgpstream_record_t *src)
{
  // bd entry
  dst->bd_entry = src->bd_entry;

  // attributes
  memcpy(&dst->attributes, &src->attributes,
         sizeof(bgpstream_record_attributes_t));

  // status
  dst->status = src->status;

  dst->dump_pos = src->dump_pos;

  return 0;
}

// fills the record with resource-level info that doesn't change per-record
static int prepopulate_record(bgpstream_record_t *record,
                              bgpstream_resource_t *res)
{
  // project
  strncpy(record->attributes.dump_project, res->project,
          BGPSTREAM_UTILS_STR_NAME_LEN);
  record->attributes.dump_project[BGPSTREAM_UTILS_STR_NAME_LEN-1] = '\0';

  // collector
  strncpy(record->attributes.dump_collector, res->collector,
          BGPSTREAM_UTILS_STR_NAME_LEN);
  record->attributes.dump_collector[BGPSTREAM_UTILS_STR_NAME_LEN-1] = '\0';

  // dump type
  record->attributes.dump_type = res->record_type;

  // dump time
  record->attributes.dump_time = res->initial_time;

  return 0;
}

/* ========== PUBLIC FUNCTIONS BELOW ========== */

bgpstream_reader_t *
bgpstream_reader_create(bgpstream_resource_t *resource,
                        bgpstream_filter_mgr_t *filter_mgr)
{
  bgpstream_reader_t *reader;

  if ((reader = malloc_zero(sizeof(bgpstream_reader_t))) == NULL) {
    return NULL;
  }

  reader->res = resource;
  reader->filter_mgr = filter_mgr;
  reader->status = OK;

  int i;
  for (i=0; i<2; i++) {
    if ((reader->rec_buf[i] = bgpstream_record_create()) == NULL ||
        prepopulate_record(reader->rec_buf[i], resource) != 0) {
      goto err;
    }
    reader->rec_buf_filled[i] = 0;
  }
  reader->rec_buf_prefetch_idx = 0;

  // initialize and start the thread to open the resource
  // this will also pre-fetch the first record
  pthread_mutex_init(&reader->mutex, NULL);
  pthread_cond_init(&reader->dump_ready_cond, NULL);
  reader->dump_ready = 0;
  reader->skip_dump_check = 0;
  pthread_create(&reader->opener_thread, NULL, threaded_opener, reader);

  return reader;

 err:
  bgpstream_reader_destroy(reader);
  return NULL;
}

void bgpstream_reader_destroy(bgpstream_reader_t *reader)
{
  if (reader == NULL) {
    return;
  }

  // Ensure the thread is done
  pthread_join(reader->opener_thread, NULL);
  pthread_mutex_destroy(&reader->mutex);
  pthread_cond_destroy(&reader->dump_ready_cond);

  int i;
  for (i=0; i<2; i++) {
    bgpstream_record_destroy(reader->rec_buf[i]);
    reader->rec_buf[i] = NULL;
  }

  bgpdump_close_dump(reader->bgpdump);
  reader->bgpdump = NULL;

  free(reader);
}

int bgpstream_reader_open_wait(bgpstream_reader_t *reader)
{
  if (reader->skip_dump_check != 0) {
    return 0;
  }

  pthread_mutex_lock(&reader->mutex);
  while (reader->dump_ready == 0) {
    pthread_cond_wait(&reader->dump_ready_cond, &reader->mutex);
  }
  pthread_mutex_unlock(&reader->mutex);

  if (reader->status == CANT_OPEN_DUMP) {
    return -1;
  }

  reader->skip_dump_check = 1;
  return 0;
}

int bgpstream_reader_get_next_record(bgpstream_reader_t *reader,
                                     bgpstream_record_t *record)
{
  // DO NOT use the prefetch record before open_wait!

  if (bgpstream_reader_open_wait(reader) != 0) {
    // cant even open the dump file
    assert(reader->successful_read_cnt == 0 && reader->valid_read_cnt == 0);
    // we're not going to last long, but we should return the record saying we're
    // a failure
    if (record_copy(record, reader->rec_buf[PREFETCH_IDX]) != 0) {
      return -1;
    }
    record->status = BGPSTREAM_RECORD_STATUS_CORRUPTED_SOURCE;
    assert(record->bd_entry == NULL);
    return 0; // EOF
  }

  // mark the previous record as unfilled (about to become PREFETCH_IDX)
  reader->rec_buf_filled[EXPORTED_IDX] = 0;

  // by here we are guaranteed to have a prefetched record waiting, so flip-flop
  // the buffers
  reader->rec_buf_prefetch_idx = EXPORTED_IDX;

  // prefetch the next message (so we can see if the record we're about to
  // export would be the last one)
  if (reader->status == OK &&
      prefetch_record(reader) != 0) {
    bgpstream_log(BGPSTREAM_LOG_ERR, "Prefetch failed");
    return -1;
  }

  // if the EXPORT record is not filled, then we're done
  if (reader->rec_buf_filled[EXPORTED_IDX] == 0) {
    // EOS
    assert(reader->status != OK);
    return 0;
  }

  // we have something in our EXPORT record, so go ahead and copy that into the
  // user's record
  if (record_copy(record, reader->rec_buf[EXPORTED_IDX]) != 0) {
    bgpstream_log(BGPSTREAM_LOG_ERR, "Could not copy record");
    return -1;
  }
  // unlink our bgpdump record, since we've given up ownership
  reader->rec_buf[EXPORTED_IDX]->bd_entry = NULL;

  return 1;
}
