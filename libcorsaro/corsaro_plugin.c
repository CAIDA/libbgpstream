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
#include <stdlib.h>
#include <string.h>

#include "parse_cmd.h"
#include "utils.h"

#include "corsaro_log.h"

#include "corsaro_plugin.h"

/* include the plugins that have been enabled */
#ifdef WITH_PLUGIN_PCAP
#include "corsaro_pcap.h"
#endif

#ifdef WITH_PLUGIN_SIXT
#include "corsaro_flowtuple.h"
#endif

#ifdef WITH_PLUGIN_DOS
#include "corsaro_dos.h"
#endif

#ifdef WITH_PLUGIN_SMEE
#include "corsaro_smee.h"
#endif

#ifdef WITH_PLUGIN_ANON
#include "corsaro_anon.h"
#endif

#ifdef WITH_PLUGIN_IPMETA
#include "corsaro_ipmeta.h"
#endif

#ifdef WITH_PLUGIN_FILTERGEO
#include "corsaro_filtergeo.h"
#endif

#ifdef WITH_PLUGIN_FILTERPFX
#include "corsaro_filterpfx.h"
#endif

#ifdef WITH_PLUGIN_FILTERBPF
#include "corsaro_filterbpf.h"
#endif

/*
 * add new plugin includes below using:
 *
 * #ifdef WITH_PLUGIN_<macro_name>
 * #include "<full_name>.h"
 * #endif
 */


/** Shortcut to get the logfile pointer from the manager */
#define LOG(manager) (manager->logfile)

#define MAXOPTS 1024

#define PLUGIN_INIT_ADD(plugin)					  \
  {								  \
    tail = add_plugin(manager, tail, plugin##_alloc(),	  \
		      "##plugin##");				  \
    if(list == NULL)						  \
      {								  \
	list = tail;						  \
      }								  \
    plugin_cnt++;						  \
  }

#ifdef DEBUG
#ifdef ED_PLUGIN_INIT_ALL_ENABLED
static void corsaro_plugin_verify(corsaro_plugin_t *plugin)
{
  /* some sanity checking to make sure this plugin has been implemented
     with the features we need. #if 0 this for production */
  assert(plugin != NULL);
  assert(plugin->name != NULL);
  assert(plugin->id > 0 && plugin->id <= CORSARO_PLUGIN_ID_MAX);
  assert(plugin->magic > 0x010101); /* this is a pretty rough check */
  assert(plugin->probe_filename != NULL);
  assert(plugin->probe_magic != NULL);
  assert(plugin->init_input != NULL);
  assert(plugin->init_output != NULL);
  assert(plugin->close_input != NULL);
  assert(plugin->close_output != NULL);
  assert(plugin->read_record != NULL);
  assert(plugin->read_global_data_record != NULL);
  assert(plugin->start_interval != NULL);
  assert(plugin->end_interval != NULL);
  assert(plugin->process_packet != NULL);

  /* don't set the next plugin yourself... duh */
  assert(plugin->next == NULL);
}
#endif
#endif

#ifdef ED_PLUGIN_INIT_ALL_ENABLED
static corsaro_plugin_t *add_plugin(corsaro_plugin_manager_t *manager,
				  corsaro_plugin_t *tail,
				  corsaro_plugin_t *p,
				  const char *name)
{
  corsaro_plugin_t *plugin = NULL;

  /* before we add this plugin, let's check that the user wants it */
  if(corsaro_plugin_is_enabled(manager, p) == 0)
    {
      return tail;
    }

  /* we assume that we need to copy the plugin structure that the plugin
     gives us. this allows us to use the same plugin twice at once */
  if((plugin = malloc(sizeof(corsaro_plugin_t))) == NULL)
    {
      corsaro_log_file(__func__, NULL, "could not malloc plugin");
      return NULL;
    }
  memcpy(plugin, p, sizeof(corsaro_plugin_t));

  /* make sure it initialized */
  if(plugin == NULL)
    {
      corsaro_log_file(__func__, LOG(manager),
		       "%s plugin failed to initialize", name);
      return tail;
    }

#ifdef DEBUG
  corsaro_plugin_verify(plugin);
#endif

  /* create the default argv for the plugin */
  plugin->argc = 1;
  /* malloc the pointers for the array */
  if((plugin->argv = malloc(sizeof(char*) * (plugin->argc+1))) == NULL)
    {
      corsaro_log_file(__func__, LOG(manager), "could not malloc plugin argv");
      return NULL;
    }
  plugin->argv[0] = strndup(plugin->name, strlen(plugin->name));
  plugin->argv[1] = NULL;

  if(tail != NULL)
    {
      assert(tail->next == NULL);
      tail->next = plugin;
    }
  /*plugin->next = head;*/
  return plugin;
}
#endif

