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

#include "libtrace.h"

#include "corsaro_file.h"
#include "corsaro_io.h"
#include "corsaro_log.h"
#include "utils.h"

#ifdef WITH_PLUGIN_SIXT
#include "corsaro_flowtuple.h"
#endif

/** @file
 *
 * @brief Code which implements the public functions of libcorsaro
 *
 * @author Alistair King
 *
 */

static corsaro_packet_t *corsaro_packet_alloc(corsaro_t *corsaro)
{
  corsaro_packet_t *pkt;

  if((pkt = malloc_zero(sizeof(corsaro_packet_t))) == NULL)
    {
      corsaro_log(__func__, corsaro, "could not malloc corsaro_packet");
      return NULL;
    }
  /* this corsaro packet still needs a libtrace packet to be loaded... */
  return pkt;
}

static inline void corsaro_packet_state_reset(corsaro_packet_t *packet)
{
  assert(packet != NULL);
  
  /* This is dangerous to do field-by-field, new plugins may not be responsible
     and reset their own fields. Therefore we will do this by force */
  memset(&packet->state, 0, sizeof(corsaro_packet_state_t));
}

static void corsaro_packet_free(corsaro_packet_t *packet)
{
  /* we will assume that somebody else is taking care of the libtrace packet */
  if(packet != NULL)
    {
      free(packet);
    }
}

static void corsaro_free(corsaro_t *corsaro)
{
  corsaro_plugin_t *p = NULL;
  
  if(corsaro == NULL)
    {
      /* nothing to be done... */
      return;
    }

  /* free up the plugins first, they may try and use some of our info before
     closing */
  if(corsaro->plugin_manager != NULL)
    {
      while((p = corsaro_plugin_next(corsaro->plugin_manager, p)) != NULL)
	{
	  p->close_output(corsaro);
	}

      corsaro_plugin_manager_free(corsaro->plugin_manager);
      corsaro->plugin_manager = NULL;
    }

  if(corsaro->uridata != NULL)
    {
      free(corsaro->uridata);
      corsaro->uridata = NULL;
    }

  if(corsaro->monitorname != NULL)
    {
      free(corsaro->monitorname);
      corsaro->monitorname = NULL;
    }

  if(corsaro->template != NULL)
    {
      free(corsaro->template);
      corsaro->template = NULL;
    }
 
  if(corsaro->packet != NULL)
    {
      corsaro_packet_free(corsaro->packet);
      corsaro->packet = NULL;
    }

  if(corsaro->global_file != NULL)
    {
      corsaro_file_close(corsaro, corsaro->global_file);
      corsaro->global_file = NULL;
    }

  /* close this as late as possible */
  corsaro_log_close(corsaro);

  free(corsaro);

  return;
}

static void populate_interval(corsaro_interval_t *interval, uint32_t number,
			      uint32_t time)
{
  interval->corsaro_magic = CORSARO_MAGIC;
  interval->magic = CORSARO_MAGIC_INTERVAL;
  interval->number = number;
  interval->time = time;
}

static int is_meta_rotate_interval(corsaro_t *corsaro)
{
  assert(corsaro != NULL);

  if(corsaro->meta_output_rotate < 0)
    {
      return corsaro_is_rotate_interval(corsaro);
    }
  else if(corsaro->meta_output_rotate > 0 &&
	  (corsaro->interval_start.number+1) % 
	  corsaro->meta_output_rotate == 0)
    {
      return 1;
    }
  else
    {
      return 0;
    }
}

static corsaro_t *corsaro_init(char *template, corsaro_file_mode_t mode)
{
  corsaro_t *e;

  if((e = malloc_zero(sizeof(corsaro_t))) == NULL)
    {
      corsaro_log(__func__, NULL, "could not malloc corsaro_t");
      return NULL;
    }

  /* what time is it? */
  gettimeofday_wrap(&e->init_time);

  /* uridata doesn't *need* to be set */

  /* set a default monitorname for when im bad and directly retrieve it
     from the structure */
  e->monitorname = strdup(STR(CORSARO_MONITOR_NAME));

  /* template does, however */
  /* check that it is valid */
  if(corsaro_io_validate_template(e, template) == 0)
    {
      corsaro_log(__func__, e, "invalid template %s", template);
      goto err;
    }
  if((e->template = strdup(template)) == NULL)
    {
      corsaro_log(__func__, e, 
		"could not duplicate template string (no memory?)");
      goto err;
    }

  /* as does the mode */
  e->output_mode = mode;

  /* check if compression should be used based on the file name */
  e->compress = corsaro_file_detect_compression(e, e->template);

  /* use the default compression level for now */
  e->compress_level = CORSARO_FILE_COMPRESS_LEVEL_DEFAULT;

  /* lets get us a wrapper packet ready */
  if((e->packet = corsaro_packet_alloc(e)) == NULL)
    {
      corsaro_log(__func__, e, "could not create corsaro packet");
      goto err;
    }

  /* ask the plugin manager to get us some plugins */
  /* this will init all compiled plugins but not start them, this gives
     us a chance to wait for the user to choose a subset to enable
     with corsaro_enable_plugin and then we'll re-init */
  if((e->plugin_manager = corsaro_plugin_manager_init(e->logfile)) == NULL)
    {
      corsaro_log(__func__, e, "could not initialize plugin manager");
      goto err;
    }

  /* set the default interval alignment value */
  e->interval_align = CORSARO_INTERVAL_ALIGN_DEFAULT;

  /* interval doesn't need to be actively set, use the default for now */
  e->interval = CORSARO_INTERVAL_DEFAULT;

  /* default for meta rotate should be to follow output_rotate */
  e->meta_output_rotate = -1;

  /* initialize the current interval */
  populate_interval(&e->interval_start, 0, 0);

  /* set the libtrace related values to unknown for now */
  e->accepted_pkts = UINT64_MAX;
  e->dropped_pkts = UINT64_MAX;

  /* the rest are zero, as they should be. */
 
  /* ready to rock and roll! */

  return e;

 err:
  /* 02/26/13 - ak comments because it is up to the user to call
     corsaro_finalize_output to free the memory */
  /*corsaro_free(e);*/
  return NULL;
}

