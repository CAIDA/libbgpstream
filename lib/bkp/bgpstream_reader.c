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



static bgpstream_reader_t *bgpstream_reader_create(const bgpstream_input_t * const bs_input, 
 						   bgpstream_filter_mgr_t *filter_mgr) {
  debug("\tBSR: create reader start"); 
   if(bs_input == NULL) { */
     return NULL; // no input to read 
   }
 
   // malloc memory for reader
   bgpstream_reader_t *bs_reader = (bgpstream_reader_t*) malloc(sizeof(bgpstream_reader_t)); 
   if(bs_reader == NULL) {
     return NULL; // can't allocate memory for reader 
   } 
   bs_reader->next = NULL; 
   strcpy(bs_reader->filename, bs_input->filename);
   bs_reader->bd_mgr = NULL; 
   bs_reader->num_valid_records = 0;
   bs_reader->bs_record = NULL; 
   bs_reader->status = BS_READER_OFF;

   // read the next record and update status
   int ret = bgpstream_reader_read(bs_reader, filter_mgr);
   
   debug("\tBSR: create reader stop"); 
   return bs_reader;
}


static int bgpstream_reader_read(bgpstream_reader_t *bs_reader,
				  bgpstream_filter_mgr_t *filter_mgr) {
  debug("\tBSR: reader read start"); 
  if(bs_reader == NULL) { */
    return NULL; // no input to read 
  }

  bs_reader->bs_record = (bgpstream_record_t*) malloc(sizeof(bgpstream_record_t)); 
  if(bs_reader->bs_record == NULL) {  
    debug("BSR: something wrong with record memory allocation");
    return -1; // can't allocate memory for record */
  } 
  // default values for bs_record
  bs_reader->bs_record->attributes.status = EMPTY_DUMP; 
  bs_reader->bs_record->bd_entry = NULL;
  
  // if reader is off we turn on bgpdump
  if(bs_reader->status == BS_READER_OFF) {
    // opening bgpdump */
    bs_reader->bd_mgr = bgpdump_open_dump(bs_reader->filename);    
    // if we are unable to open the dump we still
    // signal the client with a special bs_record
    if(! bs_reader->bd_mgr) {
      bs_reader->status == BS_READER_LAST;
      bs_reader->bs_record->attributes.status = CANT_OPEN_DUMP;      
      return 0;
    }
  }


    else {
      bs_reader->status == BS_READER_ON;
      // read until we find a valid entry

      long nr_time; 
      int valid_record = 0; 
      int eof = bs_reader->bd_mgr->eof; 
      // while record is not valid and end of file has not been reached 
      while(valid_record == 0 && eof == 0) { 
	// reading one record (i.e. 1 bgpdump entry + attributes) 
	(bs_reader->bs_record)->bd_entry = bgpdump_read_next(bs_reader->bd_mgr); 
	/*     (bs_reader->bs_record)->attributes.status = 1; // attribute setting example! */
	eof = bs_reader->bd_mgr->eof;	
	// reading time 
	nr_time = (long) (bs_reader->bs_record)->bd_entry->time;  
	// time is always required 
	if(nr_time >= filter_mgr->time_interval_start &&  
	   nr_time <= filter_mgr->time_interval_stop ) { 
	  // if no peer filtering is required 
	  if(strcmp(filter_mgr->peer,"") == 0) { 
	    valid_record = 1; 
	    bs_reader->num_valid_records++;
	    debug("\tBSR: read one valid entry\n"); 
	    break; 
	  } 
	  else{ 
	    // WARNING!!!!!!!!
	    // at this point we had to check if the peer is correct
	    // it requires work on bgpdump
	    valid_record = 1;
	    break;
	  }
	}
	bgpdump_free_mem((bs_reader->bs_record)->bd_entry); 
	(bs_reader->bs_record)->bd_entry = NULL; 
	(bs_reader->bs_record)->attributes.status = EMPTY_DUMP; 
      }
    }


  debug("\tBSR: reader read stop"); 
}






// bgpstream_reader_mgr creation

/* 1) foreach value in the input list
 *    create a reader (which also means read the
 *    first bgpdump entry)
 * 2) then insert the reader in the priority queue 
 */

// bgpstream_reader_mgr get next

/* 1) pop reader from priority queue head
 * 2) save record
 * 3) get next record from reader 
 * 4) insert the reader in the priority queue 
 */



