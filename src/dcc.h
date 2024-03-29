/* $Id: dcc.h 295 2005-12-16 19:44:24Z tsaviran $
 * -------------------------------------------------------
 * Copyright (C) 2003-2005 Tommi Saviranta <wnd@iki.fi>
 *	(C) 1998-2001 Sebastian Kienzl <zap@riot.org>
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

#ifndef DCC_H_
#define DCC_H_

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#ifdef DCCBOUNCE

#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>



char *dcc_initiate(char *param, size_t dsize, int fromclient);
void dcc_socketsubscribe(fd_set* readset, fd_set* writeset);
void dcc_socketcheck(fd_set* readset, fd_set* writeset);
void dcc_timer(void); /* To be called every second ! */



#endif /* ifdef DCCBOUNCE */

#endif /* ifdef DCC_H_ */
