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
 * $Id: parse_cmd.c 153 2009-07-02 22:50:00Z salcock $
 *
 */

static void skip_white(char **buf)
{
	while(**buf==' ')
		(*buf)++;
}

/* Get the next word in a line
 *
 * Returns
 *  NULL : End of line
 *  other: Pointer to a word
 *
 * Side effects:
 *  updates *buf
 *  modified *buf
 *
 * ' foo bar baz' => 'foo' 'bar baz'
 * ' "foo bar" baz' => 'foo bar' ' baz'
 */
static char * split_cmd(char **buf)
{
	char *ret = 0;

	skip_white(buf);

	if (**buf=='"') /* Quoted */
	{
		(*buf)++;
		ret=*buf;

		while(**buf && **buf!='"')
			(*buf)++;

		if (**buf)
		{
			**buf='\0';
			(*buf)++;
		}
	} else
	{
		ret=*buf;

		while(**buf && **buf!=' ')
			(*buf)++;

		if (**buf)
		{
			**buf='\0';
			(*buf)++;
		}
	}
	return ret;
}

/* Split a command line up into parc,parv
 * using command line rules
 */
void parse_cmd(char *buf,int *parc, char *parv[], int MAXTOKENS,
	       const char *command_name)
{
	int i=0;
	parv[0] = (char*) command_name;
	*parc=1;

	while(*buf)
	{
		parv[(*parc)++]=split_cmd(&buf);
		if (*parc>(MAXTOKENS-1))
		{
			parv[*parc]=buf;
			break;
		}
	}
	for(i=*parc;i<MAXTOKENS;i++)
		parv[i]="";
}





