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

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <bgpdump_lib.h>

#include "utils.h"

#include "bgpstream_int.h"
#include "bgpstream_debug.h"

/* TEMPORARY STRUCTURES TO FAKE DATA INTERFACE PLUGIN API */

/* this should be the complete list of interface types */
static bgpstream_data_interface_id_t bgpstream_data_interfaces[] = {
  BGPSTREAM_DATA_INTERFACE_MYSQL,
  BGPSTREAM_DATA_INTERFACE_CUSTOMLIST,
  BGPSTREAM_DATA_INTERFACE_CSVFILE,
};

static bgpstream_data_interface_info_t bgpstream_data_interface_infos[] = {
  { /* NO VALID IF WITH ID 0 */ },

  {
    BGPSTREAM_DATA_INTERFACE_MYSQL,
    "mysql",
    "Retrieve metadata information from the bgparchive mysql database",
  },
  {
    BGPSTREAM_DATA_INTERFACE_CUSTOMLIST,
    "custom-list",
    "TODO: Mock datasource used to test the library",
  },
  {
    BGPSTREAM_DATA_INTERFACE_CSVFILE,
    "csvfile",
    "Retrieve metadata information from a csv file",
  },
};

/* this should be a complete list of per-interface options */
static bgpstream_data_interface_option_t bgpstream_mysql_options[] = {
  /* Database Name */
  {
    BGPSTREAM_DATA_INTERFACE_MYSQL,
    0,
    "db-name",
    "Name of the mysql database to use (default: bgparchive)",
  },

  /* Database username */
  {
    BGPSTREAM_DATA_INTERFACE_MYSQL,
    1,
    "db-user",
    "Mysql username to use (default: bgpstream)",
  },
  /* Database password */
  {
    BGPSTREAM_DATA_INTERFACE_MYSQL,
    2,
    "db-password",
    "mysql password to use (default: none)",
  },

  /* Database host */
  {
    BGPSTREAM_DATA_INTERFACE_MYSQL,
    3,
    "db-host",
    "Hostname/IP of the mysql server (default: mysql default host)",
  },
  /* Database connection port */
  {
    BGPSTREAM_DATA_INTERFACE_MYSQL,
    4,
    "db-port",
    "Port of the mysql server (default: mysql default port)",
  },
  /* Database Unix socket */
  {
    BGPSTREAM_DATA_INTERFACE_MYSQL,
    5,
    "db-socket",
    "Unix socket of the mysql server (default: mysql default socket)",
  },
  /* RIS data path */
  {
    BGPSTREAM_DATA_INTERFACE_MYSQL,
    6,
    "ris-path",
    "Prefix path of RIS data (default: RIS path contained in mysql db projects table)",
  },
  /* Routeviews data path */
  {
    BGPSTREAM_DATA_INTERFACE_MYSQL,
    7,
    "rv-path",
    "Prefix path of RouteViews data (default: Routeviews path contained in mysql db projects table)",
  },

};

static bgpstream_data_interface_option_t *bgpstream_customlist_options = NULL;

static bgpstream_data_interface_option_t bgpstream_csvfile_options[] = {
  /* File name */
  {
    BGPSTREAM_DATA_INTERFACE_CSVFILE,
    0,
    "filename",
    "Hostname/IP of the mysql server (default: TODO)",
  },
};


/* allocate memory for a new bgpstream interface
 */
bgpstream_t *bgpstream_create() {
  bgpstream_debug("BS: create start");
  bgpstream_t *  bs = (bgpstream_t*) malloc(sizeof(bgpstream_t));
  if(bs == NULL) {
    return NULL; // can't allocate memory
  }
  bs->filter_mgr = bgpstream_filter_mgr_create();
  if(bs->filter_mgr == NULL) {
    bgpstream_destroy(bs);
    bs = NULL;
    return NULL;
  }
  bs->datasource_mgr = bgpstream_datasource_mgr_create();
  if(bs->datasource_mgr == NULL) {
    bgpstream_destroy(bs);
    return NULL;
  }
  /* create an empty input mgr
   * the input queue will be populated when a
   * bgpstream record is requested */
  bs->input_mgr = bgpstream_input_mgr_create();
  if(bs->input_mgr == NULL) {
    bgpstream_destroy(bs);
    bs = NULL;
    return NULL;
  }
  bs->reader_mgr = bgpstream_reader_mgr_create(bs->filter_mgr);
  if(bs->reader_mgr == NULL) {
    bgpstream_destroy(bs);
    bs = NULL;
    return NULL;
  }
  /* memory for the bgpstream interface has been
   * allocated correctly */
  bs->status = BGPSTREAM_STATUS_ALLOCATED;
  bgpstream_debug("BS: create end");
  return bs;
}
/* side note: filters are part of the bgpstream so they
 * can be accessed both from the input_mgr and the
 * reader_mgr (input_mgr use them to apply a coarse-grained
 * filtering, the reader_mgr applies a fine-grained filtering
 * of the data provided by the input_mgr)
 */



