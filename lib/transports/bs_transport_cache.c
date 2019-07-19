/*
 * Copyright (C) 2017 The Regents of the University of California.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Authors:
 *   Mingwei Zhang
 *   Ken Keys
 */

#include "bs_transport_cache.h"
#include "bgpstream_transport_interface.h"
#include "bgpstream_log.h"
#include "utils.h"
#include "wandio.h"
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdarg.h>
#include <errno.h>

#define STATE ((cache_state_t *)(transport->state))

#define CACHE_FILE_SUFFIX ".cache"
#define CACHE_LOCK_FILE_SUFFIX ".lock"
#define CACHE_TEMP_FILE_SUFFIX ".temp"

typedef struct cache_state {
  /** absolute path for the local cache file */
  char *cache_file_path;

  /** absolute path for the local cache lock file */
  char *lock_file_path;

  /** absolute path for the local cache temporary file */
  char *temp_file_path;

  /** filename or URL of reader */
  char *reader_name;

  /** file descriptor of cache lock file */
  int lock_fd;

  /** content reader, either from local cache or from remote URL */
  io_t *reader;

  /** cache content writer, or NULL if we're not writing */
  iow_t *writer;

} cache_state_t;

// Like sprintf(), but first allocate a string large enough to hold the output.
#ifdef __GNUC__
__attribute__((format(printf, 2, 3)))
#endif
static int bs_asprintf(char **strp, const char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  int len = vsnprintf(NULL, 0, fmt, ap);
  va_end(ap);
  if (len < 0 || !(*strp = malloc(len+1))) {
    *strp = NULL;
    return -1;
  }
  va_start(ap, fmt);
  len = vsnprintf(*strp, len+1, fmt, ap);
  va_end(ap);
  if (len < 0) { // shouldn't happen
    free(*strp);
    *strp = NULL;
  }
  return len;
}

/**
   Initialize the cache_state_t data structure;
*/
static int init_state(bgpstream_transport_t *transport)
{
  char resource_hash[1024];

  // allocate memory for cache_state type in transport data structure
  if ((transport->state = malloc_zero(sizeof(cache_state_t))) == NULL) {
    bgpstream_log(BGPSTREAM_LOG_ERR,
                  "ERROR: Could not allocate memory for cache_state_t.");
    return -1;
  }

  STATE->lock_fd = -1;
  STATE->reader = NULL;
  STATE->writer = NULL;

  // get a "hash" string from the resource
  if ((bgpstream_resource_hash_snprintf(resource_hash, sizeof(resource_hash),
                                        transport->res)) >=
      sizeof(resource_hash)) {
    bgpstream_log(BGPSTREAM_LOG_WARN,
                  "WARNING: Could not get resource hash for cache file naming.");
    return 0; // not fatal; we can still read, but won't be able to cache
  }

  // get storage directory path
  const char *cache_dir_path = bgpstream_resource_get_attr(
         transport->res, BGPSTREAM_RESOURCE_ATTR_CACHE_DIR_PATH);
  if (cache_dir_path == NULL) {
    bgpstream_log(BGPSTREAM_LOG_WARN,
      "WARNING: Could not read local cache directory path in resource.");
    return 0; // not fatal; we can't use cache, but can still read remote
  }

  // set cache file paths (lock_file is last so if that's set we'll know all
  // three are set)
  if (bs_asprintf(&STATE->cache_file_path, "%s/%s%s",
                  cache_dir_path, resource_hash, CACHE_FILE_SUFFIX) < 0 ||
    bs_asprintf(&STATE->temp_file_path, "%s%s",
                STATE->cache_file_path, CACHE_TEMP_FILE_SUFFIX) < 0 ||
    bs_asprintf(&STATE->lock_file_path, "%s%s",
                STATE->cache_file_path, CACHE_LOCK_FILE_SUFFIX) < 0)
  {
    bgpstream_log(BGPSTREAM_LOG_WARN,
                  "WARNING: Could not set cache file names.");
    return 0; // not fatal; we can't use cache, but can still read remote
  }

  return 0;
}

