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
 * libbgpstream is distributed in the hope that it will be usefuql,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with libbgpstream.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "bgpstream_lib.h"
#include <stdio.h>
#include <time.h>

int main(){
  bgpstream_t *bs = bgpstream_create();
  // bgpstream_set_feeder_plugin(bs, sqlite_feeder, "/Users/chiara/Desktop/test_downloader/downloaded.db",0,0);
  bgpstream_set_feeder_plugin(bs, feeder_default, "default",0,0);
  int read = 0;
  bgpstream_record_t *bs_rec = NULL;
  while((bs_rec = bgpstream_get_next(bs))) {
      read++;
      size_t record_size = sizeof(bs_rec->bd_entry->body);
      //printf("\t\t\t--------------> 1 record of size: %zu READ :)\n", record_size);   
      time_t record_time = bs_rec->bd_entry->time;
      //      printf("time: %s\n",ctime(&record_time));
      bgpstream_free_mem(bs_rec); 
  }
  bgpstream_destroy(bs);
  printf("Read %d values\n", read);
  return 0;
}