/* configure filters in order to select a subset of the bgp data available */
void bgpstream_add_filter(bgpstream_t *bs,
                          bgpstream_filter_type_t filter_type,
			  const char* filter_value) {
  bgpstream_debug("BS: set_filter start");
  if(bs == NULL || (bs != NULL && bs->status != BGPSTREAM_STATUS_ALLOCATED)) {
    return; // nothing to customize
  }
  bgpstream_filter_mgr_filter_add(bs->filter_mgr, filter_type, filter_value);
  bgpstream_debug("BS: set_filter end");
}


void bgpstream_add_interval_filter(bgpstream_t *bs,
				   uint32_t begin_time,
                                   uint32_t end_time) {
  bgpstream_debug("BS: set_filter start");
  if(bs == NULL || (bs != NULL && bs->status != BGPSTREAM_STATUS_ALLOCATED)) {
    return; // nothing to customize
  }
  bgpstream_filter_mgr_interval_filter_add(bs->filter_mgr, begin_time, end_time);
  bgpstream_debug("BS: set_filter end");
}

int bgpstream_get_data_interfaces(bgpstream_t *bs,
                                  bgpstream_data_interface_id_t **if_ids)
{
  assert(if_ids != NULL);
  *if_ids = bgpstream_data_interfaces;
  return ARR_CNT(bgpstream_data_interfaces);
}

bgpstream_data_interface_id_t
bgpstream_get_data_interface_id_by_name(bgpstream_t *bs, const char *name)
{
  int i;

  for(i=1; i<ARR_CNT(bgpstream_data_interface_infos); i++)
    {
      if(strcmp(bgpstream_data_interface_infos[i].name, name) == 0)
        {
          return bgpstream_data_interface_infos[i].id;
        }
    }

  return 0;
}

bgpstream_data_interface_info_t *
bgpstream_get_data_interface_info(bgpstream_t *bs,
                                  bgpstream_data_interface_id_t if_id)
{
  return &bgpstream_data_interface_infos[if_id];
}

int bgpstream_get_data_interface_options(bgpstream_t *bs,
                                         bgpstream_data_interface_id_t if_id,
                                         bgpstream_data_interface_option_t **opts)
{
  assert(opts != NULL);

  switch(if_id)
    {
    case BGPSTREAM_DATA_INTERFACE_MYSQL:
      *opts = bgpstream_mysql_options;
      return ARR_CNT(bgpstream_mysql_options);
      break;

    case BGPSTREAM_DATA_INTERFACE_CUSTOMLIST:
      *opts = bgpstream_customlist_options;
      return ARR_CNT(bgpstream_customlist_options);
      break;

    case BGPSTREAM_DATA_INTERFACE_CSVFILE:
      *opts = bgpstream_csvfile_options;
      return ARR_CNT(bgpstream_csvfile_options);
      break;

    default:
      *opts = NULL;
      return 0;
      break;
    }
}

bgpstream_data_interface_option_t *
bgpstream_get_data_interface_option_by_name(bgpstream_t *bs,
                                            bgpstream_data_interface_id_t if_id,
                                            const char *name)
{
  bgpstream_data_interface_option_t *options;
  int opt_cnt = 0;
  int i;

  opt_cnt = bgpstream_get_data_interface_options(bs, if_id, &options);

  if(options == NULL || opt_cnt == 0)
    {
      return NULL;
    }

  for(i=0; i<opt_cnt; i++)
    {
      if(strcmp(options[i].name, name) == 0)
        {
          return &options[i];
        }
    }

  return NULL;
}

/* configure the datasource interface options */

void bgpstream_set_data_interface_option(bgpstream_t *bs,
			        bgpstream_data_interface_option_t *option_type,
                                const char *option_value) {

  bgpstream_debug("BS: set_data_interface_options start");
  if(bs == NULL || (bs != NULL && bs->status != BGPSTREAM_STATUS_ALLOCATED)) {
    return; // nothing to customize
  }

  bgpstream_datasource_mgr_set_data_interface_option(bs->datasource_mgr,
                                                     option_type, option_value);

  bgpstream_debug("BS: set_data_interface_options stop");
}

/* configure the interface so that it connects
 * to a specific datasource interface
 */
void bgpstream_set_data_interface(bgpstream_t *bs,
                                  bgpstream_data_interface_id_t datasource) {
  bgpstream_debug("BS: set_data_interface start");
  if(bs == NULL || (bs != NULL && bs->status != BGPSTREAM_STATUS_ALLOCATED)) {
    return; // nothing to customize
  }
  bgpstream_datasource_mgr_set_data_interface(bs->datasource_mgr, datasource);
  bgpstream_debug("BS: set_data_interface stop");
}


/* configure the interface so that it blocks
 * waiting for new data
 */
