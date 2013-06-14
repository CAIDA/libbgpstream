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
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#ifdef HAVE_SYS_TIME_H
#include <time.h>
#endif
#ifdef HAVE_TIME_H
#include <sys/time.h>
#endif

#include "corsaro.h"
#include "corsaro_file.h"
#include "corsaro_log.h"
#include "corsaro_plugin.h"
#include "utils.h"

/* plugin headers */
#ifdef WITH_PLUGIN_SIXT
#include "corsaro_flowtuple.h"
#endif
#ifdef WITH_PLUGIN_DOS
#include "corsaro_dos.h"
#endif

#include "corsaro_io.h"
#include "corsaro_plugin.h"

/** Structure with pointers to output functions for a mode */
typedef struct output_funcs
{
  int (*headers)(corsaro_t *corsaro, corsaro_file_t *file, 
		 corsaro_header_t *header);
  int (*interval_start)(corsaro_t *corsaro, corsaro_file_t *file,
			corsaro_interval_t *int_start);
  int (*interval_end)(corsaro_t *corsaro, corsaro_file_t *file, 
		      corsaro_interval_t *int_end);
  int (*plugin_start)(corsaro_t *corsaro, corsaro_file_t *file, 
		      corsaro_plugin_t *plugin);
  int (*plugin_end)(corsaro_t *corsaro, corsaro_file_t *file, 
		    corsaro_plugin_t *plugin);
  int (*trailers)(corsaro_t *corsaro, corsaro_file_t *file, 
		  corsaro_trailer_t *trailer);
} output_funcs_t;

#define GENERATE_OUTPUT_FUNCS(o)					\
  {output_headers_##o, output_interval_start_##o, output_interval_end_##o, \
      output_plugin_start_##o, output_plugin_end_##o, output_trailers_##o}

#define GENERATE_OUTPUT_FUNC_PROTOS(o)					\
  static int output_headers_##o(corsaro_t *corsaro, corsaro_file_t *file, \
				corsaro_header_t *header);		\
  static int output_interval_start_##o(corsaro_t *corsaro,		\
				       corsaro_file_t *file,		\
				       corsaro_interval_t *int_start);	\
  static int output_interval_end_##o(corsaro_t *corsaro,		\
				     corsaro_file_t *file,		\
				     corsaro_interval_t *int_end);	\
  static int output_plugin_start_##o(corsaro_t *corsaro, corsaro_file_t *file, \
				     corsaro_plugin_t *plugin);		\
  static int output_plugin_end_##o(corsaro_t *corsaro, corsaro_file_t *file, \
				   corsaro_plugin_t *plugin);		\
  static int output_trailers_##o(corsaro_t *corsaro, corsaro_file_t *file, \
				 corsaro_trailer_t *trailer);

GENERATE_OUTPUT_FUNC_PROTOS(ascii);
GENERATE_OUTPUT_FUNC_PROTOS(binary);

static output_funcs_t output_funcs[] = {
  GENERATE_OUTPUT_FUNCS(ascii),  /* CORSARO_FILE_MODE_ASCII */
  GENERATE_OUTPUT_FUNCS(binary), /* CORSARO_FILE_MODE_BINARY */
  {NULL},                        /* CORSARO_FILE_MODE_TRACE */
};


/*
 * print_headers
 *
 * prints the global corsaro details which appear at the head of the output
 * file
 */
static int output_headers_ascii(corsaro_t *corsaro, corsaro_file_t *file,
				corsaro_header_t *header)
{
  corsaro_plugin_t *tmp = NULL;
  uint32_t bytes_out = 0;

  corsaro_header_t *ph = NULL;
  corsaro_header_t h;

  int i;

  if(header == NULL)
    {
      h.version_major = CORSARO_MAJOR_VERSION;
      h.version_minor = CORSARO_MINOR_VERSION;
      h.local_init_time = corsaro->init_time.tv_sec;
      h.interval_length = corsaro->interval;
      h.traceuri = (uint8_t*)corsaro->uridata;

      ph = &h;
    }
  else
    {
      ph = header;
    }


  bytes_out += corsaro_file_printf(corsaro, file, 
				   "# CORSARO_VERSION %"PRIu8".%"PRIu8"\n", 
				   ph->version_major, ph->version_minor);
  bytes_out += corsaro_file_printf(corsaro, file, "# CORSARO_INITTIME %ld\n", 
				   ph->local_init_time);
  bytes_out += corsaro_file_printf(corsaro, file, "# CORSARO_INTERVAL %d\n", 
				   ph->interval_length);
  if(ph->traceuri != NULL)
    {
      bytes_out += corsaro_file_printf(corsaro, file, "# CORSARO_TRACEURI %s\n", 
				       ph->traceuri);
    }

  if(header == NULL)
    {
      while((tmp = corsaro_plugin_next(corsaro->plugin_manager, tmp)) != NULL)
	{
	  bytes_out += corsaro_file_printf(corsaro, file, 
					   "# CORSARO_PLUGIN %s\n", 
					   tmp->name);
	}
    }
  else
    {
      for(i=0;i<header->plugin_cnt;i++)
	{
	  bytes_out += corsaro_file_printf(corsaro, file, 
					   "# CORSARO_PLUGIN %s\n",
					   corsaro_plugin_get_name_by_magic(
									    corsaro->plugin_manager,
									    header->plugin_magics[i]
									    ));
	}
    }

  return bytes_out;
}

