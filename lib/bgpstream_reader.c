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
#include "bgpstream_record_int.h"
#include "bgpstream_log.h"
#include "utils.h"
#include <assert.h>
#include <pthread.h>
#include <unistd.h>

#define DUMP_OPEN_MAX_RETRIES 5
#define DUMP_OPEN_MIN_RETRY_WAIT 10

#define PREFETCH_IDX (reader->rec_buf_prefetch_idx)
#define EXPORTED_IDX ((reader->rec_buf_prefetch_idx + 1) % 2)


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

  // status of the underlying reader
  bgpstream_format_status_t status;

  // handle for the thread that will do the actual opening
  pthread_t opener_thread;

  // ALL BELOW HERE MUST USE MUTEX

  // format instance
  bgpstream_format_t *format;

  // has the thread opened the dump? (must use mutex)
  int dump_ready;
  pthread_cond_t dump_ready_cond;
  pthread_mutex_t mutex;

  // can the dump open check be skipped?
  int skip_dump_check;

  // what is the time of the next record (PREFETCH)
  uint32_t next_time;
};

static int prefetch_record(bgpstream_reader_t *reader)
{
  bgpstream_record_t *record;
  assert(reader->status == BGPSTREAM_FORMAT_OK);
  assert(reader->rec_buf_filled[PREFETCH_IDX] == 0);

  record = reader->rec_buf[PREFETCH_IDX];

  // first, clear up our record
  // note that this only destroys the reader struct and resets the elem
  // generator. it does not clear the collector name etc as we reuse that.
  bgpstream_record_clear(record);

  // try and get the next entry from the resource (will do filtering)
  reader->status = bgpstream_format_populate_record(reader->format, record);

  // if we got any of the non-error END_OF_DUMP messages but this is a stream
  // resource, then pretend we're ok.  but beware that now we'll be "OK", with
  // an unfilled prefetch record
  if (reader->res->duration == BGPSTREAM_FOREVER &&
      (reader->status == BGPSTREAM_FORMAT_END_OF_DUMP ||
       reader->status == BGPSTREAM_FORMAT_FILTERED_DUMP ||
       reader->status == BGPSTREAM_FORMAT_EMPTY_DUMP)) {
    reader->status = BGPSTREAM_FORMAT_OK;
    return 0;
  }

  // did we read a record?
  if (reader->status == BGPSTREAM_FORMAT_OK) {
    // we did (the normal case)
    reader->next_time = record->attrs.time_sec;
  }

  // set the previous record position to END if we didn't skip any records. we
  // know this because the format has set the position of the current record to
  // END (if records were skipped, it would be set to MIDDLE)
  if (reader->status == BGPSTREAM_FORMAT_END_OF_DUMP &&
      record->dump_pos == BGPSTREAM_DUMP_END &&
      reader->rec_buf_filled[EXPORTED_IDX] == 1) {
    reader->rec_buf[EXPORTED_IDX]->dump_pos = BGPSTREAM_DUMP_END;
  }

  // we export a meta record for every status except end of dump
  if (reader->status != BGPSTREAM_FORMAT_END_OF_DUMP) {
    reader->rec_buf_filled[PREFETCH_IDX] = 1;
  }

  return 0;
}

// fills the record with resource-level info that doesn't change per-record
static int prepopulate_record(bgpstream_record_t *record,
                              bgpstream_resource_t *res)
{
  // project
  strncpy(record->attrs.project_name, res->project,
          BGPSTREAM_UTILS_STR_NAME_LEN);
  record->attrs.project_name[BGPSTREAM_UTILS_STR_NAME_LEN-1] = '\0';

  // collector
  strncpy(record->attrs.collector_name, res->collector,
          BGPSTREAM_UTILS_STR_NAME_LEN);
  record->attrs.collector_name[BGPSTREAM_UTILS_STR_NAME_LEN-1] = '\0';

  // dump type
  record->attrs.type = res->record_type;

  // dump time
  record->attrs.dump_time_sec = res->initial_time;

  return 0;
}

