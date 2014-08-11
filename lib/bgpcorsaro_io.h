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

#ifndef __CORSARO_IO_H
#define __CORSARO_IO_H

#include "config.h"

#include "corsaro_int.h"

#include "corsaro_file.h"
#include "corsaro_plugin.h"

/** @file
 *
 * @brief Header file dealing with the corsaro file IO
 *
 * @author Alistair King
 *
 */

/** Length of the fixed part of the global corsaro header */
#define CORSARO_IO_HEADER_FIXED_BYTE_LEN (4+4+1+1+4+2)

/** Length of the interval header */
#define CORSARO_IO_INTERVAL_HEADER_BYTE_LEN sizeof(corsaro_interval_t)
/*(4+4+2+4)*/

/** Length of the corsaro trailer */
#define CORSARO_IO_TRAILER_BYTE_LEN sizeof(corsaro_trailer_t)
/* (4+4+8+8+8+4+4+4+4) */

/** The character to replace with the name of the plugin */
#define CORSARO_IO_PLUGIN_PATTERN  'P'
/** The pattern to replace in the output file name with the name of the plugin */
#define CORSARO_IO_PLUGIN_PATTERN_STR  "%P"

/** The character to replace with the monitor name */
#define CORSARO_IO_MONITOR_PATTERN 'N'
/** The pattern to replace in the output file name with monitor name */
#define CORSARO_IO_MONITOR_PATTERN_STR "%N"

/** The name to use for the global 'plugin' file */
#define CORSARO_IO_GLOBAL_NAME     "global"
/** The name to use for the log 'plugin' file */
#define CORSARO_IO_LOG_NAME        "log"

/** Uses the given settings to open an corsaro file for the given plugin
 *
 * @param corsaro          The corsaro object associated with the file
 * @param plugin_name    The name of the plugin (inserted into the template)
 * @param interval       The first interval start time represented in the file
 *                       (inserted into the template)
 * @param mode           The corsaro file mode to use
 * @param compress       The corsaro file compression type to use
 * @param compress_level The corsaro file compression level to use
 * @param flags          The flags to use when opening the file (e.g. O_CREAT)
 * @return A pointer to a new corsaro output file, or NULL if an error occurs
 */
corsaro_file_t *corsaro_io_prepare_file_full(corsaro_t *corsaro, 
					     const char *plugin_name,
					     corsaro_interval_t *interval,
					     corsaro_file_mode_t mode,
					     corsaro_file_compress_t compress,
					     int compress_level,
					     int flags);

/** Uses the current settings to open an corsaro file for the given plugin
 *
 * @param corsaro      The corsaro object associated with the file
 * @param plugin_name  The name of the plugin (inserted into the template)
 * @param interval     The first interval start time represented in the file
 *                     (inserted into the template)
 * @return A pointer to a new corsaro output file, or NULL if an error occurs
 */
corsaro_file_t *corsaro_io_prepare_file(corsaro_t* corsaro, 
					const char *plugin_name,
					corsaro_interval_t *interval);

/** Validates a output file template for needed features
 *
 * @param corsaro       The corsaro object associated with the template
 * @param template      The file template to be validated
 * @return 1 if the template is valid, 0 if it is invalid
 */
int corsaro_io_validate_template(corsaro_t *corsaro, char *template);

/** Determines whether there are any time-related patterns in the file template.
 *
 * @param corsaro       The corsaro object to check
 * @return 1 if there are time-related patterns, 0 if not
 */
int corsaro_io_template_has_timestamp(corsaro_t *corsaro);

/** Write the corsaro headers to the file
 *
 * @param corsaro        The corsaro object associated with the file
 * @param file           The corsaro output file to write to
 * @param header         The header to write out (NULL to generate one) 
 * @return The amount of data written, or -1 if an error occurs
 */
off_t corsaro_io_write_header(corsaro_t *corsaro, corsaro_file_t *file,
			      corsaro_header_t *header);

