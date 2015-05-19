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

#include "bgpstream_datasource_sqlite.h"
#include "bgpstream_debug.h"

#include <inttypes.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errmsg.h>


struct struct_bgpstream_sqlite_datasource_t {

  /* sqlite connection handler */
  sqlite3 *db;

};


bgpstream_sqlite_datasource_t *
bgpstream_sqlite_datasource_create(bgpstream_filter_mgr_t *filter_mgr,
                                    char * sqlite_file)
{
  return NULL;
}

int
bgpstream_sqlite_datasource_update_input_queue(bgpstream_sqlite_datasource_t* sqlite_ds,
                                                bgpstream_input_mgr_t *input_mgr)
{
  return 0;
}

void
bgpstream_sqlite_datasource_destroy(bgpstream_sqlite_datasource_t* sqlite_ds)
{
  return;
}

