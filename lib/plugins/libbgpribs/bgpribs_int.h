/*
 * bgpcorsaro
 *
 * Chiara Orsini, CAIDA, UC San Diego
 * corsaro-info@caida.org
 *
 * Copyright (C) 2014 The Regents of the University of California.
 *
 * This file is part of bgpcorsaro.
 *
 * bgpcorsaro is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * bgpcorsaro is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with bgpcorsaro.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef __BGPRIBS_INT_H
#define __BGPRIBS_INT_H


#include "config.h"
#ifdef WITH_BGPWATCHER
#include "bgpribs_bgpwatcher_client.h"
#endif


#include "bgpribs_collectors_table.h"

struct bgpribs {
  int interval_start;                            /// interval start time 
  int interval_end;                              /// interval end time 
  int interval_processing_start;                 /// local time when a new interval is started
  collectors_table_wrapper_t *collectors_table;  /// set of collectors to manage
#ifdef WITH_BGPWATCHER
  bw_client_t *bw_client;
#endif
};

#endif /* __BGPRIBS_INT_H */