static int output_headers_binary(corsaro_t *corsaro, corsaro_file_t *file,
				 corsaro_header_t *header)
{
  corsaro_plugin_t *p = NULL;
  uint8_t fbuffer[CORSARO_IO_HEADER_FIXED_BYTE_LEN];
  uint8_t *ptr = &fbuffer[0];
  uint16_t tmp_len;
  uint8_t tmp[4];

  uint32_t bytes_out = 0;

  /* magic numbers */
  bytes_htonl(ptr, CORSARO_MAGIC);
  ptr+=4;
  bytes_htonl(ptr, CORSARO_MAGIC_HEADER);
  ptr+=4;

  /* version */
  *ptr = CORSARO_MAJOR_VERSION;
  ptr++;
  *ptr = CORSARO_MID_VERSION;
  ptr++;

  /* init time */
  bytes_htonl(ptr, corsaro->init_time.tv_sec);
  ptr+=4;

  /* interval length */
  bytes_htons(ptr, corsaro->interval);

  if(corsaro_file_write(corsaro, file, &fbuffer[0], 
			CORSARO_IO_HEADER_FIXED_BYTE_LEN) != 
     CORSARO_IO_HEADER_FIXED_BYTE_LEN)
    {
      corsaro_log(__func__, corsaro, "could not dump byte array to file");
      return -1;
    }

  bytes_out += CORSARO_IO_HEADER_FIXED_BYTE_LEN;

  /* the traceuri */
  if(corsaro->uridata != NULL)
    {
      tmp_len = strlen(corsaro->uridata);
      bytes_htons(&tmp[0], tmp_len);
      if(corsaro_file_write(corsaro, file, &tmp[0], 2) != 2)
	{
	  corsaro_log(__func__, corsaro, "could not dump traceuri length to file");
	  return -1;
	}
      if(corsaro_file_write(corsaro, file, corsaro->uridata, tmp_len) != tmp_len)
	{
	  corsaro_log(__func__, corsaro, "could not dump traceuri string to file");
	  return -1;
	}
      bytes_out += 2 + tmp_len;
    }
  else
    {
      memset(&tmp[0], 0, 2);
      if(corsaro_file_write(corsaro, file, &tmp[0], 2) != 2)
	{
	  corsaro_log(__func__, corsaro, "could not dump zero traceuri length to file");
	  return -1;
	}
      bytes_out += 2;
    }
  
  /* the plugin list */
  if(corsaro->plugin_manager->plugins_enabled != NULL)
    {
      bytes_htons(&tmp[0], corsaro->plugin_manager->plugins_enabled_cnt);
    }
  else
    {
      bytes_htons(&tmp[0], corsaro->plugin_manager->plugins_cnt);
    }

  if(corsaro_file_write(corsaro, file, &tmp[0], 2) != 2)
    {
      corsaro_log(__func__, corsaro, "could not dump plugins cnt to file");
      return -1;
    }
  bytes_out += 2;

  while((p = corsaro_plugin_next(corsaro->plugin_manager, p)) != NULL)
    {
      bytes_htonl(&tmp[0], p->magic);
      if(corsaro_file_write(corsaro, file, &tmp[0], 4) != 4)
	{
	  corsaro_log(__func__, corsaro, "could not dump plugin magic to file");
	  return -1;
	}
      bytes_out += 4;
    }

  return bytes_out;
}

/*
 * output_interval
 *
 * prints data for the interval which is about to be completed.
 */
static int output_interval_start_ascii(corsaro_t *corsaro, corsaro_file_t *file,
				       corsaro_interval_t *int_start)
{
  return corsaro_file_printf(corsaro, file, 
			     "# CORSARO_INTERVAL_START %d %ld\n", 
			     int_start->number, int_start->time);
}

static int output_interval_end_ascii(corsaro_t *corsaro, corsaro_file_t *file, 
				     corsaro_interval_t *int_end)
{
  return corsaro_file_printf(corsaro, file, 
			     "# CORSARO_INTERVAL_END %d %ld\n", 
			     int_end->number, 
			     int_end->time);
}

static int write_interval_header_binary(corsaro_t * corsaro, corsaro_file_t *file, 
					corsaro_interval_t *interval)
{
  corsaro_interval_t nint;

#if 0
  uint8_t ibuff[CORSARO_IO_INTERVAL_HEADER_BYTE_LEN];
  uint8_t *iptr = &ibuff[0];

  /* interval header */
  bytes_htonl(iptr, CORSARO_MAGIC);
  iptr+=4;
  bytes_htonl(iptr, CORSARO_MAGIC_INTERVAL);
  iptr+=4;
  bytes_htons(iptr, corsaro->interval_cnt);
  iptr+=2;
  bytes_htonl(iptr, tv.tv_sec);

  if(corsaro_file_write(corsaro, file, &ibuff[0], 
			CORSARO_IO_INTERVAL_HEADER_BYTE_LEN) != 
     CORSARO_IO_INTERVAL_HEADER_BYTE_LEN)
    {
      corsaro_log(__func__, corsaro, "could not dump interval header to file");
      return -1;
    }
  return CORSARO_IO_INTERVAL_HEADER_BYTE_LEN;
#endif

  /* byte flip all the fields */
  nint.corsaro_magic = htonl(interval->corsaro_magic);
  nint.magic = htonl(interval->magic);
  nint.number = htons(interval->number);
  nint.time = htonl(interval->time);
  
  if(corsaro_file_write(corsaro, file, &nint, sizeof(corsaro_interval_t)) !=
     sizeof(corsaro_interval_t))
    {
      corsaro_log(__func__, corsaro, "could not dump interval header to file");
      return -1;
    }

  return sizeof(corsaro_interval_t);
}

static int output_interval_start_binary(corsaro_t *corsaro, corsaro_file_t *file,
					corsaro_interval_t *int_start)
{
  return write_interval_header_binary(corsaro, file, int_start);
}

static int output_interval_end_binary(corsaro_t *corsaro, corsaro_file_t *file, 
				      corsaro_interval_t *int_end)
{
  return write_interval_header_binary(corsaro, file, int_end);
}

static int output_plugin_start_ascii(corsaro_t *corsaro, corsaro_file_t *file, 
				     corsaro_plugin_t *plugin)
{
  return corsaro_file_printf(corsaro, file, 
			     "# CORSARO_PLUGIN_DATA_START %s\n", 
			     plugin->name);
}

