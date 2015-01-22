/**
 * Wrapper for libwandio that implements enough of the cfile_tools api to
 * satisfy bgpdump.
 *
 * Author: Alistair King <alistair@caida.org>
 * 2014-08-14
 */

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <wandio.h>
#include "bgpdump_cfile_tools.h"

#define WFILE(x) ((io_t*)(x)->data2)

// API Functions

CFRFILE *cfr_open(const char *path) {
  CFRFILE *cfr = NULL;

  if((cfr = malloc(sizeof(CFRFILE))) == NULL)
    {
      return NULL;
    }
  memset(cfr, 0, sizeof(CFRFILE));

  /* sweet hax */
  cfr->data2 = wandio_create(path);

  return cfr;
}

int cfr_close(CFRFILE *stream) {
  wandio_destroy(WFILE(stream));
  stream->closed = 1;

  free(stream);
  return 0;
}

size_t cfr_read_n(CFRFILE *stream, void *ptr, size_t bytes) {
  return wandio_read(WFILE(stream), ptr, bytes);
}
