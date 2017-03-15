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

#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "bgpstream_log.h"
#include "bgpstream_reader.h"
#include "bgpdump/bgpdump_lib.h"
#include "utils.h"

#define BUFFER_LEN 1024

#define DUMP_OPEN_MAX_RETRIES 5
#define DUMP_OPEN_MIN_RETRY_WAIT 10

// TODO: clean up


static void *thread_producer(void *user)
{
  bgpstream_reader_t *bsr = (bgpstream_reader_t *)user;
  int retries = 0;
  int delay = DUMP_OPEN_MIN_RETRY_WAIT;

  /* all we do is open the dump */
  /* but try a few times in case there is a transient failure */
  while (retries < DUMP_OPEN_MAX_RETRIES && bsr->bd_mgr == NULL) {
    if ((bsr->bd_mgr = bgpdump_open_dump(bsr->dump_name)) == NULL) {
      fprintf(stderr, "WARN: Could not open dumpfile (%s). Attempt %d of %d\n",
              bsr->dump_name, retries + 1, DUMP_OPEN_MAX_RETRIES);
      retries++;
      if (retries < DUMP_OPEN_MAX_RETRIES) {
        sleep(delay);
        delay *= 2;
      }
    }
  }

  pthread_mutex_lock(&bsr->mutex);
  if (bsr->bd_mgr == NULL) {
    fprintf(
      stderr,
      "ERROR: Could not open dumpfile (%s) after %d attempts. Giving up.\n",
      bsr->dump_name, DUMP_OPEN_MAX_RETRIES);
    bsr->status = BGPSTREAM_READER_STATUS_CANT_OPEN_DUMP;
  }
  bsr->dump_ready = 1;
  pthread_cond_signal(&bsr->dump_ready_cond);
  pthread_mutex_unlock(&bsr->mutex);

  return NULL;
}

static BGPDUMP_ENTRY *get_next_entry(bgpstream_reader_t *bsr)
{
  if (bsr->skip_dump_check == 0) {
    pthread_mutex_lock(&bsr->mutex);
    while (bsr->dump_ready == 0) {
      pthread_cond_wait(&bsr->dump_ready_cond, &bsr->mutex);
    }
    pthread_mutex_unlock(&bsr->mutex);

    if (bsr->status == BGPSTREAM_READER_STATUS_CANT_OPEN_DUMP) {
      return NULL;
    }

    bsr->skip_dump_check = 1;
  }

  /* now, grab an entry from bgpdump */
  return bgpdump_read_next(bsr->bd_mgr);
}

/* -------------- Reader functions -------------- */

static bool
filter_bd_entry(BGPDUMP_ENTRY *bd_entry,
                bgpstream_filter_mgr_t *filter_mgr)
{
  bgpstream_log(BGPSTREAM_LOG_VFINE, "\t\tBSR: filter entry: start");
  bgpstream_interval_filter_t *tif;
  if (bd_entry != NULL && filter_mgr != NULL) {
    if (filter_mgr->time_intervals == NULL) {
      bgpstream_log(BGPSTREAM_LOG_VFINE, "\t\tBSR: filter entry: end");
      return true; // no time filtering,
    }
    int current_entry_time = bd_entry->time;
    tif = filter_mgr->time_intervals;
    while (tif != NULL) {
      if (current_entry_time >= tif->begin_time &&
          (tif->end_time == BGPSTREAM_FOREVER ||
           current_entry_time <= tif->end_time)) {
        bgpstream_log(BGPSTREAM_LOG_VFINE, "\t\tBSR: filter entry: OK");
        return true;
      }
      tif = tif->next;
    }
  }
  bgpstream_log(BGPSTREAM_LOG_VFINE, "\t\tBSR: filter entry: DISCARDED");
  return false;
}