static int output_plugin_end_ascii(corsaro_t *corsaro, corsaro_file_t *file, 
				   corsaro_plugin_t *plugin)
{
  return corsaro_file_printf(corsaro, file, 
			     "# CORSARO_PLUGIN_DATA_END %s\n", 
			     plugin->name);
}

static int write_plugin_header_binary(corsaro_t *corsaro, corsaro_file_t *file, 
				      corsaro_plugin_t *plugin)
{
  corsaro_plugin_data_t data = {
    htonl(CORSARO_MAGIC),
    htonl(CORSARO_MAGIC_DATA),
    htonl(plugin->magic),
  };
 
  if(corsaro_file_write(corsaro, file, &data, 
			sizeof(corsaro_plugin_data_t)) != 
     sizeof(corsaro_plugin_data_t))
    {
      corsaro_log(__func__, corsaro, 
		  "could not dump interval data header to file");
      return -1;
    }
  return sizeof(corsaro_plugin_data_t);
}

static int output_plugin_start_binary(corsaro_t *corsaro, corsaro_file_t *file, 
				      corsaro_plugin_t *plugin)
{
  return write_plugin_header_binary(corsaro, file, plugin);
}

static int output_plugin_end_binary(corsaro_t *corsaro, corsaro_file_t *file, 
				    corsaro_plugin_t *plugin)
{
  return write_plugin_header_binary(corsaro, file, plugin);
}

/*
 * print_trailers
 *
 * prints the global corsaro details which appear at the tail of the output
 * file (when corsaro_finalize has been called)
 */
static int output_trailers_ascii(corsaro_t *corsaro, corsaro_file_t *file,
				 corsaro_trailer_t *trailer)
{
  struct timeval ts;
  uint32_t bytes_out = 0;

  uint64_t acnt = corsaro_get_accepted_packets(corsaro);
  uint64_t dcnt = corsaro_get_dropped_packets(corsaro);

  gettimeofday_wrap(&ts);

  bytes_out += corsaro_file_printf(corsaro, file, 
				   "# CORSARO_PACKETCNT %"PRIu64"\n", 
				   corsaro->packet_cnt);
  if(acnt != UINT64_MAX)
    {
      bytes_out += corsaro_file_printf(corsaro, file, 
				       "# CORSARO_ACCEPTEDCNT %"PRIu64"\n", 
				       acnt);
    }
  if(dcnt != UINT64_MAX)
    {
      bytes_out += corsaro_file_printf(corsaro, file, 
				       "# CORSARO_DROPPEDCNT %"PRIu64"\n", 
				       dcnt);
    }
  bytes_out += corsaro_file_printf(corsaro, file, "# CORSARO_FIRSTPKT %ld\n", 
				   corsaro->first_ts.tv_sec);
  bytes_out += corsaro_file_printf(corsaro, file, "# CORSARO_LASTPKT %ld\n", 
				   corsaro->last_ts.tv_sec);
  bytes_out += corsaro_file_printf(corsaro, file, "# CORSARO_FINALTIME %ld\n", 
				   ts.tv_sec);
  bytes_out += corsaro_file_printf(corsaro, file, "# CORSARO_RUNTIME %ld\n", 
				   ts.tv_sec-corsaro->init_time.tv_sec);

  corsaro_log(__func__, corsaro, "pkt cnt: %"PRIu64, corsaro->packet_cnt);
  
  return bytes_out;
}

static int output_trailers_binary(corsaro_t *corsaro, corsaro_file_t *file,
				  corsaro_trailer_t *trailer)
{
  uint8_t buff[CORSARO_IO_TRAILER_BYTE_LEN];
  uint8_t *ptr = &buff[0];

  struct timeval ts;
  gettimeofday_wrap(&ts);

  bytes_htonl(ptr, CORSARO_MAGIC);
  ptr+=4;

  bytes_htonl(ptr, CORSARO_MAGIC_TRAILER);
  ptr+=4;

  bytes_htonll(ptr, corsaro->packet_cnt);
  ptr+=8;

  bytes_htonll(ptr, corsaro_get_accepted_packets(corsaro));
  ptr+=8;
  
  bytes_htonll(ptr, corsaro_get_dropped_packets(corsaro));
  ptr+=8;

  bytes_htonl(ptr, corsaro->first_ts.tv_sec);
  ptr+=4;

  bytes_htonl(ptr, corsaro->last_ts.tv_sec);
  ptr+=4;

  bytes_htonl(ptr, ts.tv_sec);
  ptr+=4;

  bytes_htonl(ptr, ts.tv_sec-corsaro->init_time.tv_sec);

  if(corsaro_file_write(corsaro, file, &buff[0], CORSARO_IO_TRAILER_BYTE_LEN) != 
     CORSARO_IO_TRAILER_BYTE_LEN)
    {
      corsaro_log(__func__, corsaro, "could not dump trailers to file");
      return -1;
    }
  
  corsaro_log(__func__, corsaro, "pkt cnt: %"PRIu64, corsaro->packet_cnt);
  return CORSARO_IO_TRAILER_BYTE_LEN;
}

static char *stradd(const char *str, char *bufp, char *buflim)
{
  while(bufp < buflim && (*bufp = *str++) != '\0')
    {
      ++bufp;
    }
  return bufp;
}

