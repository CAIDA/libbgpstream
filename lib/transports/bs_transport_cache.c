/*
 * This file is part of bgpstream
 *
 * CAIDA, UC San Diego
 * bgpstream-info@caida.org
 *
 * Copyright (C) 2012 The Regents of the University of California.
 * Authors: Alistair King, Chiara Orsini, Mingwei Zhang
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

#define STATE ((cache_state_t*)(transport->state))

#define CACHE_FILE_SUFFIX ".cache"
#define CACHE_LOCK_FILE_SUFFIX ".lock"
#define CACHE_TEMP_FILE_SUFFIX ".temp"

typedef struct cache_state {
  /** A 0/1 value indicates whether current read is from a local cache
      file or a remote transport file:
      0 - read from remote;
      1 - read from local cache.
  */
  int write_to_cache;

  /** absolute path for bgpstream local cache directory */
  char* cache_directory_path;

  /** absolute path for the local cache file */
  char* cache_file_path;

  /** absolute path for the local cache lock file */
  char* lock_file_path;

  /** absolute path for the local cache temporary file */
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

  // define local variables
  char resource_hash[1024];
  int len_cache_file_path;
  int len_lock_file_path;
  int len_temp_file_path;

  // get a "hash" string from the resource
  if((bgpstream_resource_hash_snprintf(
                        resource_hash,
                        sizeof(resource_hash),
                        transport->res)) >= sizeof(resource_hash)){
    bgpstream_log(BGPSTREAM_LOG_ERR, "ERROR: Could not get resource hash for cache file naming.");
    return -1;
  }

  // allocate memory for cache_state type in transport data structure
  if ((transport->state = malloc_zero(sizeof(cache_state_t))) == NULL) {
    bgpstream_log(BGPSTREAM_LOG_ERR, "ERROR: Could not allocate memory for cache_state_t.");
    return -1;
  }

  STATE->reader = NULL;
  STATE->writer = NULL;

  // set storage directory path
  if ((STATE->cache_directory_path = strdup(bgpstream_resource_get_attr(
          transport->res, BGPSTREAM_RESOURCE_ATTR_CACHE_DIR_PATH))) == NULL) {
    bgpstream_log(BGPSTREAM_LOG_ERR, "ERROR: Could not read local cache directory path in resource.");
    return -1;
  }

  // set cache file path
  len_cache_file_path =
    strlen(STATE->cache_directory_path) +
    strlen(resource_hash) +
    strlen(CACHE_FILE_SUFFIX)+ 2;
  if((STATE->cache_file_path = (char *) malloc( sizeof( char ) * len_cache_file_path )
      ) == NULL) {
    bgpstream_log(BGPSTREAM_LOG_ERR, "ERROR: Could not allocate space for cache file name variable.");
    return -1;
  }
  if(
     (snprintf(STATE->cache_file_path, len_cache_file_path,
              "%s/%s%s", STATE->cache_directory_path, resource_hash, CACHE_FILE_SUFFIX))
      >= len_cache_file_path){
    bgpstream_log(BGPSTREAM_LOG_ERR, "ERROR: Could not set cache file name variable.");
    return-1;
  }

  // set lock file name: cache_file_path + ".lock"
  len_lock_file_path = strlen(STATE->cache_file_path) + strlen(CACHE_LOCK_FILE_SUFFIX)+ 2;
  if((STATE->lock_file_path = (char *) malloc( sizeof( char ) * len_lock_file_path)) == NULL) {
    bgpstream_log(BGPSTREAM_LOG_ERR, "ERROR: Could not allocate space for lock file name variable.");
    return -1;
  }
  if((snprintf(STATE->lock_file_path, len_lock_file_path,
               "%s%s", STATE->cache_file_path, CACHE_LOCK_FILE_SUFFIX) )
     >= len_lock_file_path){
    bgpstream_log(BGPSTREAM_LOG_ERR, "ERROR: Could not set lock file name variable.");
    return-1;
  }

  // set temporary cache file name: cache_file_path + ".temp"
  len_temp_file_path = strlen(STATE->cache_file_path) + strlen(CACHE_TEMP_FILE_SUFFIX)+ 2;
  if((STATE->temp_file_path = (char *) malloc( sizeof( char ) * len_temp_file_path)) == NULL) {
    bgpstream_log(BGPSTREAM_LOG_ERR, "ERROR: Could not allocate space for temp file name variable.");
    return -1;
  }
  if((snprintf(STATE->temp_file_path, len_temp_file_path,
               "%s%s", STATE->cache_file_path, CACHE_TEMP_FILE_SUFFIX) )
     >= len_temp_file_path){
    bgpstream_log(BGPSTREAM_LOG_ERR, "ERROR: Could not set temp file name variable.");
    return-1;
  }

  return 0;
}

