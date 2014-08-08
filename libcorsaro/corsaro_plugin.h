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

#ifndef __CORSARO_PLUGIN_H
#define __CORSARO_PLUGIN_H

#include "corsaro_int.h"

/** @file
 *
 * @brief Header file dealing with the corsaro plugin manager
 *
 * @author Alistair King
 *
 */

/** Convenience macro that defines all the function prototypes for the corsaro
 * plugin API
 *
 * @todo split this into corsaro-out and corsaro-in macros
 */
#define CORSARO_PLUGIN_GENERATE_PROTOS(plugin)				\
  corsaro_plugin_t * plugin##_alloc();					\
  int plugin##_probe_filename(const char *fname);			\
  int plugin##_probe_magic(struct corsaro_in * corsaro, corsaro_file_in_t *file); \
  int plugin##_init_input(struct corsaro_in *corsaro);			\
  int plugin##_init_output(struct corsaro *corsaro);			\
  int plugin##_close_input(struct corsaro_in *corsaro);			\
  int plugin##_close_output(struct corsaro *corsaro);			\
  off_t plugin##_read_record(struct corsaro_in *corsaro,			\
			   enum corsaro_in_record_type *record_type,	\
			   struct corsaro_in_record *record);		\
  off_t plugin##_read_global_data_record(struct corsaro_in *corsaro,	\
			   enum corsaro_in_record_type *record_type,	\
			   struct corsaro_in_record *record);		\
  int plugin##_start_interval(struct corsaro *corsaro,			\
			      struct corsaro_interval *int_start);	\
  int plugin##_end_interval(struct corsaro *corsaro,			\
			    struct corsaro_interval *int_end);		\
  int plugin##_process_packet(struct corsaro *corsaro,			\
			      struct corsaro_packet *packet);

#ifdef WITH_PLUGIN_SIXT
#define CORSARO_PLUGIN_GENERATE_FT_PROTO(plugin)			\
  int plugin##_process_flowtuple(struct corsaro *corsaro,		\
				 corsaro_flowtuple_t *flowtuple,	\
				 struct corsaro_packet_state *state);	\
  int plugin##_process_flowtuple_class_start(struct corsaro *corsaro,	\
					     corsaro_flowtuple_class_start_t *class); \
  int plugin##_process_flowtuple_class_end(struct corsaro *corsaro,	\
					   corsaro_flowtuple_class_end_t *class);
#endif

/** Convenience macro that defines all the function pointers for the corsaro
 * plugin API
 *
 * If the FlowTuple plugin is enabled, we by default set the pointer to NULL
 * and expect plugins which support this feature to set this manually
 *
 * @todo split this into corsaro-out and corsaro-in macros
 */
#ifdef WITH_PLUGIN_SIXT
#define CORSARO_PLUGIN_GENERATE_PTRS(plugin)		\
  plugin##_probe_filename,				\
    plugin##_probe_magic,				\
    plugin##_init_input,				\
    plugin##_init_output,				\
    plugin##_close_input,				\
    plugin##_close_output,				\
    plugin##_read_record,				\
    plugin##_read_global_data_record,			\
    plugin##_start_interval,				\
    plugin##_end_interval,				\
    plugin##_process_packet,			        \
    NULL,						\
    NULL,						\
    NULL

#define CORSARO_PLUGIN_GENERATE_PTRS_FT(plugin)	\
  plugin##_probe_filename,				\
    plugin##_probe_magic,				\
    plugin##_init_input,				\
    plugin##_init_output,				\
    plugin##_close_input,				\
    plugin##_close_output,				\
    plugin##_read_record,				\
    plugin##_read_global_data_record,			\
    plugin##_start_interval,				\
    plugin##_end_interval,				\
    plugin##_process_packet,			        \
    plugin##_process_flowtuple,				\
    plugin##_process_flowtuple_class_start,		\
    plugin##_process_flowtuple_class_end
