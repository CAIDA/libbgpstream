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
#include "utils.h"
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <zlib.h>

#define STATE ((cache_state_t*)(transport->state))

typedef struct cache_state {
  /* user-provided options: */

  /** A 0/1 value indicates whether current read is from a local cache
      file or a remote transport file:
      0 - read from remote;
      1 - read from local cache.
  */
  int read_from_cache;

  /** absolute path for the local cache file */
  char* cache_file_path;

  /** content reader, either from local cache or from remote URI */
  io_t* reader;

  /** cache content writer */
  iow_t* writer;

} cache_state_t;


int bs_transport_cache_create(bgpstream_transport_t *transport)
{
  // reset transport method
  BS_TRANSPORT_SET_METHODS(cache, transport);

  if ((transport->state = malloc_zero(sizeof(cache_state_t))) == NULL) {
    return -1;
  }

  // create cache folder
  char *storage_folder_path = "/tmp/bgpstream_tmp";
  char *file_suffix = ".gz";
  mkdir(storage_folder_path, 0755);
  // get file name from url
  char *pLastSlash = strrchr(transport->res->uri, '/');
  char *output_name = (char *) malloc( sizeof( char ) *
                                       (
                                        strlen(storage_folder_path) +
                                        strlen(pLastSlash) +
                                        strlen(file_suffix)+ 1
                                        )
                                       );
  strcpy(output_name, storage_folder_path);
  strcat(output_name, pLastSlash);
  strcat(output_name, file_suffix);
  STATE->cache_file_path = output_name;

  // If the file exists, don't create writer, and set readFromCache = 1
  if( access( STATE->cache_file_path, F_OK ) != -1 ) {
    // file exists
    STATE->read_from_cache = 1;
    bgpstream_log(BGPSTREAM_LOG_INFO, "READ FROM CACHE!\n%s\n", STATE->cache_file_path);

    if ((STATE->reader = wandio_create(STATE->cache_file_path)) == NULL) {
      bgpstream_log(BGPSTREAM_LOG_ERR, "Could not open %s for reading",
                    STATE->cache_file_path);
      return -1;
    }
  } else {
    // if local cache file doesn't exist

    // create file transport
    if ((STATE->reader = wandio_create(transport->res->uri)) == NULL) {
      bgpstream_log(BGPSTREAM_LOG_ERR, "Could not open %s for reading",
                    transport->res->uri);
      return -1;
    }

    // ZLib default compression level is 6: https://zlib.net/manual.html
    if ((STATE->writer = wandio_wcreate(output_name, WANDIO_COMPRESS_ZLIB, Z_DEFAULT_COMPRESSION, O_CREAT)) == NULL) {
      bgpstream_log(BGPSTREAM_LOG_ERR, "Could not open %s for local caching", output_name);
      return -1;
    }

    STATE->read_from_cache = 0;
  }

  return 0;
}

int64_t bs_transport_cache_read(bgpstream_transport_t *transport,
                                uint8_t *buffer, int64_t len)
{

  int64_t ret = wandio_read(STATE->reader, buffer, len);

  if(STATE->read_from_cache == 0){
    // not reading from cache, then write it
    if(STATE->writer != NULL){
      wandio_wwrite(STATE->writer, buffer, len);
    } else{
      bgpstream_log(BGPSTREAM_LOG_ERR, "Error writing cache");
    }
  }

  return ret;
}

void bs_transport_cache_destroy(bgpstream_transport_t *transport)
{
  if (STATE->reader != NULL) {
    wandio_destroy(STATE->reader);
    STATE->reader = NULL;
  }

  if (STATE->writer != NULL) {
    wandio_wdestroy(STATE->writer);
    STATE->writer = NULL;
  }
}