static int start_interval(corsaro_t *corsaro, struct timeval int_start)
{
  corsaro_plugin_t *tmp = NULL;

  /* record this so we know when the interval started */
  /* the interval number is already incremented by end_interval */
  corsaro->interval_start.time = int_start.tv_sec;

  /* the following is to support file rotation */
  /* initialize the log file */
  if(corsaro->logfile == NULL)
    {
      /* if this is the first interval, let them know we are switching to
	 logging to a file */
      if(corsaro->interval_start.number == 0)
	{
	  /* there is a replica of this message in the other place that
	   * corsaro_log_init is called */
	  corsaro_log(
		      __func__, corsaro, "now logging to file"
#ifdef DEBUG
		      " (and stderr)"
#endif
		      );
	}

      if(corsaro_log_init(corsaro) != 0)
	{
	  corsaro_log(__func__, corsaro, "could not initialize log file");
	  corsaro_free(corsaro);
	  return -1;
	}
    }

  /* initialize the global output file */
  if(corsaro->global_file == NULL)
    {
      if((corsaro->global_file = 
	  corsaro_io_prepare_file(corsaro, 
				  CORSARO_IO_GLOBAL_NAME,
				  &corsaro->interval_start)) == NULL)
	{
	  corsaro_log(__func__, corsaro, "could not open global output file");
	  corsaro_free(corsaro);
	  return -1;
	}

      /* write headers to the global file */
      if(corsaro_io_write_header(corsaro, corsaro->global_file, NULL) <= 0)
	{
	  corsaro_log(__func__, corsaro, "could not write global headers");
	  corsaro_free(corsaro);
	  return -1;
	}
    }

  /* ask each plugin to start a new interval */
  /* plugins should rotate their files now too */
  while((tmp = corsaro_plugin_next(corsaro->plugin_manager, tmp)) != NULL)
    {
      if(tmp->start_interval(corsaro, &corsaro->interval_start) != 0)
	{
	  corsaro_log(__func__, corsaro, "%s failed to start interval at %ld", 
		    tmp->name, int_start.tv_sec);
	  return -1;
	}
    }
  return 0;
}

static int end_interval(corsaro_t *corsaro, struct timeval int_end)
{
  corsaro_plugin_t *tmp = NULL;

  corsaro_interval_t interval_end;

  populate_interval(&interval_end, corsaro->interval_start.number,
		    int_end.tv_sec);

  /* write the global interval start header */
  if(corsaro_io_write_interval_start(corsaro, corsaro->global_file,
				   &corsaro->interval_start) <= 0)
    {
      corsaro_log(__func__, corsaro, 
		"could not write global interval start headers at %ld",
		corsaro->interval_start.time);
      return -1;
    }
  /* ask each plugin to end the current interval */
  while((tmp = corsaro_plugin_next(corsaro->plugin_manager, tmp)) != NULL)
    {
      if(tmp->end_interval(corsaro, &interval_end) != 0)
	{
	  corsaro_log(__func__, corsaro, "%s failed to end interval at %ld",
		    tmp->name, int_end.tv_sec);
	  return -1;
	}
    }
  /* write the global interval end header */
  if(corsaro_io_write_interval_end(corsaro, corsaro->global_file, 
				 &interval_end) <= 0)
    {
      corsaro_log(__func__, corsaro, 
		"could not write global interval end headers at %ld",
		interval_end.time);
      return -1;
    }

  /* if we are rotating, now is the time to close our output files */
  if(is_meta_rotate_interval(corsaro))
    {
      if(corsaro->global_file != NULL)
	{
	  /* write headers to the global file */
	  if(corsaro_io_write_trailer(corsaro, 
				      corsaro->global_file, NULL) <= 0)
	    {
	      corsaro_log(__func__, corsaro, 
			  "could not write global trailers");
	      corsaro_free(corsaro);
	      return -1;
	    }
	  corsaro_file_close(corsaro, corsaro->global_file);
	  corsaro->global_file = NULL;
	}

      /* we should also update the long-term counters at this point */
      if(corsaro->trace != NULL)
	{
	  corsaro->accepted_pkts = trace_get_accepted_packets(corsaro->trace);
	  corsaro->dropped_pkts = trace_get_dropped_packets(corsaro->trace);
	}

      /* this MUST be the last thing closed in case any of the other things want
	 to log their end-of-interval activities (e.g. the pkt cnt from writing
	 the trailers */
      if(corsaro->logfile != NULL)
	{
	  corsaro_log_close(corsaro);
	}
    }

  corsaro->interval_end_needed = 0;
  return 0;
}

