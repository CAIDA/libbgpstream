/*
 * This file is part of bgpstream
 *
 * Copyright (C) 2015 The Regents of the University of California.
 * Authors: Alistair King, Chiara Orsini
 *
 * All rights reserved.
 *
 * This code has been developed by CAIDA at UC San Diego.
 * For more information, contact bgpstream-info@caida.org
 *
 * This source code is proprietary to the CAIDA group at UC San Diego and may
 * not be redistributed, published or disclosed without prior permission from
 * CAIDA.
 *
 * Report any bugs, questions or comments to bgpstream-info@caida.org
 *
 */

#ifndef _BGPSTREAM_INT_H
#define _BGPSTREAM_INT_H

#include "bgpstream.h"

#include "bgpstream_datasource.h"
#include "bgpstream_input.h"
#include "bgpstream_reader.h"
#include "bgpstream_filter.h"


typedef enum {
  BGPSTREAM_STATUS_ALLOCATED,
  BGPSTREAM_STATUS_ON,
  BGPSTREAM_STATUS_OFF
} bgpstream_status;

struct struct_bgpstream_t {
  bgpstream_input_mgr_t *input_mgr;
  bgpstream_reader_mgr_t *reader_mgr;
  bgpstream_filter_mgr_t *filter_mgr;
  bgpstream_datasource_mgr_t *datasource_mgr;
  bgpstream_status status;
};



#endif /* _BGPSTREAM_INT_H */