/** Write the corsaro headers to stdout
 *
 * @param plugin_manager  The plugin manager
 * @param header          The header to write out
 */
void corsaro_io_print_header(corsaro_plugin_manager_t *plugin_manager,
			     corsaro_header_t *header);

/** Write the corsaro trailers to the file
 *
 * @param corsaro        The corsaro object associated with the file
 * @param file           The corsaro output file to write to
 * @param trailer        The trailer to write out (NULL to generate one)
 * @return The amount of data written, or -1 if an error occurs
 */
off_t corsaro_io_write_trailer(corsaro_t *corsaro, corsaro_file_t *file,
			       corsaro_trailer_t *trailer);

/** Write the corsaro trailers to stdout
 *
 * @param trailer      The trailer to write out
 */
void corsaro_io_print_trailer(corsaro_trailer_t *trailer);

/** Write the appropriate interval headers to the file
 *
 * @param corsaro          The corsaro object associated with the file
 * @param file           The corsaro output file to write to
 * @param int_start      The start interval to write out
 * @return The amount of data written, or -1 if an error occurs
 */
off_t corsaro_io_write_interval_start(corsaro_t *corsaro, corsaro_file_t *file,
				    corsaro_interval_t *int_start);

/** Write the interval headers to stdout
 *
 * @param int_start      The start interval to write out
 */
void corsaro_io_print_interval_start(corsaro_interval_t *int_start);

/** Write the appropriate interval trailers to the file
 *
 * @param corsaro          The corsaro object associated with the file
 * @param file           The corsaro output file to write to
 * @param int_end        The end interval to write out
 * @return The amount of data written, or -1 if an error occurs
 */
off_t corsaro_io_write_interval_end(corsaro_t *corsaro, corsaro_file_t *file, 
				  corsaro_interval_t *int_end);

/** Write the interval trailers to stdout
 *
 * @param int_end      The end interval to write out
 */
void corsaro_io_print_interval_end(corsaro_interval_t *int_end);

/** Write the appropriate plugin header to the file
 *
 * @param corsaro          The corsaro object associated with the file
 * @param file           The corsaro output file to write to
 * @param plugin         The plugin object to write a start record for
 * @return The amount of data written, or -1 if an error occurs
 */
off_t corsaro_io_write_plugin_start(corsaro_t *corsaro, corsaro_file_t *file, 
				    corsaro_plugin_t *plugin);

/** Write the appropriate plugin trailer to the file
 *
 * @param corsaro        The corsaro object associated with the file
 * @param file           The corsaro output file to write to
 * @param plugin         The plugin object to write an end record for
 * @return The amount of data written, or -1 if an error occurs
 */
off_t corsaro_io_write_plugin_end(corsaro_t *corsaro, corsaro_file_t *file, 
				  corsaro_plugin_t *plugin); 

/** Write a generic corsaro record to the file
 *
 * @param corsaro          The corsaro object associated with the file
 * @param file           The corsaro output file to write to
 * @param record_type    The type of the record
 * @param record         The record to be written
 * @return The amount of data written, or -1 if an error occurs
 */
off_t corsaro_io_write_record(corsaro_t *corsaro, corsaro_file_t *file,
			    corsaro_in_record_type_t record_type,
			    corsaro_in_record_t *record);

/** Print a generic corsaro record to stdout
 *
 * @param plugin_manager The plugin manager associated with the record
 * @param record_type    The type of the record
 * @param record         The record to be written
 * @return 0 if the record_type was recognized, -1 if an error occurs
 */
int corsaro_io_print_record(corsaro_plugin_manager_t *plugin_manager,
			    corsaro_in_record_type_t record_type,
			    corsaro_in_record_t *record);

/** Read an corsaro header from the file
 *
 * @param corsaro           The corsaro object associated with the file
 * @param file              The corsaro input file to read from
 * @param[out] record_type  The record type read from the file
 * @param[out] record       A record object to read into
 * @return The amount of data read, or -1 if an error occurs
 */
