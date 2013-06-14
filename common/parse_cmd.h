/*
 * This file is part of wdcap
 *
 * Copyright (c) 2004-2009 The University of Waikato, Hamilton, New Zealand.
 * Authors: Daniel Lawson
 *          Shane Alcock
 *          Perry Lorier
 *
 * All rights reserved.
 *
 * This code has been developed by the University of Waikato WAND
 * research group. For further information please see http://www.wand.net.nz/
 *
 * This source code is proprietary to the University of Waikato
 * WAND research group and may not be redistributed, published or disclosed
 * without prior permission from WAND.
 *
 * Report any bugs, questions or comments to contact@wand.net.nz
 *
 * $Id: parse_cmd.h 153 2009-07-02 22:50:00Z salcock $
 *
 */

#ifndef PARSE_CMD_H
#define PARSE_CMD_H 1

void parse_cmd(char *buf, int *pargc, char *pargv[], int MAXTOKENS,
	       const char *command_name);


#endif // PARSE_CMD_H
