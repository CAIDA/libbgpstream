/*
 * libbgpstream
 *
 * Chiara Orsini, CAIDA, UC San Diego
 * chiara@caida.org
 *
 * Copyright (C) 2013 The Regents of the University of California.
 *
 * This file is part of libbgpstream.
 *
 * libbgpstream is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * libbgpstream is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with libbgpstream.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef _BGPSTREAM_OPTIONS_H
#define _BGPSTREAM_OPTIONS_H

typedef enum {BS_PROJECT, BS_COLLECTOR, BS_BGP_TYPE, BS_TIME_INTERVAL} bgpstream_filter_type;

typedef enum {BS_MYSQL, BS_CUSTOMLIST, BS_CSVFILE} bgpstream_datasource_type;

typedef enum {BS_MYSQL_DB, BS_MYSQL_USER, BS_MYSQL_HOST, BS_CSVFILE_FILE} bgpstream_datasource_option;


#endif /* _BGPSTREAM_OPTIONS_H */
