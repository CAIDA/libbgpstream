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

#include "bgpstream_lib.h"
#include "bgpdump_lib.h"
#include <stdio.h>


// modified bgpdump process
void bgpdump_process(BGPDUMP_ENTRY *my_entry);


int main(){
    
  const char *filename = "./rib.example.bz2";
  //const char *filename = "./updates.example.bz2";

  printf("%s\n", filename);
  BGPDUMP *my_dump = bgpdump_open_dump(filename);
  if(!my_dump){
    return 1;
  }
  printf("%s - dump opened\n", filename);

  do {
    BGPDUMP_ENTRY *my_entry = bgpdump_read_next(my_dump);
    if(my_entry) {
      //process(my_entry);
      printf("read 1 entry\n");


      struct tm * ptm  = localtime(&(my_entry->time));
      printf("Get next record time: %ld\n", (long)mktime(ptm));
      //sleep(10);
      bgpdump_process(my_entry);

      bgpdump_free_mem(my_entry);
    }
  } while(my_dump->eof==0);

  bgpdump_close_dump(my_dump);
  printf("%s - dump closed\n", filename);
  
  return 0;
}