static void *threaded_opener(void *user)
{
  bgpstream_reader_t *reader = (bgpstream_reader_t *)user;
  int retries = 0;
  int delay = DUMP_OPEN_MIN_RETRY_WAIT;
  int i;

  /* all we do is open the dump */
  /* but try a few times in case there is a transient failure */
  while (retries < DUMP_OPEN_MAX_RETRIES && reader->format == NULL) {
    if ((reader->format =
           bgpstream_format_create(reader->res, reader->filter_mgr)) == NULL) {
      bgpstream_log(BGPSTREAM_LOG_WARN,
                    "Could not open (%s). Attempt %d of %d",
                    reader->res->uri, retries + 1, DUMP_OPEN_MAX_RETRIES);
      retries++;
      if (retries < DUMP_OPEN_MAX_RETRIES) {
        sleep(delay);
        delay *= 2;
      }
    }
  }

  pthread_mutex_lock(&reader->mutex);
  if (reader->format == NULL) {
    bgpstream_log(BGPSTREAM_LOG_ERR,
      "Could not open dumpfile (%s) after %d attempts. Giving up.",
      reader->res->uri, DUMP_OPEN_MAX_RETRIES);
    reader->status = BGPSTREAM_FORMAT_CANT_OPEN_DUMP;
  } else {
    // create the pair of records
    for (i=0; i<2; i++) {
      if ((reader->rec_buf[i] = bgpstream_record_create(reader->format)) ==
          NULL ||
          prepopulate_record(reader->rec_buf[i], reader->res) != 0) {
        reader->status = BGPSTREAM_FORMAT_CANT_OPEN_DUMP;
        break;
      }
      reader->rec_buf_filled[i] = 0;
    }
    reader->rec_buf_prefetch_idx = 0;
    if (reader->status != BGPSTREAM_FORMAT_CANT_OPEN_DUMP) {
      // prefetch the first record (will set reader->status to error if needed)
      prefetch_record(reader);
    }
  }
  reader->dump_ready = 1;
  pthread_cond_signal(&reader->dump_ready_cond);
  pthread_mutex_unlock(&reader->mutex);

  return NULL;
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
  reader->status = BGPSTREAM_FORMAT_OK;

  // initialize and start the thread to open the resource
  // this will also pre-fetch the first record
  pthread_mutex_init(&reader->mutex, NULL);
  pthread_cond_init(&reader->dump_ready_cond, NULL);
  reader->dump_ready = 0;
  reader->skip_dump_check = 0;
  pthread_create(&reader->opener_thread, NULL, threaded_opener, reader);

  return reader;
}

uint32_t bgpstream_reader_get_next_time(bgpstream_reader_t *reader)
{
  assert(bgpstream_reader_open_wait(reader) == 0);
  return reader->next_time;
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

  bgpstream_format_destroy(reader->format);

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

  if (reader->status == BGPSTREAM_FORMAT_CANT_OPEN_DUMP) {
    return -1;
  }

  reader->skip_dump_check = 1;
  return 0;
}

int bgpstream_reader_get_next_record(bgpstream_reader_t *reader,
                                     bgpstream_record_t **record)
{
  // DO NOT use the prefetch record before open_wait!

  if (bgpstream_reader_open_wait(reader) != 0) {
    // cant even open the dump file
    // we're not going to last long, but we should return the record saying we're
    // a failure
    *record = reader->rec_buf[PREFETCH_IDX];
    (*record)->status = BGPSTREAM_RECORD_STATUS_CORRUPTED_SOURCE;
    assert((*record)->__int->data == NULL);
    return BGPSTREAM_READER_STATUS_EOS;
  }

  // mark the previous record as unfilled (about to become PREFETCH_IDX)
  reader->rec_buf_filled[EXPORTED_IDX] = 0;
  // the record contents will be cleared by the next prefetch

  // flip-flop the buffers (EXPORTED will mean "about to export")
  reader->rec_buf_prefetch_idx = EXPORTED_IDX;

  // prefetch the next message (so we can see if the record we're about to
  // export would be the last one)
  if (reader->status == BGPSTREAM_FORMAT_OK &&
      prefetch_record(reader) != 0) {
    bgpstream_log(BGPSTREAM_LOG_ERR, "Prefetch failed");
    return -1;
  }

  // if the EXPORT record is not filled then we need to return EOS or AGAIN
  if (reader->rec_buf_filled[EXPORTED_IDX] == 0) {
    if (reader->res->duration == BGPSTREAM_FOREVER &&
        reader->status != BGPSTREAM_FORMAT_OUTSIDE_TIME_INTERVAL) {
      return BGPSTREAM_READER_STATUS_AGAIN;
    } else {
      return BGPSTREAM_READER_STATUS_EOS;
    }
  }

  // we have something in our EXPORT record, so go ahead and copy that into the
  // user's record
  *record = reader->rec_buf[EXPORTED_IDX];

  return BGPSTREAM_READER_STATUS_OK;
}
