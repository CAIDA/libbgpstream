/*
 * This file is part of bgpstream
 *
 * Copyright (C) 2015 The Regents of the University of California.
 * Authors: Alistair King, Chiara Orsini
 *
 * All rights reserved.
 *
 * This code has been developed by CAIDA at UC San Diego.
 * For more information, contact bgpstream-info@caida.org
 *
 * This source code is proprietary to the CAIDA group at UC San Diego and may
 * not be redistributed, published or disclosed without prior permission from
 * CAIDA.
 *
 * Report any bugs, questions or comments to bgpstream-info@caida.org
 *
 * Some code adapted from http://c.learncodethehardway.org/book/ex20.html
 *
 */

#ifndef _BGPSTREAM_DEBUG_H
#define _BGPSTREAM_DEBUG_H

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <time.h>


// #define NDEBUG 

// compile with NDEBUG defined -> then "no debug" messages will remain.
#ifdef NDEBUG 
#define bgpstream_debug(M, ...)
#else
#define bgpstream_debug(M, ...) fprintf(stderr, "DEBUG %s:%d: " M "\n", __FILE__, __LINE__, ##__VA_ARGS__)
#endif

// get a safe readable version of errno.
#define bgpstream_clean_errno() (errno == 0 ? "None" : strerror(errno))

// macros for logging messages meant for the end use
#define bgpstream_log_err(M, ...) fprintf(stderr, "[ERROR] (%s:%d: errno: %s) " M "\n", __FILE__, __LINE__, bgpstream_clean_errno(), ##__VA_ARGS__)

#define bgpstream_log_warn(M, ...) fprintf(stderr, "[WARN] (%s:%d: errno: %s) " M "\n", __FILE__, __LINE__, bgpstream_clean_errno(), ##__VA_ARGS__)

#define bgpstream_log_info(M, ...) fprintf(stderr, "[INFO] (%s:%d) " M "\n", __FILE__, __LINE__, ##__VA_ARGS__)

// check will make sure the condition A is true, and if not logs the error M (with variable arguments for log_err), then jumps to the function's error: for cleanup.
#define bgpstream_check(A, M, ...) if(!(A)) { bgpstream_log_err(M, ##__VA_ARGS__); errno=0; goto error; }

// sentinel is placed in any part of a function that shouldn't run, and if it does prints an error message then jumps to the error: label. You put this in if-statements and switch-statements to catch conditions that shouldn't happen, like the default:.
#define bgpstream_sentinel(M, ...)  { bgpstream_log_err(M, ##__VA_ARGS__); errno=0; goto error; }

// check_mem that makes sure a pointer is valid, and if it isn't reports it as an error with "Out of memory."
#define bgpstream_check_mem(A) bgpstream_check((A), "Out of memory.")

// An alternative macro check_debug that still checks and handles an error, but if the error is common then you don't want to bother reporting it. In this one it will use debug instead of log_err to report the message, so when you define NDEBUG the check still happens, the error jump goes off, but the message isn't printed.
#define bgpstream_check_debug(A, M, ...) if(!(A)) { bgpstream_debug(M, ##__VA_ARGS__); errno=0; goto error; }


#endif /* _BGPSTREAM_DEBUG_H */
