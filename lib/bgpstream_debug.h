/*
 * libbgpstream
 *
 * Chiara Orsini, CAIDA, UC San Diego
 * chiara@caida.org
 *
 * Copyright (C) 2013 The Regents of the University of California.
 *
 * http://c.learncodethehardway.org/book/ex20.html
 *
 */

#ifndef _DEBUG_H
#define _DEBUG_H

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <time.h>


//#define NDEBUG 

// compile with NDEBUG defined -> then "no debug" messages will remain.
#ifdef NDEBUG 
#define debug(M, ...)
#else
#define debug(M, ...) fprintf(stderr, "DEBUG %s:%d: " M "\n", __FILE__, __LINE__, ##__VA_ARGS__)
#endif

// get a safe readable version of errno.
#define clean_errno() (errno == 0 ? "None" : strerror(errno))

// macros for logging messages meant for the end use
#define log_err(M, ...) fprintf(stderr, "[ERROR] (%s:%d: errno: %s) " M "\n", __FILE__, __LINE__, clean_errno(), ##__VA_ARGS__)

#define log_warn(M, ...) fprintf(stderr, "[WARN] (%s:%d: errno: %s) " M "\n", __FILE__, __LINE__, clean_errno(), ##__VA_ARGS__)

#define log_info(M, ...) fprintf(stderr, "[INFO] (%s:%d) " M "\n", __FILE__, __LINE__, ##__VA_ARGS__)

// check will make sure the condition A is true, and if not logs the error M (with variable arguments for log_err), then jumps to the function's error: for cleanup.
#define check(A, M, ...) if(!(A)) { log_err(M, ##__VA_ARGS__); errno=0; goto error; }

// sentinel is placed in any part of a function that shouldn't run, and if it does prints an error message then jumps to the error: label. You put this in if-statements and switch-statements to catch conditions that shouldn't happen, like the default:.
#define sentinel(M, ...)  { log_err(M, ##__VA_ARGS__); errno=0; goto error; }

// check_mem that makes sure a pointer is valid, and if it isn't reports it as an error with "Out of memory."
#define check_mem(A) check((A), "Out of memory.")

// An alternative macro check_debug that still checks and handles an error, but if the error is common then you don't want to bother reporting it. In this one it will use debug instead of log_err to report the message, so when you define NDEBUG the check still happens, the error jump goes off, but the message isn't printed.
#define check_debug(A, M, ...) if(!(A)) { debug(M, ##__VA_ARGS__); errno=0; goto error; }


#endif /* _DEBUG_H */