static char *generate_file_name(corsaro_t *corsaro, const char *plugin,
				corsaro_interval_t *interval,
				corsaro_file_compress_t compress)
{
  /* some of the structure of this code is borrowed from the 
     FreeBSD implementation of strftime */

  /* the output buffer */
  /* @todo change the code to dynamically realloc this if we need more
     space */
  char buf[1024];
  char tbuf[1024];
  char *bufp = buf;
  char *buflim = buf+sizeof(buf);

  char *tmpl = corsaro->template;
  char secs[11]; /* length of UINT32_MAX +1 */
  struct timeval tv;

  for(; *tmpl; ++tmpl)
    {
      if(*tmpl == '.' && compress == CORSARO_FILE_COMPRESS_NONE)
	{
	  if(strncmp(tmpl, CORSARO_FILE_ZLIB_SUFFIX,
		     strlen(CORSARO_FILE_ZLIB_SUFFIX)) == 0 ||
	     strncmp(tmpl, CORSARO_FILE_BZ2_SUFFIX,
		     strlen(CORSARO_FILE_BZ2_SUFFIX)) == 0)
	    {
	      break;
	    }
	}
      else if(*tmpl == '%')
	{
	  switch(*++tmpl)
	    {
	    case '\0':
	      --tmpl;
	      break;

	      /* BEWARE: if you add a new pattern here, you must also add it to
	       * corsaro_io_template_has_timestamp */
	      
	    case CORSARO_IO_MONITOR_PATTERN:
	      bufp = stradd(corsaro->monitorname, bufp, buflim);
	      continue;

	    case CORSARO_IO_PLUGIN_PATTERN:
	      bufp = stradd(plugin, bufp, buflim);
	      continue;

	    case 's':
	      if(interval != NULL)
		{
		  snprintf(secs, sizeof(secs), "%"PRIu32, interval->time);
		  bufp = stradd(secs, bufp, buflim);
		  continue;
		}
	      /* fall through */
	    default:
	      /* we want to be generous and leave non-recognized formats
		 intact - especially for strftime to use */
	      --tmpl;
	    }
	}
      if (bufp == buflim)
	break;
      *bufp++ = *tmpl;
    }

  *bufp = '\0';

  /* now let strftime have a go */
  if(interval != NULL)
    {
      tv.tv_sec = interval->time;
      strftime(tbuf, sizeof(tbuf), buf, gmtime(&tv.tv_sec));
      return strdup(tbuf);
    }

  return strdup(buf);
}

static int validate_header_static(corsaro_header_t *h)
{
  /* WARNING: do not try and access past traceuri_len */

  /* byteswap the values */
  h->corsaro_magic = ntohl(h->corsaro_magic);
  h->magic = ntohl(h->magic);
  h->local_init_time = ntohl(h->local_init_time);
  h->interval_length = ntohs(h->interval_length);
  h->traceuri_len = ntohs(h->traceuri_len);

  /* do some sanity checking on the interval */
  if(h->corsaro_magic != CORSARO_MAGIC ||
     h->magic != CORSARO_MAGIC_HEADER)
    {
      return 0;
    }
  return 1;
}

static int validate_interval(corsaro_interval_t *interval)
{
  /* byteswap the values */
  interval->corsaro_magic = ntohl(interval->corsaro_magic);
  interval->magic = ntohl(interval->magic);
  interval->number = ntohs(interval->number);
  interval->time = ntohl(interval->time);

  /* do some sanity checking on the interval */
  if(interval->corsaro_magic != CORSARO_MAGIC ||
     interval->magic != CORSARO_MAGIC_INTERVAL)
    {
      return 0;
    }
  return 1;
}

static int validate_plugin_data(corsaro_plugin_data_t *pd)
{
  /* byteswap the values */
  pd->corsaro_magic = ntohl(pd->corsaro_magic);
  pd->magic = ntohl(pd->magic);
  pd->plugin_magic = ntohl(pd->plugin_magic);

  /* do some sanity checking on the interval */
  if(pd->corsaro_magic != CORSARO_MAGIC ||
     pd->magic != CORSARO_MAGIC_DATA)
    {
      return 0;
    }
  return 1;
}

static int validate_trailer(corsaro_trailer_t *t)
{
  /* WARNING: do not try and access past traceuri_len */

  /* byteswap the values */
  t->corsaro_magic = ntohl(t->corsaro_magic);
  t->magic = ntohl(t->magic);
  t->packet_cnt = ntohll(t->packet_cnt);
  t->accepted_cnt = ntohll(t->accepted_cnt);
  t->dropped_cnt = ntohll(t->dropped_cnt);
  t->first_packet_time = ntohl(t->first_packet_time);
  t->last_packet_time = ntohl(t->last_packet_time);
  t->local_final_time = ntohl(t->local_final_time);
  t->runtime = ntohl(t->runtime);

  /* do some sanity checking on the interval */
  if(t->corsaro_magic != CORSARO_MAGIC ||
     t->magic != CORSARO_MAGIC_TRAILER)
    {
      return 0;
    }
  return 1;
}

off_t read_plugin_data(corsaro_in_t *corsaro, 
		       corsaro_file_in_t *file, 
		       corsaro_in_record_type_t *record_type,
		       corsaro_in_record_t *record)
{
  off_t bread = -1;
  if((bread = corsaro_io_read_bytes(corsaro, record, 
				    sizeof(corsaro_plugin_data_t))) != 
     sizeof(corsaro_plugin_data_t))
    {
      *record_type = CORSARO_IN_RECORD_TYPE_NULL;
      return -1;
    }

  if(validate_plugin_data((corsaro_plugin_data_t *)record->buffer) != 1)
    {
      corsaro_log_in(__func__, corsaro, "could not validate plugin data");
      *record_type = CORSARO_IN_RECORD_TYPE_NULL;
      return -1;
    }

  assert(bread == sizeof(corsaro_plugin_data_t));
  return bread;
}

/* == EXPORTED FUNCTIONS BELOW THIS POINT == */

corsaro_file_t *corsaro_io_prepare_file(corsaro_t *corsaro, 
					const char *plugin_name,
					corsaro_interval_t *interval)
{
  return corsaro_io_prepare_file_full(corsaro, plugin_name, interval,
				      corsaro->output_mode,
				      corsaro->compress, 
				      corsaro->compress_level,
				      O_CREAT);
}

