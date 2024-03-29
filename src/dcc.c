/* $Id: dcc.c 451 2010-08-22 11:04:54Z tsaviran $
 * -------------------------------------------------------
 * Copyright (C) 2003-2005 Tommi Saviranta <wnd@iki.fi>
 *	(C) 1998-2002 Sebastian Kienzl <zap@riot.org>
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


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#ifdef DCCBOUNCE

#include "table.h"
#include "irc.h"
#include "error.h"
#include "messages.h"
#include "tools.h"
#include "miau.h"
#include "common.h"

#include <errno.h>
#include <netdb.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>

#if HAVE_STRINGS_H
#include <strings.h>
#endif



static int dcc_bouncedata(int a, int b, int *wrflag);
static int dcc_addbounce();
static void dcc_killbounce(int dccindex);

/* #define DEBUG */

typedef struct {
	/* Sockets. */
	int	src;
	int	dest;

	/* Writeable ? */
	int	src_wa;
	int	dest_wa;

	int	connected;
	time_t	created;

	unsigned int		srcport;
	struct sockaddr_in	destaddr;
} dccbounce;


typedef struct {
	dccbounce	**data;
	int		amount;
} dccbounce_type;


dccbounce_type dccs;


typedef struct {
	char	type[16];
	int	argc;
	char	*arg1;
	int	args[3];
	int	fromclient;	/* Sent by client or by server ? */
} dcccommand;


static int dcc_realinitiate(char *dest, size_t dsize, dcccommand *dcc);
static int dcc_resume(char *dest, size_t dsize, dcccommand *dcc);


typedef struct {
	char	*cmd;
	int	(*func) (char *dest, size_t dsize, dcccommand *dcc);
} dcccall;


dcccall dcccalls[] = {
	{"CHAT", dcc_realinitiate},
	{"SEND", dcc_realinitiate},
	{"RESUME", dcc_resume},
	{"ACCEPT", dcc_resume},
	{NULL, NULL}
};


static int
dcc_bouncedata(int a, int b, int *wrflag)
{
	static char	buffer[2048];
	int		rret, wret;

	rret = (int) recv(a, buffer, 2048, MSG_PEEK);
	if (rret <= 0) {
		if (errno == EAGAIN) {
			return 1;
		}
		return 0;
	}

	wret = send(b, buffer, rret, 0);
	if (wret <= 0) {
		if (errno == EAGAIN) {
			*wrflag = 0;
		}
		return 0;
	}

	/* Remove from queue. */
	recv(a, buffer, wret, 0);

	if (wret != rret) {
		*wrflag = 0;
	}

	return 1;
} /* int dcc_bouncedata(int a, int b, int *wrflag) */



static int
dcc_addbounce(void)
{
	int	dccindex;
	dccs.data = (dccbounce **) table_add_item((void **) dccs.data,
			sizeof(dccbounce), &dccs.amount, &dccindex);
	memset((void *) dccs.data[dccindex], 0, sizeof(dccbounce));
	dccs.data[dccindex]->created = time(NULL);
#ifdef DEBUG
	printf("added bounce %d\n", dccindex);
#endif
	return dccindex;
} /* int dcc_addbounce(void) */



static void
dcc_killbounce(int dccindex)
{
	if (dccs.data[dccindex]->src) {
		rawsock_close(dccs.data[dccindex]->src);
	}
	if (dccs.data[dccindex]->dest) {
		rawsock_close(dccs.data[dccindex]->dest);
	}
	dccs.data = (dccbounce **) table_rem_item((void **) dccs.data, dccindex,
			&dccs.amount);
#ifdef DEBUG
	printf("killed bounce %d\n", dccindex);
#endif
} /* void dcc_killbounce(int dccindex) */



void
dcc_timer(void)
{
	int	i;
	time_t	t;

	t = time(NULL);

	for (i = 0; i < dccs.amount; i++) {
		if (dccs.data[i] == NULL) {
			continue;
		}
		if (dccs.data[i]->dest == 0
				&& dccs.data[i]->created + 80 < t) {
			error(DCC_TIMEOUT, i);
			dcc_killbounce(i);
			continue;
		}
	}
} /* void dcc_timer(void) */



void
dcc_socketsubscribe(fd_set *readset, fd_set *writeset)
{
	int	i;

	for (i = 0; i < dccs.amount; i++) {
		if (dccs.data[i] == NULL) {
			continue;
		}

		if (! dccs.data[i]->connected || dccs.data[i]->dest_wa) {
			FD_SET(dccs.data[i]->src, readset);
		}

		if (dccs.data[i]->connected && dccs.data[i]->src_wa) {
			FD_SET(dccs.data[i]->dest, readset);
		}

		if (dccs.data[i]->dest && ! dccs.data[i]->dest_wa) {
			FD_SET(dccs.data[i]->dest, writeset);
		}

		if (dccs.data[i]->connected && ! dccs.data[i]->src_wa) {
			FD_SET(dccs.data[i]->src, writeset);
		}
	}
} /* void dcc_socketsubscribe(fd_set *readset, fd_set *writeset) */



