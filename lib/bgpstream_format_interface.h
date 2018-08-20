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

#ifndef __BGPSTREAM_FORMAT_INTERFACE_H
#define __BGPSTREAM_FORMAT_INTERFACE_H

#include "bgpstream.h"
#include "bgpstream_format.h" /*< for bgpstream_format_t */
#include "bgpstream_transport.h"
#include "config.h"

/** @file
 *
 * @brief Header file that exposes the protected interface of the data format
 * plugin API
 *
 * @author Alistair King
 *
 */

/** Convenience macro that defines all the function prototypes for the data
 * format API */
#define BS_FORMAT_GENERATE_PROTOS(name)                                        \
  int bs_format_##name##_create(bgpstream_format_t *format,                    \
                                bgpstream_resource_t *res);                    \
  bgpstream_format_status_t bs_format_##name##_populate_record(                \
    bgpstream_format_t *format, bgpstream_record_t *record);                   \
  int bs_format_##name##_get_next_elem(bgpstream_format_t *format,             \
                                       bgpstream_record_t *record,             \
                                       bgpstream_elem_t **elem);               \
  int bs_format_##name##_init_data(bgpstream_format_t *format, void **data);   \
  void bs_format_##name##_clear_data(bgpstream_format_t *format, void *data);  \
  void bs_format_##name##_destroy_data(bgpstream_format_t *format,             \
                                       void *data);                            \
  void bs_format_##name##_destroy(bgpstream_format_t *format);

#define BS_FORMAT_SET_METHODS(classname, format)                               \
  do {                                                                         \
    (format)->populate_record = bs_format_##classname##_populate_record;       \
    (format)->get_next_elem = bs_format_##classname##_get_next_elem;           \
    (format)->init_data = bs_format_##classname##_init_data;                   \
    (format)->clear_data = bs_format_##classname##_clear_data;                 \
    (format)->destroy_data = bs_format_##classname##_destroy_data;             \
    (format)->destroy = bs_format_##classname##_destroy;                       \
  } while (0)

/** Structure which represents a data format */
struct bgpstream_format {

  /**
   * @name Format method pointers
   *
   * @{ */

  /**
   * Get the next record from this format instance
   *
   * @param format      pointer to the format object to read from
   * @param[out] record   set to point to a populated record instance
   * @return format status code
   *
   * This function should use the filter manager to only return records that
   * match the given filters.
   */
  bgpstream_format_status_t (*populate_record)(struct bgpstream_format *format,
                                               bgpstream_record_t *record);

  /** Get the next elem from the given record
   *
   * @param format        pointer to the format object to use
   * @param record        pointer to the record to use
   * @param[out] elem     set to point to a borrowed elem structure or NULL if
   *                      there are no more elems
   * @return 1 if a valid elem was returned, 0 if there are no more elems, -1 if
   * an error occurred.
   */
  int (*get_next_elem)(bgpstream_format_t *format, bgpstream_record_t *record,
                       bgpstream_elem_t **elem);

  /** Initialize/create the given format-specific record data
   *
   * @param format      pointer to the format object to use
   * @param data[out]   set to newly initialized data
   */
  int (*init_data)(struct bgpstream_format *format, void **data);

  /** Clear the given format-specific record data
   *
   * @param format      pointer to the format object to use
   * @param data        pointer to the data to clear
   */
  void (*clear_data)(struct bgpstream_format *format, void *data);

  /** Destroy the given format-specific record data
   *
   * @param format      pointer to the format object to use
   * @param data        pointer to the data to destroy
   */
  void (*destroy_data)(struct bgpstream_format *format, void *data);

  /** Destroy the given format module
   *
   * @param format        pointer to the format instance to destroy
   */
  void (*destroy)(struct bgpstream_format *format);

  /** }@ */

  /**
   * @name Data format state fields
   *
   * @{ */

  /** Pointer to the resource the format is parsing */
  bgpstream_resource_t *res;

  /** Pointer to the transport instance to read data from */
  bgpstream_transport_t *transport;

  /** Pointer to the filter manager instance to use to filter records */
  bgpstream_filter_mgr_t *filter_mgr;

  /** An opaque pointer to format-specific state if needed */
  void *state;

  /** }@ */
};

#endif /* __BGPSTREAM_FORMAT_INTERFACE_H */
