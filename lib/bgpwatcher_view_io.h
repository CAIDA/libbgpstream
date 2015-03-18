/*
 * This file is part of bgpwatcher
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