static void corsaro_in_free(corsaro_in_t *corsaro)
{
  if(corsaro == NULL)
    {
      /* nothing to be done */
      
      corsaro_log_in(__func__, corsaro, 
		   "WARNING: corsaro_in_free called on NULL object; "
		   "this could indicate a double-free");
      return;
    }

  /* free the uridata */
  free(corsaro->uridata);
  corsaro->uridata = NULL;

  /* close the plugin */
  if(corsaro->plugin != NULL)
    {
      corsaro->plugin->close_input(corsaro);
    }
  /*
  while((p = corsaro_plugin_next(corsaro->plugin_manager, p)) != NULL)
    {
      p->close_input(corsaro);
    }
  */
  
  /* free the plugin manager */
  if(corsaro->plugin_manager != NULL)
    {
      corsaro_plugin_manager_free(corsaro->plugin_manager);
      corsaro->plugin_manager = NULL;
    }

  /* close the input file */
  if(corsaro->file !=  NULL)
    {
      corsaro_file_rclose(corsaro->file);
      corsaro->file = NULL;
    }

  corsaro->started = 0;

  free(corsaro);
}

static corsaro_in_t *corsaro_in_init(const char *corsarouri)
{
  corsaro_in_t *e;

  if((e = malloc_zero(sizeof(corsaro_in_t))) == NULL)
    {
      corsaro_log_in(__func__, NULL, "could not malloc corsaro_t");
      return NULL;
    }

  if((e->uridata = strdup(corsarouri)) == NULL)
    {
      corsaro_log_in(__func__, e, 
		"could not duplicate uri string (no memory?)");
      goto err;
    }

  /* set to null until we know if this is a global file or a plugin */
  e->expected_type = CORSARO_IN_RECORD_TYPE_NULL;
  
  /* initialize the logging */
  if(corsaro_log_in_init(e) != 0)
    {
      corsaro_log_in(__func__, e, "could not initialize log file");
      goto err;
    }

  /* ask the plugin manager to get us some plugins */
  if((e->plugin_manager = corsaro_plugin_manager_init(NULL)) == 0)
    {
      corsaro_log_in(__func__, e, "could not initialize plugins");
      goto err;
    }

  /* do not init plugins here, we will init only the one needed */

  /* delay opening the input file until we 'start' */

  return e;

 err:
  corsaro_in_free(e);
  return NULL;
}

static inline int process_packet(corsaro_t *corsaro, corsaro_packet_t *packet)
{
  corsaro_plugin_t *tmp = NULL;
  while((tmp = corsaro_plugin_next(corsaro->plugin_manager, tmp)) != NULL)
    {
      if(tmp->process_packet(corsaro, packet) < 0)
	{
	  corsaro_log(__func__, corsaro, "%s failed to process packet", 
		    tmp->name);
	  return -1;
	}
    }
  return 0;
}

#ifdef WITH_PLUGIN_SIXT
static int per_flowtuple(corsaro_t *corsaro, corsaro_flowtuple_t *tuple)
{
  /* ensure that the state is clear */
  corsaro_packet_state_reset(corsaro->packet);

  corsaro_plugin_t *tmp = NULL;
  while((tmp = corsaro_plugin_next(corsaro->plugin_manager, tmp)) != NULL)
    {
      if(tmp->process_flowtuple != NULL &&
	 tmp->process_flowtuple(corsaro, tuple, &corsaro->packet->state) < 0)
	{
	  corsaro_log(__func__, corsaro, "%s failed to process flowtuple", 
		    tmp->name);
	  return -1;
	}
    }

  corsaro->packet_cnt += ntohl(tuple->packet_cnt);

  return 0;
}

static int per_flowtuple_class_start(corsaro_t *corsaro, 
				     corsaro_flowtuple_class_start_t *class)
{
  corsaro_plugin_t *tmp = NULL;
  while((tmp = corsaro_plugin_next(corsaro->plugin_manager, tmp)) != NULL)
    {
      if(tmp->process_flowtuple_class_start != NULL &&
	 tmp->process_flowtuple_class_start(corsaro, class) < 0)
	{
	  corsaro_log(__func__, corsaro, 
		      "%s failed to process flowtuple class start", 
		    tmp->name);
	  return -1;
	}
    }

  return 0;
}

static int per_flowtuple_class_end(corsaro_t *corsaro, 
				   corsaro_flowtuple_class_end_t *class)
{
  corsaro_plugin_t *tmp = NULL;
  while((tmp = corsaro_plugin_next(corsaro->plugin_manager, tmp)) != NULL)
    {
      if(tmp->process_flowtuple_class_end != NULL &&
	 tmp->process_flowtuple_class_end(corsaro, class) < 0)
	{
	  corsaro_log(__func__, corsaro, 
		      "%s failed to process flowtuple class end", 
		    tmp->name);
	  return -1;
	}
    }

  return 0;
}
#endif

static int per_interval_start(corsaro_t *corsaro, 
			      corsaro_interval_t *interval)
{
  struct timeval ts;
  ts.tv_usec = 0;
  ts.tv_sec = interval->time;
  
  /* if this is the first interval start, mark the first time */
  if(corsaro->packet_cnt == 0)
    {
      corsaro->first_ts = ts;
    }

  corsaro->interval_start.number = interval->number;
  if(start_interval(corsaro, ts) != 0)
    {
      corsaro_log(__func__, corsaro, "could not start interval at %ld", 
		  interval->time);
      return -1;
    }
  return 0;
}

static int per_interval_end(corsaro_t *corsaro, 
			    corsaro_interval_t *interval)
{
  struct timeval ts;
  ts.tv_sec = interval->time;
  corsaro->interval_start.number = interval->number;
  corsaro->last_ts = ts;
  if(end_interval(corsaro, ts) != 0)
    {
      corsaro_log(__func__, corsaro, "could not end interval at %ld", 
		  interval->time);
      /* we don't free in case the client wants to try to carry on */
      return -1;
    }
  return 0;
}

static int check_global_filename(char *fname)
{
  if(strstr(fname, CORSARO_IO_GLOBAL_NAME) != NULL)
    {
      return 1;
    }
  return 0;
}