static int populate_plugin_arrays(corsaro_plugin_manager_t *manager,
				  int plugin_cnt,
				  corsaro_plugin_t *plugin_list)
{
  corsaro_plugin_t *tmp = NULL;

  if(plugin_cnt == 0)
    {
      corsaro_log_file(__func__, LOG(manager),
		       "WARNING: No plugins are initialized");
    }
  else
    {
      /* build the plugins array */
      if((manager->plugins = malloc_zero(sizeof(corsaro_plugin_t*)
                                         *(CORSARO_PLUGIN_ID_MAX))) == NULL)
        {
          corsaro_log_file(__func__, LOG(manager),
			   "could not malloc plugin array");
          return -1;
        }
      /* allocate the plugin state array */
      if((manager->plugins_state = malloc_zero(sizeof(void*)
                                               *(CORSARO_PLUGIN_ID_MAX))) == NULL)
        {
          corsaro_log_file(__func__, LOG(manager),
			   "could not malloc plugin state array");
          return -1;
        }
      for(tmp=plugin_list;tmp!=NULL;tmp=tmp->next)
        {
          manager->plugins[tmp->id-1] = tmp;
        }
      manager->first_plugin = plugin_list;
      manager->plugins_cnt = plugin_cnt;
    }
  return 0;
}

static int copy_argv(corsaro_plugin_t *plugin, int argc, char *argv[])
{
  int i;
  plugin->argc = argc;

  /* malloc the pointers for the array */
  if((plugin->argv = malloc(sizeof(char*) * (plugin->argc+1))) == NULL)
    {
      return -1;
    }

  for (i = 0; i < plugin->argc; i++) {
    if((plugin->argv[i] = malloc(strlen(argv[i]) + 1)) == NULL)
      {
	return -1;
      }
    strncpy(plugin->argv[i], argv[i], strlen(argv[i]) + 1);
  }

  /* as per ANSI spec, the last element in argv must be a NULL pointer */
  /* can't find the actual spec, but http://en.wikipedia.org/wiki/Main_function
     as well as other sources confirm this is standard */
  plugin->argv[plugin->argc] = NULL;

  return 0;
}

/** ==== PUBLIC API FUNCTIONS BELOW HERE ==== */

corsaro_plugin_manager_t *corsaro_plugin_manager_init(corsaro_file_t *logfile)
{
  corsaro_plugin_manager_t *manager = NULL;

  corsaro_plugin_t *list = NULL;
#ifdef ED_PLUGIN_INIT_ALL_ENABLED
  corsaro_plugin_t *tail = NULL;
#endif
  int plugin_cnt = 0;

  /* allocate the manager structure */
  if((manager = malloc_zero(sizeof(corsaro_plugin_manager_t))) == NULL)
    {
      corsaro_log_file(__func__, logfile, "failed to malloc plugin manager");
      return NULL;
    }

  /* set the log file */
  manager->logfile = logfile;

  /* NOTE: the order that plugins are listed in configure.ac
     will be the order that they are run. */

  /* this macro is generated by ED_WITH_PLUGIN macro calls in configure.ac */
  /* basically, what will happen is that an ordered linked list of plugins
     will be built, which we will then turn into an array and store in the
     manager structure. this allows plugins to be accessed in both
     sequential and random access fashion */

#ifdef ED_PLUGIN_INIT_ALL_ENABLED
  ED_PLUGIN_INIT_ALL_ENABLED
#endif

    if(populate_plugin_arrays(manager, plugin_cnt, list) != 0)
      {
	corsaro_plugin_manager_free(manager);
	return NULL;
      }

  return manager;
}

