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

#include <bgpwatcher_server.h>

bgpwatcher_server_t *bgpwatcher_server_init(
				      bgpwatcher_server_callbacks_t *callbacks)
{
  return NULL;
}

int bgpwatcher_server_start(bgpwatcher_server_t *server)
{
  return -1;
}

void bgpwatcher_server_perr(bgpwatcher_server_t *server)
{
  assert(server != NULL);
  bgpwatcher_perr(&server->err);
}

void bgpwatcher_server_stop(bgpwatcher_server_t *server)
{
  return;
}

void bgpwatcher_server_free(bgpwatcher_server_t *server)
{
  return;
}
