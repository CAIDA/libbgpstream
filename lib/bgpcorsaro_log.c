/*
 * corsaro
 *
 * Alistair King, CAIDA, UC San Diego
 * corsaro-info@caida.org
 *
 * corsaro_log and timestamp_str functions adapted from scamper:
 *   http://www.wand.net.nz/scamper
 *
 * Copyright (C) 2012 The Regents of the University of California.
 *
 * This file is part of corsaro.
 *
 * corsaro is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * corsaro is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with corsaro.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "config.h"
#include "corsaro_int.h"

#include <assert.h>
#include <stdarg.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_TIME_H
#include <time.h>
#endif

#include "corsaro_file.h"
#include "corsaro_io.h"
#include "utils.h"

#include "corsaro_log.h"

static char *timestamp_str(char *buf, const size_t len)
{
  struct timeval  tv;
  struct tm      *tm;
  int             ms;
  time_t          t;

  buf[0] = '\0';
  gettimeofday_wrap(&tv);
  t = tv.tv_sec;
  if((tm = localtime(&t)) == NULL) return buf;

  ms = tv.tv_usec / 1000;
  snprintf(buf, len, "[%02d:%02d:%02d:%03d] ",
	   tm->tm_hour, tm->tm_min, tm->tm_sec, ms);

  return buf;
}

void generic_log(const char *func, corsaro_file_t *logfile, const char *format,
		 va_list ap)
{
  char     message[512];
  char     ts[16];
  char     fs[64];

  assert(format != NULL);

  vsnprintf(message, sizeof(message), format, ap);

  timestamp_str(ts, sizeof(ts));

  if(func != NULL) snprintf(fs, sizeof(fs), "%s: ", func);
  else             fs[0] = '\0';

  if(logfile == NULL)
    {
      fprintf(stderr, "%s%s%s\n", ts, fs, message);
      fflush(stderr);
    }
  else
    {
      /* we're special. we know that corsaro_file_printf can do without corsaro */
      corsaro_file_printf(NULL, logfile, "%s%s%s\n", ts, fs, message);
      corsaro_file_flush(NULL, logfile);

#ifdef DEBUG
      /* we've been asked to dump debugging information */
      fprintf(stderr, "%s%s%s\n", ts, fs, message);
      fflush(stderr);
#endif
    }
}

void corsaro_log_va(const char *func, corsaro_t *corsaro,
		  const char *format, va_list args)
{
  corsaro_file_t *lf = (corsaro == NULL) ? NULL : corsaro->logfile;
  generic_log(func, lf, format, args);
}

void corsaro_log(const char *func, corsaro_t *corsaro, const char *format, ...)
{
  va_list ap;
  va_start(ap, format);
  corsaro_log_va(func, corsaro, format, ap);
  va_end(ap);
}

void corsaro_log_in(const char *func, corsaro_in_t *corsaro,
		    const char *format, ...)
{
#ifdef DEBUG
  va_list ap;
  va_start(ap, format);
  generic_log(func, NULL, format, ap);
  va_end(ap);
#endif
}

void corsaro_log_file(const char *func, corsaro_file_t *logfile,
		      const char *format, ...)
{
  va_list ap;
  va_start(ap, format);
  generic_log(func, logfile, format, ap);
  va_end(ap);
}

int corsaro_log_init(corsaro_t *corsaro)
{
  if((corsaro->logfile = corsaro_io_prepare_file_full(corsaro,
				    CORSARO_IO_LOG_NAME,
				    &corsaro->interval_start,
				    CORSARO_FILE_MODE_ASCII,
				    CORSARO_FILE_COMPRESS_NONE,
				    0, O_CREAT)) == NULL)
    {
      fprintf(stderr, "could not open log for writing\n");
      return -1;
    }
  return 0;
}

int corsaro_log_in_init(corsaro_in_t *corsaro)
{
  /* nothing to do, corsaro_log_in only logs to stderr, and iff --enable-debug
     is passed to configure */
  return 0;
}

void corsaro_log_close(corsaro_t *corsaro)
{
  if(corsaro->logfile != NULL)
    {
      corsaro_file_close(NULL, corsaro->logfile);
      corsaro->logfile = NULL;
    }
}

void corsaro_log_in_close(corsaro_in_t *corsaro)
{
  /* nothing to be done */
}


