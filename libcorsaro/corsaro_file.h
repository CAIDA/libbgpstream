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

#ifndef __CORSARO_FILE_H
#define __CORSARO_FILE_H

#include "config.h"
#include "corsaro_int.h"

#include <fcntl.h>
#include <stdarg.h>

#include "libtrace.h"
#include "wandio.h"

/** @file
 *
 * @brief Header file dealing with the low-level file IO
 *
 * @author Alistair King
 *
 */

/** The default compression level 
 *
 * @todo make this an option to corsaro_main.c and \link corsaro_alloc_output
 * \endlink
 */
#define CORSARO_FILE_COMPRESS_LEVEL_DEFAULT 6

/** The suffix used to detect gzip output is desired */
#define CORSARO_FILE_ZLIB_SUFFIX   ".gz"

/** The suffix used to detect bzip output is desired */
#define CORSARO_FILE_BZ2_SUFFIX   ".bz2"

/** An opaque structure defining an corsaro output file */
typedef struct corsaro_file
{
  /** The requested output format for the file */
  corsaro_file_mode_t mode;

  /** Per-framework state for the file */
  union
  {
    /** ASCII & Binary mode state */
    struct
    {
      /** The wandio output file handle */
      iow_t *io;
    } ms_wandio;

    /** Trace mode state */
    struct
    {
      /** The libtrace object used to create the trace */
      libtrace_out_t *trace;
    } ms_trace;
  } mode_state;

} corsaro_file_t;

/** An opaque structure defining an corsaro input file */
typedef struct corsaro_file_in
{
  /** The requested/detected input format for the file */
  corsaro_file_mode_t mode;

  /** Per-framework state for the file */
  union
  {
    /** ASCII & Binary mode state */
    struct
    {
      /** The wandio input file handle */
      io_t *io;
    } ms_wandio;

    /** Trace mode state */
    struct
    {
      /** The libtrace object used to create the trace */
      libtrace_t *trace;
    } ms_trace;
  } mode_state;

} corsaro_file_in_t;

/** Accessor macro for getting the mode of a file */
/* this is what the 'public' should use to retrieve the mode */
#define CORSARO_FILE_MODE(file) (file->mode) 

/** Shortcut to the non-trace (wandio) state structure */
#define state_wandio   mode_state.ms_wandio
/** Shortcut to a non-trace io object */
#define wand_io       mode_state.ms_wandio.io

/** Shortcut to the trace state structure */
#define state_trace   mode_state.ms_trace
/** Shortcut to the libtrace object */
#define trace_io      mode_state.ms_trace.trace

/**
 * @name Corsaro file output API functions
 *
 * These are the functions that should be called by the plugins to open
 * and write to files with the corsaro IO sub-system.
 *
 * @{ */

/** Attempts to detect the type of compression for a file based on the suffix
 *
 * @param corsaro          The corsaro object the file is associated with
 * @param filename       The name of the file to check
 * @return the compression type to use, -1 if an error occurs
 */
corsaro_file_compress_t corsaro_file_detect_compression(struct corsaro *corsaro,
						    char *filename);

/** Creates a new corsaro file write and opens the provided file for writing.
 *
 * @param corsaro          The corsaro object the file is associated with
 * @param filename	 The name of the file to open
 * @param mode           The corsaro output mode to use when writing
 * @param compress_type  Compression type
 * @param compress_level The compression level to use when writing
 * @param flags          Flags to apply when opening the file, e.g. O_CREATE
 * @return A pointer to a new corsaro output file, or NULL if an error occurs
 */
corsaro_file_t *corsaro_file_open(struct corsaro *corsaro,
			      const char *filename, 
			      corsaro_file_mode_t mode,
			      corsaro_file_compress_t compress_type,
			      int compress_level,
			      int flags);

/** Writes the contents of a buffer using an corsaro output file
 *
 * @param corsaro          The corsaro object the file is associated with
 * @param file		The file to write the data to
 * @param buffer	The buffer to write out
 * @param len		The amount of writable data in the buffer
 * @return The amount of data written, or -1 if an error occurs
 */
off_t corsaro_file_write(struct corsaro *corsaro,
		       corsaro_file_t *file, const void *buffer, off_t len);


/** Write a libtrace packet to an corsaro output file
 *
 * @param corsaro          The corsaro object the file is associated with
 * @param file		The file to write the packet to
 * @param packet 	The packet to written
 * @return The amount of bytes written, 0 if EOF is reached, -1 if an error occurs
 *
 * This can be used on Corsaro Binary and Libtrace mode file to write a single
 * packet.
 */
off_t corsaro_file_write_packet(struct corsaro *corsaro,
			      corsaro_file_t *file, libtrace_packet_t *packet);

/** Print a string to an corsaro file
 *
 * @param corsaro         The corsaro object the file is associated with
 * @param file          The file to write to
 * @param format        The format string to write
 * @param args          The arguments to the format string
 * @return The amount of data written, or -1 if an error occurs
 *
 * The arguments for this function are the same as those for vprintf(3). See the
 * vprintf(3) manpage for more details.
 */
