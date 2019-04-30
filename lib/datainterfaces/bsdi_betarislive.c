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
 */

#include "bsdi_betarislive.h"
#include "bgpstream_log.h"
#include "config.h"
#include "utils.h"
#include <assert.h>
#include <inttypes.h>
#include <string.h>
#include <wandio.h>

#define STATE (BSDI_GET_STATE(di, betarislive))

#define FIREHOSE_URL "https://ris-live.ripe.net/v1/stream/?format=json"
#define DEFAULT_CLIENT "libbgpstream-default"

/* ---------- START CLASS DEFINITION ---------- */

/* define the internal option ID values */
enum {
  OPTION_CLIENT,         // firehose client name
};

/* define the options this data interface accepts */
static bgpstream_data_interface_option_t options[] = {
  /* Firehose Client */
  {
    BGPSTREAM_DATA_INTERFACE_BETARISLIVE, // interface ID
    OPTION_CLIENT,                        // internal ID
    "client",                             // client name
    "client name for RIS-Live firehose stream (default: " DEFAULT_CLIENT ")",
  },
};

/* create the class structure for this data interface */
BSDI_CREATE_CLASS_FULL(
  betarislive, "beta-ris-stream", BGPSTREAM_DATA_INTERFACE_BETARISLIVE,
  "Read updates in real-time from the RIPE RIS live stream (BETA)",
  options);

/* ---------- END CLASS DEFINITION ---------- */

typedef struct bsdi_betarislive_state {
  /* user-provided options: */

  // RIS live firehose client name
  char *client_name;

  // RIS live firehose full url
  char *url;

  // we only ever yield one resource
  int done;

} bsdi_betarislive_state_t;

/* ========== PRIVATE METHODS BELOW HERE ========== */

static int build_url(bsdi_t *di)
{
  if(STATE->client_name == NULL){
    // assign default client name
    if((STATE->client_name = strdup(DEFAULT_CLIENT)) == NULL){
      bgpstream_log(BGPSTREAM_LOG_ERR,
                    "Could not assign default ris-live firehose client.");
      return -1;
    }
  }

  size_t needed = snprintf(NULL, 0, "%s&client=%s", FIREHOSE_URL, STATE->client_name) + 1;
  STATE->url = malloc(needed);
  int written = sprintf(STATE->url, "%s&client=%s", FIREHOSE_URL, STATE->client_name);
  if(needed != written+1){
    return -1;
  }
  return 0;
}


/* ========== PUBLIC METHODS BELOW HERE ========== */

int bsdi_betarislive_init(bsdi_t *di)
{
  bsdi_betarislive_state_t *state;

  if ((state = malloc_zero(sizeof(bsdi_betarislive_state_t))) == NULL) {
    goto err;
  }
  BSDI_SET_STATE(di, state);

  return 0;

err:
  bsdi_betarislive_destroy(di);
  return -1;
}

int bsdi_betarislive_start(bsdi_t *di)
{
  // our defaults are sufficient to run
  return 0;
}

int bsdi_betarislive_set_option(
  bsdi_t *di, const bgpstream_data_interface_option_t *option_type,
  const char *option_value)
{
  int i;
  int found = 0;

  switch (option_type->id) {
  case OPTION_CLIENT:
    free(STATE->client_name);
    if ((STATE->client_name = strdup(option_value)) == NULL) {
      return -1;
    }
    break;

  default:
    return -1;
  }

  return 0;
}

void bsdi_betarislive_destroy(bsdi_t *di)
{
  if (di == NULL || STATE == NULL) {
    return;
  }

  free(STATE->client_name);
  STATE->client_name = NULL;

  free(STATE->url);
  STATE->url = NULL;

  free(STATE);
  BSDI_SET_STATE(di, NULL);
}

int bsdi_betarislive_update_resources(bsdi_t *di)
{
  int rc;
  bgpstream_resource_t *res = NULL;

  // we only ever yield one resource
  if (STATE->done != 0) {
    return 0;
  }
  STATE->done = 1;

  // construct url
  if(build_url(di) != 0){
    return -1;
  }

  if ((rc = bgpstream_resource_mgr_push(
         BSDI_GET_RES_MGR(di), BGPSTREAM_RESOURCE_TRANSPORT_FILE,
         BGPSTREAM_RESOURCE_FORMAT_RIPEJSON, STATE->url,
         0,                   // indicate we don't know how much historical data there is
         BGPSTREAM_FOREVER,   // indicate that the resource is a "stream"
         "ris-live",          // fix project name to "ris-live"
         "",                  // leave collector unset
         BGPSTREAM_UPDATE,
         &res)) <= 0) {
    return rc;
  }
  assert(res != NULL);

  bgpstream_log(BGPSTREAM_LOG_INFO,
                "start streaming from %s",STATE->url);

  return 0;
}