int corsaro_plugin_manager_start(corsaro_plugin_manager_t *manager)
{
  int i;
  corsaro_plugin_t *p = NULL;
  corsaro_plugin_t *tail = NULL;
  corsaro_plugin_t *head = NULL;

  if(manager->plugins_enabled != NULL)
    {
      /* we need to go through the list of plugins and recreate the list
	 with only plugins which are in the plugins_enabled array */
      for(i = 0; i < manager->plugins_enabled_cnt; i++)
	{
	  p = manager->plugins[manager->plugins_enabled[i]-1];

	  /* if this is the first enabled plugin, then this will be the head */
	  if(head == NULL)
	    {
	      head = p;
	    }

	  /* if there was a plugin before, connect it to this one */
	  if(tail != NULL)
	    {
	      tail->next = p;
	    }

	  /* make this the last plugin in the list (so far) */
	  tail = p;

	  /* disconnect the rest of the list */
	  tail->next = NULL;
	}

      /* NB we dont need to free any unused plugins as the all plugins
	 get free'd anyway when the manager gets free'd */

      /* we now have a list starting at head */
      manager->first_plugin = head;

    }

  return 0;
}

void corsaro_plugin_manager_free(corsaro_plugin_manager_t *manager)
{
  int i, j;

  assert(manager != NULL);

  if(manager->plugins != NULL)
    {
      /* each plugin MUST already have been closed by now */
      for(i = 0; i < CORSARO_PLUGIN_ID_MAX; i++)
	{
	  if(manager->plugins[i] != NULL)
	    {
	      /* free the argument strings */
	      for(j = 0; j < manager->plugins[i]->argc; j++)
		{
		  free(manager->plugins[i]->argv[j]);
		  manager->plugins[i]->argv[j] = NULL;
		}
	      free(manager->plugins[i]->argv);
	      manager->plugins[i]->argv = NULL;
	      manager->plugins[i]->argc = 0;

	      free(manager->plugins[i]);
	      manager->plugins[i] = NULL;
	    }
	}
      free(manager->plugins);
      manager->plugins = NULL;
    }

  if(manager->plugins_state != NULL)
    {
      /* as above, state will be freed by calls to the each plugins close
	 function */
      free(manager->plugins_state);
      manager->plugins_state = NULL;
    }

  if(manager->plugins_enabled != NULL)
    {
      free(manager->plugins_enabled);
      manager->plugins_enabled = NULL;
    }

  manager->first_plugin = NULL;
  manager->plugins_cnt = 0;

  /* we are NOT responsible for the logfile pointer */
  /* should maybe think about making files use reference counting? */

  free(manager);
}

corsaro_plugin_t *corsaro_plugin_get_by_id(corsaro_plugin_manager_t *manager,
					   int id)
{
  assert(manager != NULL);
  if(id < 0 || id > CORSARO_PLUGIN_ID_MAX)
    {
      return NULL;
    }

  return manager->plugins[id-1];
}

corsaro_plugin_t *corsaro_plugin_get_by_magic(corsaro_plugin_manager_t *manager,
					      uint32_t magic)
{
  assert(manager != NULL);
  corsaro_plugin_t *p = NULL;
  while((p = corsaro_plugin_next(manager, p)) != NULL)
    {
      if(p->magic == magic)
	{
	  return p;
	}
    }
  return NULL;
}

corsaro_plugin_t *corsaro_plugin_get_by_name(corsaro_plugin_manager_t *manager,
					     const char *name)
{
  corsaro_plugin_t *p = NULL;
  while((p = corsaro_plugin_next(manager, p)) != NULL)
    {
      if(strlen(name) == strlen(p->name) &&
	 strncasecmp(name, p->name, strlen(p->name)) == 0)
	{
	  return p;
	}
    }
  return NULL;
}

corsaro_plugin_t *corsaro_plugin_next(corsaro_plugin_manager_t *manager,
				  corsaro_plugin_t *plugin)
{
  /* plugins have already been freed, or not init'd */
  assert(manager != NULL && manager->plugins != NULL
	 && manager->plugins_cnt != 0);

  /* they are asking for the beginning of the list */
  if(plugin == NULL)
    {
      return manager->first_plugin;
    }

  /* otherwise, give them the next one */
  return plugin->next;
}

void corsaro_plugin_register_state(corsaro_plugin_manager_t *manager,
				 corsaro_plugin_t *plugin,
				 void *state)
{
  assert(manager != NULL);
  assert(plugin != NULL);
  assert(state != NULL);

  manager->plugins_state[plugin->id-1] = state;
}

