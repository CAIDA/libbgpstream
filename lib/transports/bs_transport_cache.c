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
  int write_to_cache;

  /** absolute path for the local cache file */
  char* cache_file_path;

  /** absolute path for the local cache temporary write file */
  char* temp_file_path;

  /** content reader, either from local cache or from remote URI */
  io_t* reader;

  /** cache content writer */
  iow_t* writer;

} cache_state_t;

/**
   Initialize the cache_state_t data structure;
*/
int init_state(bgpstream_transport_t *transport){

  // define variables
  char *temp_folder_path = "/tmp/bgpstream_temp";
  char *cache_folder_path = "/tmp/bgpstream_cache";
  char *file_suffix = ".gz";
  char *uri_file_name = strrchr(transport->res->uri, '/');
  char *cache_file_name;
  char *temp_file_name;

  // allocate memory for cache_state type in transport data structure
  if ((transport->state = malloc_zero(sizeof(cache_state_t))) == NULL) {
    bgpstream_log(BGPSTREAM_LOG_ERR, "Could not allocate memory for cache_state_t.");
    return -1;
  }

  STATE->reader = NULL;
  STATE->writer = NULL;

  // create storage folders
  mkdir(temp_folder_path, 0755);
  mkdir(cache_folder_path, 0755);
  if( access( cache_folder_path, F_OK ) == -1 ) {
    bgpstream_log(BGPSTREAM_LOG_ERR, "Could not create cache folder %s.", cache_folder_path);
    return -1;
  }
  if( access( temp_folder_path, F_OK ) == -1 ) {
    bgpstream_log(BGPSTREAM_LOG_ERR, "Could not create temp folder %s.", temp_folder_path);
    return -1;
  }

  // set cache file name
  if((cache_file_name = (char *) malloc( sizeof( char ) *
                                       (
                                        strlen(cache_folder_path) +
                                        strlen(uri_file_name) +
                                        strlen(file_suffix)+ 1
                                        )
                                     )) == NULL) {
    bgpstream_log(BGPSTREAM_LOG_ERR, "Could not allocate file name variable space.");
    return -1;
  }
  strcpy(cache_file_name, cache_folder_path);
  strcat(cache_file_name, uri_file_name);
  strcat(cache_file_name, file_suffix);
  STATE->cache_file_path = cache_file_name;

  // set temp file name
  if((temp_file_name = (char *) malloc( sizeof( char ) *
                                         (
                                          strlen(temp_folder_path) +
                                          strlen(uri_file_name) +
                                          strlen(file_suffix)+ 1
                                          )
                                         )) == NULL) {
    bgpstream_log(BGPSTREAM_LOG_ERR, "Could not allocate file name variable space.");
    return -1;
  }
  strcpy(temp_file_name, temp_folder_path);
  strcat(temp_file_name, uri_file_name);
  strcat(temp_file_name, file_suffix);
  STATE->temp_file_path = temp_file_name;

  return 0;
}


int bs_transport_cache_create(bgpstream_transport_t *transport)
{
  // reset transport method
  BS_TRANSPORT_SET_METHODS(cache, transport);

  // initialize cache_state data structure
  if(init_state(transport) != 0){
    return -1;
  }

  // If the file exists, don't create writer, and set readFromCache = 1
  if( access( STATE->cache_file_path, F_OK ) != -1 ) {
    // local cache file exists
    STATE->write_to_cache = 0;
    bgpstream_log(BGPSTREAM_LOG_INFO, "READ FROM CACHE!\n%s\n", STATE->cache_file_path);

    if ((STATE->reader = wandio_create(STATE->cache_file_path)) == NULL) {
      bgpstream_log(BGPSTREAM_LOG_ERR, "Could not open %s for reading",
                    STATE->cache_file_path);
      return -1;
    }
  } else {
    // if local cache file doesn't exist

    // open reader for remote file
    if ((STATE->reader = wandio_create(transport->res->uri)) == NULL) {
      bgpstream_log(BGPSTREAM_LOG_ERR, "Could not open %s for reading",
                    transport->res->uri);
      return -1;
    }

    // create new writer ONLY IF there is no other writers working on the cache
    if( access( STATE->temp_file_path, F_OK ) == -1 ) {
      STATE->write_to_cache = 1;

      // ZLib default compression level is 6: https://zlib.net/manual.html
      if ((STATE->writer = wandio_wcreate(STATE->temp_file_path, WANDIO_COMPRESS_ZLIB, Z_DEFAULT_COMPRESSION, O_CREAT)) == NULL) {
        bgpstream_log(BGPSTREAM_LOG_ERR, "Could not open %s for local caching", STATE->temp_file_path);
        return -1;
      }
    }
  }

  return 0;
}

int64_t bs_transport_cache_read(bgpstream_transport_t *transport,
                                uint8_t *buffer, int64_t len)
{

  int64_t ret = wandio_read(STATE->reader, buffer, len);

  // if write_to_cache is set to 1
  if(STATE->write_to_cache == 1){
    if(STATE->writer != NULL){
      wandio_wwrite(STATE->writer, buffer, len);
    } else{
      bgpstream_log(BGPSTREAM_LOG_ERR, "Error writing cache to %s.", STATE->temp_file_path);
    }
  }

  return ret;
}

void bs_transport_cache_destroy(bgpstream_transport_t *transport)
{
  if (transport->state == NULL) {
    return;
  }

  if (STATE->reader != NULL) {
    wandio_destroy(STATE->reader);
    STATE->reader = NULL;
  }

  if (STATE->writer != NULL) {
    // finished writing a temporary file
    char* command;
    if((command = (char *) malloc( sizeof( char ) *
                                           (
                                            strlen(STATE->cache_file_path) +
                                            strlen(STATE->temp_file_path) +
                                            10
                                            )
                                           )) == NULL) {
      bgpstream_log(BGPSTREAM_LOG_ERR, "Could not allocate file name variable space.");
    }

    // construct "mv" command to move temporary cache file to cache folder
    strcpy(command, "mv ");
    strcpy(command, STATE->temp_file_path);
    strcpy(command, " ");
    strcpy(command, STATE->cache_file_path);
    if(system(command) != 0){
      bgpstream_log(BGPSTREAM_LOG_ERR, "Failed to move cache file from temporary folder to cache folder.");
    }

    wandio_wdestroy(STATE->writer);
    STATE->writer = NULL;
  }

  free(STATE->cache_file_path);
  free(STATE->temp_file_path);

  free(transport->state);
  transport->state = NULL;
}
