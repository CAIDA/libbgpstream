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

#include "bgpstream.h"

int
main()
{
  /* Allocate memory for a bgpstream instance */
  bgpstream_t *bs = bs = bgpstream_create();
  if(!bs) {
    fprintf(stderr, "ERROR: Could not create BGPStream instance\n");
    return -1;
  }

  /* Allocate memory for a re-usable bgprecord instance */
  bgpstream_record_t *bs_record = bgpstream_record_create();
  if(bs_record == NULL)
    {
      fprintf(stderr, "ERROR: Could not create BGPStream record\n");
      return -1;
    }

  
  /* Configure the sqlite interface */
  bgpstream_data_interface_id_t datasource_id = bgpstream_get_data_interface_id_by_name(bs, "sqlite");
  bgpstream_set_data_interface(bs, datasource_id);

  /* Configure the sqlite interface options */
  bgpstream_data_interface_option_t *option = 
    bgpstream_get_data_interface_option_by_name(bs, datasource_id,
                                                "db-file");
  bgpstream_set_data_interface_option(bs, option, "./sqlite_test.db");

  /* Select bgp data from RRC06 and route-views.jinx collectors only */
  bgpstream_add_filter(bs, BGPSTREAM_FILTER_TYPE_COLLECTOR, "rrc06");
  bgpstream_add_filter(bs, BGPSTREAM_FILTER_TYPE_COLLECTOR, "route-views.jinx");

  /* Process updates only */
  bgpstream_add_filter(bs, BGPSTREAM_FILTER_TYPE_RECORD_TYPE, "updates");

  /* Select a time interval to process:
   * Wed, 01 Apr 2015 00:02:30 GMT -> Wed, 01 Apr 2015 00:05:00 UTC */
  bgpstream_add_interval_filter(bs,1427846550,1427846700);

  /* Start bgpstream */
  if(bgpstream_start(bs) < 0) {
    fprintf(stderr, "ERROR: Could not init BGPStream\n");
    return -1;
  }

  int get_next_ret = 0;
  int elem_counter = 0;

  /* pointer to a bgpstream elem, memory is borrowed from bgpstream,
   * use the elem_copy function to own the memory */
  bgpstream_elem_t *bs_elem = NULL;

  /* Read the stream of records */
  do
    {
      /* get next record */
      get_next_ret = bgpstream_get_next_record(bs, bs_record);
      if(get_next_ret && bs_record->status == BGPSTREAM_RECORD_STATUS_VALID_RECORD)
        {
          while((bs_elem = bgpstream_record_get_next_elem (bs_record)) != NULL)
            {
              elem_counter++;
            }
        }
    }
  while(get_next_ret > 0);

  printf("\tRead %d elems\n", elem_counter);

  /* de-allocate memory for the bgpstream */
  bgpstream_destroy(bs);

  /* de-allocate memory for the bgpstream record */
  bgpstream_record_destroy(bs_record);

  
  return 0;
}

