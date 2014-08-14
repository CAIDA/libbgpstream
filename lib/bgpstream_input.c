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

#include "bgpstream_input.h"
#include "bgpstream_debug.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>


 //function used for debug
static void print_input_queue(const bgpstream_input_t * const input_queue) {
#ifdef NDEBUG
  const bgpstream_input_t * iterator = input_queue;
  bgpstream_debug("INPUT QUEUE: start");
  int i = 1;
  while(iterator != NULL) {    
    bgpstream_debug("\t%d %s %s %d",i, iterator->filecollector, 
	  iterator->filetype, iterator->epoch_filetime);
    iterator = iterator->next;
    i++;
  }
  iterator = NULL;
  bgpstream_debug("\nINPUT QUEUE: end");  
#endif
}


/* Initialize the bgpstream input manager  
 */
bgpstream_input_mgr_t *bgpstream_input_mgr_create() {
  bgpstream_debug("\tBSI_MGR: create input mgr start");
  bgpstream_input_mgr_t *bs_input_mgr = (bgpstream_input_mgr_t*) malloc(sizeof(bgpstream_input_mgr_t));
  if(bs_input_mgr == NULL) {
    return NULL; // can't allocate memory
  }
  bs_input_mgr->head = NULL;
  bs_input_mgr->tail = NULL;
  bs_input_mgr->last_to_process = NULL;
  bs_input_mgr->status = EMPTY_INPUT_QUEUE;
  bs_input_mgr->epoch_minimum_date = 0;
  bs_input_mgr->epoch_last_ts_input = 0;
  bgpstream_debug("\tBSI_MGR: create input mgr end ");
  return bs_input_mgr;
}


/* Check if the current status is EMPTY 
 */
bool bgpstream_input_mgr_is_empty(const bgpstream_input_mgr_t * const bs_input_mgr) {
  bgpstream_debug("\tBSI_MGR: is empty start");
  if(bs_input_mgr != NULL && bs_input_mgr->status != EMPTY_INPUT_QUEUE) {
    bgpstream_debug("\tBSI_MGR: is empty end: not empty!");
    return false;
  }
  bgpstream_debug("\tBSI_MGR: is empty end: empty!");
  return true;
}


/* Add a new input object to the FIFO queue managed
 * by the bgpstream input manager 
 */
int bgpstream_input_mgr_push_input(bgpstream_input_mgr_t * const bs_input_mgr, 
			       const char * const filename, const char * fileproject,
			       const char * filecollector, const char * const filetype,
			       const int epoch_filetime) {
  bgpstream_debug("\tBSI_MGR: push input start");
  if(bs_input_mgr == NULL) {
    return 0; // if the bs_input_mgr is not initialized, then we cannot insert any new input
  }
  // create a new bgpstream_input object
  bgpstream_input_t *bs_input = (bgpstream_input_t*) malloc(sizeof(bgpstream_input_t));
  if(bs_input == NULL) {
    return 0; // can't allocate memory
  }
  // initialize bgpstream_input fields
  bs_input->next = NULL;
  // initialization done

  if((bs_input->filename = strdup(filename)) == NULL ||
     (bs_input->fileproject = strdup(fileproject)) == NULL ||
     (bs_input->filecollector = strdup(filecollector)) == NULL ||
     (bs_input->filetype = strdup(filetype)) == NULL) {
    return 0;
  }
  bs_input->epoch_filetime = epoch_filetime;
  // update the bs_input_mgr
  if(bs_input_mgr->status == EMPTY_INPUT_QUEUE) {
    bs_input_mgr->head = bs_input;
    bs_input_mgr->tail = bs_input;
    bs_input_mgr->status = NON_EMPTY_INPUT_QUEUE;
  }
  else {
    bs_input_mgr->tail->next = bs_input;
    bs_input_mgr->tail = bs_input;
  }
  bgpstream_debug("\tBSI_MGR: push input mgr end");
  return 1;
}


/* Add a new input object to the sorted queue
 * managed by the bgpstream input manager 
 * (bgpstream objects are sorted by filetime)
 */