int bs_transport_cache_create(bgpstream_transport_t *transport)
{

  int lock_fd;  // lock file descriptor

  // reset transport method
  BS_TRANSPORT_SET_METHODS(cache, transport);

  // initialize cache_state data structure
  if(init_state(transport) != 0){
    return -1;
  }

  // If the cache file exists, don't create cache writer
  if(access( STATE->cache_file_path, F_OK ) != -1) {
    // local cache file exists, disable write_to_cache flag
    STATE->write_to_cache = 0;

    // create reader that reads from existing local cache file
    if ((STATE->reader = wandio_create(STATE->cache_file_path)) == NULL) {
      bgpstream_log(BGPSTREAM_LOG_ERR, "ERROR: Could not open %s for reading",
                    STATE->cache_file_path);
      return -1;
    }
  } else {
    // local cache file doesn't exist

    // try to create a lock file for cache writing
    lock_fd = open(STATE->lock_file_path,O_CREAT|O_EXCL, 0644);

    if(lock_fd<0){
      // lock file creation failed: other thread is still writing the cache
      // disable write_to_cache flag
      STATE->write_to_cache = 0;
    } else {
      // lock file created successfully, now safe to create write cache
      // enable write_to_cache flag
      STATE->write_to_cache = 1;

      // create cache file writer using wandio with compression enabled at default compression level
      // ZLib default compression level is 6: https://zlib.net/manual.html
      if ((STATE->writer = wandio_wcreate(STATE->temp_file_path, WANDIO_COMPRESS_ZLIB, 6, O_CREAT)) == NULL) {
        bgpstream_log(BGPSTREAM_LOG_ERR, "ERROR: Could not open %s for local caching", STATE->temp_file_path);
        return -1;
      }
    }

    // open reader that reads from remote file
    if ((STATE->reader = wandio_create(transport->res->uri)) == NULL) {
      bgpstream_log(BGPSTREAM_LOG_ERR, "ERROR: Could not open %s for reading",
                    transport->res->uri);
      return -1;
    }
  }

  return 0;
}

int64_t bs_transport_cache_read(bgpstream_transport_t *transport,
                                uint8_t *buffer, int64_t len)
{

  // read content
  int64_t ret = wandio_read(STATE->reader, buffer, len);

  // if cache-writing is enabled
  if(STATE->write_to_cache == 1){

    if(ret == 0){
      // reader's EOF reached:
      //   finished reading a remote content
      //   save to close cache writer; rename temporary file to cache file; and remove write lock

      // close cache writer
      wandio_wdestroy(STATE->writer);
      STATE->writer = NULL;

      // rename temporary file to cache file
      if(rename(STATE->temp_file_path, STATE->cache_file_path) !=0){
        bgpstream_log(BGPSTREAM_LOG_ERR, "ERROR: renaming failed for file %s.", STATE->temp_file_path);
      }

      // remove lock file
      if(remove(STATE->lock_file_path) !=0){
        bgpstream_log(BGPSTREAM_LOG_ERR, "ERROR: removing lock file failed %s.", STATE->lock_file_path);
      }

    } else {
      // reader has read content, and has not reached EOF yet

      // write content to temporary cache file
      if((wandio_wwrite(STATE->writer, buffer, ret))!=ret){
        bgpstream_log(BGPSTREAM_LOG_ERR, "ERROR: incomplete write of cache content.");
        return -1;
      }
    }
  }

  return ret;
}

void bs_transport_cache_destroy(bgpstream_transport_t *transport)
{

  if (transport->state == NULL) {
    return;
  }

  // close reader
  if (STATE->reader != NULL) {
    wandio_destroy(STATE->reader);
    STATE->reader = NULL;
  }

  // close writer
  if (STATE->writer != NULL) {
    // the writer should already has been closed when the reader reaches EOF
    wandio_wdestroy(STATE->writer);
    STATE->writer = NULL;
  }

  // free up file path variables' memory space
  free(STATE->cache_file_path);
  free(STATE->lock_file_path);
  free(STATE->temp_file_path);

  // free up the cache_state_t's memory space
  free(transport->state);
  transport->state = NULL;
}
