/*
 * Copyright (C) 2017 The Regents of the University of California.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __BGPSTREAM_DATA_INTERFACE_MANAGER_H
#define __BGPSTREAM_DATA_INTERFACE_MANAGER_H

#include "config.h"
#include "bgpstream_filter.h"
#include "bgpstream_resource_mgr.h"

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

/** Get the next record from the stream
 *
 * @param di_mgr          pointer to a data interface manager instance
 * @param[out] record     set to a borrowed pointer to a record if the return
 *                        code is >0
 * @return >0 if a record was read successfully, 0 if end-of-stream has been
 * reached, <0 if an error occurred.
 *
 * If the stream is in live mode, this method will block until data is
 * available, otherwise it will return 0 to indicate EOF.
 */
int
bgpstream_di_mgr_get_next_record(bgpstream_di_mgr_t *di_mgr,
                                 bgpstream_record_t **record);

/** Destroy the given data interface manager
 *
 * @param di_mgr        pointer to a data interface manager instance to destroy
 */
void bgpstream_di_mgr_destroy(bgpstream_di_mgr_t *di_mgr);

#endif /* _BGPSTREAM_DATA_INTERFACE_MANAGER */
