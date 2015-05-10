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

#include <stdlib.h>
#include <stdio.h>


#include "bgpstream_reader.h"
#include "bgpstream_input.h"
#include "bgpstream_debug.h"


#define BUFFER_LEN 1024


/* -------------- Reader functions -------------- */

static bool bgpstream_reader_filter_bd_entry(const BGPDUMP_ENTRY * const bd_entry, 
					     const bgpstream_filter_mgr_t * const filter_mgr) {
  bgpstream_debug("\t\tBSR: filter entry: start");  
  bgpstream_interval_filter_t * tif;
  if(bd_entry != NULL && filter_mgr != NULL) {
    if(filter_mgr->time_intervals == NULL ) {
        bgpstream_debug("\t\tBSR: filter entry: end");  
	return true; // no time filtering, 
    }
    int current_entry_time = bd_entry->time;
    tif = filter_mgr->time_intervals;
    while(tif != NULL) {      
      if(current_entry_time >= tif->begin_time &&
	 current_entry_time <= tif->end_time) {
	bgpstream_debug("\t\tBSR: filter entry: OK");  
	return true;
      }
      tif = tif->next;
    }
  }  
  bgpstream_debug("\t\tBSR: filter entry: DISCARDED");    
  return false;
}


static void bgpstream_reader_read_new_data(bgpstream_reader_t * const bs_reader,
					   const bgpstream_filter_mgr_t * const filter_mgr) {
  bool significant_entry = false;
  bgpstream_debug("\t\tBSR: read new data: start");  
  if(bs_reader == NULL) {
    bgpstream_debug("\t\tBSR: read new data: invalid reader provided");    
    return;
  }  
  if(bs_reader->status != BGPSTREAM_READER_STATUS_VALID_ENTRY) {
    bgpstream_debug("\t\tBSR: read new data: reader cannot read new data (previous read was not successful)");    
    return;
  }  
  // if a valid record was read before (or it is the first time we read something)
  // assert(bs_reader->status == BGPSTREAM_READER_STATUS_VALID_ENTRY)
  bgpstream_debug("\t\tBSR: read new data: bd_entry has to be set to NULL");
  // entry should not be destroyed, otherwise we could destroy
  // what is in the current record, the next export record will take
  // care of it
  bs_reader->bd_entry = NULL;
  // check if the end of the file was already reached before
  // reading a new value
  if(bs_reader->bd_mgr->eof != 0) {
    bgpstream_debug("\t\tBSR: read new data: end of file reached");      
    bs_reader->status = BGPSTREAM_READER_STATUS_END_OF_DUMP;
    return;
  }
  bgpstream_debug("\t\tBSR: read new data (previous): %ld\t%ld\t%s\t%s\t%d",  
	bs_reader->record_time,
	bs_reader->dump_time,
	bs_reader->dump_type, 
	bs_reader->dump_collector,
	bs_reader->status);
  bgpstream_debug("\t\tBSR: read new data: reading new entry (or entries) in bgpdump");   
  bgpstream_debug("\t\tBSR: read new data: from %s", bs_reader->dump_name);
  while(!significant_entry) {
    bgpstream_debug("\t\t\tBSR: read new data: reading");   
    bs_reader->bd_entry = bgpdump_read_next(bs_reader->bd_mgr);   
    // check if an entry has been read
    if(bs_reader->bd_entry != NULL) {
      bs_reader->successful_read++;
      // check if entry is compatible with filters
      if(bgpstream_reader_filter_bd_entry(bs_reader->bd_entry, filter_mgr)) {
	// update reader fields
	bs_reader->valid_read++;
	bs_reader->status = BGPSTREAM_READER_STATUS_VALID_ENTRY;
	bs_reader->record_time = (long) bs_reader->bd_entry->time;
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
      if(bs_reader->bd_mgr->corrupted_read) {
	bs_reader->status = BGPSTREAM_READER_STATUS_CORRUPTED_DUMP;
	significant_entry = true;	
      }
      // empty read, not corrupted
      else {
	// end of file
	if(bs_reader->bd_mgr->eof != 0) {
	  significant_entry = true;
	  if(bs_reader->successful_read == 0) {
	    // file was empty
	    bs_reader->status = BGPSTREAM_READER_STATUS_EMPTY_DUMP;
	  }
	  else {
	    if(bs_reader->valid_read == 0) {
	      // valid contained entries, but
	      // none of them was compatible with
	      // filters
	      bs_reader->status = BGPSTREAM_READER_STATUS_FILTERED_DUMP;
	    }
	    else {
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
  bgpstream_debug("\t\tBSR: read new data: end");  
  return;
}


static bgpstream_reader_t * bgpstream_reader_create(const bgpstream_input_t * const bs_input,
						    const bgpstream_filter_mgr_t * const filter_mgr) {
  bgpstream_debug("\t\tBSR: create reader start");
  if(bs_input == NULL) {
    bgpstream_debug("\t\tBSR: create reader: empty bs_input provided");
    bgpstream_debug("\t\tBSR: create reader end");
    return NULL; // no input to read
  }
  // allocate memory for reader
  bgpstream_reader_t *bs_reader = (bgpstream_reader_t*) malloc(sizeof(bgpstream_reader_t));
  if(bs_reader == NULL) {
    bgpstream_debug("\t\tBSR: create reader: can't allocate memory for reader");
    bgpstream_debug("\t\tBSR: create reader end"); 
    return NULL; // can't allocate memory for reader
  }
  bgpstream_debug("\t\tBSR: create reader: initialize fields");
  // fields initialization
  bs_reader->next = NULL;
  bs_reader->bd_mgr = NULL;
  bs_reader->bd_entry = NULL;
  memset(bs_reader->dump_name, 0, BGPSTREAM_DUMP_MAX_LEN);
  memset(bs_reader->dump_project, 0, BGPSTREAM_PAR_MAX_LEN);
  memset(bs_reader->dump_collector, 0, BGPSTREAM_PAR_MAX_LEN);
  memset(bs_reader->dump_type, 0, BGPSTREAM_PAR_MAX_LEN);
  // init done
  strcpy(bs_reader->dump_name, bs_input->filename);
  strcpy(bs_reader->dump_project, bs_input->fileproject);
  strcpy(bs_reader->dump_collector, bs_input->filecollector);
  strcpy(bs_reader->dump_type, bs_input->filetype);
  bs_reader->dump_time = bs_input->epoch_filetime;
  bs_reader->record_time = bs_input->epoch_filetime;
  bs_reader->status = BGPSTREAM_READER_STATUS_VALID_ENTRY; // let's be optimistic :)
  bs_reader->valid_read = 0;
  bs_reader->successful_read = 0;
  // open bgpdump
  bgpstream_debug("\t\tBSR: create reader: bgpdump_open");
  bs_reader->bd_mgr = bgpdump_open_dump(bs_reader->dump_name);   
  if(bs_reader->bd_mgr == NULL) {     
    bgpstream_debug("\t\tBSR: create reader: bgpdump_open_dump fails to open");
    bs_reader->status = BGPSTREAM_READER_STATUS_CANT_OPEN_DUMP;
    return bs_reader; // can't open bgpdump
  }
  // call bgpstream_reader_read_new_data
  bgpstream_debug("\t\tBSR: create reader: read new data");
  bgpstream_reader_read_new_data(bs_reader, filter_mgr);
  bgpstream_debug("\t\tBSR: create reader: end");  
  // return reader
  return bs_reader;
}



static void bgpstream_reader_export_record(bgpstream_reader_t * const bs_reader, 
					   bgpstream_record_t * const bs_record) {
  bgpstream_debug("\t\tBSR: export record: start");    
  if(bs_reader == NULL) {
    bgpstream_debug("\t\tBSR: export record: invalid reader provided");    
    return;
  }  
  if(bs_record == NULL) {
    bgpstream_debug("\t\tBSR: export record: invalid record provided");    
    return;
  }  
  // if bs_reader status is BGPSTREAM_READER_STATUS_END_OF_DUMP we shouldn't have called this
  // function
  if(bs_reader->status == BGPSTREAM_READER_STATUS_END_OF_DUMP) {
    bgpstream_debug("\t\tBSR: export record: end of dump was reached");    
    return;
  }  
  // read bgpstream_reader field and copy them to a bs_record
  bgpstream_debug("\t\tBSR: export record: copying bd_entry");    
  bs_record->bd_entry = bs_reader->bd_entry;
  // disconnect reader from exported entry
  bs_reader->bd_entry = NULL;
  // memset(bs_record->attributes.dump_project, 0, BGPSTREAM_PAR_MAX_LEN);
  // memset(bs_record->attributes.dump_collector, 0, BGPSTREAM_PAR_MAX_LEN);
  bgpstream_debug("\t\tBSR: export record: copying attributes");    
  strcpy(bs_record->attributes.dump_project, bs_reader->dump_project);
  strcpy(bs_record->attributes.dump_collector, bs_reader->dump_collector);
  //   strcpy(bs_record->attributes.dump_type, bs_reader->dump_type);
  if(strcmp(bs_reader->dump_type, "ribs") == 0) {
    bs_record->attributes.dump_type = BGPSTREAM_RIB;
  }
  else {
    bs_record->attributes.dump_type = BGPSTREAM_UPDATE;
  }
  bs_record->attributes.dump_time = bs_reader->dump_time;
  bs_record->attributes.record_time = bs_reader->record_time;
  // if this is the first significant record and no previous 
  // valid record has been discarded because of time
  if(bs_reader->valid_read == 1 && bs_reader->successful_read == 1) {
    bs_record->dump_pos = BGPSTREAM_DUMP_START;
  }
  else {
    bs_record->dump_pos = BGPSTREAM_DUMP_MIDDLE;
  }
  bgpstream_debug("\t\tBSR: export record: copying status");    
  switch(bs_reader->status){
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
  bgpstream_debug("Exported: %ld\t%ld\t%d\t%s\t%d", 		   
		   bs_record->attributes.record_time,
		   bs_record->attributes.dump_time,
		   bs_record->attributes.dump_type, 
		   bs_record->attributes.dump_collector,
		   bs_record->status);
  bgpstream_debug("\t\tBSR: export record: end");    
}


static void bgpstream_reader_destroy(bgpstream_reader_t * const bs_reader) {
  bgpstream_debug("\t\tBSR: destroy reader start");
  if(bs_reader == NULL) {
    bgpstream_debug("\t\tBSR: destroy reader: null reader provided");    
    return;
  }
  // we do not deallocate memory for bd_entry
  // (the last entry may be still in use in
  // the current record)
  bs_reader->bd_entry = NULL;
  // close bgpdump
  bgpdump_close_dump(bs_reader->bd_mgr);
  bs_reader->bd_mgr = NULL;  
  // deallocate all memory for reader
  free(bs_reader);
  bgpstream_debug("\t\tBSR: destroy reader end");
}

 //function used for debug
static void print_reader_queue(const bgpstream_reader_t * const reader_queue) {
#ifdef NDEBUG
  const bgpstream_reader_t * iterator = reader_queue;
  bgpstream_debug("READER QUEUE: start");
  int i = 1;
  while(iterator != NULL) {    
    bgpstream_debug("\t%d %s %s %ld %ld %d",i, iterator->dump_collector, iterator->dump_type, iterator->dump_time, iterator->record_time, iterator->status);
    iterator = iterator->next;
    i++;
  }
  iterator = NULL;
  bgpstream_debug("\nREADER QUEUE: end");  
#endif
}




/* -------------- Reader mgr functions -------------- */


bgpstream_reader_mgr_t * bgpstream_reader_mgr_create(const bgpstream_filter_mgr_t * const filter_mgr) {
  bgpstream_debug("\tBSR_MGR: create reader mgr: start");
  // allocate memory and initialize fields
  bgpstream_reader_mgr_t *bs_reader_mgr = (bgpstream_reader_mgr_t*) malloc(sizeof(bgpstream_reader_mgr_t));
  if(bs_reader_mgr == NULL) {
    bgpstream_debug("\tBSR_MGR: create reader mgr: can't allocate memory for reader mgr");
    bgpstream_debug("\tBSR_MGR: create reader mgr end");
    return NULL; // can't allocate memory for reader
  }
  // mgr initialization
  bgpstream_debug("\tBSR_MGR: create reader mgr: initialization");
  bs_reader_mgr->reader_queue = NULL;
  bs_reader_mgr->filter_mgr = filter_mgr;
  bs_reader_mgr->status = BGPSTREAM_READER_MGR_STATUS_EMPTY_READER_MGR;
  bgpstream_debug("\tBSR_MGR: create reader mgr: end");
  return bs_reader_mgr;
}


bool bgpstream_reader_mgr_is_empty(const bgpstream_reader_mgr_t * const bs_reader_mgr) {
  bgpstream_debug("\tBSR_MGR: is_empty start");
  if(bs_reader_mgr == NULL) {
  bgpstream_debug("\tBSR_MGR: is_empty end: empty!");
    return true;
  }
  if(bs_reader_mgr->status == BGPSTREAM_READER_MGR_STATUS_EMPTY_READER_MGR) {
    bgpstream_debug("\tBSR_MGR: is_empty end: empty!");
    return true;
  }
  else {
    bgpstream_debug("\tBSR_MGR: is_empty end: non-empty!");
    return false;
  }
}


static void bgpstream_reader_mgr_sorted_insert(bgpstream_reader_mgr_t * const bs_reader_mgr, 
					       bgpstream_reader_t * const bs_reader) {
  bgpstream_debug("\tBSR_MGR: sorted insert:start");
  if(bs_reader_mgr == NULL) {
    bgpstream_debug("\tBSR_MGR: sorted insert: null reader mgr provided");    
    return;
  }
  if(bs_reader == NULL) {
    bgpstream_debug("\tBSR_MGR: sorted insert: null reader provided");    
    return;
  }  

  bgpstream_reader_t * iterator = bs_reader_mgr->reader_queue;
  bgpstream_reader_t * previous_iterator = bs_reader_mgr->reader_queue;
  bool inserted = false;
  // insert new reader in priority queue
  while(!inserted) {    
    // case 1: insertion in empty queue (reader_queue == NULL)
    if(bs_reader_mgr->status == BGPSTREAM_READER_MGR_STATUS_EMPTY_READER_MGR) {
      bs_reader_mgr->reader_queue = bs_reader;
      bs_reader->next = NULL;
      bs_reader_mgr->status = BGPSTREAM_READER_MGR_STATUS_NON_EMPTY_READER_MGR;  
      inserted = true;
      continue;
    }
    // case 2: insertion in non-empty queue
    else {
      // reached the end of the queue
      if(iterator == NULL) {
	previous_iterator->next = bs_reader;
	bs_reader->next = NULL;
	inserted = true;
	continue;
      }
      // still in the middle of the queue
      else {
	// if time is the same
	if(bs_reader->record_time == iterator->record_time) {
	  // if type is the same -> continue
	  if(strcmp(iterator->dump_type, bs_reader->dump_type) == 0){
	    previous_iterator = iterator;
	    iterator = previous_iterator->next;
	    continue;
	  }
	  // if the queue contains ribs, and the current reader
	  // is an update -> continue
	  if(strcmp(iterator->dump_type, "ribs") == 0 &&
	     strcmp(bs_reader->dump_type, "updates") == 0){
	    previous_iterator = iterator;
	    iterator = previous_iterator->next;
	    continue;
	  }
	  // if the queue contains updates, and the current reader 
	  // is a rib -> insert
	  if(strcmp(iterator->dump_type, "updates") == 0 &&
	     strcmp(bs_reader->dump_type, "ribs") == 0){
	    // insertion at the beginning of the queue
	    if(previous_iterator == bs_reader_mgr->reader_queue &&
	       iterator == bs_reader_mgr->reader_queue) {
	      bs_reader->next = bs_reader_mgr->reader_queue;
	      bs_reader_mgr->reader_queue = bs_reader;
	    }
	    // insertion in the middle of the queue
	    else {
	      previous_iterator->next = bs_reader;
	      bs_reader->next = iterator;
	    }
	    inserted = true;
	    continue;
	  }
	}
	// if time is different
	else {
	  // if reader time is lower than iterator time
	  // then insert
	  if(bs_reader->record_time < iterator->record_time) {
	    // insertion at the beginning of the queue
	    if(previous_iterator == bs_reader_mgr->reader_queue &&
	       iterator == bs_reader_mgr->reader_queue) {
	      bs_reader->next = bs_reader_mgr->reader_queue;
	      bs_reader_mgr->reader_queue = bs_reader;
	    }
	    // insertion in the middle of the queue
	    else {
	      previous_iterator->next = bs_reader;
	      bs_reader->next = iterator;
	    }
	    inserted = true;	    
	    continue;
	  }
	}
	// otherwise update the iterators
	previous_iterator = iterator;
	iterator = previous_iterator->next;
      }      
    }  
  }
  bgpstream_debug("\tBSR_MGR: sorted insert: end");
}


static int
bgpstream_reader_frequency_check(const bgpstream_input_t *in,
                                 const bgpstream_filter_mgr_t *const filter_mgr)
{
  /* consider making this buffer static at the beginning of the file */
  char buffer[BUFFER_LEN];
  khiter_t k;
  int khret;

  if((filter_mgr->update_frequency != 0  &&
      strcmp(in->filetype, "updates") == 0) ||
     (filter_mgr->rib_frequency != 0  &&
      strcmp(in->filetype, "ribs") == 0))      
    {
      snprintf(buffer, BUFFER_LEN, "%s.%s.%s",
               in->fileproject, in->filecollector, in->filetype);             
      /* first instance */
      if((k = kh_get(collector_type_ts, filter_mgr->last_processed_ts,
                     buffer)) == kh_end(filter_mgr->last_processed_ts))
        {
          k = kh_put(collector_type_ts, filter_mgr->last_processed_ts,
                     strdup(buffer), &khret);
          kh_value(filter_mgr->last_processed_ts, k) = in->epoch_filetime;
          return 1;
        }

      /* we have to check the frequency only if we get a new filetime */
      if(in->epoch_filetime != kh_value(filter_mgr->last_processed_ts, k))
        {
          if(strcmp(in->filetype, "ribs") == 0 &&
             in->epoch_filetime <
             kh_value(filter_mgr->last_processed_ts, k) + filter_mgr->rib_frequency)
            {
              return 0;
            }
          if(strcmp(in->filetype, "updates") == 0 &&
             in->epoch_filetime <
             kh_value(filter_mgr->last_processed_ts, k) + filter_mgr->update_frequency)
            {
              return 0;
            }      
          kh_value(filter_mgr->last_processed_ts, k) = in->epoch_filetime;
        }
    }

  return 1;
}


void bgpstream_reader_mgr_add(bgpstream_reader_mgr_t * const bs_reader_mgr, 
			      const bgpstream_input_t * const toprocess_queue,
			      const bgpstream_filter_mgr_t * const filter_mgr) {
  bgpstream_debug("\tBSR_MGR: add input: start");
  const bgpstream_input_t * iterator = toprocess_queue;
  bgpstream_reader_t * bs_reader = NULL;
  // foreach  bgpstream input:
  while(iterator != NULL) {
    if(bgpstream_reader_frequency_check(iterator, filter_mgr))
      {
        bgpstream_debug("\tBSR_MGR: add input: i");
        // a) create a new reader (create includes the first read)
        bs_reader = bgpstream_reader_create(iterator, filter_mgr);
        // if it creates correctly
        if(bs_reader != NULL) {
          // then add the reader to the sorted queue
          bgpstream_reader_mgr_sorted_insert(bs_reader_mgr, bs_reader);
        }
        else{
          bgpstream_log_err("ERROR: could not create reader\n");
          return;
        }
      }
    // go to the next input
    iterator = iterator->next;
  }
  print_reader_queue(bs_reader_mgr->reader_queue);
  bgpstream_debug("\tBSR_MGR: add input: end");
}



int bgpstream_reader_mgr_get_next_record(bgpstream_reader_mgr_t * const bs_reader_mgr, 
					 bgpstream_record_t *const bs_record,
					 const bgpstream_filter_mgr_t * const filter_mgr) {
  bgpstream_debug("\tBSR_MGR: get_next_record: start");
  if(bs_reader_mgr == NULL) {
    bgpstream_debug("\tBSR_MGR: get_next_record: null reader mgr provided");    
    return -1;
  }
  if(bs_record == NULL) {
    bgpstream_debug("BSR_MGR: get_next_record: null record provided");    
    return -1;
  }
  if(bs_reader_mgr->status == BGPSTREAM_READER_MGR_STATUS_EMPTY_READER_MGR) {
    bgpstream_debug("\tBSR_MGR: get_next_record: empty reader mgr");    
    return 0;
  }
  // get head from reader queue 
  bgpstream_reader_t *bs_reader = bs_reader_mgr->reader_queue;
  bs_reader_mgr->reader_queue = bs_reader_mgr->reader_queue->next;
  if(bs_reader_mgr->reader_queue == NULL) { // check if last reader
    bs_reader_mgr->status = BGPSTREAM_READER_MGR_STATUS_EMPTY_READER_MGR;
  }
  // disconnect reader from the queue
  bs_reader->next = NULL;
  // bgpstream_reader_export
  bgpstream_reader_export_record(bs_reader, bs_record);
  // we save the difference between successful read
  // and valid read, so we can check if some valid data
  // have been discarded during the last read_new_data
  int read_diff = bs_reader->successful_read - bs_reader->valid_read;
  // if previous read was successful, we read next
  // entry from same reader
  if(bs_reader->status == BGPSTREAM_READER_STATUS_VALID_ENTRY) {
    bgpstream_reader_read_new_data(bs_reader, filter_mgr);
    // if end of dump is reached after a successful read (already exported)
    // we destroy the reader
    if(bs_reader->status == BGPSTREAM_READER_STATUS_END_OF_DUMP) {
      if((bs_reader->successful_read - bs_reader->valid_read) == read_diff) {
	bs_record->dump_pos = BGPSTREAM_DUMP_END;
      }
      // otherwise we maintain the dump_pos already assigned
      bgpstream_reader_destroy(bs_reader);
    }
    // otherwise we insert the reader in the queue again
    else {
      bgpstream_reader_mgr_sorted_insert(bs_reader_mgr, bs_reader);
    }
  }
  // otherwise we destroy the reader
  else {
    bs_record->dump_pos = BGPSTREAM_DUMP_END;
    bgpstream_reader_destroy(bs_reader);
  }
  bgpstream_debug("\tBSR_MGR: get_next_record: end");
  return 1; 
}


void bgpstream_reader_mgr_destroy(bgpstream_reader_mgr_t * const bs_reader_mgr) {
  bgpstream_debug("\tBSR_MGR: destroy reader mgr: start");
  if(bs_reader_mgr == NULL) {
    bgpstream_debug("\tBSR_MGR: destroy reader mgr:  null reader mgr provided");    
    return;
  }
  bs_reader_mgr->filter_mgr = NULL;
  bgpstream_debug("\tBSR_MGR: destroy reader mgr:  destroying reader queue");    
  // foreach reader in queue: destroy reader
  bgpstream_reader_t *iterator;
  while(bs_reader_mgr->reader_queue !=NULL){
    // at every step we destroy the reader referenced
    // by the reader_queue
    iterator = bs_reader_mgr->reader_queue;
    bs_reader_mgr->reader_queue = iterator->next;
    bgpstream_reader_destroy(iterator);
  }
  bs_reader_mgr->status = BGPSTREAM_READER_MGR_STATUS_EMPTY_READER_MGR;
  free(bs_reader_mgr);
  bgpstream_debug("\tBSR_MGR: destroy reader mgr: end");
}

