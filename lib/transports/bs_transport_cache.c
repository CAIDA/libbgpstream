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
#if defined(__APPLE__)
#  define COMMON_DIGEST_FOR_OPENSSL
#  include <CommonCrypto/CommonDigest.h>
#  define SHA1 CC_SHA1
#else
#  include <openssl/md5.h>
#endif

#define STATE ((cache_state_t*)(transport->state))

typedef struct cache_state {
  /* user-provided options: */

  /** A 0/1 value indicates whether current read is from a local cache
      file or a remote transport file:
      0 - read from remote;
      1 - read from local cache.
  */
  int write_to_cache;

  /** absolute path for bgpstream local cache folder */
  char* cache_folder_path;

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
   construct md5 digest from file.
   https://stackoverflow.com/questions/10324611/how-to-calculate-the-md5-hash-of-a-large-file-in-c

   @return a 32 bytes of MD5 digest in hex number format.
*/
char* file2md5(const char* file_name)
{
  unsigned char digest[16];
  int i;
  FILE *inFile = fopen (file_name, "rb");
  MD5_CTX mdContext;
  int bytes;
  unsigned char data[1024];
  char *out = (char*)malloc(33);

  if (inFile == NULL) {
    bgpstream_log(BGPSTREAM_LOG_ERR,"MD5 source file %s can't be opened.\n", file_name);
    free(out);
    return NULL;
  }

  MD5_Init (&mdContext);
  while ((bytes = fread (data, 1, 1024, inFile)) != 0)
    MD5_Update (&mdContext, data, bytes);
  MD5_Final (digest,&mdContext);

  for (i = 0; i < 16; ++i) {
    snprintf(&(out[i*2]), 32, "%02x", (unsigned int)digest[i]);
  }

  fclose (inFile);
  return out;
}

/**
   compare checksum from the current file and the previously written checksum file.

   @return 0 if checksums match, otherwise -1.
 */
int compare_checksum(const char* cache_file_path, const char* md5_file_path){
  char* checksum_1 = file2md5(cache_file_path);
  char* checksum_2 = (char*)malloc(33);
  int i;
  FILE * fp;
  char mystring [100];

  // open file and check status
  fp = fopen (md5_file_path, "r");
  if(checksum_1==NULL || fp==NULL){
    free(checksum_2);
    return -1;
  }

  // read checksum
  fgets( checksum_2, 32, fp );
  fclose(fp);

  // compare checksum
  for(i=0;i<32;i++){
    if(checksum_1[i]!=checksum_2[i]){
      bgpstream_log(BGPSTREAM_LOG_INFO,"MD5 checksum not match for %s.\n", cache_file_path);
      return 1;
    }
  }

  return 0;
}

/**
   Initialize the cache_state_t data structure;
*/
int init_state(bgpstream_transport_t *transport){

  // define variables
  char *cache_folder_path;
  char *file_suffix = ".gz";
  char *lock_suffix = ".lock";
  char *temp_suffix = ".temp";
  char *uri_file_name = strrchr(transport->res->uri, '/');
  char *cache_file_name;
  char *lock_file_name;
  char *temp_file_name;


  // allocate memory for cache_state type in transport data structure
  if ((transport->state = malloc_zero(sizeof(cache_state_t))) == NULL) {
    bgpstream_log(BGPSTREAM_LOG_ERR, "Could not allocate memory for cache_state_t.");
    return -1;
  }

  STATE->reader = NULL;
  STATE->writer = NULL;

  // create storage folders
  STATE->cache_folder_path = "/tmp/bgpstream_cache";
  mkdir(STATE->cache_folder_path, 0755);
  if( access( STATE->cache_folder_path, F_OK ) == -1 ) {
    bgpstream_log(BGPSTREAM_LOG_ERR, "Could not create cache folder %s.", STATE->cache_folder_path);
    return -1;
  }
  cache_folder_path = STATE->cache_folder_path;

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

  // set lock file name
  if((lock_file_name = (char *) malloc( sizeof( char ) *
                                         (
                                          strlen(cache_folder_path) +
                                          strlen(uri_file_name) +
                                          strlen(lock_suffix)+ 1
                                          )
                                         )) == NULL) {
    bgpstream_log(BGPSTREAM_LOG_ERR, "Could not allocate file name variable space.");
    return -1;
  }
  strcpy(lock_file_name, cache_folder_path);
  strcat(lock_file_name, uri_file_name);
  strcat(lock_file_name, lock_suffix);
  STATE->lock_file_path = lock_file_name;

  // set temp file name
  if((temp_file_name = (char *) malloc( sizeof( char ) *
                                        (
                                         strlen(cache_folder_path) +
                                         strlen(uri_file_name) +
                                         strlen(temp_suffix)+ 1
                                         )
                                        )) == NULL) {
    bgpstream_log(BGPSTREAM_LOG_ERR, "Could not allocate file name variable space.");
    return -1;
  }
  strcpy(temp_file_name, cache_folder_path);
  strcat(temp_file_name, uri_file_name);
  strcat(temp_file_name, temp_suffix);
  STATE->temp_file_path = temp_file_name;

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

  // If the file exists, don't create writer, and set readFromCache = 1
  if(access( STATE->cache_file_path, F_OK ) != -1) {
    // local cache file exists -> good to read
    STATE->write_to_cache = 0;

    if ((STATE->reader = wandio_create(STATE->cache_file_path)) == NULL) {
      bgpstream_log(BGPSTREAM_LOG_ERR, "Could not open %s for reading",
                    STATE->cache_file_path);
      return -1;
    }
  } else {
    // if local cache file doesn't exist, or cache is being written at this moment

    lock_fd = open(STATE->lock_file_path,O_CREAT|O_EXCL);  // atomic action: try to create a lock file
    if(lock_fd<0){
      // lock file creation failed: other thread is writing the cache
      STATE->write_to_cache = 0;
    } else {
      // lock file created successfully, now safe to create write cache
      STATE->write_to_cache = 1;

      // ZLib default compression level is 6: https://zlib.net/manual.html
      if ((STATE->writer = wandio_wcreate(STATE->temp_file_path, WANDIO_COMPRESS_ZLIB, 6, O_CREAT)) == NULL) {
        bgpstream_log(BGPSTREAM_LOG_ERR, "Could not open %s for local caching", STATE->temp_file_path);
        return -1;
      }
    }

    // in either case: open reader for remote file
    if ((STATE->reader = wandio_create(transport->res->uri)) == NULL) {
      bgpstream_log(BGPSTREAM_LOG_ERR, "Could not open %s for reading",
                    transport->res->uri);
      return -1;
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
      bgpstream_log(BGPSTREAM_LOG_ERR, "Error writing cache to %s.", STATE->cache_file_path);
    }
  }

  return ret;
}

void bs_transport_cache_destroy(bgpstream_transport_t *transport)
{

  char* md5_digest;
  int i;

  if (transport->state == NULL) {
    return;
  }

  if (STATE->reader != NULL) {
    wandio_destroy(STATE->reader);
    STATE->reader = NULL;
  }

  if (STATE->writer != NULL) {
    // finished writing cache file

    // rename temp file to cache file
    if(rename(STATE->temp_file_path, STATE->cache_file_path) !=0){
      bgpstream_log(BGPSTREAM_LOG_ERR, "Error renaming finished file %s.", STATE->temp_file_path);
    }

    // remove lock file
    if(remove(STATE->lock_file_path) !=0){
      bgpstream_log(BGPSTREAM_LOG_ERR, "Error removing lock file %s.", STATE->lock_file_path);
    }

    wandio_wdestroy(STATE->writer);
    STATE->writer = NULL;
  }

  free(STATE->cache_file_path);
  free(STATE->lock_file_path);
  free(STATE->temp_file_path);

  free(transport->state);
  transport->state = NULL;
}
