/*
 * Copyright (C) 2015 The Regents of the University of California.
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
 */

#include "bsdi_singlefile.h"
#include "bgpstream_log.h"
#include "config.h"
#include "utils.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <wandio.h>

#define STATE (BSDI_GET_STATE(di, singlefile))

// mapping from type name to resource format type
static char *type_strs[] = {
  "mrt",      // BGPSTREAM_RESOURCE_FORMAT_MRT
  "bmp",      // BGPSTREAM_RESOURCE_FORMAT_BMP
  "ripejson", // BGPSTREAM_RESOURCE_FORMAT_RIPEJSON
};

/* ---------- START CLASS DEFINITION ---------- */

/* define the internal option ID values */
enum {
  OPTION_RIB_FILE,
  OPTION_RIB_TYPE,
  OPTION_UPDATE_FILE,
  OPTION_UPDATE_TYPE,
};

/* define the options this data interface accepts */
static bgpstream_data_interface_option_t options[] = {
  /* RIB file path */
  {
    BGPSTREAM_DATA_INTERFACE_SINGLEFILE, // interface ID
    OPTION_RIB_FILE,                     // internal ID
    "rib-file",                          // name
    "rib mrt file to read (default: " STR(BGPSTREAM_DI_SINGLEFILE_RIB_FILE) ")",
  },
  /* RIB file type */
  {
    BGPSTREAM_DATA_INTERFACE_SINGLEFILE, // interface ID
    OPTION_RIB_TYPE,                     // internal ID
    "rib-type",                          // name
    "rib file type (mrt/bmp) (default: mrt)",
  },
  /* Update file path */
  {
    BGPSTREAM_DATA_INTERFACE_SINGLEFILE, // interface ID
    OPTION_UPDATE_FILE,                  // internal ID
    "upd-file",                          // name
    "updates mrt file to read (default: " STR(
      BGPSTREAM_DI_SINGLEFILE_UPDATE_FILE) ")",
  },
  /* Update file type */
  {
    BGPSTREAM_DATA_INTERFACE_SINGLEFILE, // interface ID
    OPTION_UPDATE_TYPE,                  // internal ID
    "upd-type",                          // name
    "update file type (mrt/bmp/ripejson) (default: mrt)",
  },
};

/* create the class structure for this data interface */
BSDI_CREATE_CLASS(singlefile, BGPSTREAM_DATA_INTERFACE_SINGLEFILE,
                  "Read a single mrt data file (RIB and/or updates)", options);

/* ---------- END CLASS DEFINITION ---------- */

/* check for new ribs once every 30 mins */
#define RIB_FREQUENCY_CHECK 1800

/* check for new updates once every 2 minutes */
#define UPDATE_FREQUENCY_CHECK 120

/* max number of bytes to read from file header (to detect file changes) */
#define MAX_HEADER_READ_BYTES 1024

typedef struct bsdi_singlefile_state {
  /* user-provided options: */

  // Path to a RIB file to read
  char *rib_file;

  // Type of the given RIB file (MRT/BMP)
  bgpstream_resource_format_type_t rib_type;

  // Path to an update file to read
  char *update_file;

  // Type of the given Update file (MRT/BMP)
  bgpstream_resource_format_type_t update_type;

  /* internal state: */

  // a few bytes from the beginning of the RIB file (used to tell if a symlink
  // has been updated)
  char rib_header[MAX_HEADER_READ_BYTES];

  // timestamp of the last RIB read
  uint32_t last_rib_filetime;

  // a few bytes from the beginning of the update file (used to tell if a
  // symlink has been updated)
  char update_header[MAX_HEADER_READ_BYTES];

  // timestamp of the last updates read
  uint32_t last_update_filetime;
} bsdi_singlefile_state_t;

static int same_header(char *filename, char *prev_hdr)
{
  char buffer[MAX_HEADER_READ_BYTES];
  off_t bread;
  io_t *io_h;

  if ((io_h = wandio_create(filename)) == NULL) {
    bgpstream_log(BGPSTREAM_LOG_ERR, "can't open file '%s'", filename);
    return -1;
  }

  if ((bread = wandio_read(io_h, (void *)&(buffer[0]), MAX_HEADER_READ_BYTES)) <
      0) {
    bgpstream_log(BGPSTREAM_LOG_ERR, "can't read file '%s'", filename);
    wandio_destroy(io_h);
    return -1;
  }
  wandio_destroy(io_h);

  if (bcmp(buffer, prev_hdr, bread) == 0) {
    /* there is no difference, it has the same header */
    return 1;
  }

  /* its a new file, update our header */
  memcpy(prev_hdr, buffer, bread);
  return 0; // not the same header
}

