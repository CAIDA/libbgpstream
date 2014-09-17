/*
 * bgpwatcher
 *
 * Alistair King, CAIDA, UC San Diego
 * corsaro-info@caida.org
 *
 * Copyright (C) 2012 The Regents of the University of California.
 *
 * This file is part of bgpwatcher.
 *
 * bgpwatcher is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * bgpwatcher is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with bgpwatcher.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <stdint.h>

#include <bgpwatcher_client_int.h>

bgpwatcher_client_t *bgpwatcher_client_init()
{
  return NULL;
}

int bgpwatcher_client_start(bgpwatcher_client_t *client)
{
  return -1;
}

void bgpwatcher_client_perr(bgpwatcher_client_t *client)
{
  return;
}

bgpwatcher_client_pfx_table_t *bgpwatcher_client_create_pfx_table(
						   bgpwatcher_client_t *client)
{
  return NULL;
}

int bgpwatcher_client_pfx_table_add(bgpwatcher_client_pfx_table_t *table,
				    bgpwatcher_pfx_record_t *pfx)
{
  return -1;
}

int bgpwatcher_client_pfx_table_flush(bgpwatcher_client_pfx_table_t *table)
{
  return -1;
}

bgpwatcher_client_peer_table_t *bgpwatcher_client_create_peer_table(
						   bgpwatcher_client_t *client)
{
  return NULL;
}

int bgpwatcher_client_peer_table_add(bgpwatcher_client_peer_table_t *table,
				     bgpwatcher_peer_record_t *peer)
{
  return -1;
}

int bgpwatcher_client_peer_table_flush(bgpwatcher_client_peer_table_t *table)
{
  return -1;
}

void bgpwatcher_client_stop(bgpwatcher_client_t *client)
{
  return;
}

void bgpwatcher_client_free(bgpwatcher_client_t *client)
{
  return;
}
