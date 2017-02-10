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
 *
 * Some code adapted from http://c.learncodethehardway.org/book/ex20.html
 */

#ifndef _BGPSTREAM_LOG_H
#define _BGPSTREAM_LOG_H

#include <stdarg.h>

#define BGPSTREAM_LOG_ERR     0
#define BGPSTREAM_LOG_WARN   10
#define BGPSTREAM_LOG_INFO   20
#define BGPSTREAM_LOG_CONFIG 30
#define BGPSTREAM_LOG_FINE   40
#define BGPSTREAM_LOG_VFINE  50
#define BGPSTREAM_LOG_FINEST 60

// TODO: move this to configure (or even an API call)
#define BGPSTREAM_LOG_LEVEL BGPSTREAM_LOG_FINEST

#define bgpstream_log(level, ...) \
    do { \
	if ((level) <= BGPSTREAM_LOG_LEVEL) { \
	    bgpstream_log_func((level), __FILE__, __LINE__, __VA_ARGS__); \
	} \
    } while (0)

void bgpstream_log_func(int level, const char *file, int line,
                        const char * format, ...)
  __attribute__ ((format (printf, 4, 5)));

#endif /* _BGPSTREAM_DEBUG_H */