void
dcc_socketcheck(fd_set *readset, fd_set *writeset)
{
	int		i;
	int		sockopt;
	socklen_t	len;
	int		socksave;
	char		*host;

	for (i = 0; i < dccs.amount; i++) {
		if (dccs.data[i] == NULL) {
			continue;
		}

		/* writesets */
		if (dccs.data[i]->connected &&
				FD_ISSET(dccs.data[i]->src, writeset)) {
			dccs.data[i]->src_wa = 1;
#ifdef DEBUG
			printf("src is now writeable\n");
#endif
		}

		if (dccs.data[i]->dest &&
				FD_ISSET(dccs.data[i]->dest, writeset)) {
			if (! dccs.data[i]->connected) {
				len = sizeof(int);
				getsockopt(dccs.data[i]->dest, SOL_SOCKET,
						SO_ERROR, &sockopt, &len);
				if (sockopt) {
					error(DCC_ERRCONNECT, strerror(sockopt),
							i);
					dcc_killbounce(i);
					continue;
				}
				socksave = dccs.data[i]->src;

				dccs.data[i]->src = sock_accept(socksave,
						&host, 0);
				if (dccs.data[i]->src <= 0) {
					error(DCC_ERRACCEPT, net_errstr, i);
					dcc_killbounce(i);
					continue;
				}
				rawsock_close(socksave);

				dccs.data[i]->connected = 1;
				report(DCC_SUCCESS, host, i);
				xfree(host);
				continue;
			}

			else {
				dccs.data[i]->dest_wa = 1;
#ifdef DEBUG
				printf("dest is now writeable\n");
#endif
			}
		}

		/* readsets */
		if (FD_ISSET(dccs.data[i]->src, readset)) {
			if (! dccs.data[i]->connected ) {
				/* We're still waiting for a connection. */
				/*
				 * Incoming! ...but don't accept it, try to
				 * establish the other connection first !
				 */
				if (! dccs.data[i]->dest) {
					/* Maybe we got that already. */
					if ((dccs.data[i]->dest = sock_open())
							< 0) {
						error(DCC_ERRSOCK, net_errstr,
								i);
						dcc_killbounce(i);
						continue;
					}
					sock_setnonblock(dccs.data[i]->dest);

					if (connect(dccs.data[i]->dest, (struct sockaddr *) &dccs.data[i]->destaddr, sizeof(struct sockaddr)) < 0 && (errno != EINPROGRESS)) {
						error(DCC_ERRCONNECT,
								net_errstr, i);
						dcc_killbounce(i);
						continue;
					}
				}
			}
			else {
				/* all connected */
#ifdef DEBUG
				printf("bouncing from src\n");
#endif
				if (! dcc_bouncedata(dccs.data[i]->src,
							dccs.data[i]->dest,
							&dccs.data[i]->dest_wa
							)) {
					report(DCC_END, i);
					dcc_killbounce(i);
					continue;
				}
			}
		}

		if (dccs.data[i]->connected &&
				FD_ISSET(dccs.data[i]->dest, readset)) {
#ifdef DEBUG
			printf("bouncing from dest\n");
#endif
			if (! dcc_bouncedata(dccs.data[i]->dest,
						dccs.data[i]->src,
						&dccs.data[i]->src_wa)) {
				report(DCC_END, i);
				dcc_killbounce(i);
				continue;
			}
		}
	} /* for */
} /* void dcc_socketcheck(fd_set *readset, fd_set *writeset) */



char *
dcc_initiate(char *param, size_t dsize, int fromclient)
{
	dcccommand	dcc;
	char		*dparam, *chop, *check;
	int		i;

	memset((void *) &dcc, 0, sizeof(dcccommand));

	dparam = xstrdup(param);
	chop = dparam;

	dcc.fromclient = fromclient;

#define DCCINITNULLRETURN { xfree(dparam); return NULL; }

	while ((chop = strchr(chop, ' ')) != NULL && *(++chop) != '\0') {
		switch (dcc.argc) {
			case 0:
				xstrncpy(dcc.type, chop, 15);
				if (strchr(dcc.type, ' ')) {
					*strchr(dcc.type, ' ') = 0;
				}
				dcc.type[15] = 0;
				upcase(dcc.type);
				break;
			case 1:
				dcc.arg1 = chop;
				break;
			case 2:
				*(chop - 1) = 0;
			case 3:
			case 4:
				dcc.args[dcc.argc - 2] =
					strtoul(chop, &check, 10);
				if (check == chop) {
					DCCINITNULLRETURN;
				}
				break;
		}
		dcc.argc++;
	}

	if (dcc.argc < 4) {
		DCCINITNULLRETURN;
	}

#ifdef DEBUG
	printf("DCC %d [%s][%s][%u][%u][%u]\n",
			dcc.argc, dcc.type, dcc.arg1,
			dcc.args[0], dcc.args[1], dcc.args[2]);
#endif

	for (i = 0; dcccalls[i].cmd; i++) {
		if (xstrcmp(dcccalls[i].cmd, dcc.type) == 0) {
			if (dcccalls[i].func(param, dsize, &dcc) == 0) {
				DCCINITNULLRETURN;
			}
			break;
		}
	}

	xfree(dparam);

	return param;;
} /* char *dcc_initiate(char *param, size_t n, int fromclient) */



