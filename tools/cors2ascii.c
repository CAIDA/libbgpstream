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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "libtrace.h"

#include "corsaro.h"
#include "corsaro_log.h"
#include "corsaro_io.h"

/** @file
 *
 * @brief Code which uses libcorsaro to convert an corsaro output file to ascii
 *
 * @author Alistair King
 *
 */

/** The corsaro_in object for reading the input file */
static corsaro_in_t *corsaro = NULL;

/** The record object to read into */
static corsaro_in_record_t *record = NULL;

/** Cleanup and free state */
static void clean()
{ 
  if(record != NULL)
    {
      corsaro_in_free_record(record);
      record = NULL;
    }

  if(corsaro != NULL)
    {
      corsaro_finalize_input(corsaro);
      corsaro = NULL;
    }
}

/** Initialize a corsaro_in object for the given file name */
static int init_corsaro(char *corsarouri)
{
  /* get an corsaro_in object */
  if((corsaro = corsaro_alloc_input(corsarouri)) == NULL)
    {
      fprintf(stderr, "could not alloc corsaro_in\n");
      clean();
      return -1;
    }
  
  /* get a record */
  if ((record = corsaro_in_alloc_record(corsaro)) == NULL) {
    fprintf(stderr, "could not alloc record\n");
    clean();
    return -1;
  }

  /* start corsaro */
  if(corsaro_start_input(corsaro) != 0)
    {
      fprintf(stderr, "could not start corsaro\n");
      clean();
      return -1;
    }

  return 0;
}

/** Print usage information to stderr */
static void usage(const char *name)
{
  fprintf(stderr, 
	  "usage: %s input_file\n", name);
}

/** Entry point for the cors2ascii tool */
int main(int argc, char *argv[])
{ 	
  char *file = NULL;

  corsaro_in_record_type_t type = CORSARO_IN_RECORD_TYPE_NULL;
  off_t len = 0;

  if(argc != 2)
    {
      usage(argv[0]);
      exit(-1);
    }

  /* argv[1] is the corsaro file */	
  file = argv[1];

  /* this must be done before corsaro_init_output */
  if(init_corsaro(file) != 0)
    {
      fprintf(stderr, "failed to init corsaro\n");
      clean();
      return -1;
    }
  
  while ((len = corsaro_in_read_record(corsaro, &type, record)) > 0) {
    if(type == CORSARO_IN_RECORD_TYPE_NULL)
      {
	clean();
	return -1;
      }
    
    corsaro_io_print_record(corsaro->plugin_manager, type, record);

    /* reset the type to NULL to indicate we don't care */
    type = CORSARO_IN_RECORD_TYPE_NULL;
  }

  if(len < 0)
    {
      fprintf(stderr, "corsaro_in_read_record failed to read record\n");
      clean();
      return -1;
    }

  clean();
  return 0;
}