static int bs_transport_cache_lock(bgpstream_transport_t *transport)
{
  // Note: POSIX fcntl(F_SETLK) locks can not synchronize different threads in
  // the same process.  BSD flock() can, but is not POSIX.
  struct flock lock;

  if (!STATE->lock_file_path)
    return -1;

  if ((STATE->lock_fd = open(STATE->lock_file_path, O_CREAT | O_WRONLY, 0644)) < 0) {
    bgpstream_log(BGPSTREAM_LOG_WARN, "WARNING: can't open lock file %s: %s",
        STATE->lock_file_path, strerror(errno));
    return -1;
  }

  lock.l_type = F_WRLCK;
  lock.l_whence = SEEK_SET;
  lock.l_start = 0;
  lock.l_len = 0;
  if (fcntl(STATE->lock_fd, F_SETLK, &lock) < 0) {
    bgpstream_log(BGPSTREAM_LOG_WARN, "WARNING: can't lock file %s: %s",
        STATE->lock_file_path, strerror(errno));
    close(STATE->lock_fd);
    STATE->lock_fd = -1;
    return -1;
  }

  return 0;
}

static void bs_transport_cache_unlock(bgpstream_transport_t *transport)
{
  // Note: even if we never explicitly release the lock, it will be released
  // automatically when the lock_fd closes at program exit (the file will
  // remain, but it will not be locked).
  remove(STATE->lock_file_path);
  close(STATE->lock_fd);
  STATE->lock_fd = -1;
}

static int open_cache_reader(bgpstream_transport_t *transport)
{
  // Create reader that reads from existing local cache file.
  STATE->reader_name = STATE->cache_file_path;
  if ((STATE->reader = wandio_create(STATE->reader_name))) {
    bgpstream_log(BGPSTREAM_LOG_FINE, "reading cache %s", STATE->reader_name);
    return 0; // success
  }
  bgpstream_log(BGPSTREAM_LOG_WARN, "WARNING: Could not read cache %s",
                STATE->reader_name);
  return -1;
}

int bs_transport_cache_create(bgpstream_transport_t *transport)
{
  // reset transport method
  BS_TRANSPORT_SET_METHODS(cache, transport);

  // initialize cache_state data structure
  if (init_state(transport) != 0) {
    return -1;
  }

  // Check cache access before acquiring the lock, so that most cache readers
  // never need to lock and multiple cache readers won't block each other.
  if (STATE->cache_file_path && access(STATE->cache_file_path, R_OK) == 0) {
    if (open_cache_reader(transport) == 0)
      return 0; // reading from local cache
  }

  if (bs_transport_cache_lock(transport) == 0) {
    // We own the lock.
    // Check cache access again to avoid a race where another process finished
    // writing a cache between our first access() and our getting the lock.
    if (access(STATE->cache_file_path, R_OK) == 0) {
      // local cache file exists and is readable
      bs_transport_cache_unlock(transport);
      if (open_cache_reader(transport) == 0)
        return 0; // reading from local cache
    } else if (errno == ENOENT) {
      // Local cache file doesn't exist.  Hold on to the lock so we can write
      // to the cache.
    } else {
      // local cache file is not readable for some other reason
      bgpstream_log(BGPSTREAM_LOG_WARN, "WARNING: Could not read cache %s: %s",
                    STATE->cache_file_path, strerror(errno));
      bs_transport_cache_unlock(transport);
    }
  }

  // open reader that reads from remote file
  STATE->reader_name = transport->res->url;
  if ((STATE->reader = wandio_create(STATE->reader_name)) == NULL) {
    bgpstream_log(BGPSTREAM_LOG_ERR, "ERROR: Could not open %s for reading",
                  STATE->reader_name);
    return -1;
  }
  bgpstream_log(BGPSTREAM_LOG_FINE, "reading remote %s", STATE->reader_name);

  if (STATE->lock_fd >= 0) {
    // We own the lock.
    // Create cache file writer using wandio with compression enabled at
    // default compression level ZLib default compression level is 6:
    // https://zlib.net/manual.html
    STATE->writer = wandio_wcreate(STATE->temp_file_path,
                                   WANDIO_COMPRESS_ZLIB, 6, O_CREAT);
    if (STATE->writer == NULL) {
      bgpstream_log(BGPSTREAM_LOG_WARN,
                    "WARNING: Could not open %s for local caching: %s",
                    STATE->temp_file_path, strerror(errno));
      // failing to create the cache is not fatal
      bs_transport_cache_unlock(transport);
      return 0; // reading from remote file
    }
    bgpstream_log(BGPSTREAM_LOG_FINE, "writing temp cache %s",
        STATE->temp_file_path);
  }

  return 0; // reading from remote file
}

