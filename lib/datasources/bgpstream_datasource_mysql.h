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

#ifndef _BGPSTREAM_DATASOURCE_MYSQL_H
#define _BGPSTREAM_DATASOURCE_MYSQL_H

#include "bgpstream_constants.h"
#include "bgpstream_input.h"
#include "bgpstream_filter.h"

#include <stdlib.h>
#include <stdio.h>
#include <mysql.h>


/** Opaque handle that represents the mysql data source */
typedef struct struct_bgpstream_mysql_datasource_t bgpstream_mysql_datasource_t;

bgpstream_mysql_datasource_t *
bgpstream_mysql_datasource_create(bgpstream_filter_mgr_t *filter_mgr,
                                  char * mysql_dbname,
                                  char * mysql_user,
                                  char * mysql_password,
                                  char * mysql_host,
                                  unsigned int mysql_port,
                                  char * mysql_socket,
                                  char * mysql_dump_path);


int
bgpstream_mysql_datasource_update_input_queue(bgpstream_mysql_datasource_t* mysql_ds,
                                              bgpstream_input_mgr_t *input_mgr);

void
bgpstream_mysql_datasource_destroy(bgpstream_mysql_datasource_t* mysql_ds);


#endif /* _BGPSTREAM_DATASOURCE_MYSQL_H */