off_t corsaro_io_read_header(corsaro_in_t *corsaro, corsaro_file_in_t *file,
			   corsaro_in_record_type_t *record_type,
			   corsaro_in_record_t *record);

/** Read the corsaro trailers from the file
 *
 * @param corsaro           The corsaro object associated with the file
 * @param file              The corsaro input file to read from
 * @param[out] record_type  The record type read from the file
 * @param[out] record       A record object to read into
 * @return The amount of data read, or -1 if an error occurs
 */
off_t corsaro_io_read_trailer(corsaro_in_t *corsaro, corsaro_file_in_t *file,
			    corsaro_in_record_type_t *record_type,
			    corsaro_in_record_t *record);

/** Read the appropriate interval headers from the file
 *
 * @param corsaro          The corsaro object associated with the file
 * @param file           The corsaro input file to read from
 * @param[out] record_type  The record type read from the file
 * @param[out] record       A record object to read into
 * @return The amount of data read, or -1 if an error occurs
 */
off_t corsaro_io_read_interval_start(corsaro_in_t *corsaro, 
				     corsaro_file_in_t *file,
				     corsaro_in_record_type_t *record_type,
				     corsaro_in_record_t *record);

/** Read the appropriate interval trailers from the file
 *
 * @param corsaro          The corsaro object associated with the file
 * @param file           The corsaro input file to read from
 * @param[out] record_type  The record type read from the file
 * @param[out] record       A record object to read into
 * @return The amount of data read, or -1 if an error occurs
 */
off_t corsaro_io_read_interval_end(corsaro_in_t *corsaro, 
				   corsaro_file_in_t *file,
				 corsaro_in_record_type_t *record_type,
				 corsaro_in_record_t *record);

/** Read the appropriate plugin header from the file
 *
 * @param corsaro           The corsaro object associated with the file
 * @param file              The corsaro input file to read from
 * @param[out] record_type  The record type read from the file
 * @param[out] record       A record object to read into
 * @return The amount of data read, or -1 if an error occurs
 */
off_t corsaro_io_read_plugin_start(corsaro_in_t *corsaro, 
				   corsaro_file_in_t *file, 
				   corsaro_in_record_type_t *record_type,
				   corsaro_in_record_t *record);

/** Read the appropriate plugin trailer from the file
 *
 * @param corsaro          The corsaro object associated with the file
 * @param file           The corsaro input file to read from
 * @param[out] record_type  The record type read from the file
 * @param[out] record       A record object to read into
 * @return The amount of data read, or -1 if an error occurs
 */
off_t corsaro_io_read_plugin_end(corsaro_in_t *corsaro, 
				 corsaro_file_in_t *file, 
				 corsaro_in_record_type_t *record_type,
				 corsaro_in_record_t *record); 

/** Read the given number of bytes into the record
 *
 * @param corsaro          The corsaro object to read from
 * @param record         The record to read into
 * @param len            The number of bytes to read
 * @return               The number of bytes to read, 0 if EOF, -1 on error
 */
off_t corsaro_io_read_bytes(corsaro_in_t *corsaro, corsaro_in_record_t *record,
			  off_t len);

/** Read the given number of bytes into the record buffer at the given offset
 *
 * @param corsaro        The corsaro object to read from
 * @param record         The record to read into
 * @param offset         The offset into the record buffer to read data to
 * @param len            The number of bytes to read
 * @return               The number of bytes to read, 0 if EOF, -1 on error
 *
 * This function can be useful to store data that a record *points to* without
 * actually having to malloc memory. Beware that the record buffer is a fixed
 * size so don't use this for massive objects. Also remember to update the 
 * pointer in the record object to this data.
 */
off_t corsaro_io_read_bytes_offset(corsaro_in_t *corsaro, 
				   corsaro_in_record_t *record,
				   off_t offset, off_t len);

#endif /* __CORSARO_IO_H */
