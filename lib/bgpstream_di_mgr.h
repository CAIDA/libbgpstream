/*
 * This file is part of bgpstream
 *
 * CAIDA, UC San Diego
 * bgpstream-info@caida.org
 *
 * Copyright (C) 2012 The Regents of the University of California.
 * Authors: Alistair King, Chiara Orsini
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __BGPSTREAM_DATA_INTERFACE_MANAGER_H
#define __BGPSTREAM_DATA_INTERFACE_MANAGER_H

#include "config.h"
#include "bgpstream_filter.h"
#include "bgpstream_input.h"

/** Opaque struct representing the Data Interface manager */
typedef struct bgpstream_di_mgr bgpstream_di_mgr_t;

/** Opaque struct representing a Data Interface plugin */
typedef struct bsdi bsdi_t;

/** Create a new Data Interface Manager instance
 *
 * @return pointer to a manager instance if successful, NULL otherwise
 */
bgpstream_di_mgr_t *
bgpstream_di_mgr_create(bgpstream_filter_mgr_t *filter_mgr);

/** Get a list of data interfaces that are currently supported
 *
 * @param di_mgr        pointer to a data interface manager instance
 * @param[out] if_ids   set to a borrowed pointer to an array of
 *                      bgpstream_data_interface_type_t values
 * @return the number of elements the the if_ids array
 *
 * @note the returned array belongs to BGPStream. It must not be freed by the
 * user.
 */
int bgpstream_di_mgr_get_data_interfaces(bgpstream_di_mgr_t *di_mgr,
                                         bgpstream_data_interface_id_t **if_ids);

/** Get the ID of the data interface with the given name
 *
 * @param di            pointer to a data interface manager instance
 * @param name          name of the data interface to retrieve the ID for
 * @return the ID of the data interface with the given name, or
 * _BGPSTREAM_DATA_INTERFACE_INVALID if no matching interface was found
 */
bgpstream_data_interface_id_t
bgpstream_di_mgr_get_data_interface_id_by_name(bgpstream_di_mgr_t *di_mgr,
                                               const char *name);

/** Get information for the given data interface
 *
 * @param di            pointer to a data interface manager instance
 * @param if_id         ID of the interface to get the name for
 * @return borrowed pointer to an interface info structure
 */
bgpstream_data_interface_info_t *
bgpstream_di_mgr_get_data_interface_info(bgpstream_di_mgr_t *di_mgr,
                                         bgpstream_data_interface_id_t if_id);

/** Get a list of valid option types for the given data interface
 *
 * @param di            pointer to a data interface manager instance
 * @param if_id         ID of the interface to get option names for
 *                      in the returned array
 * @param[out] opts     set to a borrowed pointer to an array of options
 * @return the number of elements in the opts array
 *
 * @note the returned array belongs to BGP Stream. It must not be freed by the
 * user.
 */
int bgpstream_di_mgr_get_data_interface_options(
  bgpstream_di_mgr_t *di, bgpstream_data_interface_id_t if_id,
  bgpstream_data_interface_option_t **opts);

/** Set the current data interface
 *
 * @param di_mgr        pointer to a data interface manager instance
 * @param di_id         ID of the data interface to enable
 * @return 0 if the data interface was set successfully, -1 otherwise
 *
 * There may only be one active data interface. Activating a data interface
 * implicitly deactivates all other interfaces
 */
int
bgpstream_di_mgr_set_data_interface(bgpstream_di_mgr_t *di_mgr,
                                    bgpstream_data_interface_id_t di_id);

/** Get the ID of the currently active Data Interface
 *
 * @param di_mgr        pointer to a data interface manager instance
 * @return ID of the currently active data interface (the default if none has
 * been explicitly set)
 */
bgpstream_data_interface_id_t
bgpstream_di_mgr_get_data_interface_id(bgpstream_di_mgr_t *di_mgr);

/** Set the given option to the given value for the given data interface
 *
 * @param di_mgr        pointer to a data interface manager instance
 * @param option_type   pointer to an option spec object
 *                      (obtained using _get_options)
 * @param option_value  borrowed pointer to a string option value
 * @return 0 if the option was set successfully, -1 otherwise
 */
int bgpstream_di_mgr_set_data_interface_option(
  bgpstream_di_mgr_t *di_mgr,
  const bgpstream_data_interface_option_t *option_type,
  const char *option_value);

/** Set the data interface to blocking mode
 *
 * @param di_mgr        pointer to a data interface manager instance
 */
void bgpstream_di_mgr_set_blocking(bgpstream_di_mgr_t *di_mgr);

/** Start the data interface
 *
 * @param di_mgr        pointer to a data interface manager instance
 * @return 0 if the data interface was started successfully, -1 otherwise
 *
 * All options and filters must be set before calling this function
 */
int bgpstream_di_mgr_start(bgpstream_di_mgr_t *di_mgr);

/** Get the next batch of metadata from the active data interface
 *
 * @param di_mgr        pointer to a data interface manager instance
 * @param input_mgr     pointer to the input manager to XXX change this
 * @return the number of metadata elements in the queue if successful, -1
 * otherwise
 *
 * If the stream is in live mode, this method will block until data is
 * available, otherwise it will return an empty queue to indicate EOF.
 */
int bgpstream_di_mgr_get_queue(bgpstream_di_mgr_t *di_mgr,
                               bgpstream_input_mgr_t *input_mgr);

/** Destroy the given data interface manager
 *
 * @param di_mgr        pointer to a data interface manager instance to destroy
 */
void bgpstream_di_mgr_destroy(bgpstream_di_mgr_t *di_mgr);

#endif /* _BGPSTREAM_DATA_INTERFACE_MANAGER */