static int
dcc_realinitiate(char *dest, size_t dsize, dcccommand *dcc)
{
	unsigned int	address, port;
	struct hostent	*host;
	char		*hostname;
	unsigned int	myport;
	int		i, dccindex;

	address = htonl(dcc->args[0]);
	port = dcc->args[1];

	if (dcc->fromclient == 0 && cfg.dccbindhost != NULL) {
		host = name_lookup(cfg.dccbindhost);
	}
	else {
		if (cfg.bind != NULL) {
			host = name_lookup(cfg.bind);
		}
		else {
			hostname = (char *) xmalloc(256);
			if (gethostname(hostname, 255)) {
				xfree(hostname);
				return 0;
			}
			host = name_lookup(hostname);
			xfree(hostname);
		}
	}

	/* TODO take care of host being NULL */

	dccindex = dcc_addbounce();
	dccs.data[dccindex]->src = sock_open();
	if (dccs.data[dccindex]->src < 0) {
		error(SOCK_ERROPEN, net_errstr);
		dcc_killbounce(dccindex);
		return 0;
	}

	sock_setnonblock(dccs.data[dccindex]->src);

	i = 15;
	do {
		myport = (random() & 0xffff) | 1024;
	} while (! sock_bind(dccs.data[dccindex]->src, NULL, myport) && --i);

	if (! i) {
		error(SOCK_GENERROR, "unable to bind to any port");
		dcc_killbounce(dccindex);
		return 0;
	}

	dccs.data[dccindex]->srcport = myport;

	if (! sock_listen(dccs.data[dccindex]->src)) {
		error(SOCK_ERRLISTEN);
		dcc_killbounce(dccindex);
		return 0;
	}

	if (dcc->argc == 5) {
		snprintf(dest, dsize, "\1DCC %s %s %u %u %u\1",
				dcc->type, dcc->arg1,
				(unsigned int)
				ntohl(*(unsigned long int *) host->h_addr),
				myport, dcc->args[2]);
	} else {
		snprintf(dest, dsize, "\1DCC %s %s %u %u\1",
				dcc->type, dcc->arg1,
				(unsigned int)
				ntohl(*(unsigned long int *) host->h_addr),
				myport);
	}
	dest[dsize - 1] = '\0';

	host = name_lookup(inet_ntoa(*(struct in_addr *) &address));
	memcpy((char *) &dccs.data[dccindex]->destaddr.sin_addr, host->h_addr,
			host->h_length);
	dccs.data[dccindex]->destaddr.sin_port = htons((u_short) port);
	dccs.data[dccindex]->destaddr.sin_family = (short) host->h_addrtype;

	report(DCC_START, inet_ntoa(*(struct in_addr *) &address), port,
			dccindex);

	return 1;
} /* static int dcc_realinitiate(char *dest, size_t dsize, dcccommand *dcc) */



static int
dcc_resume(char *dest, size_t dsize, dcccommand *dcc)
{
	int	i;
	int	resume;

	resume = xstrcmp(dcc->type, "RESUME") == 0;

	for (i = 0; i < dccs.amount; i++) {
		if (dccs.data[i] == NULL) {
			continue;
		}

		/* RESUME */
		if (resume) {
			if (dccs.data[i]->srcport == dcc->args[0]) {
				snprintf(dest, dsize, "\1DCC RESUME %s %u %u\1",
						dcc->arg1,
						ntohs(dccs.data[i]->destaddr.sin_port),
						dcc->args[1]);
				dest[dsize - 1] = '\0';
				return 1;
			}
		}
		/* ACCEPT */
		else {
			if (ntohs(dccs.data[i]->destaddr.sin_port) ==
					dcc->args[0]) {
				snprintf(dest, dsize, "\1DCC ACCEPT %s %u %u\1",
						dcc->arg1,
						dccs.data[i]->srcport,
						dcc->args[1]);
				dest[dsize - 1] = '\0';
				return 1;
			}
		}
	}

	return 0;
} /* static int dcc_resume(char *dest, size_t dsize, dcccommand *dcc) */



#endif /* ifdef DCCBOUNCE */