/*
/*   if(bs_input == NULL) { */
/*     return NULL; // no input to read */
/*   } */
/*   bgpstream_reader_t *bs_reader = (bgpstream_reader_t*) malloc(sizeof(bgpstream_reader_t)); */
/*   if(bs_reader == NULL) { */
/*     return NULL; // can't allocate memory for reader */
/*   } */
/*   bs_reader->previous = NULL; */
/*   bs_reader->next = NULL; */
/*   strcpy(bs_reader->filename, bs_input->filename); */

/* x  /\* allocating memory for bs_record - bgpdump will provide the right */
/*    * memory allocation for BGPDUMP_ENTRY *\/ */
/*   bs_reader->bs_record = (bgpstream_record_t*) malloc(sizeof(bgpstream_record_t)); */
/*   if(bs_reader->bs_record == NULL) { */
/*     free(bs_reader); */
/*     return NULL; // can't allocate memory for record */
/*   } */
/*   debug("\tBSR: Init record start"); */
/*   // initialize record and its attributes */
/*   (bs_reader->bs_record)->bd_entry = NULL; */
/*   (bs_reader->bs_record)->attributes.status = 0; */
/*   debug("\tBSR: Init record end"); */
  
/*   debug("\tBSR: About to open the following dump: \n"); */
/*   debug("\tBSR: \t-%s-\n", bs_reader->filename); */

/*   // opening bgpdump */
/*   bs_reader->bd_mgr = bgpdump_open_dump(bs_reader->filename);    */
/*   if(bs_reader->bd_mgr == NULL) {      */
/*     free(bs_reader->bs_record); */
/*     bs_reader->bs_record = NULL; */
/*     free(bs_reader); */
/*     bs_reader = NULL; */
/*     return NULL; // can't open bgpdump */
/*   } */
/*   debug("\tBSR: create reader end"); */
/*   // return ptr */
/*   return bs_reader; */

  

}




/* static void bgpstream_reader_read() { */


/*   long nr_time; */
/*   int valid_record = 0; */
/*   int eof = bs_reader->bd_mgr->eof; */
/*   // while record is not valid and end of file has not been reached */
/*   while(valid_record == 0 && eof == 0) { */
/*     // reading one record (i.e. 1 bgpdump entry + attributes) */
/*     (bs_reader->bs_record)->bd_entry = bgpdump_read_next(bs_reader->bd_mgr); */
/*     (bs_reader->bs_record)->attributes.status = 1; // attribute setting example! */
/*     eof = bs_reader->bd_mgr->eof; */
/*     // reading time */
/*     nr_time = (long) (bs_reader->bs_record)->bd_entry->time;  */
/*     // time is always required */
/*     if(nr_time >= filter_mgr->time_interval_start &&  */
/*        nr_time <= filter_mgr->time_interval_stop ) { */
/*       // if no peer filtering is required */
/*       if(strcmp(filter_mgr->peer,"") == 0) { */
/* 	valid_record = 1; */
/* 	debug("\tBSR: read one valid entry\n"); */
/* 	break; */
/*       } */
/*       else{ */
/* 	// WARNING!!!!!!!! */
/* 	// at this point we had to check if the peer is correct */
/* 	// it requires work on bgpdump */
/* 	valid_record = 1; */
/* 	break; */
/*       } */
/*     } */
/*     bgpdump_free_mem((bs_reader->bs_record)->bd_entry); */
/*     (bs_reader->bs_record)->bd_entry = NULL; */
/*     (bs_reader->bs_record)->attributes.status = 0; */
/*   } */

/*   if(eof != 0) { // eof reached */
/*     bgpdump_close_dump(bs_reader->bd_mgr); // close the bgpdump */
/*     bs_reader->bd_mgr = NULL; */
/*     bs_reader->status = LAST_BS_RECORD;     // signal this is the last entry */
/*     debug("\tBSR: reader created and last record\n"); */
/*   } */
/*   else {  */
/*     bs_reader->status = NEW_BS_RECORDS_AVAILABLE; // signal new entries are available */
/*     debug("\tBSR: reader created and more records"); */
/*   } */

/* } */


/* /\* Create and initialize  bgpstream reader *\/ */
/* static bgpstream_reader_t *bgpstream_reader_create(const bgpstream_input_t * const bs_input, */
/* 						   bgpstream_filter_mgr_t *filter_mgr) { */
/*   debug("\tBSR: create reader start"); */
/*   if(bs_input == NULL) { */
/*     return NULL; // no input to read */
/*   } */
/*   bgpstream_reader_t *bs_reader = (bgpstream_reader_t*) malloc(sizeof(bgpstream_reader_t)); */
/*   if(bs_reader == NULL) { */
/*     return NULL; // can't allocate memory for reader */
/*   } */
/*   bs_reader->previous = NULL; */
/*   bs_reader->next = NULL; */
/*   strcpy(bs_reader->filename, bs_input->filename); */