corsaro_file_t *corsaro_io_prepare_file_full(corsaro_t *corsaro, 
					     const char *plugin_name,
					     corsaro_interval_t *interval,
					     corsaro_file_mode_t mode,
					     corsaro_file_compress_t compress,
					     int compress_level,
					     int flags)
{
  corsaro_file_t *f = NULL;
  char *outfileuri;

  /* generate a file name based on the plugin name */
  if((outfileuri = generate_file_name(corsaro, plugin_name, 
				      interval, compress)) == NULL)
    {
      corsaro_log(__func__, corsaro, "could not generate file name for %s", 
		  plugin_name);
      return NULL;
    }
  
  if((f = corsaro_file_open(corsaro, 
			    outfileuri, 
			    mode, 
			    compress, 
			    compress_level,
			    flags)) == NULL)
    {
      corsaro_log(__func__, corsaro, "could not open %s for writing", 
		  outfileuri);
      return NULL;
    }

  free(outfileuri);
  return f;
}

int corsaro_io_validate_template(corsaro_t *corsaro, char *template)
{
  /* be careful using corsaro here, it is likely not initialized fully */

  /* check for length first */
  if(template == NULL)
    {
      corsaro_log(__func__, corsaro, "output template must be set");
      return 0;
    }

  /* check that the plugin pattern is in the template */
  if(strstr(template, CORSARO_IO_PLUGIN_PATTERN_STR) == NULL)
    {
      corsaro_log(__func__, corsaro, "template string must contain %s",
		  CORSARO_IO_PLUGIN_PATTERN_STR);
      return 0;
    }

  /* we're good! */
  return 1;
}

int corsaro_io_template_has_timestamp(corsaro_t *corsaro)
{
  char *p = corsaro->template;
  assert(corsaro->template);
  /* be careful using corsaro here, this is called pre-start */

  /* the easiest (but not easiest to maintain) way to do this is to step through
   * each '%' character in the string and check what is after it. if it is
   * anything other than P (for plugin) or N (for monitor name), then it is a
   * timestamp. HOWEVER. If new corsaro-specific patterns are added, they must
   * also be added here. gross */

  for(; *p; ++p)
    {
      if(*p == '%')
	{
	  /* BEWARE: if you add a new pattern here, you must also add it to
	   * generate_file_name */
	  if(*(p+1) != CORSARO_IO_MONITOR_PATTERN &&
	     *(p+1) != CORSARO_IO_PLUGIN_PATTERN)
	    {
	      return 1;
	    }
	}
    }
  return 0;
  
}

off_t corsaro_io_write_header(corsaro_t *corsaro, corsaro_file_t *file,
			      corsaro_header_t *header)
{
  assert(CORSARO_FILE_MODE(file) == CORSARO_FILE_MODE_ASCII ||
	 CORSARO_FILE_MODE(file) == CORSARO_FILE_MODE_BINARY);
  return output_funcs[CORSARO_FILE_MODE(file)].headers(corsaro, file, header);
}

void corsaro_io_print_header(corsaro_plugin_manager_t *plugin_manager, 
			     corsaro_header_t *header)
{
  int i;

  fprintf(stdout, "# CORSARO_VERSION %"PRIu8".%"PRIu8"\n", 
	  header->version_major, header->version_minor);
  fprintf(stdout, "# CORSARO_INITTIME %"PRIu32"\n", 
	  header->local_init_time);
  fprintf(stdout, "# CORSARO_INTERVAL %"PRIu16"\n", header->interval_length);
  
  if(header->traceuri != NULL)
    {
      fprintf(stdout, "# CORSARO_TRACEURI %s\n", header->traceuri);
    }
  for(i=0;i<header->plugin_cnt;i++)
    {
      fprintf(stdout, "# CORSARO_PLUGIN %s\n",
	      corsaro_plugin_get_name_by_magic(
					       plugin_manager,
					       header->plugin_magics[i]
					       )
	      );
    }
  
}

off_t corsaro_io_write_trailer(corsaro_t *corsaro, corsaro_file_t *file,
			       corsaro_trailer_t *trailer)
{
  assert(CORSARO_FILE_MODE(file) == CORSARO_FILE_MODE_ASCII ||
	 CORSARO_FILE_MODE(file) == CORSARO_FILE_MODE_BINARY);
  return output_funcs[CORSARO_FILE_MODE(file)].trailers(corsaro, file, 
							trailer);
}

void corsaro_io_print_trailer(corsaro_trailer_t *trailer)
{
  fprintf(stdout, "# CORSARO_PACKETCNT %"PRIu64"\n", 
	  trailer->packet_cnt);
  if(trailer->accepted_cnt != UINT64_MAX)
    {
      fprintf(stdout, "# CORSARO_ACCEPTEDCNT %"PRIu64"\n", 
	      trailer->accepted_cnt);
    }
  if(trailer->dropped_cnt != UINT64_MAX)
    {
      fprintf(stdout, "# CORSARO_DROPPEDCNT %"PRIu64"\n", 
	      trailer->dropped_cnt);
    }
  fprintf(stdout, "# CORSARO_FIRSTPKT %"PRIu32"\n", 
	  trailer->first_packet_time);
  fprintf(stdout, "# CORSARO_LASTPKT %"PRIu32"\n",
	  trailer->last_packet_time);
  fprintf(stdout, "# CORSARO_FINALTIME %"PRIu32"\n", 
	  trailer->local_final_time);
  fprintf(stdout, "# CORSARO_RUNTIME %"PRIu32"\n", 
	  trailer->runtime);
}


off_t corsaro_io_write_interval_start(corsaro_t *corsaro, corsaro_file_t *file,
				      corsaro_interval_t *int_start)
{
  assert(CORSARO_FILE_MODE(file) == CORSARO_FILE_MODE_ASCII ||
	 CORSARO_FILE_MODE(file) == CORSARO_FILE_MODE_BINARY);
  return output_funcs[CORSARO_FILE_MODE(file)].interval_start(corsaro, file,
							      int_start);
}

