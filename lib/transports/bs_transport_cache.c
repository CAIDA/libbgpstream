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

#include "bgpstream_transport_interface.h"
#include "bgpstream_log.h"
#include "bs_transport_cache.h"
#include "wandio.h"
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

int bs_transport_cache_create(bgpstream_transport_t *transport)
{


  BS_TRANSPORT_SET_METHODS(cache, transport);
  io_t *fh = NULL;

  // create cache folder
  char *storage_folder_path = "/tmp/bgpstream_tmp";
  mkdir(storage_folder_path, 0755);
  // get file name from url
  char *pLastSlash = strrchr(transport->res->uri, '/');
  char *output_name = (char *) malloc( sizeof( char ) * (strlen(storage_folder_path) + strlen(pLastSlash) + 1) );
  strcpy(output_name, storage_folder_path);
  strcat(output_name, pLastSlash);
  strcat(output_name, ".data");

  // If the file exists, don't create writer, and set readFromCache = 1
  if( access( output_name, F_OK ) != -1 ) {
    // file exists
    transport->read_from_cache = 1;
    printf("READ FROM CACHE!\n");
    printf("%s\n",output_name);

    if ((fh = wandio_create(output_name)) == NULL) {
      bgpstream_log(BGPSTREAM_LOG_ERR, "Could not open %s for reading",
                    output_name);
      return -1;
    }

    transport->state = fh;
  } else {
    // file doesn't exist

    printf("READ FROM REMOTE, WRITE TO CACHE!\n");
    printf("%s\n",transport->res->uri);

    if ((fh = wandio_create(transport->res->uri)) == NULL) {
      bgpstream_log(BGPSTREAM_LOG_ERR, "Could not open %s for reading",
                    transport->res->uri);
      return -1;
    }

    transport->state = fh;

    iow_t *fh2 = NULL;
    if ((fh2 = wandio_wcreate(output_name, 0, 0, 0)) == NULL) {
      bgpstream_log(BGPSTREAM_LOG_ERR, "Could not open %s for local caching", output_name);
      return -1;
    }

    transport->read_from_cache = 0;
    transport->cache_writer = fh2;
  }

  return 0;
}

int64_t bs_transport_cache_read(bgpstream_transport_t *transport,
                                uint8_t *buffer, int64_t len)
{

  int64_t ret = wandio_read((io_t*)transport->state, buffer, len);

  if(transport->read_from_cache == 0){
    // not reading from cache, then write it

    printf("write cache\n");
    if(transport->cache_writer != NULL){
      wandio_wwrite((iow_t*)transport->cache_writer, buffer, len);
    } else{
      printf("error writing cache!");
    }
  }

  return ret;
}

void bs_transport_cache_destroy(bgpstream_transport_t *transport)
{
  if (transport->state != NULL) {
    wandio_destroy((io_t*)transport->state);
    transport->state = NULL;
  }

  if (transport->cache_writer != NULL) {
    wandio_wdestroy((iow_t*)transport->cache_writer);
    transport->cache_writer = NULL;
  }
}