#else
#define CORSARO_PLUGIN_GENERATE_PTRS(plugin)		\
  plugin##_probe_filename,				\
    plugin##_probe_magic,				\
    plugin##_init_input,				\
    plugin##_init_output,				\
    plugin##_close_input,				\
    plugin##_close_output,				\
    plugin##_read_record,				\
    plugin##_read_global_data_record,			\
    plugin##_start_interval,				\
    plugin##_end_interval,				\
    plugin##_process_packet			        
#endif

/** Convenience macro that defines all the 'remaining' blank fields in a corsaro
 *  plugin object 
 *
 *  This becomes useful if we add more fields to the end of the plugin
 *  structure, because each plugin does not need to be updated in order to
 *  correctly 'zero' these fields.
 */
#define CORSARO_PLUGIN_GENERATE_TAIL		\
  NULL,				/* next */	\
    0,				/* argc */	\
    NULL                        /* argv */

/** Convenience macro to cast the state pointer in the plugin 
 *
 * Plugins should use extend this macro to provide access to their state
 */
#define CORSARO_PLUGIN_STATE(corsaro, type, id)			\
  ((struct corsaro_##type##_state_t*)				\
   ((corsaro)->plugin_manager->plugins_state[(id)-1]))

/** Convenience macro to get this plugin from corsaro
 *
 * Plugins should use extend this macro to provide access to themself
 */
#define CORSARO_PLUGIN_PLUGIN(corsaro, id)						\
  ((corsaro)->plugin_manager->plugins[(id)-1])

/** A unique identifier for a plugin, used when writing binary data 
 *
 * @note this identifier does not affect the order in which plugins are
 *       passed packets. Plugin precedence is determined either by the
 *       order of the ED_WITH_PLUGIN macros in configure.ac, or by the
 *       order of the plugins that have been explicitly enabled using
 *       \ref corsaro_enable_plugin
 */
typedef enum corsaro_plugin_id
{
  /** 
   * Pass-through PCAP plugin
   * 
   * Allows Corsaro to be used to capture PCAP files from a live interface.
   * This should always be the highest priority plugin 
   */
  CORSARO_PLUGIN_ID_PCAP             = 1,
  
  /** IP address anonymization plugin */
  CORSARO_PLUGIN_ID_ANON             = 2,

  /** libipmeta lookup plugin */
  CORSARO_PLUGIN_ID_IPMETA           = 3,

  /** FilterGeo plugin */
  CORSARO_PLUGIN_ID_FILTERGEO        = 6,

  /** FilterPfx plugin */
  CORSARO_PLUGIN_ID_FILTERPFX        = 7,

  /** FilterBPF plugin */
  CORSARO_PLUGIN_ID_FILTERBPF        = 8,

  /** FlowTuple plugin */
  CORSARO_PLUGIN_ID_FLOWTUPLE        = 20,

  /** RS DoS plugin */
  CORSARO_PLUGIN_ID_DOS              = 30,

  /** Smee plugin */
  CORSARO_PLUGIN_ID_SMEE             = 80,

  /** Tag stats plugin */
  CORSARO_PLUGIN_ID_TAGSTATS         = 90,

  /** Maximum plugin ID assigned */
  CORSARO_PLUGIN_ID_MAX              = CORSARO_PLUGIN_ID_TAGSTATS
} corsaro_plugin_id_t;

/** An corsaro packet processing plugin */
/* All functions should return -1, or NULL on failure */
typedef struct corsaro_plugin
{
  /** The name of this plugin used in the ascii output and eventually to allow
   * plugins to be enabled and disabled */
  const char *name;

#if 0
  /** The version of this plugin */
  /** This will be encoded in the magic number unfortunately */
  /** @todo actually implement this */
  /*const char *version;*/
#endif

  /** The corsaro plugin id for this plugin */
  const corsaro_plugin_id_t id;

  /** The magic number for this plugin's data */
  const uint32_t magic;

  /** Given a filename, return if this is the most likely plugin.
   * Used to "guess" the plugin when it is not specified.
   *
   * @param fname         The name of the device or file to examine
   * @return 1 if the name matches the plugin, 0 otherwise
   */
  int (*probe_filename)(const char *fname);
  
  /** Given a file, looks at next 4 bytes to determine if this
   * is the right plugin. Used to "guess" the plugin when it is not
   * specified
   *
   * @param file            An corsaro file to peek at
   * @return 1 if the file matches the plugin, 0 otherwise
   */
  int (*probe_magic)(struct corsaro_in *corsaro, corsaro_file_in_t *file);
  
  /** Initialises an input file using the plugin
   *
   * @param corsaro 	The corsaro input to be initialized
   * @return 0 if successful, -1 in the event of error 
   */
  int (*init_input)(struct corsaro_in *corsaro);

  /** Initialises an output file using the plugin
   *
   * @param corsaro	The corsaro output to be initialized
   * @return 0 if successful, -1 in the event of error
   */
  int (*init_output)(struct corsaro *corsaro);

  /** Concludes an input file and cleans up the plugin data.
   *
   * @param corsaro 	The corsaro input to be concluded
   * @return 0 if successful, -1 if an error occurs
   */
  int (*close_input)(struct corsaro_in *corsaro);

  /** Concludes an output file and cleans up the plugin data.
   *
   * @param corsaro 	The output file to be concluded
   * @return 0 if successful, -1 if an error occurs
   */
  int (*close_output)(struct corsaro *corsaro);

  /** Reads the next block of plugin data from an input file
   *
   * @param corsaro	           The input file to read from
   * @param[in,out] record_type    The type of record to read, NULL for wildcard
   * @param[in,out] record         A pointer to the record object to fill
   * @return The the number of bytes read from the file, or -1 if an error
   *  occurs. 0 is returned when the plugin reaches the end of it's data. 
   *
   * If no more data is available for reading, this function should return 0.
   * The returned pointer should be cast to the appropriate plugin data struct.
   */
  off_t (*read_record)(struct corsaro_in *corsaro, 
		       enum corsaro_in_record_type *record_type, 
		       struct corsaro_in_record *record);

  /** Reads a plugin global data block from an input file
   *
   * @param corsaro	           The input file to read from
   * @param[in,out] record_type    The type of record to read, NULL for wildcard
   * @param[in,out] record         A pointer to the record object to fill
   * @return The the number of bytes read from the file, or -1 if an error
   *  occurs. 0 is returned when the plugin reaches the end of it's data. 
   *
   * If no more data is available for reading, this function should return 0.
   * The returned pointer should be cast to the appropriate plugin data struct.
   */
  off_t (*read_global_data_record)(struct corsaro_in *corsaro, 
		       enum corsaro_in_record_type *record_type, 
		       struct corsaro_in_record *record);

  /** Starts a new interval
   *
   * @param corsaro 	The output object to start the interval on
   * @param int_start   The start structure for the interval
   * @return 0 if successful, -1 if an error occurs
   */
  int (*start_interval)(struct corsaro *corsaro, struct corsaro_interval *int_start);

  /** Ends an interval
   *
   * @param corsaro 	The output object end the interval on
   * @param int_end     The end structure for the interval
   * @return 0 if successful, -1 if an error occurs
   *
   * This is likely when the plugin will write it's data to it's output file
   */
  int (*end_interval)(struct corsaro *corsaro, struct corsaro_interval *int_end);

  /** 
   * Process a packet
   *
   * @param corsaro       The output object to process the packet for
   * @param packet        The packet to process
   * @return 0 if successful, -1 if an error occurs
   *
   * This is where the magic happens, the plugin should do any processing
   * needed for this packet and update internal state and optionally
   * update the corsaro_packet_state object to pass on discoveries to later
   * plugins.
   */
  int (*process_packet)(struct corsaro *corsaro, struct corsaro_packet *packet);

#ifdef WITH_PLUGIN_SIXT
  /** 
   * Process a flowtuple
   *
   * @param corsaro       The output object to process the flowtuple for
   * @param flowtuple     The flowtuple to process
   * @return 0 if successful, -1 if an error occurs
   *
   * This is an optional function which allows plugins to re-process flowtuple
   * data.
   */
  int (*process_flowtuple)(struct corsaro *corsaro, 
			   corsaro_flowtuple_t *flowtuple,
			   struct corsaro_packet_state *state);

  /** 
   * Process a flowtuple class start record
   *
   * @param corsaro       The output object to process the flowtuple for
   * @param class         The class start record to process
   * @return 0 if successful, -1 if an error occurs
   *
   * This is an optional function which allows plugins to re-process flowtuple
   * data class start records.
   */
  int (*process_flowtuple_class_start)(struct corsaro *corsaro, 
				       corsaro_flowtuple_class_start_t *class);

  /** 
   * Process a flowtuple class end record
   *
   * @param corsaro       The output object to process the flowtuple for
   * @param class         The class end record to process
   * @return 0 if successful, -1 if an error occurs
   *
   * This is an optional function which allows plugins to re-process flowtuple
   * data class end records.
   */
  int (*process_flowtuple_class_end)(struct corsaro *corsaro, 
				     corsaro_flowtuple_class_end_t *class);
#endif

  /** Next pointer, should always be NULL - used by the plugin
   * manager. */
  struct corsaro_plugin *next;

  /** Count of arguments in argv */
  int argc;
  
  /** Array of plugin arguments
   * This is populated by the plugin manager in corsaro_plugin_enable_plugin.
   * It is the responsibility of the plugin to do something sensible with it
   */
  char **argv;

#ifdef WITH_PLUGIN_TIMING
  /* variables that hold timing information for this plugin */

  /** Number of usec spent in the init_output function */
  uint64_t init_output_usec;

  /** Number of usec spent in the process_packet or process_flowtuple
      functions */
  uint64_t process_packet_usec;

  /** Number of usec spent in the start_interval function */
  uint64_t start_interval_usec;

  /** Number of usec spent in the end_interval function */
  uint64_t end_interval_usec;
#endif

} corsaro_plugin_t;

/** Holds the metadata for the plugin manager
 * 
 * This allows both corsaro_t and corsaro_in_t objects to use the plugin
 * infrastructure without needing to pass references to themselves
 */
typedef struct corsaro_plugin_manager
{
  /** An array of plugin ids that have been enabled by the user
   * 
   * If this array is NULL, then assume all have been enabled.
   */
  uint16_t *plugins_enabled;

  /** The number of plugin ids in the plugins_enabled array */
  uint16_t plugins_enabled_cnt;

  /** A pointer to the array of plugins in use */
  corsaro_plugin_t **plugins;

  /** A pointer to the first plugin in the list */
  corsaro_plugin_t *first_plugin;

  /** A pointer to the array of plugin states */
  void **plugins_state;

  /** The number of active plugins */
  uint16_t plugins_cnt;

  /** A pointer to the logfile to use */
  corsaro_file_t *logfile;

} corsaro_plugin_manager_t;

/** Initialize the plugin manager and all in-use plugins
 *
 * @return A pointer to the plugin manager state or NULL if an error occurs
 */
corsaro_plugin_manager_t *corsaro_plugin_manager_init();

/** Start the plugin manager 
 *
 * @param manager  The manager to start
 */
int corsaro_plugin_manager_start(corsaro_plugin_manager_t *manager); 

/** Free the plugin manager and all in-use plugins
 *
 * @param manager  The plugin manager to free
 *
 * @note the plugins registered with the manager MUST have already 
 * been closed (either plugin->close_output or plugin->close_input).
 * Also, the logfile is NOT closed, as it is assumed to be shared with
 * another object (corsaro_t or corsaro_in_t).
 */
void corsaro_plugin_manager_free(corsaro_plugin_manager_t *manager);

/** Attempt to retrieve a plugin by id
 *
 * @param manager   The plugin manager to search with
 * @param id        The id of the plugin to get
 * @return the plugin corresponding to the id if found, NULL otherwise
 */
corsaro_plugin_t *corsaro_plugin_get_by_id(corsaro_plugin_manager_t *manager, 
					   int id);

/** Attempt to retrieve a plugin by magic number (not by using magic)
 *
 * @param manager   The plugin manager to search with
 * @param id        The magic number of the plugin to get
 * @return the plugin corresponding to the magic number if found, NULL otherwise
 */
corsaro_plugin_t *corsaro_plugin_get_by_magic(corsaro_plugin_manager_t *manager, 
					      uint32_t id);

/** Attempt to retrieve a plugin by name
 *
 * @param manager   The plugin manager to search with
 * @param name      The name of the plugin to get
 * @return the plugin corresponding to the name if found, NULL otherwise
 */
corsaro_plugin_t *corsaro_plugin_get_by_name(corsaro_plugin_manager_t *manager, 
					 const char *name);

/** Retrieve the next plugin in the list
 *
 * @param manager   The plugin manager to get the next plugin for
 * @param plugin    The current plugin
 * @return the plugin which follows the current plugin, NULL if the end of the
 * plugin list has been reached. If plugin is NULL, the first plugin will be 
 * returned.
 */
corsaro_plugin_t *corsaro_plugin_next(corsaro_plugin_manager_t *manager, 
				  corsaro_plugin_t *plugin);

/** Register the state for a plugin
 *
 * @param manager   The plugin manager to register state with
 * @param plugin    The plugin to register state for
 * @param state     A pointer to the state object to register
 */
void corsaro_plugin_register_state(corsaro_plugin_manager_t *manager, 
				 corsaro_plugin_t *plugin,
				 void *state);

/** Free the state for a plugin
 *
 * @param manager   The plugin manager associated with the state
 * @param plugin    The plugin to free state for
 */
void corsaro_plugin_free_state(corsaro_plugin_manager_t *manager, 
			     corsaro_plugin_t *plugin);

/** Check a filename to see if it contains a plugin's name
 *
 * @param fname     The file name to check
 * @param plugin    The plugin to check for
 * @return 1 if the name matches the plugin, 0 otherwise
 */
int corsaro_plugin_probe_filename(const char *fname, corsaro_plugin_t *plugin);

/** Get the name of a plugin given it's ID number
 * 
 * @param manager    The plugin manager associated with the state
 * @param id         The plugin id to retrieve the name for
 * @return the name of the plugin as a string, NULL if no plugin matches
 * the given id
 */
const char *corsaro_plugin_get_name_by_id(corsaro_plugin_manager_t *manager,
					  int id);

/** Get the name of a plugin given it's magic number
 * 
 * @param manager    The plugin manager associated with the state
 * @param magic      The plugin magic number to retrieve the name for
 * @return the name of the plugin as a string, NULL if no plugin matches
 * the given magic number
 */
const char *corsaro_plugin_get_name_by_magic(corsaro_plugin_manager_t *manager,
					     uint32_t magic);

/** Determine whether this plugin is enabled for use
 *
 * @param manager    The plugin manager associated with the state
 * @param plugin     The plugin to check the status of
 * @return 1 if the plugin is enabled, 0 if it is disabled
 *
 *  A plugin is enabled either explicitly by the corsaro_enable_plugin()
 *  function, or implicitly because all plugins are enabled.
 */
int corsaro_plugin_is_enabled(corsaro_plugin_manager_t *manager,
			      corsaro_plugin_t *plugin);

/** Attempt to enable a plugin by its name
 *
 * @param manager      The plugin manager associated with the state
 * @param plugin_name  The name of the plugin to enable
 * @param plugin_args  The arguments to pass to the plugin (for config)
 * @return 0 if the plugin was successfully enabled, -1 otherwise
 *
 * See corsaro_enable_plugin for more details.
 */
int corsaro_plugin_enable_plugin(corsaro_plugin_manager_t *manager,
				 const char *plugin_name,
				 const char *plugin_args);

#endif /* __CORSARO_PLUGIN_H */
