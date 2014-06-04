/*
 * libbgpstream
 *
 * Chiara Orsini, CAIDA, UC San Diego
 * chiara@caida.org
 *
 * Copyright (C) 2013 The Regents of the University of California.
 *
 * This file is part of libbgpstream.
 *
 * libbgpstream is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * libbgpstream is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with libbgpstream.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "bgpstream_reader.h"
#include "bgpstream_input.h"

#include <stdlib.h>
#include <stdio.h>
#include "debug.h"


/* -------------- Reader functions -------------- */

static bool bgpstream_reader_filter_bd_entry(const BGPDUMP_ENTRY * const bd_entry, 
					     const bgpstream_filter_mgr_t * const filter_mgr) {
  debug("\t\tBSR: filter entry: start");  
  bool res = false;
  if(bd_entry != NULL && filter_mgr != NULL) {
    long current_entry_time = (long) bd_entry->time;
    if(current_entry_time >= filter_mgr->time_interval_start &&
       current_entry_time <= filter_mgr->time_interval_stop) {
      debug("\t\tBSR: filter entry: OK");  
      res = true;
    }
    else{
      debug("\t\tBSR: filter entry: DISCARDED");  
    }
  }
  debug("\t\tBSR: filter entry: end");  
  return res;
}


static void bgpstream_reader_read_new_data(bgpstream_reader_t * const bs_reader,
					   const bgpstream_filter_mgr_t * const filter_mgr) {
  bool significant_entry = false;
  debug("\t\tBSR: read new data: start");  
  if(bs_reader == NULL) {
    debug("\t\tBSR: read new data: invalid reader provided");    
    return;
  }  
  if(bs_reader->status != VALID_ENTRY) {
    debug("\t\tBSR: read new data: reader cannot read new data (previous read was not successful)");    
    return;
  }  
  // if a valid record was read before (or it is the first time we read something)
  // assert(bs_reader->status == VALID_ENTRY)
  debug("\t\tBSR: read new data: bd_entry has to be set to NULL");
  // entry should not be destroyed, otherwise we could destroy
  // what is in the current record, the next export record will take
  // care of it
  bs_reader->bd_entry = NULL;
  // check if the end of the file was already reached before
  // reading a new value
  if(bs_reader->bd_mgr->eof != 0) {
    debug("\t\tBSR: read new data: end of file reached");      
    bs_reader->status = END_OF_DUMP;
    return;
  }
  debug("\t\tBSR: read new data (previous): %ld\t%ld\t%s\t%s\t%d",  
	bs_reader->record_time,
	bs_reader->dump_time,
	bs_reader->dump_type, 
	bs_reader->dump_collector,
	bs_reader->status);
  debug("\t\tBSR: read new data: reading new entry (or entries) in bgpdump");   
  debug("\t\tBSR: read new data: from %s", bs_reader->dump_name);
  while(!significant_entry) {
    debug("\t\t\tBSR: read new data: reading");   
    bs_reader->bd_entry = bgpdump_read_next(bs_reader->bd_mgr);   
    // check if an entry has been read
    if(bs_reader->bd_entry != NULL) {
      bs_reader->successful_read++;
      // check if entry is compatible with filters
      if(bgpstream_reader_filter_bd_entry(bs_reader->bd_entry, filter_mgr)) {
	// update reader fields
	bs_reader->valid_read++;
	bs_reader->status = VALID_ENTRY;
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
	bs_reader->status = CORRUPTED_DUMP;
	significant_entry = true;	
      }
      // empty read, not corrupted
      else {
	// end of file
	if(bs_reader->bd_mgr->eof != 0) {
	  significant_entry = true;
	  if(bs_reader->successful_read == 0) {
	    // file was empty
	    bs_reader->status = EMPTY_DUMP;
	  }
	  else {
	    if(bs_reader->valid_read == 0) {
	      // valid contained entries, but
	      // none of them was compatible with
	      // filters
	      bs_reader->status = FILTERED_DUMP;
	    }
	    else {
	      // something valid has bee read before
	      // reached the end of file
	      bs_reader->status = END_OF_DUMP;	
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
  debug("\t\tBSR: read new data: end");  
  return;
}


static bgpstream_reader_t * bgpstream_reader_create(const bgpstream_input_t * const bs_input,
						    const bgpstream_filter_mgr_t * const filter_mgr) {
  debug("\t\tBSR: create reader start");
  if(bs_input == NULL) {
    debug("\t\tBSR: create reader: empty bs_input provided");
    debug("\t\tBSR: create reader end");
    return NULL; // no input to read
  }
  // allocate memory for reader
  bgpstream_reader_t *bs_reader = (bgpstream_reader_t*) malloc(sizeof(bgpstream_reader_t));
  if(bs_reader == NULL) {
    debug("\t\tBSR: create reader: can't allocate memory for reader");
    debug("\t\tBSR: create reader end"); 
    return NULL; // can't allocate memory for reader
  }
  debug("\t\tBSR: create reader: initialize fields");
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
  bs_reader->status = VALID_ENTRY; // let's be optimistic :)
  bs_reader->valid_read = 0;
  bs_reader->successful_read = 0;
  // open bgpdump
  debug("\t\tBSR: create reader: bgpdump_open");
  bs_reader->bd_mgr = bgpdump_open_dump(bs_reader->dump_name);   
  if(bs_reader->bd_mgr == NULL) {     
    debug("\t\tBSR: create reader: bgpdump_open_dump fails to open");
    bs_reader->status = CANT_OPEN_DUMP;
    return bs_reader; // can't open bgpdump
  }
  // call bgpstream_reader_read_new_data
  debug("\t\tBSR: create reader: read new data");
  bgpstream_reader_read_new_data(bs_reader, filter_mgr);
  debug("\t\tBSR: create reader: end");  
  // return reader
  return bs_reader;
}



static void bgpstream_reader_export_record(bgpstream_reader_t * const bs_reader, 
					   bgpstream_record_t * const bs_record) {
  debug("\t\tBSR: export record: start");    
  if(bs_reader == NULL) {
    debug("\t\tBSR: export record: invalid reader provided");    
    return;
  }  
  if(bs_record == NULL) {
    debug("\t\tBSR: export record: invalid record provided");    
    return;
  }  
  // if bs_reader status is END_OF_DUMP we shouldn't have called this
  // function
  if(bs_reader->status == END_OF_DUMP) {
    debug("\t\tBSR: export record: end of dump was reached");    
    return;
  }  
  // if bs_record contains an initialized bgpdump entry we destroy it
  if(bs_record->bd_entry != NULL) {
    debug("\t\tBSR: export record: free memory for bgpdump entry");    
    // at this point the client/user has already used (or made a
    // a copy of) the record
    bgpdump_free_mem(bs_record->bd_entry);
    bs_record->bd_entry = NULL;
  }
  // read bgpstream_reader field and copy them to a bs_record
  debug("\t\tBSR: export record: copying bd_entry");    
  bs_record->bd_entry = bs_reader->bd_entry;
  // disconnect reader from exported entry
  bs_reader->bd_entry = NULL;
  memset(bs_record->attributes.dump_project, 0, BGPSTREAM_PAR_MAX_LEN);
  memset(bs_record->attributes.dump_collector, 0, BGPSTREAM_PAR_MAX_LEN);
  memset(bs_record->attributes.dump_type, 0, BGPSTREAM_PAR_MAX_LEN);
  debug("\t\tBSR: export record: copying attributes");    
  strcpy(bs_record->attributes.dump_project, bs_reader->dump_project);
  strcpy(bs_record->attributes.dump_collector, bs_reader->dump_collector);
  strcpy(bs_record->attributes.dump_type, bs_reader->dump_type);
  bs_record->attributes.dump_time = bs_reader->dump_time;
  bs_record->attributes.record_time = bs_reader->record_time;
  debug("\t\tBSR: export record: copying status");    
  switch(bs_reader->status){
  case VALID_ENTRY:
    bs_record->status = VALID_RECORD;
    break;
  case FILTERED_DUMP:
    bs_record->status = FILTERED_SOURCE;
    break;
  case EMPTY_DUMP:
    bs_record->status = EMPTY_SOURCE;
    break;
  case CANT_OPEN_DUMP:
    bs_record->status = CORRUPTED_SOURCE;
    break;
  case CORRUPTED_DUMP:
    bs_record->status = CORRUPTED_RECORD;
    break;
  default:
    bs_record->status = EMPTY_SOURCE;
  }
  debug("Exported: %ld\t%ld\t%s\t%s\t%d\n", 		   
		   bs_record->attributes.record_time,
		   bs_record->attributes.dump_time,
		   bs_record->attributes.dump_type, 
		   bs_record->attributes.dump_collector,
		   bs_record->status);
  debug("\t\tBSR: export record: end");    
}


static void bgpstream_reader_destroy(bgpstream_reader_t * const bs_reader) {
  debug("\t\tBSR: destroy reader start");
  if(bs_reader == NULL) {
    debug("\t\tBSR: destroy reader: null reader provided");    
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
  debug("\t\tBSR: destroy reader end");
}

/* //function used for debug
static void print_reader_queue(const bgpstream_reader_t * const reader_queue) {
  const bgpstream_reader_t * iterator = reader_queue;
  debug("QUEUE: start\n");
  while(iterator != NULL) {    
    debug("\t%s(%d/%p)",iterator->dump_collector, iterator->status, iterator);
    iterator = iterator->next;
  }
  iterator = NULL;
  debug("\nQUEUE: end\n");  
}
*/



/* -------------- Reader mgr functions -------------- */


bgpstream_reader_mgr_t * bgpstream_reader_mgr_create(const bgpstream_filter_mgr_t * const filter_mgr) {
  debug("\tBSR_MGR: create reader mgr: start");
  // allocate memory and initialize fields
  bgpstream_reader_mgr_t *bs_reader_mgr = (bgpstream_reader_mgr_t*) malloc(sizeof(bgpstream_reader_mgr_t));
  if(bs_reader_mgr == NULL) {
    debug("\tBSR_MGR: create reader mgr: can't allocate memory for reader mgr");
    debug("\tBSR_MGR: create reader mgr end");
    return NULL; // can't allocate memory for reader
  }
  // mgr initialization
  debug("\tBSR_MGR: create reader mgr: initialization");
  bs_reader_mgr->reader_queue = NULL;
  bs_reader_mgr->filter_mgr = filter_mgr;
  bs_reader_mgr->status = EMPTY_READER_MGR;
  debug("\tBSR_MGR: create reader mgr: end");
  return bs_reader_mgr;
}


bool bgpstream_reader_mgr_is_empty(const bgpstream_reader_mgr_t * const bs_reader_mgr) {
  debug("\tBSR_MGR: is_empty start");
  if(bs_reader_mgr == NULL) {
  debug("\tBSR_MGR: is_empty end: empty!");
    return true;
  }
  if(bs_reader_mgr->status == EMPTY_READER_MGR) {
    debug("\tBSR_MGR: is_empty end: empty!");
    return true;
  }
  else {
    debug("\tBSR_MGR: is_empty end: non-empty!");
    return false;
  }
}


static void bgpstream_reader_mgr_sorted_insert(bgpstream_reader_mgr_t * const bs_reader_mgr, 
					       bgpstream_reader_t * const bs_reader) {
  debug("\tBSR_MGR: sorted insert:start");
  if(bs_reader_mgr == NULL) {
    debug("\tBSR_MGR: sorted insert: null reader mgr provided");    
    return;
  }
  if(bs_reader == NULL) {
    debug("\tBSR_MGR: sorted insert: null reader provided");    
    return;
  }  

  bgpstream_reader_t * iterator = bs_reader_mgr->reader_queue;
  bgpstream_reader_t * previous_iterator = bs_reader_mgr->reader_queue;
  bool inserted = false;
  // insert new reader in priority queue
  while(!inserted) {    
    // case 1: insertion in empty queue (reader_queue == NULL)
    if(bs_reader_mgr->status == EMPTY_READER_MGR) {
      bs_reader_mgr->reader_queue = bs_reader;
      bs_reader->next = NULL;
      bs_reader_mgr->status = NON_EMPTY_READER_MGR;  
      inserted = true;
    }
    // case 2: insertion in non-empty queue
    else {
      // reached the end of the queue
      if(iterator == NULL) {
	previous_iterator->next = bs_reader;
	bs_reader->next = NULL;
	inserted = true;
      }
      // still in the middle of the queue
      else {
	// if new position has been found
	if(bs_reader->record_time < iterator->record_time) {
	  // insertion at the beginning of the queue
	  if(previous_iterator == bs_reader_mgr->reader_queue &&
	     iterator == bs_reader_mgr->reader_queue) {
	    bs_reader->next = bs_reader_mgr->reader_queue;
	    bs_reader_mgr->reader_queue = bs_reader;
	    inserted = true;
	  }
	  // insertion in the middle of the queue
	  else {
	    previous_iterator->next = bs_reader;
	    bs_reader->next = iterator;
	    inserted = true;	    
	  }
	}
	// otherwise update the iterators
	else {
	  previous_iterator = iterator;
	  iterator = previous_iterator->next;
	}
      }
    }  
  }
  debug("\tBSR_MGR: sorted insert: end");
}



void bgpstream_reader_mgr_add(bgpstream_reader_mgr_t * const bs_reader_mgr, 
			      const bgpstream_input_t * const toprocess_queue,
			      const bgpstream_filter_mgr_t * const filter_mgr) {
  debug("\tBSR_MGR: add input: start");
  const bgpstream_input_t * iterator = toprocess_queue;
  bgpstream_reader_t * bs_reader = NULL;
  // foreach  bgpstream input:
  while(iterator != NULL) {
    debug("\tBSR_MGR: add input: i");
    // a) create a new reader (create includes the first read)
    bs_reader = bgpstream_reader_create(iterator, filter_mgr);
    // if it creates correctly
    if(bs_reader != NULL) {
      // then add the reader to the sorted queue
      bgpstream_reader_mgr_sorted_insert(bs_reader_mgr, bs_reader);
    }
    else{
      log_err("ERROR\n");
      return;
    }
    // go to the next input
    iterator = iterator->next;
  }
  debug("\tBSR_MGR: add input: end");
}



int bgpstream_reader_mgr_get_next_record(bgpstream_reader_mgr_t * const bs_reader_mgr, 
					 bgpstream_record_t *const bs_record,
					 const bgpstream_filter_mgr_t * const filter_mgr) {
  debug("\tBSR_MGR: get_next_record: start");
  if(bs_reader_mgr == NULL) {
    debug("\tBSR_MGR: get_next_record: null reader mgr provided");    
    return -1;
  }
  if(bs_record == NULL) {
    debug("BSR_MGR: get_next_record: null record provided");    
    return -1;
  }
  if(bs_reader_mgr->status == EMPTY_READER_MGR) {
    debug("\tBSR_MGR: get_next_record: empty reader mgr");    
    return 0;
  }
  // get head from reader queue 
  bgpstream_reader_t *bs_reader = bs_reader_mgr->reader_queue;
  bs_reader_mgr->reader_queue = bs_reader_mgr->reader_queue->next;
  if(bs_reader_mgr->reader_queue == NULL) { // check if last reader
    bs_reader_mgr->status = EMPTY_READER_MGR;
  }
  // disconnect reader from the queue
  bs_reader->next = NULL;
  // bgpstream_reader_export
  bgpstream_reader_export_record(bs_reader, bs_record);
  // if previous read was successful, we read next
  // entry from same reader
  if(bs_reader->status == VALID_ENTRY) {
    bgpstream_reader_read_new_data(bs_reader, filter_mgr);
    // if end of dump is reached after a successful read (already exported)
    // we destroy the reader
    if(bs_reader->status == END_OF_DUMP) {
      bgpstream_reader_destroy(bs_reader);
    }
    // otherwise we insert the reader in the queue again
    else {
      bgpstream_reader_mgr_sorted_insert(bs_reader_mgr, bs_reader);
    }
  }
  // otherwise we destroy the reader
  else {
      bgpstream_reader_destroy(bs_reader);
  }
  debug("\tBSR_MGR: get_next_record: end");
  return 1; 
}


void bgpstream_reader_mgr_destroy(bgpstream_reader_mgr_t * const bs_reader_mgr) {
  debug("\tBSR_MGR: destroy reader mgr: start");
  if(bs_reader_mgr == NULL) {
    debug("\tBSR_MGR: destroy reader mgr:  null reader mgr provided");    
    return;
  }
  bs_reader_mgr->filter_mgr = NULL;
  debug("\tBSR_MGR: destroy reader mgr:  destroying reader queue");    
  // foreach reader in queue: destroy reader
  bgpstream_reader_t *iterator;
  while(bs_reader_mgr->reader_queue !=NULL){
    // at every step we destroy the reader referenced
    // by the reader_queue
    iterator = bs_reader_mgr->reader_queue;
    bs_reader_mgr->reader_queue = iterator->next;
    bgpstream_reader_destroy(iterator);
  }
  bs_reader_mgr->status = EMPTY_READER_MGR;
  free(bs_reader_mgr);
  debug("\tBSR_MGR: destroy reader mgr: end");
}