void
bgpstream_reader_read_new_data(bgpstream_reader_t *bs_reader,
                               bgpstream_filter_mgr_t *filter_mgr)
{
  bool significant_entry = false;
  bgpstream_log(BGPSTREAM_LOG_VFINE, "\t\tBSR: read new data: start");
  if (bs_reader == NULL) {
    bgpstream_log(BGPSTREAM_LOG_VFINE, "\t\tBSR: read new data: invalid reader provided");
    return;
  }
  if (bs_reader->status != BGPSTREAM_READER_STATUS_VALID_ENTRY) {
    bgpstream_log(BGPSTREAM_LOG_VFINE, "\t\tBSR: read new data: reader cannot read new data "
                    "(previous read was not successful)");
    return;
  }
  // if a valid record was read before (or it is the first time we read
  // something)
  // assert(bs_reader->status == BGPSTREAM_READER_STATUS_VALID_ENTRY)
  bgpstream_log(BGPSTREAM_LOG_VFINE, "\t\tBSR: read new data: bd_entry has to be set to NULL");
  // entry should not be destroyed, otherwise we could destroy
  // what is in the current record, the next export record will take
  // care of it
  bs_reader->bd_entry = NULL;
  // check if the end of the file was already reached before
  // reading a new value
  // if(bs_reader->bd_mgr->eof != 0) {
  //  bgpstream_log(BGPSTREAM_LOG_VFINE, "\t\tBSR: read new data: end of file reached");
  //  bs_reader->status = BGPSTREAM_READER_STATUS_END_OF_DUMP;
  //  return;
  //}
  bgpstream_log(BGPSTREAM_LOG_VFINE, "\t\tBSR: read new data (previous): %ld\t%ld\t%d\t%s\t%d",
                  bs_reader->record_time, bs_reader->dump_time,
                  bs_reader->dump_type, bs_reader->dump_collector,
                  bs_reader->status);
  bgpstream_log(BGPSTREAM_LOG_VFINE, 
    "\t\tBSR: read new data: reading new entry (or entries) in bgpdump");
  bgpstream_log(BGPSTREAM_LOG_VFINE, "\t\tBSR: read new data: from %s", bs_reader->dump_name);
  while (!significant_entry) {
    bgpstream_log(BGPSTREAM_LOG_VFINE, "\t\t\tBSR: read new data: reading");
    // try and get the next entry
    // will block until dump is open (or fails)
    bs_reader->bd_entry = get_next_entry(bs_reader);
    // check if there was an error opening the dump
    if (bs_reader->status == BGPSTREAM_READER_STATUS_CANT_OPEN_DUMP) {
      return;
    }
    // check if an entry has been read
    if (bs_reader->bd_entry != NULL) {
      bs_reader->successful_read++;
      // check if entry is compatible with filters
      if (filter_bd_entry(bs_reader->bd_entry, filter_mgr)) {
        // update reader fields
        bs_reader->valid_read++;
        bs_reader->status = BGPSTREAM_READER_STATUS_VALID_ENTRY;
        bs_reader->record_time = (long)bs_reader->bd_entry->time;
        significant_entry = true;
      }
      // if not compatible, destroy bgpdump entry
      else {
        bgpdump_free_mem(bs_reader->bd_entry);
        bs_reader->bd_entry = NULL;
        // significant_entry = false;
      }
    }
    // if an entry has not been read (i.e. b_e = NULL)
    else {
      // if the corrupted entry flag is
      // active, then dump is corrupted
      if (bs_reader->bd_mgr->corrupted_read) {
        bs_reader->status = BGPSTREAM_READER_STATUS_CORRUPTED_DUMP;
        significant_entry = true;
      }
      // empty read, not corrupted
      else {
        // end of file
        if (bs_reader->bd_mgr->eof != 0) {
          significant_entry = true;
          if (bs_reader->successful_read == 0) {
            // file was empty
            bs_reader->status = BGPSTREAM_READER_STATUS_EMPTY_DUMP;
          } else {
            if (bs_reader->valid_read == 0) {
              // valid contained entries, but
              // none of them was compatible with
              // filters
              bs_reader->status = BGPSTREAM_READER_STATUS_FILTERED_DUMP;
            } else {
              // something valid has been read before
              // reached the end of file
              bs_reader->status = BGPSTREAM_READER_STATUS_END_OF_DUMP;
            }
          }
        }
        // empty space at the *beginning* of file
        else {
          // do nothing - continue to read
          // significant_entry = false;
        }
      }
    }
  }
  // a significant entry has been found
  // and the reader has been updated accordingly
  bgpstream_log(BGPSTREAM_LOG_VFINE, "\t\tBSR: read new data: end");
  return;
}