void corsaro_io_print_interval_start(corsaro_interval_t *int_start)
{
  fprintf(stdout,
	  "# CORSARO_INTERVAL_START %d %"PRIu32"\n", 
	  int_start->number, int_start->time);
}

off_t corsaro_io_write_interval_end(corsaro_t *corsaro, corsaro_file_t *file, 
				    corsaro_interval_t *int_end)
{
  assert(CORSARO_FILE_MODE(file) == CORSARO_FILE_MODE_ASCII ||
	 CORSARO_FILE_MODE(file) == CORSARO_FILE_MODE_BINARY);
  return (output_funcs[CORSARO_FILE_MODE(file)].interval_end)(corsaro, file, 
							      int_end);
}

void corsaro_io_print_interval_end(corsaro_interval_t *int_end)
{
  fprintf(stdout,  
	  "# CORSARO_INTERVAL_END %d %"PRIu32"\n", 
	  int_end->number, 
	  int_end->time);  
}

off_t corsaro_io_write_plugin_start(corsaro_t *corsaro, corsaro_file_t *file,
				    corsaro_plugin_t *plugin)
{
  assert(CORSARO_FILE_MODE(file) == CORSARO_FILE_MODE_ASCII ||
	 CORSARO_FILE_MODE(file) == CORSARO_FILE_MODE_BINARY);
  assert(plugin != NULL);

  return (output_funcs[CORSARO_FILE_MODE(file)].plugin_start)(corsaro, file, 
							      plugin);
}

void corsaro_io_print_plugin_start(corsaro_plugin_t *plugin)
{
  fprintf(stdout, "# CORSARO_PLUGIN_DATA_START %s\n", plugin->name);
}

off_t corsaro_io_write_plugin_end(corsaro_t *corsaro, corsaro_file_t *file, 
				  corsaro_plugin_t *plugin)
{
  assert(CORSARO_FILE_MODE(file) == CORSARO_FILE_MODE_ASCII ||
	 CORSARO_FILE_MODE(file) == CORSARO_FILE_MODE_BINARY);
  assert(plugin != NULL);

  return (output_funcs[CORSARO_FILE_MODE(file)].plugin_end)(corsaro, file, 
							    plugin);
}

void corsaro_io_print_plugin_end(corsaro_plugin_t *plugin)
{
  fprintf(stdout, "# CORSARO_PLUGIN_DATA_END %s\n", plugin->name); 
}

/**
 * @todo change the switch to an array of function pointers, one for each type
 * @todo add code to \link corsaro_file_open \endlink that creates a special 'stdout' file
 */
off_t corsaro_io_write_record(corsaro_t *corsaro, corsaro_file_t *file,
			      corsaro_in_record_type_t record_type,
			      corsaro_in_record_t *record)
{
  corsaro_plugin_t *plugin = NULL;
  corsaro_plugin_data_t *data = NULL;

  switch(record_type)
    {
    case CORSARO_IN_RECORD_TYPE_NULL:
      /*
	corsaro_log(__func__, corsaro, "refusing to write null record to file");
      */
      return 0;
      break;
      
    case CORSARO_IN_RECORD_TYPE_IO_HEADER:
      return corsaro_io_write_header(corsaro, file,
				     (corsaro_header_t *)record->buffer);
      break;
    case CORSARO_IN_RECORD_TYPE_IO_TRAILER:
      return corsaro_io_write_trailer(corsaro, file,
				      (corsaro_trailer_t *)record->buffer);
      break;
      
    case CORSARO_IN_RECORD_TYPE_IO_INTERVAL_START:
      return corsaro_io_write_interval_start(corsaro, file, 
					     (corsaro_interval_t *)record->buffer);
      break;

    case CORSARO_IN_RECORD_TYPE_IO_INTERVAL_END:
      return corsaro_io_write_interval_end(corsaro, file, 
					   (corsaro_interval_t *)record->buffer);
      break;

    case CORSARO_IN_RECORD_TYPE_IO_PLUGIN_START:
      data = (corsaro_plugin_data_t*)record->buffer;
      if((plugin = corsaro_plugin_get_by_magic(corsaro->plugin_manager,
					       data->plugin_magic)) == NULL)
	{
	  corsaro_log(__func__, corsaro, "invalid plugin magic detected");
	  corsaro_log(__func__, corsaro, "is corsaro built with all"
		      "necessary plugins?");
	  return 0;
	}
      return corsaro_io_write_plugin_start(corsaro, file, plugin);
      break;

    case CORSARO_IN_RECORD_TYPE_IO_PLUGIN_END:
      data = (corsaro_plugin_data_t*)record->buffer;
      if((plugin = corsaro_plugin_get_by_magic(corsaro->plugin_manager,
					       data->plugin_magic)) == NULL)
	{
	  corsaro_log(__func__, corsaro, "invalid plugin magic detected");
	  corsaro_log(__func__, corsaro, "is corsaro built with all"
		      "necessary plugins?");
	  return 0;
	}
      return corsaro_io_write_plugin_end(corsaro, file, plugin);
      break;

    case CORSARO_IN_RECORD_TYPE_FLOWTUPLE_CLASS_START:
    case CORSARO_IN_RECORD_TYPE_FLOWTUPLE_CLASS_END:
    case CORSARO_IN_RECORD_TYPE_FLOWTUPLE_FLOWTUPLE:
#ifdef WITH_PLUGIN_SIXT
      return corsaro_flowtuple_record_fprint(corsaro, file, record_type, record);
#else
      corsaro_log(__func__, corsaro, "corsaro is not built with flowtuple support");
      return 0;
#endif
      break;

    case CORSARO_IN_RECORD_TYPE_DOS_GLOBAL_HEADER:
    case CORSARO_IN_RECORD_TYPE_DOS_HEADER:
    case CORSARO_IN_RECORD_TYPE_DOS_ATTACK_VECTOR:
#ifdef WITH_PLUGIN_DOS
      return corsaro_dos_record_fprint(corsaro, file, record_type, record);
#else
      corsaro_log(__func__, corsaro, "corsaro is not built with dos support");
      return 0;
#endif
      break;

    default:
      corsaro_log(__func__, corsaro, "invalid record type %d\n", (int)record_type);
      return 0;
    }

  return -1;
}