static int check_global_magic(corsaro_in_t *corsaro, corsaro_file_in_t *file)
{
  char buffer[1024];
  int len;

  len = corsaro_file_rpeek(file, buffer, sizeof(buffer));

  /* an corsaro global file will have 'EDGRHEAD' as the first 8 bytes */
  if(len < 8)
    {
      return 0;
    }

  /* lets make this easy and just do a string compare */
  buffer[8] = '\0';

  if(strncmp(&buffer[0], "EDGRHEAD", 8) == 0)
    {
      return 1;
    }
  return 0;
}

static int is_plugin_data_or_interval(corsaro_in_t *corsaro)
{
  char buffer[1024];
  int len;

  /* we need to peek and see if there is any plugin data */
  len = corsaro_file_rpeek(corsaro->file, buffer, 
			   sizeof(buffer));
  if(len < sizeof(corsaro_plugin_data_t) 
     && len < sizeof(corsaro_interval_t))
    {
      corsaro_log_in(__func__, corsaro, 
		     "invalid corsaro global file");
      return -1;
    }
	  
  /* a plugin data record will have 'EDGRDATA' */
  /* an interval start record will have 'EDGRINTR' */
  /* either way, use strncmp */
  buffer[8] = '\0';
  if(strncmp(buffer, "EDGRDATA", 8) == 0)
    {
      return 1;
    }
  else if(strncmp(buffer, "EDGRINTR", 8) == 0)
    {
      return 0;

    }
  else
    {
      return -1;
    }
}

static int is_trailer_or_interval(corsaro_in_t *corsaro)
{
  char buffer[1024];
  int len;

  /* we need to peek and see if there is any plugin data */
  len = corsaro_file_rpeek(corsaro->file, buffer, 
			   sizeof(buffer));
  if(len < sizeof(corsaro_trailer_t) 
     && len < sizeof(corsaro_interval_t))
    {
      corsaro_log_in(__func__, corsaro, 
		     "invalid corsaro global file");
      return -1;
    }
	  
  /* a plugin data record will have 'EDGRFOOT' */
  /* an interval start record will have 'EDGRINTR' */
  /* either way, use strncmp */
  buffer[8] = '\0';
  if(strncmp(buffer, "EDGRFOOT", 8) == 0)
    {
      return 1;
    }
  else if(strncmp(buffer, "EDGRINTR", 8) == 0)
    {
      return 0;
    }
  else
    {
      return -1;
    }
}

static off_t read_record(corsaro_in_t *corsaro, 
			 corsaro_in_record_type_t *record_type, 
			 corsaro_in_record_t *record)
{
  off_t bytes_read = -1;
  int rc;

  /* this code is adapted from corsaro_flowtuple.c */
  switch(corsaro->expected_type)
    {
    case CORSARO_IN_RECORD_TYPE_IO_HEADER:
      /* ask the io subsystem to read it for us */
      bytes_read = corsaro_io_read_header(corsaro, corsaro->file, record_type,
					  record);
      if(bytes_read > 0)
	{
	  corsaro->expected_type = CORSARO_IN_RECORD_TYPE_IO_INTERVAL_START;
	}
      break;

    case CORSARO_IN_RECORD_TYPE_IO_INTERVAL_START:
      /* ask the io subsystem to read it for us */
      bytes_read = corsaro_io_read_interval_start(corsaro, corsaro->file, 
						  record_type, record);
      if(bytes_read == sizeof(corsaro_interval_t))
	{
	  rc = is_plugin_data_or_interval(corsaro);
	  if(rc == 1)
	    {
	      corsaro->expected_type = CORSARO_IN_RECORD_TYPE_IO_PLUGIN_START;
	    }
	  else if(rc == 0)
	    {
	      corsaro->expected_type = CORSARO_IN_RECORD_TYPE_IO_INTERVAL_END;
	    }
	  else
	    {
	      *record_type = CORSARO_IN_RECORD_TYPE_NULL;
	    }
	}
      break;

    case CORSARO_IN_RECORD_TYPE_IO_PLUGIN_START:
      /* ask the io subsystem to read it for us */
      bytes_read = corsaro_io_read_plugin_start(corsaro, corsaro->file, 
						record_type, record);
      if(bytes_read == sizeof(corsaro_plugin_data_t))
	{
	  /* which plugin is this? */
	  if((corsaro->plugin = corsaro_plugin_get_by_magic(
			 corsaro->plugin_manager,
         	         ((corsaro_plugin_data_t*)record->buffer)->plugin_magic)
	      )
	     == NULL)
	    {
	      corsaro_log_in(__func__, corsaro, "invalid plugin magic detected");
	      corsaro_log_in(__func__, corsaro, "is corsaro built with all "
			     "necessary plugins?");
	      *record_type = CORSARO_IN_RECORD_TYPE_NULL;
	    }
	  else
	    {
	      /* we'll pass these over to the plugin */
	      corsaro->expected_type = CORSARO_IN_RECORD_TYPE_INTERNAL_REDIRECT; 
	    }	  
	}
      else
	{
	  corsaro_log_in(__func__, corsaro, 
			 "failed to read plugin data start");
	  *record_type = CORSARO_IN_RECORD_TYPE_NULL;
	}
      break;

    case CORSARO_IN_RECORD_TYPE_INTERNAL_REDIRECT:
      /* pass this over to the plugin */
      assert(corsaro->plugin != NULL);
      bytes_read = corsaro->plugin->read_global_data_record(corsaro, record_type, record);

      if(bytes_read >= 0)
	{
	  corsaro->expected_type = CORSARO_IN_RECORD_TYPE_IO_PLUGIN_END;
	}
      break;

    case CORSARO_IN_RECORD_TYPE_IO_PLUGIN_END:
      /* ask the io subsystem to read it for us */
      bytes_read = corsaro_io_read_plugin_end(corsaro, corsaro->file,
					      record_type, record);
      if(bytes_read == sizeof(corsaro_plugin_data_t))
	{
	  /* check if there is more plugin data */
	  rc = is_plugin_data_or_interval(corsaro);
	  if(rc == 1)
	    {
	      corsaro->expected_type = CORSARO_IN_RECORD_TYPE_IO_PLUGIN_START;
	    }
	  else if(rc == 0)
	    {
	      corsaro->expected_type = CORSARO_IN_RECORD_TYPE_IO_INTERVAL_END;
	    }
	  else
	    {
	      *record_type = CORSARO_IN_RECORD_TYPE_NULL;
	    }
	}
      break;

    case CORSARO_IN_RECORD_TYPE_IO_INTERVAL_END:
      /* ask the io subsystem to read it for us */
      bytes_read = corsaro_io_read_interval_end(corsaro, corsaro->file,
					      record_type, record);
      if(bytes_read == sizeof(corsaro_interval_t))
	{
	  /* check for an interval start, or for a trailer */
	  rc = is_trailer_or_interval(corsaro);
	  if(rc == 0)
	    {
	      corsaro->expected_type = CORSARO_IN_RECORD_TYPE_IO_INTERVAL_START;
	    }
	  else if(rc == 1)
	    {
	      corsaro->expected_type = CORSARO_IN_RECORD_TYPE_IO_TRAILER;
	    }
	  else
	    {
	      *record_type = CORSARO_IN_RECORD_TYPE_NULL;
	    }
	}
      break;

    case CORSARO_IN_RECORD_TYPE_IO_TRAILER:
      /* ask the io subsystem to read it for us */
      bytes_read = corsaro_io_read_trailer(corsaro, corsaro->file, record_type,
					   record);
      if(bytes_read == sizeof(corsaro_trailer_t))
	{
	  corsaro->expected_type = CORSARO_IN_RECORD_TYPE_IO_HEADER;
	}
      break;

    default:
      corsaro_log_in(__func__, corsaro, "invalid expected record type");
    }

  return bytes_read;
}