/* x  /\* allocating memory for bs_record - bgpdump will provide the right */
/*    * memory allocation for BGPDUMP_ENTRY *\/ */
/*   bs_reader->bs_record = (bgpstream_record_t*) malloc(sizeof(bgpstream_record_t)); */
/*   if(bs_reader->bs_record == NULL) { */
/*     free(bs_reader); */
/*     return NULL; // can't allocate memory for record */
/*   } */
/*   debug("\tBSR: Init record start"); */
/*   // initialize record and its attributes */
/*   (bs_reader->bs_record)->bd_entry = NULL; */
/*   (bs_reader->bs_record)->attributes.status = 0; */
/*   debug("\tBSR: Init record end"); */
  
/*   debug("\tBSR: About to open the following dump: \n"); */
/*   debug("\tBSR: \t-%s-\n", bs_reader->filename); */

/*   // opening bgpdump */
/*   bs_reader->bd_mgr = bgpdump_open_dump(bs_reader->filename);    */
/*   if(bs_reader->bd_mgr == NULL) {      */
/*     free(bs_reader->bs_record); */
/*     bs_reader->bs_record = NULL; */
/*     free(bs_reader); */
/*     bs_reader = NULL; */
/*     return NULL; // can't open bgpdump */
/*   } */
/*   debug("\tBSR: create reader end"); */
/*   // return ptr */
/*   return bs_reader; */
/* } */


/* Destroy bgpstream record 
 */
void bgpstream_reader_destroy_record(bgpstream_record_t *bs_record) {
  debug("\tBSR: record destroy start");
  if(bs_record == NULL) {
    debug("\tBSR: record destroy end");
    return; // nothing to do
  }
  if(bs_record->bd_entry != NULL){
    debug("\t\ttBSR - free bgpdump");
    bgpdump_free_mem(bs_record->bd_entry);
    bs_record->bd_entry = NULL;
  }
  debug("\t\ttBSR - free bsrecord");
  free(bs_record);
  debug("\tBSR: record destroy end");
}


/* Destroy bgpstream reader 
 */
static void bgpstream_reader_destroy(bgpstream_reader_t *bs_reader) {
  debug("\tBSR: reader destroy start");
  if(bs_reader == NULL) {
    debug("\tBSR: reader destroy end");
    return; // nothing to do`
  }
  // deallocating last bs_record read
  bgpstream_reader_destroy_record(bs_reader->bs_record);
  // closing bgpdump file
  if(bs_reader->bd_mgr != NULL) {
    debug("\t\tX - here");
    bgpdump_close_dump(bs_reader->bd_mgr);
    bs_reader->bd_mgr = NULL; // close dump should deallocate memory for bdump
    debug("\t\tX - here");
  }
  // deallocating bs_reader
  free(bs_reader);
  bs_reader = NULL;
  debug("\tBSR: reader destroy end");
}


/* Initialize the bgpstream reader manager  */
bgpstream_reader_mgr_t *bgpstream_reader_mgr_create(bgpstream_filter_mgr_t *filter_mgr) {
  debug("\tBSR: create mgr start");
  bgpstream_reader_mgr_t *bs_reader_mgr = (bgpstream_reader_mgr_t*) malloc(sizeof(bgpstream_reader_mgr_t));
  if(bs_reader_mgr == NULL) {
    return NULL; // can't allocate memory
  }
  // mgr initialization
  bs_reader_mgr->reader_priority_queue = NULL;
  bs_reader_mgr->filter_mgr = filter_mgr;
  bs_reader_mgr->status = EMPTY_READER;
  debug("\tBSR: mgr create end");
  return bs_reader_mgr;
}


/* Check if the current status is EMPTY 
 */
bool bgpstream_reader_mgr_is_empty(const bgpstream_reader_mgr_t * const bs_reader_mgr) {
  if(bs_reader_mgr != NULL && bs_reader_mgr->status != EMPTY_READER) {
    return false;
  }
  return true;
}


/* Returns 1 if the reader has been inserted correctly into the 
 * priority queue, returns 0 if no reader has been inserted
 */