void corsaro_plugin_free_state(corsaro_plugin_manager_t *manager,
			     corsaro_plugin_t *plugin)
{
  assert(manager != NULL);
  assert(plugin != NULL);

  free(manager->plugins_state[plugin->id-1]);
  manager->plugins_state[plugin->id-1] = NULL;
}

int corsaro_plugin_probe_filename(const char *fname, corsaro_plugin_t *plugin)
{
  assert(fname != NULL);
  assert(plugin != NULL && plugin->name != NULL);

  if(strstr(fname, plugin->name) != NULL)
    {
      return 1;
    }
  return 0;
}

const char *corsaro_plugin_get_name_by_id(corsaro_plugin_manager_t *manager,
					  int id)
{
  corsaro_plugin_t *plugin = NULL;
  if((plugin = corsaro_plugin_get_by_id(manager, id)) == NULL)
    {
      return NULL;
    }
  return plugin->name;
}

const char *corsaro_plugin_get_name_by_magic(corsaro_plugin_manager_t *manager,
					     uint32_t magic)
{
  corsaro_plugin_t *plugin = NULL;
  if((plugin = corsaro_plugin_get_by_magic(manager, magic)) == NULL)
    {
      return NULL;
    }
  return plugin->name;
}

int corsaro_plugin_is_enabled(corsaro_plugin_manager_t *manager,
			      corsaro_plugin_t *plugin)
{
  int i;

  if(manager->plugins_enabled == NULL)
    {
      /* no plugins have been explicitly enabled, therefore all plugins
	 are implicitly enabled */
      return 1;
    }

  for(i=0; i<manager->plugins_enabled_cnt; i++)
    {
      if(manager->plugins_enabled[i] == plugin->id)
	{
	  return 1;
	}
    }

  return 0;
}

int corsaro_plugin_enable_plugin(corsaro_plugin_manager_t *manager,
				 const char *plugin_name,
				 const char *plugin_args)
{
  corsaro_plugin_t *plugin = NULL;
  int i;
  char *local_args = NULL;
  char *process_argv[MAXOPTS];
  int process_argc = 0;

  assert(manager != NULL);

  /* first, let us find the plugin with this name */
  if((plugin = corsaro_plugin_get_by_name(manager, plugin_name)) == NULL)
    {
      corsaro_log_file(__func__, LOG(manager),
		       "No plugin found with the name '%s'", plugin_name);
      corsaro_log_file(__func__, LOG(manager),
		       "Is corsaro compiled with all necessary plugins?");
      return -1;
    }

  corsaro_log(__func__, NULL, "enabling %s", plugin_name);

  /* now lets set the arguments for the plugin */
  /* we do this here, before we check if it is enabled to allow the args
     to be re-set, so long as it is before the plugin is started */
  if(plugin_args != NULL && strlen(plugin_args) > 0)
    {
      /* parse the args into argc and argv */
      local_args = strdup(plugin_args);
      parse_cmd(local_args, &process_argc, process_argv, MAXOPTS, plugin_name);
    }

  /* remove the default arguments from the plugin (but only if new ones have
     been given to us */
  assert(plugin->argv != NULL && plugin->argc == 1);
  if(process_argc > 0)
    {
      free(plugin->argv[0]);
      free(plugin->argv);
      plugin->argc = 0;

      if(copy_argv(plugin, process_argc, process_argv) != 0)
	{
	  if(local_args != NULL)
	    {
	      corsaro_log(__func__, NULL, "freeing local args");
	      free(local_args);
	    }
	  return -1;
	}

      if(local_args != NULL)
	{
	  /* this is the storage for the strings until copy_argv is complete */
	  free(local_args);
	}
    }

  /* now we need to ensure it isn't already enabled */
  for(i = 0; i < manager->plugins_enabled_cnt; i++)
    {
      if(plugin->id == manager->plugins_enabled[i])
	{
	  return 0;
	}
    }

  /* now we need to remalloc the array */
  if((manager->plugins_enabled = realloc(manager->plugins_enabled,
					 sizeof(uint16_t)*
					 (manager->plugins_enabled_cnt+1)
					 )) == NULL)
    {
      corsaro_log_file(__func__, LOG(manager),
		       "could not extend the enabled plugins array");
      return -1;
    }

  /* now we shall store this id in this spot! */
  manager->plugins_enabled[manager->plugins_enabled_cnt++] = plugin->id;

  return 0;
}
