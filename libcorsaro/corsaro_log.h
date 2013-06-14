/* 
 * corsaro
 *
 * Alistair King, CAIDA, UC San Diego
 * corsaro-info@caida.org
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

#ifndef __CORSARO_LOG_H
#define __CORSARO_LOG_H

#include "config.h"

#include <stdarg.h>

#include "corsaro_int.h"
#include "corsaro_file.h"

/** @file
 *
 * @brief Header file dealing with the corsaro logging sub-system
 *
 * @author Alistair King
 *
 */

/** Write a formatted string to the logfile associated with an corsaro object
 *
 * @param func         The name of the calling function (__func__)
 * @param corsaro        The corsaro output object to log for
 * @param format       The printf style formatting string
 * @param args         Variable list of arguments to the format string
 *
 * This function takes the same style of arguments that printf(3) does.
 */
void corsaro_log_va(const char *func, corsaro_t *corsaro, 
		  const char *format, va_list args);

/** Write a formatted string to the logfile associated with an corsaro object
 *
 * @param func         The name of the calling function (__func__)
 * @param corsaro        The corsaro output object to log for
 * @param format       The printf style formatting string
 * @param ...          Variable list of arguments to the format string
 *
 * This function takes the same style of arguments that printf(3) does.
 */
void corsaro_log(const char *func, corsaro_t *corsaro, const char *format, ...);

/** Write a formatted string to the logfile associated with an corsaro input object
 *
 * @param func         The name of the calling function (__func__)
 * @param corsaro        The corsaro input object to log for
 * @param format       The printf style formatting string
 * @param ...          Variable list of arguments to the format string
 *
 * This function takes the same style of arguments that printf(3) does.
 */
void corsaro_log_in(const char *func, corsaro_in_t *corsaro, const char *format, ...);

/** Write a formatted string to a generic log file
 *
 * @param func         The name of the calling function (__func__)
 * @param file         The file to log to (STDERR if NULL is passed)
 * @param format       The printf style formatting string
 * @param ...          Variable list of arguments to the format string
 *
 * This function takes the same style of arguments that printf(3) does.
 */
void corsaro_log_file(const char *func, corsaro_file_t *logfile, 
		    const char *format, ...);

/** Initialize the logging sub-system for an corsaro output object
 *
 * @param corsaro        The corsaro object to associate the logger with
 * @return 0 if successful, -1 if an error occurs
 */
int corsaro_log_init(corsaro_t *corsaro);

/** Initialize the logging sub-system for an corsaro input object
 *
 * @param corsaro        The corsaro object to associate the logger with
 * @return 0 if successful, -1 if an error occurs
 */
int corsaro_log_in_init(corsaro_in_t *corsaro);

/** Close the log file for an corsaro output object
 *
 * @param corsaro         The corsaro output object to close logging for
 */
void corsaro_log_close(corsaro_t *corsaro);

/** Close the log file for an corsaro input object
 *
 * @param corsaro         The corsaro output object to close logging for
 */
void corsaro_log_in_close(corsaro_in_t *corsaro);

#endif /* __CORSARO_LOG_H */