static int bgpstream_reader_insert_in_priorityqueue(bgpstream_reader_t *reader_priority_queue,
						    bgpstream_reader_t *new_reader) {
  debug("\tBSR: sorted insert in priority queue start");
  if(reader_priority_queue == NULL || new_reader == NULL ||
     new_reader->bs_record == NULL ) {
    return 0;
  }
  long nr_time = (long) new_reader->bs_record->bd_entry->time;
  new_reader->next = NULL;
  // priority queue is empty
  // new reader is inserted at the beginning of the queue
  if(reader_priority_queue == NULL) {
    reader_priority_queue = new_reader;
    new_reader->next = NULL;
  }
  // else priority queue contains elements
  else {
    bgpstream_reader_t *iterator_previous = NULL;
    bgpstream_reader_t *iterator_current = reader_priority_queue;
    long qr_time; // queue record time
    while(iterator_current != NULL) {
      qr_time = (long) iterator_current->bs_record->bd_entry->time;
      if(qr_time < nr_time) {
	iterator_previous = iterator_current;
	iterator_current = iterator_previous->next;
      }
      else{
	// insert new record in the middle of the queue
	iterator_previous->next = new_reader;
	new_reader->next = iterator_current;
	break;
      }
    }
    // new record needs to be inserted at the end of the queue
    if(iterator_current == NULL) {
      iterator_previous->next = new_reader;
    }    
  }
  debug("\tBSR: sorted insert in priority queue end");
  return 1;
}


static bgpstream_reader_t *bgpstream_reader_pop_from_priorityqueue(bgpstream_reader_t *reader_priority_queue) {
  debug("\tBSR: pop from priority queue start");
  if(reader_priority_queue == NULL) {
    return NULL;
  }
  bgpstream_reader_t *r = reader_priority_queue;
  reader_priority_queue = r->next;
  r->next = NULL;
  debug("\tBSR: pop from priority queue end");
  return 1;
}



/* it creates a new reader circular queue given a queue of
 * bgpstream objects to process. It returns 1 if the process
 * ends correctly. It returns 0 if the priority queue is not empty
 * when this function is called (or the reader mgr is null).
 */
int bgpstream_reader_mgr_set(bgpstream_reader_mgr_t * const bs_reader_mgr, 
			     const bgpstream_input_t * const toprocess_queue) {
  debug("\tBSR: create mgr set start");
  if(bs_reader_mgr == NULL || bs_reader_mgr->status == NON_EMPTY_READER) {
    return 0;
  }
  const bgpstream_input_t *iterator = toprocess_queue;
  bgpstream_reader_t *prev = NULL;
  while(iterator != NULL) {
    bgpstream_reader_t *new_reader = bgpstream_reader_create(iterator, reader_mgr->filter_mgr);
    

    if(new_reader != NULL) {
      if(prev == NULL) { //first element in queue
	bs_reader_mgr->reader_priority_queue = new_reader;
	bs_reader_mgr->reader_priority_queue->previous = NULL;
      }
      else {
	prev->next = new_reader;
	new_reader->previous = prev;
      }
      new_reader->next = NULL; // next is initalized afterwards
      prev = new_reader;
    }
    iterator = iterator->next;
  }
  // now we have to link the first and the last reader objects
  if(prev != NULL) { // at least 1 object has been inserted in the reader cqueue
    prev->next = bs_reader_mgr->reader_cqueue;
    bs_reader_mgr->reader_cqueue->previous = prev;
    bs_reader_mgr->status = NON_EMPTY_READER;
  }
  debug("\tBSR: create mgr set end");
  return 1;
}


/* destroy the bgpstream reader manager
 */
void bgpstream_reader_mgr_destroy(bgpstream_reader_mgr_t *bs_reader_mgr) {
  debug("\tBSR: reader mgr destroy start");
  if(bs_reader_mgr == NULL) {
    return; // nothing to do
  }
  // deallocating memory for circualar queue
  bgpstream_reader_t *iterator;
  /* the following condition is != from (status == NON_EMPTY_READER)
   * when deallocating the entire manager we do not update the pointers
   * and the status */
  while(bs_reader_mgr->reader_cqueue !=NULL){
    iterator = bs_reader_mgr->reader_cqueue;
    bs_reader_mgr->reader_cqueue =  bs_reader_mgr->reader_cqueue->next;
    bgpstream_reader_destroy(iterator);
  }
  // deallocating bs_reader_mgr
  free(bs_reader_mgr);
  debug("\tBSR: reader mgr destroy end");
}


/* Remove the current reader from the circular 
 * queue (and update it accordingly)
 */
