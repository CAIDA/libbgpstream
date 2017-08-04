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

#include "bgpstream_format.h"
#include "bgpstream_record_int.h"
#include "bgpstream_log.h"
#include "bgpstream_resource.h"
#include "bgpstream_transport.h"
#include "utils.h"
#include <assert.h>

#include "bs_format_mrt.h"
#if 0
#include "bs_format_bmp.h"
#endif

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

#if 0
  bs_format_bmp_create,
#else
  NULL,
#endif

};

bgpstream_format_t *bgpstream_format_create(bgpstream_resource_t *res,
                                            bgpstream_filter_mgr_t *filter_mgr)
{
  bgpstream_format_t *format = NULL;

  // check that the format type is valid
  if ((int)res->format_type >= ARR_CNT(create_functions)) {
    bgpstream_log(BGPSTREAM_LOG_ERR,
                  "Invalid format module for %s (ID: %d)",
                  res->uri, res->format_type);
    goto err;
  }

  // check that the format is enabled
  if (create_functions[res->format_type] == NULL) {
    bgpstream_log(BGPSTREAM_LOG_ERR,
                  "Could not find format module for %s (ID: %d)",
                  res->uri, res->format_type);
    goto err;
  }

  // create the empty instance object
  if ((format = malloc_zero(sizeof(bgpstream_format_t))) == NULL) {
    goto err;
  }

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
  assert(record->__format_data->data == NULL);
  record->__format_data->format = format;
  return format->populate_record(format, record);
}

#define DATA(record) ((record)->__format_data)

void bgpstream_format_destroy_data(bgpstream_record_t *record)
{
  if (record == NULL || DATA(record)->format == NULL) {
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