/* == PUBLIC CORSARO API FUNCS BELOW HERE == */

corsaro_t *corsaro_alloc_output(char *template, corsaro_file_mode_t mode)
{
  corsaro_t *corsaro;

  /* quick sanity check that folks aren't trying to write to stdout */
  if(template == NULL || strcmp(template, "-") == 0)
    {
      corsaro_log(__func__, NULL, "writing to stdout not supported");
      return NULL;
    }
  
  /* initialize the corsaro object */
  if((corsaro = corsaro_init(template, mode)) == NULL)
    {
      corsaro_log(__func__, NULL, "could not initialize corsaro object");
      return NULL;
    }

  /* only initialize the log file if there are no time format fields in the file
     name (otherwise they will get a log file with a zero timestamp. */
  /* Note that if indeed it does have a timestamp, the initialization error
     messages will not be logged to a file. In fact nothing will be logged until
     the first packet is received. */
  assert(corsaro->logfile == NULL);
  if(corsaro_io_template_has_timestamp(corsaro) == 0)
    {
      corsaro_log(
		  __func__, corsaro, "now logging to file"
#ifdef DEBUG
		  " (and stderr)"
#endif
		  );

      if(corsaro_log_init(corsaro) != 0)
	{
	  return NULL;
	}
    }

  return corsaro;
}

int corsaro_start_output(corsaro_t *corsaro)
{
  corsaro_plugin_t *p = NULL;

  assert(corsaro != NULL);

  /* ask the plugin manager to start up */
  /* this allows it to remove disabled plugins */
  if(corsaro_plugin_manager_start(corsaro->plugin_manager) != 0)
    {
      corsaro_log(__func__, corsaro, "could not start plugin manager");
      /*      corsaro_free(corsaro); */
      return -1;
    }

  /* now, ask each plugin to open its output file */
  /* we need to do this here so that the corsaro object is fully set up
     that is, the traceuri etc is set */
  while((p = corsaro_plugin_next(corsaro->plugin_manager, p)) != NULL)
    {
      if(p->init_output(corsaro) != 0)
	{
	  /* 02/25/13 - ak comments debug message */
	  /*
	  corsaro_log(__func__, corsaro, "plugin could not init output");
	  */
	  /*	  corsaro_free(corsaro); */
	  return -1;
	}
    }

  corsaro->started = 1;

  return 0;
}

void corsaro_set_interval_alignment(corsaro_t *corsaro, 
				    corsaro_interval_align_t align)
{
  assert(corsaro != NULL);
  /* you cant set interval alignment once corsaro has started */
  assert(corsaro->started == 0);
  
  corsaro_log(__func__, corsaro, "setting interval alignment to %d",
	      align);

  corsaro->interval_align = align;
}

void corsaro_set_interval(corsaro_t *corsaro, unsigned int i)
{
  assert(corsaro != NULL);
  /* you can't set the interval once corsaro has been started */
  assert(corsaro->started == 0);

  corsaro_log(__func__, corsaro, "setting interval length to %d",
	      i);

  corsaro->interval = i;
}

