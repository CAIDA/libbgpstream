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

#include "bsdi_singlefile.h"
#include "bgpstream_debug.h"
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

/* ---------- START CLASS DEFINITION ---------- */

/* define the internal option ID values */
enum {
  OPTION_RIB_FILE,
  OPTION_UPDATE_FILE,
};

/* define the options this data interface accepts */
static bgpstream_data_interface_option_t options[] = {
  /* RIB file path */
  {
    BGPSTREAM_DATA_INTERFACE_SINGLEFILE, // interface ID
    OPTION_RIB_FILE, // internal ID
    "rib-file", // name
    "rib mrt file to read (default: " STR(BGPSTREAM_DI_SINGLEFILE_RIB_FILE) ")",
  },
  /* Update file path */
  {
    BGPSTREAM_DATA_INTERFACE_SINGLEFILE, // interface ID
    OPTION_UPDATE_FILE, //internal ID
    "upd-file", //name
    "updates mrt file to read (default: " STR(
      BGPSTREAM_DI_SINGLEFILE_UPDATE_FILE) ")",
  },
};

/* create the class structure for this data interface */
BSDI_CREATE_CLASS(
  singlefile,
  BGPSTREAM_DATA_INTERFACE_SINGLEFILE,
  "Read a single mrt data file (RIB and/or updates)",
  options
);

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

  // Path to an update file to read
  char *update_file;

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
    bgpstream_log_err("Singlefile: can't open file '%s'",
                      filename);
    return -1;
  }

  if ((bread =
       wandio_read(io_h, (void *)&(buffer[0]), MAX_HEADER_READ_BYTES)) < 0) {
    bgpstream_log_err("Singlefile: can't read file '%s'", filename);
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
  // none

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
    fprintf(stderr,
            "ERROR: At least one of the 'rib-file' and 'upd-file' "
            "options must be set\n");
    return -1;
  }
}

int bsdi_singlefile_set_option(bsdi_t *di,
                           const bgpstream_data_interface_option_t *option_type,
                           const char *option_value)
{
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

    if (bgpstream_resource_mgr_push(BSDI_GET_RES_MGR(di),
                                    BGPSTREAM_TRANSPORT_FILE,
                                    BGPSTREAM_FORMAT_MRT,
                                    STATE->rib_file,
                                    STATE->last_rib_filetime,
                                    RIB_FREQUENCY_CHECK,
                                    "singlefile",
                                    "singlefile",
                                    BGPSTREAM_RIB) == NULL) {
      goto err;
    }
  }

  if (STATE->update_file != NULL &&
      (now - STATE->last_update_filetime) > UPDATE_FREQUENCY_CHECK &&
      same_header(STATE->update_file, STATE->update_header) == 0) {
    STATE->last_update_filetime = now;

    if (bgpstream_resource_mgr_push(BSDI_GET_RES_MGR(di),
                                    BGPSTREAM_TRANSPORT_FILE,
                                    BGPSTREAM_FORMAT_MRT,
                                    STATE->update_file,
                                    STATE->last_update_filetime,
                                    UPDATE_FREQUENCY_CHECK,
                                    "singlefile",
                                    "singlefile",
                                    BGPSTREAM_UPDATE) == NULL) {
      goto err;
    }
  }

  return 0;

 err:
  return -1;
}