off_t corsaro_file_vprintf(struct corsaro *corsaro, corsaro_file_t *file, 
			 const char *format, va_list args);

/** Print a string to an corsaro file
 *
 * @param corsaro         The corsaro object the file is associated with
 * @param file          The file to write to
 * @param format        The format string to write
 * @param ...           The arguments to the format string
 * @return The amount of data written, or -1 if an error occurs
 *
 * The arguments for this function are the same as those for printf(3). See the
 * printf(3) manpage for more details.
 */
off_t corsaro_file_printf(struct corsaro *corsaro, corsaro_file_t *file, 
			const char *format, ...);

/** Force all buffered data for the file to be written out
 *
 * @param corsaro         The corsaro object the file is associated with
 * @param file          The file to flush
 */
void corsaro_file_flush(struct corsaro *corsaro, corsaro_file_t *file);

/** Closes an corsaro output file and frees the writer structure.
 *
 * @param corsaro         The corsaro object the file is associated with
 * @param file		The file to close
 */
void corsaro_file_close(struct corsaro *corsaro, corsaro_file_t *file);

/** @} */

/**
 * @name Corsaro file input API functions
 *
 * These are the functions that should be called by the plugins to open
 * and read from files with the corsaro IO sub-system.
 *
 * @todo create a corsaro_file_rreadline function?
 * @{ */

/** Creates a new corsaro file reader and opens the provided file for reading.
 *
 * @param filename	The name of the file to open
 * @return A pointer to a new corsaro input file, or NULL if an error occurs
 *
 * This function will use wandio/libtrace to attempt to detect the compression 
 * format used for given file (if any), provided that libtrace was built with 
 * the appropriate libraries. It will also attempt to detect the mode that was
 * used to write the file.
 */
corsaro_file_in_t *corsaro_file_ropen(const char *filename);

/** Reads from an corsaro input file into the provided buffer.
 *
 * @param file		The file to read from
 * @param buffer	The buffer to read into
 * @param len		The size of the buffer
 * @return The amount of bytes read, 0 if EOF is reached, -1 if an error occurs
 */
off_t corsaro_file_rread(corsaro_file_in_t *file, void *buffer, off_t len);

/** Reads a string from an corsaro input file into the provided buffer.
 *
 * @param file		The file to read from
 * @param buffer	The buffer to read into
 * @param len		The size of the buffer
 * @return The amount of bytes read, 0 if EOF is reached, -1 if an error occurs
 *
 * This function is almost identical to fgets(3), it will read at most one less
 * than len bytes from the file and store them in buffer. Reading stops after an
 * EOF or a newline. If a newline is read, it is stored in the buffer. A null
 * byte will also be stored after the last character in the buffer.
 */
off_t corsaro_file_rgets(corsaro_file_in_t *file, void *buffer, off_t len);

/** Read a libtrace packet from an corsaro input file
 *
 * @param file		The file to read from
 * @param packet 	The packet to read into
 * @param len		The size of the packet to be read
 * @return The amount of bytes read, 0 if EOF is reached, -1 if an error occurs
 *
 * This can be used on Corsaro Binary and Libtrace mode file to retrieve a single
 * packet. If the file is in trace mode, the len parameter is ignored.
 */
off_t corsaro_file_rread_packet(corsaro_file_in_t *file, 
				libtrace_packet_t *packet,
				uint16_t len);

/** Reads from an corsaro input file into the provided buffer, but does not
 * update the read pointer.
 *
 * @param file		The file to read from
 * @param buffer 	The buffer to read into
 * @param len		The size of the buffer
 * @return The amount of bytes read, 0 if EOF is reached, -1 if an error occurs
 */
off_t corsaro_file_rpeek(corsaro_file_in_t *file, void *buffer, off_t len);

/** Changes the read pointer offset to the specified value for an corsaro input
 * file.
 *
 * @param file		The file to adjust the read pointer for
 * @param offset	The new offset for the read pointer
 * @param whence	Indicates where to set the read pointer from. Can be 
 * 			one of SEEK_SET, SEEK_CUR or SEEK_END.
 * @return The new value for the read pointer, or -1 if an error occurs
 *
 * The arguments for this function are the same as those for lseek(2). See the
 * lseek(2) manpage for more details.
 */
off_t corsaro_file_rseek(corsaro_file_in_t *file, off_t offset, int whence);

/** Returns the current offset of the read pointer for an corsaro input file. 
 *
 * @param file		The file to get the read offset for
 * @return The offset of the read pointer, or -1 if an error occurs
 */
off_t corsaro_file_rtell(corsaro_file_in_t *file);

/** Closes an corsaro input file and frees the reader structure.
 *
 * @param file		The file to close
 */
void corsaro_file_rclose(corsaro_file_in_t *file);

/** @} */

#endif /* __CORSARO_FILE_H */