void corsaro_set_output_rotation(corsaro_t *corsaro,
				 int intervals)
{
  assert(corsaro != NULL);
  /* you can't enable rotation once corsaro has been started */
  assert(corsaro->started == 0);

  corsaro_log(__func__, corsaro, 
	      "setting output rotation after %d interval(s)",
	      intervals);

  /* if they have asked to rotate, but did not put a timestamp in the template,
   * we will end up clobbering files. warn them. */
  if(corsaro_io_template_has_timestamp(corsaro) == 0)
    {
      /* we skip the log and write directly out so that it is clear even if they
       * have debugging turned off */
      fprintf(stderr, "WARNING: using output rotation without any timestamp "
	      "specifiers in the template.\n");
      fprintf(stderr, 
	      "WARNING: output files will be overwritten upon rotation\n");
      /* @todo consider making this a fatal error */
    }

  corsaro->output_rotate = intervals;
}

void corsaro_set_meta_output_rotation(corsaro_t *corsaro,
				      int intervals)
{
  assert(corsaro != NULL);
  /* you can't enable rotation once corsaro has been started */
  assert(corsaro->started == 0);

  corsaro_log(__func__, corsaro, 
	      "setting meta output rotation after %d intervals(s)",
	      intervals);

  corsaro->meta_output_rotate = intervals;
}

int corsaro_is_rotate_interval(corsaro_t *corsaro)
{
  assert(corsaro != NULL);

  if(corsaro->output_rotate == 0)
    {
      return 0;
    }
  else if((corsaro->interval_start.number+1) % corsaro->output_rotate == 0)
    {
      return 1;
    }
  else
    {
      return 0;
    }
}

int corsaro_set_trace(corsaro_t *corsaro, libtrace_t *trace)
{
  assert(corsaro != NULL);

  /* this function can actually be called once corsaro is started */
  if(corsaro->trace != NULL)
    {
      corsaro_log(__func__, corsaro, "updating trace pointer");
    }
  else
    {
      corsaro_log(__func__, corsaro, "setting trace pointer");
    }

  /* reset the counters */
  corsaro->accepted_pkts = 0;
  corsaro->dropped_pkts = 0;

  corsaro->trace = trace;
  return 0;
}

int corsaro_set_traceuri(corsaro_t *corsaro, char *uri)
{
  assert(corsaro != NULL);

  if(corsaro->started != 0)
    {
      corsaro_log(__func__, corsaro, 
		"trace uri can only be set before "
		"corsaro_start_output is called");
      return -1;
    }

  if(corsaro->uridata != NULL)
    {
      corsaro_log(__func__, corsaro, "updating trace uri from %s to %s",
		corsaro->uridata, uri);
    }
  else
    {
      corsaro_log(__func__, corsaro, "setting trace uri to %s",
		  uri);
    }

  if((corsaro->uridata = strdup(uri)) == NULL)
    {
      corsaro_log(__func__, corsaro, 
		"could not duplicate uri string (no memory?)");
      return -1;
    }
  return 0;
}

int corsaro_enable_plugin(corsaro_t *corsaro, const char *plugin_name,
			  const char *plugin_args)
{
  assert(corsaro != NULL);
  assert(corsaro->plugin_manager != NULL);

  return corsaro_plugin_enable_plugin(corsaro->plugin_manager, plugin_name,
				      plugin_args);
}

int corsaro_get_plugin_names(char ***plugin_names)
{
  /* we make this work by creating a plugin manager, walking the list of
   *  plugins, and dumping their names.
   */
  /* @todo add a 'usage' method to the plugin API so we can explicitly dump
     the usage for each plugin */

  int i = 0;
  char **names = NULL;
  corsaro_plugin_t *tmp = NULL;
  corsaro_plugin_manager_t *tmp_manager = NULL;
  if((tmp_manager = corsaro_plugin_manager_init(NULL)) == NULL)
    {
      return -1;
    }

  /* initialize the array of char pointers */
  if((names = malloc(sizeof(char *) * tmp_manager->plugins_cnt)) == NULL)
    {
      return -1;
    }
  
  while((tmp = corsaro_plugin_next(tmp_manager, tmp)) != NULL)
    {
      names[i] = strndup(tmp->name, strlen(tmp->name));
      i++;
    }

  corsaro_plugin_manager_free(tmp_manager);

  *plugin_names = names;
  return i;
}

void corsaro_free_plugin_names(char **plugin_names, int plugin_cnt)
{
  int i;
  if(plugin_names == NULL)
    {
      return;
    }
  for(i = 0; i < plugin_cnt; i++)
    {
      if(plugin_names[i] != NULL)
	{
	  free(plugin_names[i]);
	}
      plugin_names[i] = NULL;
    }
  free(plugin_names);
}

uint64_t corsaro_get_accepted_packets(corsaro_t *corsaro)
{
  return corsaro->accepted_pkts == UINT64_MAX ? 
    UINT64_MAX : 
    trace_get_accepted_packets(corsaro->trace) - corsaro->accepted_pkts;
}

uint64_t corsaro_get_dropped_packets(corsaro_t *corsaro)
{
  return corsaro->dropped_pkts == UINT64_MAX ? 
    UINT64_MAX : 
    trace_get_dropped_packets(corsaro->trace) - corsaro->dropped_pkts;
}

const char *corsaro_get_traceuri(corsaro_t *corsaro)
{
  return corsaro->uridata;
}

int corsaro_set_monitorname(corsaro_t *corsaro, char *name)
{
  assert(corsaro != NULL);

  if(corsaro->started != 0)
    {
      corsaro_log(__func__, corsaro, 
		"monitor name can only be set before "
		"corsaro_start_output is called");
      return -1;
    }

  if(corsaro->monitorname != NULL)
    {
      corsaro_log(__func__, corsaro, 
		  "updating monitor name from %s to %s",
		  corsaro->monitorname, name);
    }
  else
    {
      corsaro_log(__func__, corsaro, "setting monitor name to %s",
		  name);
    }

  if((corsaro->monitorname = strdup(name)) == NULL)
    {
      corsaro_log(__func__, corsaro, 
		"could not duplicate monitor name string (no memory?)");
      return -1;
    }
  corsaro_log(__func__, corsaro, "%s", corsaro->monitorname);
  return 0;
}