int bgpstream_input_mgr_push_sorted_input(bgpstream_input_mgr_t * const bs_input_mgr, 
			       const char * const filename, const char * fileproject,
			       const char * filecollector, const char * const filetype,
			       const int epoch_filetime) {
  bgpstream_debug("\t\tBSI: push input start");
  if(bs_input_mgr == NULL) {
    return 0; // if the bs_input_mgr is not initialized, then we cannot insert any new input
  }
  // create a new bgpstream_input object
  bgpstream_input_t *bs_input = (bgpstream_input_t*) malloc(sizeof(bgpstream_input_t));
  if(bs_input == NULL) {
    return 0; // can't allocate memory    
  }
  // initialize bgpstream_input fields
  bs_input->next = NULL;
  // initialization done

  bs_input->filename = filename; /* we already own the buffer */
  if((bs_input->fileproject = strdup(fileproject)) == NULL ||
     (bs_input->filecollector = strdup(filecollector)) == NULL ||
     (bs_input->filetype = strdup(filetype)) == NULL) {
    return 0;
  }

  bs_input->epoch_filetime = epoch_filetime;
  // update the bs_input_mgr
  if(bs_input_mgr->status == EMPTY_INPUT_QUEUE) {
    // if the queue is empty a new input is added
    // as first element
    bs_input_mgr->head = bs_input;
    bs_input_mgr->tail = bs_input;
    bs_input_mgr->status = NON_EMPTY_INPUT_QUEUE;
  }
  else {
    // otherwise we scan the queue until we
    // reach the end or find an element whose
    // filetime is newer
    bgpstream_input_t *current = bs_input_mgr->head;
    bgpstream_input_t *previous = bs_input_mgr->head;
    while(current != NULL && current->epoch_filetime <= epoch_filetime) {
      // if the file is already in queue we do not add a 
      // duplicate
      if(current->epoch_filetime == epoch_filetime &&
	 strcmp(current->filecollector, filecollector) == 0 &&
	 strcmp(current->fileproject, fileproject) == 0 &&
	 strcmp(current->filetype, filetype) == 0) {
	return 0;
      }
      // ribs have higher priority (ribs and updates are grouped
      // together, ribs come before updates - if they have the same
      // filetime)
      if(current->epoch_filetime == epoch_filetime &&
	 filetype[0] == 'r' && /* "ribs" */
	 current->filetype[0] == 'u') { /* "updates" */
	break;
      }
      else{
	previous = current;
	current = current->next;
      }
    }
    // new input object is inserted at
    // previous -> new object -> current
    if(current == NULL) {
      // case 1: end of queue reached
      bs_input_mgr->tail->next = bs_input;
      bs_input_mgr->tail = bs_input;
    }
    else {
      if(current == previous) {
	// case 2: head of queue
	bs_input->next = bs_input_mgr->head;
	bs_input_mgr->head = bs_input;
      }
      else {
	// case 3: internal position
	bs_input->next = current;
	previous->next = bs_input;
      }
    }
  }
  bgpstream_debug("\tBSI_MGR: sorted push: %s",filename);
  bgpstream_debug("\tBSI_MGR: sorted push input mgr end");
  return 1;
}



/* Set the last input to process  
 * the function is static: it is not visible outside this file
 */
