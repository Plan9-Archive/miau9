/* $Id: commands.h 295 2005-12-16 19:44:24Z tsaviran $
 * -------------------------------------------------------
 * Copyright (C) 2003-2005 Tommi Saviranta <wnd@iki.fi>
 *	(C) 2002 Lee Hardy <lee@leeh.co.uk>
 * -------------------------------------------------------
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef COMMANDS_H_
#define COMMANDS_H_

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* ifdef HAVE_CONFIG_H */



/* The maximum number that can be supported in 4 bits. */
#define MAX_CMD 16

/*
 * The bitmask list of all our commands, extend to 5 bits if more than 16 are
 * needed.
 */
#define CMD_NONE	0x0000
#define CMD_PING	0x0001
#define CMD_PONG	0x0002
#define CMD_NICK	0x0004
#define CMD_KICK	0x0008
#define CMD_JOIN	0x0010
#define CMD_PART	0x0020
#define CMD_MODE	0x0040
#define CMD_TOPIC	0x0080
#define CMD_KILL	0x0100
#define CMD_PRIVMSG	0x0200
#define CMD_NOTICE	0x0400
#define CMD_ERROR	0x0800
#define CMD_QUIT	0x1000

#define MINCOMMANDVALUE	1000		/* IRC uses codes 000-999 */



int command_find(char *p);
void command_setup(void);
void command_free(void);



#endif /* ifdef COMMANDS_H_ */
