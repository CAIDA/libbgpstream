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

#ifndef __BGPSTREAM_DI_INTERFACE_H
#define __BGPSTREAM_DI_INTERFACE_H

#include "config.h"
#include "bgpstream.h"
#include "bgpstream_di_mgr.h" /*< for bsdi_t */

/** @file
 *
 * @brief Header file that exposes the protected interface of the data interface
 * plugin API
 *
 * @author Alistair King
 *
 */

/** Convenience macro to allow implementations to retrieve their state object
 */
#define BSDI_GET_STATE(interface, type)         \
  ((bsdi_##type##_state_t *)(interface)->state)

/** Convenience macro to allow implementations to store a state pointer */
#define BSDI_SET_STATE(interface, ptr)                                  \
  do {                                                                  \
    (interface)->state = ptr;                                           \
  } while (0)

#define BSDI_GET_FILTER_MGR(interface) ((interface)->filter_mgr)

/** Convenience macro that defines all the function prototypes for the data
 * interface API
 */
#define BSDI_GENERATE_PROTOS(ifname)                                    \
  bsdi_t *bsdi_##ifname##_alloc();                                      \
  int bsdi_##ifname##_init(bsdi_t *di);                                 \
  int bsdi_##ifname##_start(bsdi_t *di);                                \
  int bsdi_##ifname##_set_option(bsdi_t *di,                            \
                                 const bgpstream_data_interface_option_t *option_type, \
                                 const char *option_value);             \
  void bsdi_##ifname##_destroy(bsdi_t *di);                             \
  int bsdi_##ifname##_get_queue(bsdi_t *di, bgpstream_input_mgr_t *input_mgr);

/** Convenience macro that creates a class structure for a data interface */
#define BSDI_CREATE_CLASS(classname, id, desc, options)                 \
  static bsdi_t bsdi_##classname = {                                    \
    {                                                                   \
      (id),                                                             \
      STR(classname),                                                   \
      desc,                                                             \
    },                                                                  \
    (options),                                                          \
    ARR_CNT(options),                                                   \
    bsdi_##classname##_init,                                            \
    bsdi_##classname##_start,                                           \
    bsdi_##classname##_set_option,                                      \
    bsdi_##classname##_destroy,                                         \
    bsdi_##classname##_get_queue,                                       \
    NULL,                                                               \
    NULL,                                                               \
  };                                                                    \
  bsdi_t *bsdi_##classname##_alloc() {                                  \
    return &bsdi_##classname;                                           \
  }

/** Structure which represents a data interface */
struct bsdi {
  /** Data interface information fields
   *
   * These fields are always filled, even if the interface is not enabled.
   *
   */
  bgpstream_data_interface_info_t info;

  /** Description of the options available for this interface */
  bgpstream_data_interface_option_t *opts;

  /** Number of options in the opts array */
  int opts_cnt;

  /**
   * @name Interface method pointers
   *
   * These pointers are always filled, even if an interface is not enabled.
   * Until the interface is enabled, only the init function can be called.
   *
   * @{ */

  /** Initialize and enable this data interface
   *
   * @param di          The data interface object to allocate
   * @return 0 if the interface is successfully initialized, -1 otherwise
   *
   * This method is for creating state. If the interface needs to open
   * connections, databases, etc. this should be done in the `start` method.
   */
  int (*init)(struct bsdi *di);

  /** Start this data interface
   *
   * @param di          The data interface to start
   * @return 0 if the interface was started successfully, -1 otherwise
   */
  int (*start)(struct bsdi *di);

  /** Set a data interface option
   *
   * @param di            pointer to the data interface to configure
   * @param option_type   pointer to the option to set
   * @param option_value  value to set the option to
   * @return 0 if the option was set successfully, -1 otherwise
   */
  int (*set_option)(bsdi_t *di,
                    const bgpstream_data_interface_option_t *option_type,
                    const char *option_value);

  /** Shutdown and free interface-specific state for this data interface
   *
   * @param di          The data interface object to free
   *
   * @note interfaces should *only* free interface-specific state. All other
   * state will be free'd for them by the data interface manager.
   */
  void (*destroy)(struct bsdi *di);

  /** Get the next batch of metadata from this interface
   *
   * @param di          pointer to the data interface
   * @param input_mgr   pointer to the input manager to XXX change this
   * @return the number of metadata elements in the queue if successful, -1
   * otherwise
   *
   * If the stream is in live mode, this method will block until data is
   * available, otherwise it will return an empty queue to indicate EOF.
   */
  int (*get_queue)(bsdi_t *di, bgpstream_input_mgr_t *input_mgr);

  /** }@ */

  /**
   * @name Data interface state fields
   *
   * These fields are only set if the interface is initialized
   * @note These fields should *not* be directly manipulated by
   * interfaces. Instead they should use accessor functions provided by the
   * interface manager.
   *
   * @{ */

  /** An opaque pointer to interface-specific state if needed by the
      interface */
  void *state;

  /** Borrowed pointer to a Filter Manager instance */
  bgpstream_filter_mgr_t *filter_mgr;

  /** }@ */
};

#endif /* __BGPSTREAM_DI_INTERFACE_H */
