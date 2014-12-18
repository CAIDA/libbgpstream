/*
 * bgpwatcher
 *
 * Chiara Orsini, CAIDA, UC San Diego
 * corsaro-info@caida.org
 *
 * Copyright (C) 2014 The Regents of the University of California.
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

#ifndef __BGPWATCHER_VIEW_IO_H
#define __BGPWATCHER_VIEW_IO_H

#include "bgpwatcher_view.h"

/** Send the given view to the given socket
 *
 * @param dest          socket to send the prefix to
 * @param view          pointer to the view to send
 * @return 0 if the view was sent successfully, -1 otherwise
 */
int bgpwatcher_view_send(void *dest, bgpwatcher_view_t *view);

/** Receive a view from the given socket
 *
 * @param src           socket to receive on
 * @param view          pointer to the clear/new view to receive into
 * @return pointer to the view instance received, NULL if an error occurred.
 */
int bgpwatcher_view_recv(void *src, bgpwatcher_view_t *view);

#endif /* __BGPWATCHER_VIEW_IO_H */
