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
 */

#include "bgpstream.h"

#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <string.h>

#define SINGLEFILE_RECORDS 537347
#define CSVFILE_RECORDS    559424
#define SQLITE_RECORDS     538308

bgpstream_t *bs;
bgpstream_record_t *bs_record;
bgpstream_data_interface_id_t datasource_id = 0;
bgpstream_data_interface_option_t *option;



int run_bgpstream(char *interface)
{
  if(bgpstream_start(bs) < 0) {
    fprintf(stderr, "ERROR: Could not init BGPStream\n");
    return -1;
  }
  int get_next_ret = 0;
  int counter = 0;
  do
    {
      get_next_ret = bgpstream_get_next_record(bs, bs_record);
      if(get_next_ret && bs_record->status == BGPSTREAM_RECORD_STATUS_VALID_RECORD)
        {
          counter++;
        }
    }
  while(get_next_ret > 0);

  printf("\tread %d valid records\n", counter);
  
  bgpstream_stop(bs);

  if((strcmp(interface, "singlefile")== 0 && counter == SINGLEFILE_RECORDS) ||
    (strcmp(interface, "csvfile")== 0 && counter == CSVFILE_RECORDS) ||
     (strcmp(interface, "sqlite")== 0 && counter == SQLITE_RECORDS))
    {
      printf("\tinterface is working correctly\n\n");
      return 0;
    }
  else
    {
      printf("\tinterface is NOT working correctly\n\n");
      return -1; 
    }

  return 0;
}


int main()
{

  /* Testing bgpstream */

  bs = bgpstream_create();
  if(!bs) {
    fprintf(stderr, "ERROR: Could not create BGPStream instance\n");
    return -1;
  }

  bs_record = bgpstream_record_create();
  if(bs_record == NULL)
    {
      fprintf(stderr, "ERROR: Could not create BGPStream record\n");
      bgpstream_destroy(bs);
      return -1;
    }
  bgpstream_destroy(bs);

  int res = 0;
  
  /* Testing singlefile interface */
  printf("Testing singlefile interface...\n");
  bs = bgpstream_create();
  datasource_id =
    bgpstream_get_data_interface_id_by_name(bs, "singlefile");
  bgpstream_set_data_interface(bs, datasource_id);

  option =
    bgpstream_get_data_interface_option_by_name(bs, datasource_id,
                                                "rib-file");
  bgpstream_set_data_interface_option(bs, option, "./routeviews.route-views.jinx.ribs.1427846400.bz2");
  option =
    bgpstream_get_data_interface_option_by_name(bs, datasource_id,
                                                "upd-file");
  bgpstream_set_data_interface_option(bs, option, "./ris.rrc06.updates.1427846400.gz");
  res += run_bgpstream("singlefile");
  bgpstream_destroy(bs);

  /* Testing csvfile interface */
  printf("Testing csvfile interface...\n");
  bs = bgpstream_create();
  datasource_id =
    bgpstream_get_data_interface_id_by_name(bs, "csvfile");
  bgpstream_set_data_interface(bs, datasource_id);

  option =
    bgpstream_get_data_interface_option_by_name(bs, datasource_id,
                                                "csv-file");
  bgpstream_set_data_interface_option(bs, option, "csv_test.csv");
  bgpstream_add_filter(bs, BGPSTREAM_FILTER_TYPE_COLLECTOR, "rrc06");
  res += run_bgpstream("csvfile");
  bgpstream_destroy(bs);

  /* Testing sqlite interface */
  printf("Testing sqlite interface...\n");
  bs = bgpstream_create();
  datasource_id =
    bgpstream_get_data_interface_id_by_name(bs, "sqlite");
  bgpstream_set_data_interface(bs, datasource_id);

  option =
    bgpstream_get_data_interface_option_by_name(bs, datasource_id,
                                                "db-file");
  bgpstream_set_data_interface_option(bs, option, "sqlite_test.db");
  bgpstream_add_filter(bs, BGPSTREAM_FILTER_TYPE_PROJECT, "routeviews");
  res += run_bgpstream("sqlite");
  bgpstream_destroy(bs);

  bgpstream_record_destroy(bs_record);
  /* res is going to be zero if everything worked fine, */
  /* negative if something failed */
  return res;
}