int64_t bs_transport_cache_readline(bgpstream_transport_t *transport,
                                    uint8_t *buffer, int64_t len)
{

  return wandio_generic_fgets(transport, buffer, len, 1,
                              (read_cb_t *)bs_transport_cache_read);
}

static void close_cache_writer(bgpstream_transport_t *transport, int valid)
{
  if (!STATE->writer)
    return;

  wandio_wdestroy(STATE->writer);
  STATE->writer = NULL;

  if (valid) {
    // rename temporary file to cache file
    if (rename(STATE->temp_file_path, STATE->cache_file_path) != 0) {
      bgpstream_log(BGPSTREAM_LOG_WARN, "WARNING: failed to rename %s: %s",
                    STATE->temp_file_path, strerror(errno));
    }

  } else {
    // the cache is incomplete or corrupt; remove temporary file
    if (remove(STATE->temp_file_path) != 0) {
      bgpstream_log(BGPSTREAM_LOG_WARN, "WARNING: failed to remove %s: %s",
                    STATE->temp_file_path, strerror(errno));
    }
  }

  bs_transport_cache_unlock(transport);
}

int64_t bs_transport_cache_read(bgpstream_transport_t *transport,
                                uint8_t *buffer, int64_t len)
{
  // read content
  int64_t ret = wandio_read(STATE->reader, buffer, len);

  if (ret < 0) {
    // reader encountered an error
    bgpstream_log(BGPSTREAM_LOG_ERR, "ERROR reading from %s",
        STATE->reader_name);
    close_cache_writer(transport, 0);

  } else if (ret == 0) {
    // reader reached EOF
    bgpstream_log(BGPSTREAM_LOG_FINE, "EOF on %s", STATE->reader_name);
    close_cache_writer(transport, 1);

  } else if (STATE->writer) {
    // reader has read content, and caching is enabled
    int64_t wret = wandio_wwrite(STATE->writer, buffer, ret);
    if (wret != ret) {
      bgpstream_log(BGPSTREAM_LOG_WARN, "WARNING: %s to cache %s.",
                    wret < 0 ? "error writing" : "incomplete write",
                    STATE->temp_file_path);
      close_cache_writer(transport, 0);
      // caching is now disabled, but we can keep reading
    }
  }

  return ret;
}

void bs_transport_cache_destroy(bgpstream_transport_t *transport)
{
  bgpstream_log(BGPSTREAM_LOG_FINE, "destroy reader %s", STATE->reader_name);
  if (transport->state == NULL) {
    return;
  }

  // close writer
  while (STATE->writer) {
    // Cache may be incomplete, so we continue copying remote contents to the
    // cache.  (The other option would be to delete the cache.)
    // bs_transport_cache_read() will eventually get EOF or error, and close
    // the cache writer.
    uint8_t buf[4096];
    bs_transport_cache_read(transport, buf, sizeof(buf));
  }

  // close reader
  if (STATE->reader != NULL) {
    wandio_destroy(STATE->reader);
    STATE->reader = NULL;
  }

  // free up file path variables' memory space
  free(STATE->cache_file_path);
  free(STATE->temp_file_path);

  // free up the cache_state_t's memory space
  free(transport->state);
  transport->state = NULL;
}