/* ========== PUBLIC METHODS BELOW HERE ========== */

int bsdi_singlefile_init(bsdi_t *di)
{
  bsdi_singlefile_state_t *state;

  if ((state = malloc_zero(sizeof(bsdi_singlefile_state_t))) == NULL) {
    goto err;
  }
  BSDI_SET_STATE(di, state);

  /* set default state */
  state->rib_type = BGPSTREAM_RESOURCE_FORMAT_MRT;
  state->update_type = BGPSTREAM_RESOURCE_FORMAT_MRT;

  return 0;
err:
  bsdi_singlefile_destroy(di);
  return -1;
}

int bsdi_singlefile_start(bsdi_t *di)
{
  if (STATE->rib_file || STATE->update_file) {
    return 0;
  } else {
    bgpstream_log(BGPSTREAM_LOG_ERR,
                  "At least one of the 'rib-file' and 'upd-file' "
                  "options must be set\n");
    return -1;
  }
}

int bsdi_singlefile_set_option(
  bsdi_t *di, const bgpstream_data_interface_option_t *option_type,
  const char *option_value)
{
  int i;

  switch (option_type->id) {
  case OPTION_RIB_FILE:
    // replaces our current RIB file
    if (STATE->rib_file != NULL) {
      free(STATE->rib_file);
      STATE->rib_file = NULL;
    }
    if ((STATE->rib_file = strdup(option_value)) == NULL) {
      return -1;
    }
    break;

  case OPTION_RIB_TYPE:
    for (i = 0; i < ARR_CNT(type_strs); i++) {
      if (strcmp(option_value, type_strs[i]) == 0) {
        STATE->rib_type = i;
        break;
      }
    }
    break;

  case OPTION_UPDATE_FILE:
    // replaces our current update file
    if (STATE->update_file != NULL) {
      free(STATE->update_file);
      STATE->update_file = NULL;
    }
    if ((STATE->update_file = strdup(option_value)) == NULL) {
      return -1;
    }
    break;

  case OPTION_UPDATE_TYPE:
    for (i = 0; i < ARR_CNT(type_strs); i++) {
      if (strcmp(option_value, type_strs[i]) == 0) {
        STATE->update_type = i;
        break;
      }
    }
    break;

  default:
    return -1;
  }

  return 0;
}

void bsdi_singlefile_destroy(bsdi_t *di)
{
  if (di == NULL || STATE == NULL) {
    return;
  }

  free(STATE->rib_file);
  STATE->rib_file = NULL;

  free(STATE->update_file);
  STATE->update_file = NULL;

  free(STATE);
  BSDI_SET_STATE(di, NULL);
}

int bsdi_singlefile_update_resources(bsdi_t *di)
{
  uint32_t now = epoch_sec();

  /* if this is the first time we've read the file, then add it to the queue,
     otherwise check the header to see if it has changed */

  if (STATE->rib_file != NULL &&
      (now - STATE->last_rib_filetime) > RIB_FREQUENCY_CHECK &&
      same_header(STATE->rib_file, STATE->rib_header) == 0) {
    STATE->last_rib_filetime = now;

    if (bgpstream_resource_mgr_push(
          BSDI_GET_RES_MGR(di), BGPSTREAM_RESOURCE_TRANSPORT_FILE,
          STATE->rib_type, STATE->rib_file, STATE->last_rib_filetime,
          RIB_FREQUENCY_CHECK, "singlefile", "singlefile", BGPSTREAM_RIB,
          NULL) < 0) {
      goto err;
    }
  }

  if (STATE->update_file != NULL &&
      (now - STATE->last_update_filetime) > UPDATE_FREQUENCY_CHECK &&
      same_header(STATE->update_file, STATE->update_header) == 0) {
    STATE->last_update_filetime = now;

    if (bgpstream_resource_mgr_push(
          BSDI_GET_RES_MGR(di), BGPSTREAM_RESOURCE_TRANSPORT_FILE,
          STATE->update_type, STATE->update_file, STATE->last_update_filetime,
          UPDATE_FREQUENCY_CHECK, "singlefile", "singlefile", BGPSTREAM_UPDATE,
          NULL) < 0) {
      goto err;
    }
  }

  return 0;

err:
  return -1;
}
