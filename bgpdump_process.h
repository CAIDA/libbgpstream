/*
 * libbgpdump-caida
 *
 * Chiara Orsini, CAIDA, UC San Diego
 * chiara@caida.org
 *
 * Copyright (C) 2013 The Regents of the University of California.
 *
 * This file is part of libbgpdump-caida.
 *
 * libbgpdump-caida is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * libbgpdump-caida is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with libbgpdump-caida.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "bgpdump_lib.h"

/** Print a BGPDUMP_ENTRY record to stdout in the same way that the RIPE bgpdump
 * tool does.
 *
 * @param entry         pointer to a BGP Dump entry
 */
void bgpdump_print_entry(BGPDUMP_ENTRY *entry);
