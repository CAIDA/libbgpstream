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

#include "config.h"
#include "corsaro_int.h"

#include <assert.h>
#include <stdarg.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <wandio.h>

#include "wandio_utils.h"

#include "corsaro_file.h"
#include "corsaro_log.h"

/* fix for solaris (data-login.sdsc.edu) */
#if (defined (__SVR4) && defined (__sun))
extern int vasprintf(char **, const char *, __va_list);
#endif

/** The string that is assumed to be at the start of any corsaro ASCII file */
#define CORSARO_FILE_ASCII_CHECK      "# CORSARO"

/** The string to prefix file names with when creating trace files */
#define CORSARO_FILE_TRACE_FORMAT "pcapfile:"

corsaro_file_compress_t corsaro_file_detect_compression(char *filename)
{
  return wandio_detect_compression_type(filename);
}

corsaro_file_t *corsaro_file_open(corsaro_t *corsaro,
			      const char *filename,
			      corsaro_file_mode_t mode,
			      corsaro_file_compress_t compress_type,
			      int compress_level,
			      int flags)
{
  corsaro_file_t *f = NULL;

  size_t flen, rlen, len;
  char *ptr, *traceuri;

  if((f = malloc(sizeof(corsaro_file_t))) == NULL)
    {
      corsaro_log(__func__, corsaro, "could not malloc new corsaro_file_t");
      return NULL;
    }

  f->mode = mode;

  /* did they ask for a libtrace file? */
  switch(mode)
    {
    case CORSARO_FILE_MODE_TRACE:
      flen = strlen(CORSARO_FILE_TRACE_FORMAT);
      rlen = strlen(filename);
      len = flen+rlen+1;
      if((ptr = traceuri = malloc(len)) == NULL)
	{
	  corsaro_log(__func__, corsaro, "could not malloc traceuri");
	  return NULL;
	}
      strncpy(traceuri, CORSARO_FILE_TRACE_FORMAT, flen);
      ptr += flen;
      strncpy(ptr, filename, rlen);
      traceuri[len-1] = '\0';
      f->trace_io = trace_create_output(traceuri);
      free(traceuri);

      if (trace_is_err_output(f->trace_io))
	{
	  corsaro_log(__func__, corsaro, "trace_create_output failed for %s",
		    filename);
	  return NULL;
	}
      if(trace_config_output(f->trace_io, TRACE_OPTION_OUTPUT_COMPRESS,
			     &compress_level) ||
	 trace_config_output(f->trace_io, TRACE_OPTION_OUTPUT_COMPRESSTYPE,
			     &compress_type) != 0)
	{
	  corsaro_log(__func__, corsaro,
		    "could not set compression levels for trace");
	  return NULL;
	}
      if (trace_start_output(f->trace_io) == -1) {
	corsaro_log(__func__, corsaro, "trace_start_output failed for %s",
		  filename);
	return NULL;
      }
      /* trace is configured! */
      break;

    case CORSARO_FILE_MODE_ASCII:
    case CORSARO_FILE_MODE_BINARY:
      if((f->wand_io = wandio_wcreate(filename, compress_type,
				      compress_level, flags)) == NULL)
	{
	  corsaro_log(__func__, corsaro, "wandio could not create file %s",
		    filename);
	  free(f);
	  return NULL;
	}
      break;

    default:
      corsaro_log(__func__, corsaro, "invalid file mode %d", mode);
      free(f);
      return NULL;
    }

  return f;
}

off_t corsaro_file_write(corsaro_t *corsaro,
		       corsaro_file_t *file, const void *buffer, off_t len)
{
  /* let's not try and write raw bytes to a libtrace file... */
  assert(file->mode == CORSARO_FILE_MODE_ASCII ||
	 file->mode == CORSARO_FILE_MODE_BINARY ||
	 file->mode == CORSARO_FILE_MODE_UNKNOWN);
  assert(file->wand_io != NULL);

  return wandio_wwrite(file->wand_io, buffer, len);
}

