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

#include "bgpstream_format.h"
#include "bgpstream_record_int.h"
#include "bgpstream_log.h"
#include "bgpstream_resource.h"
#include "bgpstream_transport.h"
#include "utils.h"
#include <assert.h>

#include "bs_format_bmp.h"
#include "bs_format_mrt.h"
#include "bs_format_rislive.h"

/** Convenience typedef for the format create function type */
typedef int (*format_create_func_t)(bgpstream_format_t *format,
                                    bgpstream_resource_t *res);

/** Array of format create functions.
 *
 * This MUST be kept in sync with the bgpstream_resource_format_type_t enum
 * (in bgpstream_resource.h)
 */
static const format_create_func_t create_functions[] = {

  bs_format_mrt_create,

  bs_format_bmp_create,

  bs_format_rislive_create,

};

bgpstream_format_t *bgpstream_format_create(bgpstream_resource_t *res,
                                            bgpstream_filter_mgr_t *filter_mgr)
{
  bgpstream_format_t *format = NULL;

  // check that the format type is valid
  if (res->format_type >= ARR_CNT(create_functions)) {
    bgpstream_log(BGPSTREAM_LOG_ERR, "Invalid format module for %s (ID: %d)",
                  res->uri, res->format_type);
    goto err;
  }

  // check that the format is enabled
  if (create_functions[res->format_type] == NULL) {
    bgpstream_log(BGPSTREAM_LOG_ERR,
                  "Could not find format module for %s (ID: %d)", res->uri,
                  res->format_type);
    goto err;
  }

  // create the empty instance object
  if ((format = malloc_zero(sizeof(bgpstream_format_t))) == NULL) {
    goto err;
  }

  format->res = res;

  // create the transport reader
  if ((format->transport = bgpstream_transport_create(res)) == NULL) {
    goto err;
  }

  format->filter_mgr = filter_mgr;

  if (create_functions[res->format_type](format, res) != 0) {
    goto err;
  }

  return format;

err:
  free(format);
  return NULL;
}

bgpstream_format_status_t
bgpstream_format_populate_record(bgpstream_format_t *format,
                                 bgpstream_record_t *record)
{
  // it is a programming error to use a record with a different format
  assert(record->__int->format == format);
  return format->populate_record(format, record);
}

int bgpstream_format_get_next_elem(bgpstream_format_t *format,
                                   bgpstream_record_t *record,
                                   bgpstream_elem_t **elem)
{
  assert(record->__int->format == format);
  *elem = NULL;
  return format->get_next_elem(format, record, elem);
}

#define DATA(record) ((record)->__int)

int bgpstream_format_init_data(bgpstream_record_t *record)
{
  return DATA(record)->format->init_data(DATA(record)->format,
                                         &DATA(record)->data);
}

void bgpstream_format_clear_data(bgpstream_record_t *record)
{
  if (record == NULL || DATA(record)->format == NULL) {
    assert(DATA(record)->data == NULL);
    return;
  }
  DATA(record)->format->clear_data(DATA(record)->format, DATA(record)->data);
}

void bgpstream_format_destroy_data(bgpstream_record_t *record)
{
  if (record == NULL || DATA(record)->format == NULL) {
    assert(DATA(record)->data == NULL);
    return;
  }
  DATA(record)->format->destroy_data(DATA(record)->format, DATA(record)->data);

  DATA(record)->format = NULL;
  DATA(record)->data = NULL;
}

void bgpstream_format_destroy(bgpstream_format_t *format)
{
  if (format == NULL) {
    return;
  }

  format->destroy(format);
  format->state = NULL;

  bgpstream_transport_destroy(format->transport);
  format->transport = NULL;

  free(format);
}