static void bgpstream_input_mgr_set_last_to_process(bgpstream_input_mgr_t * const bs_input_mgr){
  bgpstream_debug("\tBSI_MGR: last to process set start");
  /* !!! multiple policies can be implemented !!!
   * Here we consider a unique policy: we process 
   * files with the same filetype, then if type:
   * - ribs -> then we consider all the files with the same time
   *   bgp_file_time (epoch_filetime)
   * - updates -> then we consider all the files with the same time
   *   filetype + routeviews 1 timestamp + ris 3 timestamps
   */
  bs_input_mgr->last_to_process = NULL;
  bgpstream_input_t *iterator = bs_input_mgr->head;
  int rv_update = 0;
  int rv_update_time = 0;
  if(strcmp(bs_input_mgr->head->filetype,"ribs") == 0) {
    while( iterator != NULL &&						\
	   strcmp(iterator->filetype,bs_input_mgr->head->filetype) == 0 && \
	   iterator->epoch_filetime == bs_input_mgr->head->epoch_filetime ){
      bs_input_mgr->last_to_process = iterator;
      iterator = iterator->next;
    }
  }
  if(strcmp(bs_input_mgr->head->filetype,"updates") == 0) {
    while( iterator != NULL &&						\
	   strcmp(iterator->filetype,bs_input_mgr->head->filetype) == 0 && \
	   iterator->epoch_filetime == bs_input_mgr->head->epoch_filetime ){   
      if(strcmp(iterator->fileproject,"routeviews") == 0) {
	rv_update = 1; // check if a routeviews update has been read and save its time
	rv_update_time = iterator->epoch_filetime;
      }
      bs_input_mgr->last_to_process = iterator;
      iterator = iterator->next;
    }
    if(rv_update == 1) {
      // when a routeviews update is present in the list, we also collect all
      // the ris updates within the same interval (i.e. rv_time already included,
      // plus rv_time + 5 mins plus rv_time + 10 mins). In fact, while routeviews
      // updates provides information related to a 15 mins interval, ris does
      // that for a 5 mins interval
      while( iterator != NULL &&					\
	     strcmp(iterator->filetype,bs_input_mgr->head->filetype) == 0 && \
	     iterator->epoch_filetime <= (rv_update_time + 10 * 60)  ){
	bs_input_mgr->last_to_process = iterator;
	iterator = iterator->next;
      }      
    }    
  }
  bgpstream_debug("\tBSI_MGR: last to process set end");
}


/* Remove a sublist-queue of input objects to process 
 * from the FIFO queue managed by the bgpstream input manager 
 */
bgpstream_input_t *bgpstream_input_mgr_get_queue_to_process(bgpstream_input_mgr_t * const bs_input_mgr) {
  bgpstream_debug("\tBSI_MGR: get subqueue to process start");
  if(bs_input_mgr == NULL) {
    return NULL; // if the bs_input_mgr is not initialized, then we cannot remove any input
  }
  if(bs_input_mgr->status == EMPTY_INPUT_QUEUE){
    return NULL; // can't get a sublist from an empty queue
  }
  /* this is the only function that call the function 
   * bgpstream_input_set_last_to_process */
  bgpstream_input_mgr_set_last_to_process(bs_input_mgr);
  if(bs_input_mgr->last_to_process == NULL ){
    return NULL;
  }
  bgpstream_input_t* to_process = bs_input_mgr->head;
  // change FIFO queue internal connections
  bs_input_mgr->head = bs_input_mgr->last_to_process->next;
  /* we don't want external functions to access the queue
   * also we need to establish the end of the sub-queue exported */
  bs_input_mgr->last_to_process->next = NULL; 
  bs_input_mgr->last_to_process = NULL; // reset last_to_process ptr
  if(bs_input_mgr->head == NULL) { // check if we removed the entire queue
    bs_input_mgr->tail = NULL;
    bs_input_mgr->status = EMPTY_INPUT_QUEUE;
  }
  print_input_queue(to_process);
  bgpstream_debug("\tBSI_MGR: get subqueue to process end");
  return to_process;
}


/* Recursively destroy the bgpstream input objects in the
 * queue
 */
void bgpstream_input_mgr_destroy_queue(bgpstream_input_t *queue) {
  bgpstream_debug("\tBSI_MGR: subqueue destroy start");
  bgpstream_input_t *iterator = queue;
  bgpstream_input_t *current = NULL;
  while(iterator!=NULL) {
    current = iterator;
    iterator = iterator->next;
    // deallocating memory for current bgpstream_input object
    free(current->filename);
    free(current->fileproject);
    free(current->filecollector);
    free(current->filetype);
    free(current);
  }
  bgpstream_debug("\tBSI_MGR: subqueue destroy end");
}


/* Destroy the bgpstream_input manager 
*/
void bgpstream_input_mgr_destroy(bgpstream_input_mgr_t *bs_input_mgr){
  bgpstream_debug("\tBSI_MGR: input mgr destroy start");
  if(bs_input_mgr == NULL){
    return; // already empty
  }  
  bgpstream_input_mgr_destroy_queue(bs_input_mgr->head); // destroy queue
  bs_input_mgr->head = NULL;
  bs_input_mgr->tail = NULL;
  bs_input_mgr->last_to_process = NULL;
  // free bgpstream_input_mgr
  free(bs_input_mgr);
  bgpstream_debug("\tBSI_MGR: input mgr destroy end");
}