void bgpstream_reader_destroy(bgpstream_reader_t *bs_reader)
{

  bgpstream_log(BGPSTREAM_LOG_VFINE, "\t\tBSR: destroy reader start");
  if (bs_reader == NULL) {
    bgpstream_log(BGPSTREAM_LOG_VFINE, "\t\tBSR: destroy reader: null reader provided");
    return;
  }

  /* Ensure the thread is done */
  pthread_join(bs_reader->producer, NULL);
  pthread_mutex_destroy(&bs_reader->mutex);
  pthread_cond_destroy(&bs_reader->dump_ready_cond);

  // we do not deallocate memory for bd_entry
  // (the last entry may be still in use in
  // the current record)
  bs_reader->bd_entry = NULL;
  // close bgpdump
  bgpdump_close_dump(bs_reader->bd_mgr);
  bs_reader->bd_mgr = NULL;
  // deallocate all memory for reader
  free(bs_reader);
  bgpstream_log(BGPSTREAM_LOG_VFINE, "\t\tBSR: destroy reader end");
}

bgpstream_reader_t *
bgpstream_reader_create(bgpstream_resource_t *resource,
                        bgpstream_filter_mgr_t *filter_mgr)
{
  assert(resource);
  // allocate memory for reader
  bgpstream_reader_t *bs_reader =
    (bgpstream_reader_t *)malloc(sizeof(bgpstream_reader_t));
  if (bs_reader == NULL) {
    bgpstream_log(BGPSTREAM_LOG_VFINE, "\t\tBSR: create reader: can't allocate memory for reader");
    bgpstream_log(BGPSTREAM_LOG_VFINE, "\t\tBSR: create reader end");
    return NULL; // can't allocate memory for reader
  }
  bgpstream_log(BGPSTREAM_LOG_VFINE, "\t\tBSR: create reader: initialize fields");
  // fields initialization
  bs_reader->next = NULL;
  bs_reader->bd_mgr = NULL;
  bs_reader->bd_entry = NULL;
  // init done
  strcpy(bs_reader->dump_name, resource->uri);
  strcpy(bs_reader->dump_project, resource->project);
  strcpy(bs_reader->dump_collector, resource->collector);
  bs_reader->dump_type = resource->record_type;
  bs_reader->dump_time = bs_reader->record_time = resource->initial_time;
  bs_reader->status = BGPSTREAM_READER_STATUS_VALID_ENTRY;
  bs_reader->valid_read = 0;
  bs_reader->successful_read = 0;

  pthread_mutex_init(&bs_reader->mutex, NULL);
  pthread_cond_init(&bs_reader->dump_ready_cond, NULL);
  bs_reader->dump_ready = 0;
  bs_reader->skip_dump_check = 0;

  // bgpdump is created in the thread
  pthread_create(&bs_reader->producer, NULL, thread_producer, bs_reader);

  /* // call bgpstream_reader_read_new_data */
  /* bgpstream_log(BGPSTREAM_LOG_VFINE, "\t\tBSR: create reader: read new data"); */
  /* bgpstream_reader_read_new_data(bs_reader, filter_mgr); */
  /* bgpstream_log(BGPSTREAM_LOG_VFINE, "\t\tBSR: create reader: end");   */
  // return reader
  return bs_reader;
}