const char *corsaro_get_monitorname(corsaro_t *corsaro)
{
  return corsaro->monitorname;
}

int corsaro_per_packet(corsaro_t *corsaro, libtrace_packet_t *ltpacket)
{
  struct timeval ts;
  struct timeval report;

  assert(corsaro != NULL);
  assert(corsaro->started == 1 && "corsaro_start_output must be called before"
	 "packets can be processed");

  /* poke this ltpacket into our corsaro packet */
  corsaro->packet->ltpacket = ltpacket;

  /* ensure that the state is clear */
  corsaro_packet_state_reset(corsaro->packet);
  
  /* this is now the latest packet we have seen */
  corsaro->last_ts = ts = trace_get_timeval(ltpacket);

  /* it also means we need to dump an interval end record */
  corsaro->interval_end_needed = 1;

  /* if this is the first acket we record, keep the timestamp */
  if(corsaro->packet_cnt == 0)
    {
      corsaro->first_ts = ts;
      if(start_interval(corsaro, ts) != 0)
	{
	  corsaro_log(__func__, corsaro, "could not start interval at %ld", 
		    ts.tv_sec);
	  return -1;
	}
      
      corsaro->next_report = ts.tv_sec + corsaro->interval;

      /* if we are aligning our intervals, truncate the end down */
      if(corsaro->interval_align == CORSARO_INTERVAL_ALIGN_YES)
	{
	  corsaro->next_report = (corsaro->next_report/corsaro->interval)
	    *corsaro->interval;
	}
    }

  /* using an interval value of less than zero disables intervals
     such that only one distribution will be generated upon completion */
  while(corsaro->interval >= 0 && (uint32_t)ts.tv_sec >= corsaro->next_report)
    {
      /* we want to mark the end of the interval such that all pkt times are <=
	 the time of the end of the interval. 
	 because we deal in second granularity, we simply subtract one from the
	 time */
      report.tv_sec = corsaro->next_report-1;
      
      if(end_interval(corsaro, report) != 0)
	{
	  corsaro_log(__func__, corsaro, "could not end interval at %ld", 
		    ts.tv_sec);
	  /* we don't free in case the client wants to try to carry on */
	  return -1;
	}

      corsaro->interval_start.number++;

      /* we now add the second back on to the time to get the start time */
      report.tv_sec = corsaro->next_report;
      if(start_interval(corsaro, report) != 0)
	{
	  corsaro_log(__func__, corsaro, "could not start interval at %ld", 
		    ts.tv_sec);
	  /* we don't free in case the client wants to try to carry on */
	  return -1;
	}
      corsaro->next_report += corsaro->interval;
    }

  /* count this packet for our overall packet count */
  corsaro->packet_cnt++;

  /* ask each plugin to process this packet */
  return process_packet(corsaro, corsaro->packet);
}

int corsaro_per_record(corsaro_t *corsaro, 
		       corsaro_in_record_type_t type,
		       corsaro_in_record_t *record)
{
  corsaro_interval_t *interval = NULL;

#ifdef WITH_PLUGIN_SIXT
  corsaro_flowtuple_t *flowtuple = NULL;
  corsaro_flowtuple_class_start_t *class_start = NULL;
  corsaro_flowtuple_class_end_t *class_end = NULL;
#endif

  /* we want trigger interval start/end events based on the incoming
     interval start and end */
  if(type == CORSARO_IN_RECORD_TYPE_IO_INTERVAL_START)
    {
      interval = (corsaro_interval_t *)
	corsaro_in_get_record_data(record);
      
      return per_interval_start(corsaro, interval);
    }
  else if(type == CORSARO_IN_RECORD_TYPE_IO_INTERVAL_END)
    {
      interval = (corsaro_interval_t *)
	corsaro_in_get_record_data(record);
      
      return per_interval_end(corsaro, interval);
    }
#ifdef WITH_PLUGIN_SIXT
  else if(type == CORSARO_IN_RECORD_TYPE_FLOWTUPLE_FLOWTUPLE)
    {
      flowtuple = (corsaro_flowtuple_t *)
	corsaro_in_get_record_data(record);
      
      return per_flowtuple(corsaro, flowtuple);
    }
  else if(type == CORSARO_IN_RECORD_TYPE_FLOWTUPLE_CLASS_START)
    {
      class_start = (corsaro_flowtuple_class_start_t *)
	corsaro_in_get_record_data(record);
      
      return per_flowtuple_class_start(corsaro, class_start);
    }
  else if(type == CORSARO_IN_RECORD_TYPE_FLOWTUPLE_CLASS_END)
    {
      class_end = (corsaro_flowtuple_class_end_t *)
	corsaro_in_get_record_data(record);
      
      return per_flowtuple_class_end(corsaro, class_end);
    }
#endif

  return 0;
}

int corsaro_finalize_output(corsaro_t *corsaro)
{
  if(corsaro == NULL)
    {
      return 0;
    }
  if(corsaro->started != 0)
    {
      if(corsaro->interval_end_needed != 0 && 
	 end_interval(corsaro, corsaro->last_ts) != 0)
	{
	  corsaro_log(__func__, corsaro, "could not end interval at %ld", 
		      corsaro->last_ts.tv_sec);
	  corsaro_free(corsaro);
	  return -1;
	}
      if(corsaro->global_file != NULL &&
	 corsaro_io_write_trailer(corsaro, corsaro->global_file, NULL) <= 0)
	{
	  corsaro_log(__func__, corsaro, 
		      "could not write global trailers");
	  corsaro_free(corsaro);
	  return -1;
	}
    }
  
  corsaro_free(corsaro);
  return 0;
}