static void bgpstream_reader_mgr_remove_current_reader_from_cqueue(bgpstream_reader_mgr_t * const bs_reader_mgr) {
    bgpstream_reader_t *current_reader = bs_reader_mgr->reader_cqueue;
    if (bs_reader_mgr->reader_cqueue == bs_reader_mgr->reader_cqueue->next) { // last BS reader
      	bgpstream_reader_destroy(current_reader);
	bs_reader_mgr->reader_cqueue = NULL;
	// signal empty reader cqueue
	bs_reader_mgr->status = EMPTY_READER; 
    }
    else { // at least another reader is available (update cqueue then destroy)
      // Nth-next -> new_cqueue     
      bs_reader_mgr->reader_cqueue->previous->next = bs_reader_mgr->reader_cqueue->next;
      // new_cqueue-previous -> Nth
      bs_reader_mgr->reader_cqueue->next->previous = bs_reader_mgr->reader_cqueue->previous;
      // cqueue = new_queue
      bs_reader_mgr->reader_cqueue = bs_reader_mgr->reader_cqueue->next;
      // destroy reader
      bgpstream_reader_destroy(current_reader);
      // (redundant) signal non empty reader cqueue
      bs_reader_mgr->status = NON_EMPTY_READER;
    }     
}



/* Get the next bgpstream record available and update
 * the circular queue
 */
bgpstream_record_t *bgpstream_reader_mgr_get_next_record(bgpstream_reader_mgr_t * const bs_reader_mgr) {
  debug("\tBSR: reader mgr get next record start");
  if(bs_reader_mgr == NULL) {
    return NULL; // invalid mgr provided
  }
  if(bs_reader_mgr->status == EMPTY_READER) {
    return NULL; // no reader available
  }
  // reader_cqueue points to the next available record
  bgpstream_reader_t *current_reader = bs_reader_mgr->reader_cqueue;
  // getting pointer to the bs_record to export
  bgpstream_record_t *exported_bs_record = current_reader->bs_record;
  check_mem(exported_bs_record);

  /* check_mem(&exported_bs_record->bd_entry->type); */
  /* printf("\tBSR: reader mgr get next record type: %u \n",exported_bs_record->bd_entry->type); */
  /* debug("\tBSR: reader mgr get next record type: %u",exported_bs_record->bd_entry->type); */
  /* struct tm * ptm  = localtime(&(exported_bs_record->bd_entry->time)); */
  /* debug("\tBSR: reader mgr get next record time: %ld", (long)mktime(ptm)); */
  
  current_reader->bs_record = NULL; // a new record has to be allocated if necessary
  /* updating record structure: */
  switch(current_reader->status){
  case NEW_BS_RECORDS_AVAILABLE: /* case 1: 
				  * new entries are available 
				  * read next bgp_entry using bgpdump */  
    debug("\tBSR: reader mgr new records available");
    current_reader->bs_record = (bgpstream_record_t*) malloc(sizeof(bgpstream_record_t));
    check(current_reader->bs_record!=NULL,"\tBSR: reader mgr get next record - malloc failed");
    // reading next entry
    current_reader->bs_record->bd_entry = bgpdump_read_next(current_reader->bd_mgr);
    if(current_reader->bs_record->bd_entry == NULL) { /* bgpdump did not read anything
						       * then we remove the current reader */
      debug("\tBSR: end of file not reached but read next returned an empty bdump entry");    
      bgpstream_reader_mgr_remove_current_reader_from_cqueue(bs_reader_mgr);      
    }
    else{ // bgpdump read something: at least another record is available
      // check if bgpdump has reached the end of the file
      if(current_reader->bd_mgr->eof == 0) { // eof not reached
	current_reader->status = NEW_BS_RECORDS_AVAILABLE; // signal 
	debug("\tBSR: reader mgr next is not last");      
      }
      else{ // eof reached
	bgpdump_close_dump(current_reader->bd_mgr); // close bgpdump
	current_reader->bd_mgr = NULL;
	current_reader->status = LAST_BS_RECORD; // signal last bd_record
	debug("\tBSR: reader mgr next is last");      
      }
    }
    break;
  case LAST_BS_RECORD: /* case 2: 
			* this was the last record -> then we have to 
			* update the circular queue */
    debug("\tBSR: reader mgr retrieving last record available ");
    bgpstream_reader_mgr_remove_current_reader_from_cqueue(bs_reader_mgr);       
    break;
  default: /* we should never be in this condition unless we define something different
	    * from LAST_BS_RECORD and  NEW_BS_RECORDS_AVAILABLE */
      sentinel("BGPSTREAM READER MGR in unknown state");
  }
  /* Note:
   * the memory for the bs_record is still maintained. Those who will use the record
   * will have to destroy it using the bgpstream_reader_destroy_record function 
   * exported by the interface */
  debug("\tBSR: reader mgr get next record end");
  return exported_bs_record;
 error: // if the reader mgr is in an anomalous state
  bgpstream_reader_mgr_destroy(bs_reader_mgr);
  return NULL;
}