off_t corsaro_file_write_packet(corsaro_t *corsaro,
			      corsaro_file_t *file, libtrace_packet_t *packet)
{
  uint8_t *pkt_buf = NULL;
  libtrace_linktype_t linktype;

  switch(file->mode)
    {
    case CORSARO_FILE_MODE_ASCII:
      assert(file->wand_io != NULL);
#ifdef HAVE_LIBPACKETDUMP
      corsaro_log(__func__, corsaro,
		"libpacketdump currently does not support dumping "
		"to a file");
      return 0;
#else
      corsaro_log(__func__, corsaro,
		"corsaro must be built with libpacketdump to dump "
		"a packet to ASCII");
      return 0;
#endif
      break;

    case CORSARO_FILE_MODE_BINARY:
      assert(file->wand_io != NULL);
      if((pkt_buf = trace_get_packet_buffer(packet,
					    &linktype, NULL)) == NULL)
	{
	  corsaro_log(__func__, corsaro, "could not get packet buffer");
	  return -1;
	}
      return corsaro_file_write(corsaro, file, pkt_buf,
			      trace_get_capture_length(packet));

    case CORSARO_FILE_MODE_TRACE:
        assert(file->trace_io != NULL);
      return trace_write_packet(file->trace_io, packet);

    default:
      corsaro_log(__func__, corsaro, "invalid corsaro file mode %d", file->mode);
      return -1;
    }

  return -1;
}

off_t corsaro_file_vprintf(corsaro_t *corsaro, corsaro_file_t *file,
			 const char *format, va_list args)
{
  /* let's not try and print text to a libtrace file... */
  assert(file != NULL);
  assert(file->mode == CORSARO_FILE_MODE_ASCII ||
	 file->mode == CORSARO_FILE_MODE_BINARY ||
	 file->mode == CORSARO_FILE_MODE_UNKNOWN);
  assert(file->wand_io != NULL);

  return wandio_vprintf(file->wand_io, format, args);

}

off_t corsaro_file_printf(corsaro_t *corsaro,
			corsaro_file_t *file, const char *format, ...)
{
  va_list ap;

  va_start(ap, format);
  return corsaro_file_vprintf(corsaro, file, format, ap);
  va_end(ap);
}

void corsaro_file_flush(corsaro_t *corsaro, corsaro_file_t *file)
{
  /* not supported by wandio? */
  return;
}

void corsaro_file_close(corsaro_t *corsaro, corsaro_file_t *file)
{
  switch(file->mode)
    {
    case CORSARO_FILE_MODE_ASCII:
    case CORSARO_FILE_MODE_BINARY:
      /* close the wandio object */
      assert(file->wand_io != NULL);
      wandio_wdestroy(file->wand_io);
      file->wand_io = NULL;
      break;

    case CORSARO_FILE_MODE_TRACE:
      assert(file->trace_io != NULL);
      trace_destroy_output(file->trace_io);
      file->trace_io = NULL;
      break;

    default:
      assert(0);
    }

  free(file);
  return;
}

corsaro_file_in_t *corsaro_file_ropen(const char *filename)
{
  corsaro_file_in_t *f = NULL;
  char buffer[1024];
  int len;

  /* 2013-01-22 AK has removed all of the logging output on failures this is
     because i dont want to need a corsaro_t object to open a file. but also
     because i think it should be up to the caller to log the errors. logs from
     this deep in corsaro just confuse people when somewhat common errors occur
     (file not found etc). */

  if((f = malloc(sizeof(corsaro_file_in_t))) == NULL)
    {
      return NULL;
    }

  /* we need to try and guess the mode... */
  /* if there is a : in the uri, we guess it is a libtrace file */
  /* this should be refined to do something more intelligent */
  if(strchr(filename, ':') != NULL)
    {
      f->mode = CORSARO_FILE_MODE_TRACE;

      /* open this as a trace file */
      f->trace_io = trace_create(filename);

      if(trace_is_err(f->trace_io))
	{
	  free(f);
	  return NULL;
	}

      if (trace_start(f->trace_io) == -1) {
	free(f);
	return NULL;
      }
      /* trace is set to go! */
      return f;
    }
  else
    {
      /* lets open the file and take a peek to see what we find */
      if((f->wand_io = wandio_create(filename)) == NULL)
	{
	  free(f);
	  return NULL;
	}

      len = wandio_peek(f->wand_io, buffer, sizeof(buffer));

      /* an ASCII corsaro file will start with "# CORSARO_VERSION" */
      if(len >= strlen(CORSARO_FILE_ASCII_CHECK) &&
	 memcmp(CORSARO_FILE_ASCII_CHECK, buffer,
		strlen(CORSARO_FILE_ASCII_CHECK)) == 0)
	{
	  f->mode = CORSARO_FILE_MODE_ASCII;
	}
      /* a binary corsaro file will start with an corsaro header "EDGRHEAD" but,
	 it is possible that an old binary corsaro file can just start with an
	 interval header - "EDGRINTR", so we will only look for "EDGR" */
      else if(len >= 4 && buffer[0] == 'E' && buffer[1] == 'D' &&
	      buffer[2] == 'G' && buffer[3] == 'R')
	{
	  f->mode = CORSARO_FILE_MODE_BINARY;
	}
      else
	{
	  /* who knows, but maybe someone wants to read a non-corsaro file */
	  f->mode = CORSARO_FILE_MODE_UNKNOWN;
	}
    }

  return f;
}