void bgpstream_set_blocking(bgpstream_t *bs) {
  bgpstream_debug("BS: set_blocking start");
  if(bs == NULL || (bs != NULL && bs->status != BGPSTREAM_STATUS_ALLOCATED)) {
    return; // nothing to customize
  }
  bgpstream_datasource_mgr_set_blocking(bs->datasource_mgr);
  bgpstream_debug("BS: set_blocking stop");
}


/* turn on the bgpstream interface, i.e.:
 * it makes the interface ready
 * for a new get next call
*/
int bgpstream_start(bgpstream_t *bs) {
  bgpstream_debug("BS: init start");
  if(bs == NULL || (bs != NULL && bs->status != BGPSTREAM_STATUS_ALLOCATED)) {
    return 0; // nothing to init
  }
  // turn on datasource interface
  bgpstream_datasource_mgr_init(bs->datasource_mgr, bs->filter_mgr);
  if(bs->datasource_mgr->status == BGPSTREAM_DATASOURCE_STATUS_ON) {
    bs->status = BGPSTREAM_STATUS_ON; // interface is on
    bgpstream_debug("BS: init end: ok");
    return 1;
  }
  else{
    // interface is not on (something wrong with datasource)
    bs->status = BGPSTREAM_STATUS_ALLOCATED;
    bgpstream_debug("BS: init warning: check if the datasource provided is ok");
    bgpstream_debug("BS: init end: not ok");
    return -1;
  }
}

/* this function returns the next available record read
 * if the input_queue (i.e. list of files connected from
 * an external source) or the reader_cqueue (i.e. list
 * of bgpdump currently open) are empty then it
 * triggers a mechanism to populate the queues or
 * return 0 if nothing is available
 */
int bgpstream_get_next_record(bgpstream_t *bs,
                              bgpstream_record_t *record) {
  bgpstream_debug("BS: get next");
  if(bs == NULL || (bs != NULL && bs->status != BGPSTREAM_STATUS_ON)) {
    return -1; // wrong status
  }
  // bgpstream_record_t *record = NULL;
  int num_query_results = 0;
  bgpstream_input_t *bs_in = NULL;

  // if bs_record contains an initialized bgpdump entry we destroy it
  bgpstream_record_clear(record);

  while(bgpstream_reader_mgr_is_empty(bs->reader_mgr)) {
    bgpstream_debug("BS: reader mgr is empty");
    // get new data to process and set the reader_mgr
    while(bgpstream_input_mgr_is_empty(bs->input_mgr)) {
      bgpstream_debug("BS: input mgr is empty");
      /* query the external source and append new
       * input objects to the input_mgr queue */
      num_query_results =
        bgpstream_datasource_mgr_update_input_queue(bs->datasource_mgr,
                                                    bs->input_mgr);
      if(num_query_results == 0){
	bgpstream_debug("BS: no (more) data are available");
	return 0; // no (more) data are available
      }
      if(num_query_results < 0){
	bgpstream_debug("BS: error during datasource_mgr_update_input_queue");
	return -1; // error during execution
      }
      bgpstream_debug("BS: got results from datasource");
      //DEBUG fprintf(stderr, "Finished with loading mysql results in memory!\n");
    }
    bgpstream_debug("BS: input mgr not empty");
    bs_in = bgpstream_input_mgr_get_queue_to_process(bs->input_mgr);
    bgpstream_reader_mgr_add(bs->reader_mgr, bs_in, bs->filter_mgr);
    bgpstream_input_mgr_destroy_queue(bs_in);
    bs_in = NULL;
  }
  bgpstream_debug("BS: reader mgr not empty");
  return bgpstream_reader_mgr_get_next_record(bs->reader_mgr, record,
                                              bs->filter_mgr);
}


/* turn off the bgpstream interface */
void bgpstream_stop(bgpstream_t *bs) {
  bgpstream_debug("BS: close start");
  if(bs == NULL || (bs != NULL && bs->status != BGPSTREAM_STATUS_ON)) {
    return; // nothing to close
  }
  bgpstream_datasource_mgr_close(bs->datasource_mgr);
  bs->status = BGPSTREAM_STATUS_OFF; // interface is off
  bgpstream_debug("BS: close end");
}


/* destroy a bgpstream interface istance
 */
void bgpstream_destroy(bgpstream_t *bs){
  bgpstream_debug("BS: destroy start");
  if(bs == NULL) {
    return; // nothing to destroy
  }
  bgpstream_input_mgr_destroy(bs->input_mgr);
  bs->input_mgr = NULL;
  bgpstream_reader_mgr_destroy(bs->reader_mgr);
  bs->reader_mgr = NULL;
  bgpstream_filter_mgr_destroy(bs->filter_mgr);
  bs->filter_mgr = NULL;
  bgpstream_datasource_mgr_destroy(bs->datasource_mgr);
  bs->datasource_mgr = NULL;
  free(bs);
  bgpstream_debug("BS: destroy end");
}
