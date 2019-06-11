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

#ifndef __BGPSTREAM_DI_INTERFACE_H
#define __BGPSTREAM_DI_INTERFACE_H

#include "bgpstream.h"
#include "bgpstream_di_mgr.h" /* for bsdi_t */
#include "config.h"

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
#define BSDI_GET_STATE(interface, type)                                        \
  ((bsdi_##type##_state_t *)(interface)->state)

/** Convenience macro to allow implementations to store a state pointer */
#define BSDI_SET_STATE(interface, ptr)                                         \
  do {                                                                         \
    (interface)->state = ptr;                                                  \
  } while (0)

#define BSDI_GET_FILTER_MGR(interface) ((interface)->filter_mgr)
#define BSDI_GET_RES_MGR(interface) ((interface)->res_mgr)

/** Convenience macro that defines all the function prototypes for the data
 * interface API
 */
#define BSDI_GENERATE_PROTOS(ifname)                                           \
  bsdi_t *bsdi_##ifname##_alloc(void);                                         \
  int bsdi_##ifname##_init(bsdi_t *di);                                        \
  int bsdi_##ifname##_start(bsdi_t *di);                                       \
  int bsdi_##ifname##_set_option(                                              \
    bsdi_t *di, const bgpstream_data_interface_option_t *option_type,          \
    const char *option_value);                                                 \
  void bsdi_##ifname##_destroy(bsdi_t *di);                                    \
  int bsdi_##ifname##_update_resources(bsdi_t *di);

/** Convenience macros that create a class structure for a data interface */
#define BSDI_CREATE_CLASS_FULL(classname, classnamestr, id, desc, options)     \
  static bsdi_t bsdi_##classname = {                                           \
    {                                                                          \
      (id),                                                                    \
      classnamestr,                                                            \
      desc,                                                                    \
    },                                                                         \
    (options),                                                                 \
    ARR_CNT(options),                                                          \
    bsdi_##classname##_init,                                                   \
    bsdi_##classname##_start,                                                  \
    bsdi_##classname##_set_option,                                             \
    bsdi_##classname##_destroy,                                                \
    bsdi_##classname##_update_resources,                                       \
    NULL,                                                                      \
    NULL,                                                                      \
    NULL,                                                                      \
  };                                                                           \
  bsdi_t *bsdi_##classname##_alloc()                                           \
  {                                                                            \
    return &bsdi_##classname;                                                  \
  }

#define BSDI_CREATE_CLASS(classname, id, desc, options)                        \
  BSDI_CREATE_CLASS_FULL(classname, STR(classname), id, desc, options)

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

  /** Get the next batch of resource metadata from this interface
   *
   * @param di          pointer to the data interface
   * @return 0 if the queue was updated successfully, -1 otherwise
   *
   * If the stream is in live mode, this method will block until data is
   * available, otherwise it will return an empty queue to indicate EOF.
   */
  int (*update_resources)(bsdi_t *di);

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

  /** Borrowed pointer to a resource manager instance */
  bgpstream_resource_mgr_t *res_mgr;

  /** }@ */
};

#endif /* __BGPSTREAM_DI_INTERFACE_H */