off_t corsaro_file_rread(corsaro_file_in_t *file, void *buffer, off_t len)
{
  /* refuse to read from a libtrace file */
  assert(file->mode == CORSARO_FILE_MODE_ASCII ||
	 file->mode == CORSARO_FILE_MODE_BINARY ||
	 file->mode == CORSARO_FILE_MODE_UNKNOWN);
  assert(file->wand_io != NULL);

  return wandio_read(file->wand_io, buffer, len);
}

off_t corsaro_file_rgets(corsaro_file_in_t *file, void *buffer, off_t len)
{
  /* refuse to read from a libtrace file */
  assert(file != NULL);
  assert(file->mode == CORSARO_FILE_MODE_ASCII ||
	 file->mode == CORSARO_FILE_MODE_BINARY ||
	 file->mode == CORSARO_FILE_MODE_UNKNOWN);
  assert(file->wand_io != NULL);

  /* just hand off to the helper func in wandio_utils (no chomp) */
  return wandio_fgets(file->wand_io, buffer, len, 0);
}

off_t corsaro_file_rread_packet(corsaro_file_in_t *file,
				libtrace_packet_t *packet,
				uint16_t len)
{
  uint8_t *pktbuf;

  switch(file->mode)
    {
    case CORSARO_FILE_MODE_BINARY:
      if((pktbuf = malloc(len)) == NULL)
	{
	  fprintf(stderr, "could not malloc the packet buffer\n");
	  return -1;
	}
      if(wandio_read(file->wand_io, pktbuf, len) != len)
	{
	  fprintf(stderr, "could not read packet into buffer\n");
	  return -1;
	}
      trace_construct_packet(packet, TRACE_TYPE_ETH,
			     pktbuf, len);
      return len;
      break;

    case CORSARO_FILE_MODE_TRACE:
      return trace_read_packet(file->trace_io, packet);
      break;

    case CORSARO_FILE_MODE_ASCII:
    case CORSARO_FILE_MODE_UNKNOWN:
      /* refuse to read a packet from an ascii file */
      /* this is a design flaw in the code if we get here */
      assert(1);
      return -1;
    }

  return -1;
}

off_t corsaro_file_rpeek(corsaro_file_in_t *file, void *buffer, off_t len)
{
  /* refuse to read from a libtrace file */
  assert(file->mode == CORSARO_FILE_MODE_ASCII ||
	 file->mode == CORSARO_FILE_MODE_BINARY ||
	 file->mode == CORSARO_FILE_MODE_UNKNOWN);
  assert(file->wand_io != NULL);

  return wandio_peek(file->wand_io, buffer, len);
}

off_t corsaro_file_rseek(corsaro_file_in_t *file, off_t offset, int whence)
{
  /* refuse to read from a libtrace file */
  assert(file->mode == CORSARO_FILE_MODE_ASCII ||
	 file->mode == CORSARO_FILE_MODE_BINARY ||
	 file->mode == CORSARO_FILE_MODE_UNKNOWN);
  assert(file->wand_io != NULL);

  return wandio_seek(file->wand_io, offset, whence);
}

off_t corsaro_file_rtell(corsaro_file_in_t *file)
{
  /* refuse to read from a libtrace file */
  assert(file->mode == CORSARO_FILE_MODE_ASCII ||
	 file->mode == CORSARO_FILE_MODE_BINARY ||
	 file->mode == CORSARO_FILE_MODE_UNKNOWN);
  assert(file->wand_io != NULL);

  return wandio_tell(file->wand_io);
}

void corsaro_file_rclose(corsaro_file_in_t *file)
{
  switch(file->mode)
    {
    case CORSARO_FILE_MODE_ASCII:
    case CORSARO_FILE_MODE_BINARY:
    case CORSARO_FILE_MODE_UNKNOWN:
      /* close the wandio object */
      if(file->wand_io != NULL)
	{
	  wandio_destroy(file->wand_io);
	  file->wand_io = NULL;
	}
      /* just for sanity */
      file->trace_io = NULL;
      break;

    case CORSARO_FILE_MODE_TRACE:
      if(file->trace_io != NULL)
	{
	  trace_destroy(file->trace_io);
	  file->trace_io = NULL;
	}
      /* just for sanity */
      file->wand_io = NULL;
      break;
            
    default:
      /* do nothing - it was already freed? */
      /* leave the assert here for when people are debugging */
      assert(0);
      return;
    }
    /* this will prevent an idiot from using this object again */
  file->mode = -1;

  free(file);

  return;
}