int corsaro_io_print_record(corsaro_plugin_manager_t *plugin_manager,
			    corsaro_in_record_type_t record_type,
			    corsaro_in_record_t *record)
{
  corsaro_plugin_t *plugin = NULL;
  corsaro_plugin_data_t *data = NULL;

  switch(record_type)
    {
    case CORSARO_IN_RECORD_TYPE_NULL:
      /*
	fprintf(stderr, "refusing to write null record to file");
      */
      break;
      
    case CORSARO_IN_RECORD_TYPE_IO_HEADER:
      corsaro_io_print_header(plugin_manager,
			      (corsaro_header_t *)record->buffer);
      break;

    case CORSARO_IN_RECORD_TYPE_IO_TRAILER:
      corsaro_io_print_trailer((corsaro_trailer_t *)record->buffer);
      break;
      
    case CORSARO_IN_RECORD_TYPE_IO_INTERVAL_START:
      corsaro_io_print_interval_start((corsaro_interval_t *)record->buffer);
      break;

    case CORSARO_IN_RECORD_TYPE_IO_INTERVAL_END:
      corsaro_io_print_interval_end((corsaro_interval_t *)record->buffer);
      break;

    case CORSARO_IN_RECORD_TYPE_IO_PLUGIN_START:
      data = (corsaro_plugin_data_t*)record->buffer;
      if((plugin = corsaro_plugin_get_by_magic(plugin_manager,
					       data->plugin_magic)) == NULL)
	{
	  fprintf(stderr, "invalid plugin magic detected\n");
	  fprintf(stderr, "is corsaro built with all"
		  "necessary plugins?\n");
	  return 0;
	}
      corsaro_io_print_plugin_start(plugin);
      break;

    case CORSARO_IN_RECORD_TYPE_IO_PLUGIN_END:
      data = (corsaro_plugin_data_t*)record->buffer;
      if((plugin = corsaro_plugin_get_by_magic(plugin_manager,
					       data->plugin_magic)) == NULL)
	{
	  fprintf(stderr, "invalid plugin magic detected\n");
	  fprintf(stderr, "is corsaro built with all"
		  "necessary plugins?\n");
	  return 0;
	}
      corsaro_io_print_plugin_end(plugin);
      break;

    case CORSARO_IN_RECORD_TYPE_FLOWTUPLE_CLASS_START:
    case CORSARO_IN_RECORD_TYPE_FLOWTUPLE_CLASS_END:
    case CORSARO_IN_RECORD_TYPE_FLOWTUPLE_FLOWTUPLE:
#ifdef WITH_PLUGIN_SIXT
      return corsaro_flowtuple_record_print(record_type, record);
#else
      fprintf(stdout, "corsaro is not built with flowtuple support\n");
      return 0;
#endif
      break;

    case CORSARO_IN_RECORD_TYPE_DOS_GLOBAL_HEADER:
    case CORSARO_IN_RECORD_TYPE_DOS_HEADER:
    case CORSARO_IN_RECORD_TYPE_DOS_ATTACK_VECTOR:
#ifdef WITH_PLUGIN_DOS
      return corsaro_dos_record_print(record_type, record);
#else
      fprintf(stdout, "corsaro is not built with dos support\n");
      return 0;
#endif
      break;

    default:
      fprintf(stderr, "invalid record type %d\n", (int)record_type);
      return -1;
    }
    
  return 0;
}

/* ==== INPUT FUNCTIONS ==== */

off_t corsaro_io_read_header(corsaro_in_t *corsaro, corsaro_file_in_t *file,
			     corsaro_in_record_type_t *record_type,
			     corsaro_in_record_t *record)
{
  off_t bread;
  off_t bsbread = CORSARO_IO_HEADER_FIXED_BYTE_LEN+sizeof(uint16_t);
  corsaro_header_t *header;
  off_t offset = sizeof(corsaro_header_t);
  int i;

  /* read the static portion of the header */
  if((bread = corsaro_io_read_bytes(corsaro, record, bsbread)) != bsbread)
    {
      *record_type = CORSARO_IN_RECORD_TYPE_NULL;
      return bread;
    }

  header = (corsaro_header_t*)record->buffer;

  if(validate_header_static(header) != 1)
    {
      corsaro_log_in(__func__, corsaro, "could not validate header");
      *record_type = CORSARO_IN_RECORD_TYPE_NULL;
      return -1;
    }

  if(header->traceuri_len == 0)
    {
      header->traceuri = NULL;
    }
  else
    {
      /* read the traceuri into the buffer */
      if((bread += corsaro_io_read_bytes_offset(corsaro, record, 
						offset,
						header->traceuri_len)) != 
	 (bsbread+=header->traceuri_len))
	{
	  corsaro_log_in(__func__, corsaro, 
			 "failed to read traceuri from file");
	  *record_type = CORSARO_IN_RECORD_TYPE_NULL;
	  return bread;
	}
      header->traceuri = record->buffer+sizeof(corsaro_header_t);
      offset += header->traceuri_len;
      *(record->buffer+offset) = '\0';
      offset++;
    }

  /* now, read the plugin count */
  if((bread += corsaro_io_read_bytes_offset(corsaro, record, 
					    CORSARO_IO_HEADER_FIXED_BYTE_LEN
					    +sizeof(uint16_t)
					    +sizeof(uint8_t*),
					    sizeof(uint16_t))) != 
     (bsbread+=sizeof(uint16_t)))
    {
      corsaro_log_in(__func__, corsaro, 
		     "failed to read plugin count from file");
      *record_type = CORSARO_IN_RECORD_TYPE_NULL;
      return bread;
    }

  header->plugin_cnt = ntohs(header->plugin_cnt);

  if(header->plugin_cnt == 0)
    {
      header->plugin_magics = NULL;
    }
  else
    {
      /* read the plugin array into the buffer */
      if((bread += corsaro_io_read_bytes_offset(corsaro, record, 
						offset,
						sizeof(uint32_t)
						*header->plugin_cnt)) != 
	 (bsbread+=sizeof(uint32_t)*header->plugin_cnt))
	{
	  corsaro_log_in(__func__, corsaro, 
			 "failed to read plugin magics from file");
	  *record_type = CORSARO_IN_RECORD_TYPE_NULL;
	  return bread;
	}
      header->plugin_magics = (uint32_t*)(record->buffer+offset);
    }

  for(i =0; i<header->plugin_cnt;i++)
    {
      header->plugin_magics[i] = ntohl(header->plugin_magics[i]);
    }

  assert(bread == CORSARO_IO_HEADER_FIXED_BYTE_LEN+sizeof(uint16_t)
	 +header->traceuri_len+sizeof(uint16_t)
	 +(header->plugin_cnt*sizeof(uint32_t)));

  *record_type = CORSARO_IN_RECORD_TYPE_IO_HEADER;

  return bread;
}