/* ===== corsaro_in API functions ===== */

corsaro_in_t *corsaro_alloc_input(const char *corsarouri)
{
  corsaro_in_t *corsaro;
  
  /* initialize the corsaro object */
  if((corsaro = corsaro_in_init(corsarouri)) == NULL)
    {
      corsaro_log_in(__func__, NULL, "could not initialize corsaro_in object");
      return NULL;
    }

  return corsaro;
}

int corsaro_start_input(corsaro_in_t *corsaro)
{
  corsaro_plugin_t *p = NULL;

  assert(corsaro != NULL);
  assert(corsaro->started == 0);
  assert(corsaro->plugin == NULL);

  /* open the file! */
  if((corsaro->file = corsaro_file_ropen(corsaro->uridata)) == NULL)
    {
      corsaro_log_in(__func__, corsaro, "could not open input file %s", 
		   corsaro->uridata);
      /* ak comments the following, leave it up to the caller to free
	 the state object */
      /*corsaro_in_free(corsaro);*/
      return -1;
    }

  /* determine the plugin which created this file */
  while((p = corsaro_plugin_next(corsaro->plugin_manager, p)) != NULL && 
	corsaro->plugin == NULL)
    {
      if(p->probe_filename(corsaro->uridata) == 1)
	{
	  corsaro_log_in(__func__, corsaro, 
		       "%s plugin selected to read %s (using file name)", 
		       p->name, corsaro->uridata);
	  corsaro->plugin = p;
	}
    }

  /* if the previous method for detection failed, lets try peeking into
     the file */
  p = NULL;
  while(corsaro->plugin == NULL && 
	(p = corsaro_plugin_next(corsaro->plugin_manager, p)) != NULL)
    {
      if(p->probe_magic(corsaro, corsaro->file) == 1)
	{
	  corsaro_log_in(__func__, corsaro, 
		       "%s plugin selected to read %s (using magic)",
		       p->name, corsaro->uridata);
	  corsaro->plugin = p;
	}
    }

  /* if corsaro->plugin is still NULL, see if this is the global output */
  if(corsaro->plugin == NULL)
    {
      if(check_global_filename(corsaro->uridata) != 1 && 
	 check_global_magic(corsaro, corsaro->file) != 1)
	{
	  /* we have no idea what this file was created by */
	  corsaro_log_in(__func__, corsaro, 
			 "unable to find plugin to decode %s\n"
		       " - is this a corsaro file?\n"
		       " - is corsaro compiled with all needed plugins?", 
		       corsaro->uridata);
	  return -1;
	}
      else
	{
	  /* this the corsaro global output */
	  /* we initially expect an corsaro header record in a global file */
	  corsaro->expected_type = CORSARO_IN_RECORD_TYPE_IO_HEADER;
	  corsaro_log_in(__func__, corsaro, "corsaro_global selected to read %s",
			 corsaro->uridata);
	}
    }
  else
    {
      /* start up the plugin we detected */
      if(corsaro->plugin->init_input(corsaro) != 0)
	{
	  corsaro_log_in(__func__, corsaro, "could not initialize %s", 
		       corsaro->plugin->name);
	  return -1;
	}
    }

  corsaro->started = 1;

  return 0;
}

corsaro_in_record_t *corsaro_in_alloc_record(corsaro_in_t *corsaro)
{
  corsaro_in_record_t *record = NULL;

  if((record = malloc(sizeof(corsaro_in_record_t))) == NULL)
    {
      corsaro_log_in(__func__, corsaro, "could not malloc corsaro_in_record_t");
      return NULL;
    }

  record->corsaro = corsaro;

  /* pre-allocate some memory for the buffer */
  if((record->buffer = malloc(sizeof(uint8_t)*
			      CORSARO_IN_RECORD_DEFAULT_BUFFER_LEN)) == NULL)
    {
      corsaro_log_in(__func__, corsaro, "could not malloc record buffer");
      corsaro_in_free_record(record);
      return NULL;
    }

  record->buffer_len = CORSARO_IN_RECORD_DEFAULT_BUFFER_LEN;

  record->type = -1;

  return record;
}

void corsaro_in_free_record(corsaro_in_record_t *record)
{
  if(record == NULL)
    {
      corsaro_log_file(__func__, NULL, "possible double free of record pointer");
      return;
    }

  if(record->buffer != NULL)
    {
      free(record->buffer);
      record->buffer = NULL;
    }
  record->buffer_len = -1;

  record->type = -1;

  free(record);
}

off_t corsaro_in_read_record(corsaro_in_t *corsaro,
			 corsaro_in_record_type_t *record_type,
			 corsaro_in_record_t *record)
{
  /* the first check makes sure we don't have a plugin that we have assigned
     the file to, the second ensures that if there is a plugin, we will
     only directly use it when we're not in global mode */
  if(corsaro->plugin != NULL 
     && corsaro->expected_type == CORSARO_IN_RECORD_TYPE_NULL)
    {
      return corsaro->plugin->read_record(corsaro, record_type, record);
    }
  else
    {
      /* this is the global plugin, handle it ourselves */
      return read_record(corsaro, record_type, record);
    }
  
  return -1;
}

void *corsaro_in_get_record_data(corsaro_in_record_t *record)
{
  return record->buffer;
}

int corsaro_finalize_input(corsaro_in_t *corsaro)
{
  corsaro_in_free(corsaro);
  return 0;
}