void
bgpstream_reader_export_record(bgpstream_reader_t *bs_reader,
                               bgpstream_record_t *bs_record,
                               bgpstream_filter_mgr_t *filter_mgr)
{
  bgpstream_log(BGPSTREAM_LOG_VFINE, "\t\tBSR: export record: start");
  if (bs_reader == NULL) {
    bgpstream_log(BGPSTREAM_LOG_VFINE, "\t\tBSR: export record: invalid reader provided");
    return;
  }
  if (bs_record == NULL) {
    bgpstream_log(BGPSTREAM_LOG_VFINE, "\t\tBSR: export record: invalid record provided");
    return;
  }
  // if bs_reader status is BGPSTREAM_READER_STATUS_END_OF_DUMP we shouldn't
  // have called this
  // function
  if (bs_reader->status == BGPSTREAM_READER_STATUS_END_OF_DUMP) {
    bgpstream_log(BGPSTREAM_LOG_VFINE, "\t\tBSR: export record: end of dump was reached");
    return;
  }
  // read bgpstream_reader field and copy them to a bs_record
  bgpstream_log(BGPSTREAM_LOG_VFINE, "\t\tBSR: export record: copying bd_entry");
  bs_record->bd_entry = bs_reader->bd_entry;
  // disconnect reader from exported entry
  bs_reader->bd_entry = NULL;
  // memset(bs_record->attributes.dump_project, 0, BGPSTREAM_PAR_MAX_LEN);
  // memset(bs_record->attributes.dump_collector, 0, BGPSTREAM_PAR_MAX_LEN);
  bgpstream_log(BGPSTREAM_LOG_VFINE, "\t\tBSR: export record: copying attributes");
  strcpy(bs_record->attributes.dump_project, bs_reader->dump_project);
  strcpy(bs_record->attributes.dump_collector, bs_reader->dump_collector);
  //   strcpy(bs_record->attributes.dump_type, bs_reader->dump_type);
  bs_record->attributes.dump_type = bs_reader->dump_type;
  bs_record->attributes.dump_time = bs_reader->dump_time;
  bs_record->attributes.record_time = bs_reader->record_time;
  // if this is the first significant record and no previous
  // valid record has been discarded because of time
  if (bs_reader->valid_read == 1 && bs_reader->successful_read == 1) {
    bs_record->dump_pos = BGPSTREAM_DUMP_START;
  } else {
    bs_record->dump_pos = BGPSTREAM_DUMP_MIDDLE;
  }
  bgpstream_log(BGPSTREAM_LOG_VFINE, "\t\tBSR: export record: copying status");
  switch (bs_reader->status) {
  case BGPSTREAM_READER_STATUS_VALID_ENTRY:
    bs_record->status = BGPSTREAM_RECORD_STATUS_VALID_RECORD;
    break;
  case BGPSTREAM_READER_STATUS_FILTERED_DUMP:
    bs_record->status = BGPSTREAM_RECORD_STATUS_FILTERED_SOURCE;
    break;
  case BGPSTREAM_READER_STATUS_EMPTY_DUMP:
    bs_record->status = BGPSTREAM_RECORD_STATUS_EMPTY_SOURCE;
    break;
  case BGPSTREAM_READER_STATUS_CANT_OPEN_DUMP:
    bs_record->status = BGPSTREAM_RECORD_STATUS_CORRUPTED_SOURCE;
    break;
  case BGPSTREAM_READER_STATUS_CORRUPTED_DUMP:
    bs_record->status = BGPSTREAM_RECORD_STATUS_CORRUPTED_RECORD;
    break;
  default:
    bs_record->status = BGPSTREAM_RECORD_STATUS_EMPTY_SOURCE;
  }
  bgpstream_log(BGPSTREAM_LOG_VFINE, 
    "Exported: %ld\t%ld\t%d\t%s\t%d", bs_record->attributes.record_time,
    bs_record->attributes.dump_time, bs_record->attributes.dump_type,
    bs_record->attributes.dump_collector, bs_record->status);

  /** safe option for rib period filter*/
  char buffer[BUFFER_LEN];
  khiter_t k;
  int khret;
  if (filter_mgr->rib_period != 0 &&
      (bs_record->status == BGPSTREAM_RECORD_STATUS_CORRUPTED_SOURCE ||
       bs_record->status == BGPSTREAM_RECORD_STATUS_CORRUPTED_RECORD)) {
    snprintf(buffer, BUFFER_LEN, "%s.%s", bs_record->attributes.dump_project,
             bs_record->attributes.dump_collector);
    if ((k = kh_get(collector_ts, filter_mgr->last_processed_ts, buffer)) ==
        kh_end(filter_mgr->last_processed_ts)) {
      k = kh_put(collector_ts, filter_mgr->last_processed_ts, strdup(buffer),
                 &khret);
    }
    kh_value(filter_mgr->last_processed_ts, k) = 0;
  }

  bgpstream_log(BGPSTREAM_LOG_VFINE, "\t\tBSR: export record: end");
}