off_t corsaro_io_read_trailer(corsaro_in_t *corsaro, corsaro_file_in_t *file,
			      corsaro_in_record_type_t *record_type,
			      corsaro_in_record_t *record)
{
  off_t bytes_read;

  if((bytes_read = corsaro_io_read_bytes(corsaro, record, 
					 sizeof(corsaro_trailer_t))) != 
     sizeof(corsaro_trailer_t))
    {
      *record_type = CORSARO_IN_RECORD_TYPE_NULL;
      return bytes_read;
    }

  if(validate_trailer((corsaro_trailer_t *)record->buffer) != 1)
    {
      corsaro_log_in(__func__, corsaro, "could not validate trailer");
      *record_type = CORSARO_IN_RECORD_TYPE_NULL;
      return -1;
    }

  *record_type = CORSARO_IN_RECORD_TYPE_IO_TRAILER;

  return bytes_read;
}

off_t corsaro_io_read_interval_start(corsaro_in_t *corsaro, corsaro_file_in_t *file,
				     corsaro_in_record_type_t *record_type,
				     corsaro_in_record_t *record)
{
  off_t bread;

  if((bread = corsaro_io_read_bytes(corsaro, record, 
				    CORSARO_IO_INTERVAL_HEADER_BYTE_LEN)) != 
     CORSARO_IO_INTERVAL_HEADER_BYTE_LEN)
    {
      *record_type = CORSARO_IN_RECORD_TYPE_NULL;
      return bread;
    }

  if(validate_interval((corsaro_interval_t *)record->buffer) != 1)
    {
      corsaro_log_in(__func__, corsaro, "could not validate interval");
      *record_type = CORSARO_IN_RECORD_TYPE_NULL;
      return -1;
    }

  assert(bread == CORSARO_IO_INTERVAL_HEADER_BYTE_LEN);

  *record_type = CORSARO_IN_RECORD_TYPE_IO_INTERVAL_START;

  return bread;
}

off_t corsaro_io_read_interval_end(corsaro_in_t *corsaro, corsaro_file_in_t *file,
				   corsaro_in_record_type_t *record_type,
				   corsaro_in_record_t *record)
{
  off_t bread;

  if((bread = corsaro_io_read_bytes(corsaro, record, 
				    CORSARO_IO_INTERVAL_HEADER_BYTE_LEN)) != 
     CORSARO_IO_INTERVAL_HEADER_BYTE_LEN)
    {
      *record_type = CORSARO_IN_RECORD_TYPE_NULL;
      return bread;
    }

  if(validate_interval((corsaro_interval_t *)record->buffer) != 1)
    {
      corsaro_log_in(__func__, corsaro, "could not validate interval");
      *record_type = CORSARO_IN_RECORD_TYPE_NULL;
      return -1;
    }

  assert(bread == CORSARO_IO_INTERVAL_HEADER_BYTE_LEN);

  *record_type = CORSARO_IN_RECORD_TYPE_IO_INTERVAL_END;

  return bread;
}

off_t corsaro_io_read_plugin_start(corsaro_in_t *corsaro, 
				   corsaro_file_in_t *file, 
				   corsaro_in_record_type_t *record_type,
				   corsaro_in_record_t *record)
{
  off_t bread = read_plugin_data(corsaro, file, record_type, record);

  if(bread > 0)
    {
      *record_type = CORSARO_IN_RECORD_TYPE_IO_PLUGIN_START;
    }

  return bread;
}

off_t corsaro_io_read_plugin_end(corsaro_in_t *corsaro, corsaro_file_in_t *file, 
				 corsaro_in_record_type_t *record_type,
				 corsaro_in_record_t *record)
{
  off_t bread = read_plugin_data(corsaro, file, record_type, record);

  if(bread > 0)
    {
      *record_type = CORSARO_IN_RECORD_TYPE_IO_PLUGIN_END;
    }

  return bread;
}

off_t corsaro_io_read_bytes(corsaro_in_t *corsaro, corsaro_in_record_t *record,
			    off_t len)
{
  /* fix this with a realloc later? */
  assert(record->buffer_len >= len);

  return corsaro_file_rread(corsaro->file, record->buffer, len);
}

off_t corsaro_io_read_bytes_offset(corsaro_in_t *corsaro, 
				   corsaro_in_record_t *record,
				   off_t offset, off_t len)
{
  /* fix this with a realloc later? */
  assert(record->buffer_len >= offset+len);

  return corsaro_file_rread(corsaro->file, 
			    (record->buffer)+offset, len);
}
