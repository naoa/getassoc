/*	$Id: xstem.c,v 1.26 2011/09/14 03:06:09 nis Exp $	*/

/*-
 * Copyright (c) 2006 Shingo Nishioka.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#if ! defined lint
static char rcsid[] = "$Id: xstem.c,v 1.26 2011/09/14 03:06:09 nis Exp $";
#endif

#include <config.h>

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <sys/un.h>

#include <netinet/in.h>
#include <netinet/tcp.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <syslog.h>
#include <unistd.h>

#include "nwam.h"

#include "util.h"
#include "nio.h"

#include "xstem.h"

#include <gifnoc.h>

/* #define DBG	1 /* */

#define	READN(d, p, l, t, f) \
	((t) ?  \
	recvn((d), (p), (l), (f)) : \
	readn((d), (p), (l)))

#define	WRITEN(d, p, l, t, f) \
	((t) ?  \
	sendn((d), (p), (l), (f)) : \
	writen((d), (p), (l)))

#if defined HAVE_WSASTARTUP
#define	CLOSE(d, t) \
	((t) ? \
	closesocket((d)) : \
	close((d)))
#else
#define	CLOSE(d, t) close((d))
#endif

struct xstems {
	int flags;
	int fds[2];
	char *sc;
	size_t size;
};

struct xstems *
xstem_create(host, serv, localsocket, stemmer)
char const *host;
char const *serv;
char const *localsocket;
char const *stemmer;
{
	struct xstems *xp;
	int s;
#if defined TCP_NODELAY
	int one = 1;
#endif

	if (!(xp = calloc(1, sizeof *xp))) {
		perror("calloc");
		return NULL;
	}

	xp->sc = NULL;
	xp->size = 0;

	if ((s = ga_nnet_cli(host, serv, localsocket)) == -1) {
		fprintf(stderr, "ga_nnet_cli: %s:%s:%s %s", host ? host : "(null)", serv ? serv : "(null)", localsocket ? localsocket : "(null)", strerror(errno));
		return NULL;
	}
	xp->fds[0] = xp->fds[1] = s;
	xp->flags = XSTEM_SOCKET;

#if defined TCP_NODELAY
	(void)setsockopt(xp->fds[1], IPPROTO_TCP, TCP_NODELAY, (char *)&one, sizeof one);
#endif

	if (xstem_send(xp, stemmer) == -1) {
		fprintf(stderr, "xstem_send: %s", strerror(errno));
		CLOSE(xp->fds[0], xp->flags & XSTEM_SOCKET);
		free(xp);
		return NULL;
	}
	return xp;
}

struct xstems *
xstem_create_pipe(fds, stemmer, flags)
int fds[2];
char const *stemmer;
{
	struct xstems *xp;

/*fprintf(stderr, "xstem_create_pipe: %d %d\n", fds[0], fds[1]);*/

	if (!(xp = calloc(1, sizeof *xp))) {
		return NULL;
	}

	xp->sc = NULL;
	xp->size = 0;

	xp->fds[0] = fds[0];
	xp->fds[1] = fds[1];
	xp->flags = flags;

	if (xp->fds[1] == -1) {
		/* read only */
		return xp;
	}

	if (xstem_send(xp, stemmer) == -1) {
		fprintf(stderr, "xstem_send: %s", strerror(errno));
		if (xp->fds[0] != -1) {
/*fprintf(stderr, "xstem_create_pipe: close %d\n", xp->fds[0]);*/
			CLOSE(xp->fds[0], xp->flags & XSTEM_SOCKET);
		}
		if (xp->fds[1] != xp->fds[0]) {
/*fprintf(stderr, "xstem_create_pipe: close %d\n", xp->fds[1]);*/
			CLOSE(xp->fds[1], xp->flags & XSTEM_SOCKET);
		}
		free(xp);
		return NULL;
	}
	return xp;
}

char *
xstem(xp, src)
struct xstems *xp;
char const *src;
{
	if (xstem_send(xp, src) == -1) {
		return NULL;
	}
	return xstem_recv(xp);
}

int
xstem_send(xp, src)
struct xstems *xp;
char const *src;
{
	char length[XSTEM_HDR_LEN];
	ssize_t l;

#if defined DBG
char msg[64];
snprintf(msg, sizeof msg, "xstem_send (%d) start\n", xp->fds[1]); fputs(msg, stderr);
#endif
	if (xp->fds[1] == -1) {
		/* read only */
		return -1;
	}

	l = strlen(src);
	snprintf(length, sizeof length, "%"PRIdSSIZE_T, (pri_ssize_t)l);
#if defined DBG
snprintf(msg, sizeof msg, "xstem_send (%d) length = %s\n", xp->fds[1], length); fputs(msg, stderr);
#endif
	if (WRITEN(xp->fds[1], length, sizeof length, xp->flags & XSTEM_SOCKET, 0) != sizeof length) {
		fprintf(stderr, "writen: %s", strerror(errno));
		return -1;
	}
	if (WRITEN(xp->fds[1], src, l, xp->flags & XSTEM_SOCKET, 0) != l) {
		fprintf(stderr, "writen: %s", strerror(errno));
		return -1;
	}
#if defined DBG
snprintf(msg, sizeof msg, "xstem_send (%d) done\n", xp->fds[1]); fputs(msg, stderr);
#endif
	return 0;
}

char *
xstem_recv(xp)
struct xstems *xp;
{
	char length[XSTEM_HDR_LEN];
	ssize_t l;

#if defined DBG
char msg[64];
snprintf(msg, sizeof msg, "xstem_recv (%d) start\n", xp->fds[0]); fputs(msg, stderr);
#endif
	if (xp->fds[0] == -1) {
		/* write only */
		return NULL;
	}

	if (READN(xp->fds[0], length, sizeof length, xp->flags & XSTEM_SOCKET, 0) != sizeof length) {
		fprintf(stderr, "readn-(%d): %s\n", xp->fds[0], strerror(errno));
		return NULL;
	}
	length[sizeof length - 1] = '\0';
#if defined DBG
snprintf(msg, sizeof msg, "xstem_recv (%d) length = %s\n", xp->fds[0], length); fputs(msg, stderr);
#endif
	l = strtol(length, NULL, 10);
	if (xp->size <= l) {
		void *new;
		size_t newsize = l;
		newsize = NA(newsize + 1, 32);
		if (!(new = realloc(xp->sc, newsize))) {
			fprintf(stderr, "realloc*: %s", strerror(errno));
			return NULL;
		}
		xp->size = newsize;
		xp->sc = new;
	}
	if (l != 0) {
		if (READN(xp->fds[0], xp->sc, l, xp->flags & XSTEM_SOCKET, 0) != l) {
			fprintf(stderr, "readn+(%d): %s", xp->fds[0], strerror(errno));
			return NULL;
		}
	}
	xp->sc[l] = '\0';
#if defined DBG
snprintf(msg, sizeof msg, "xstem_recv (%d) done\n", xp->fds[0]); fputs(msg, stderr);
#endif

	return xp->sc;
}

int
xstem_destroy(xp)
struct xstems *xp;
{
	char length[XSTEM_HDR_LEN];

	if (xp->fds[1] != -1) {
		snprintf(length, sizeof length, "%d", -1);
		if (WRITEN(xp->fds[1], length, sizeof length, xp->flags & XSTEM_SOCKET, 0) != sizeof length) {
			fprintf(stderr, "writen: %s", strerror(errno));
			if (!(xp->flags & XSTEM_NOCLOSE) && xp->fds[0] != -1) {
/*fprintf(stderr, "xstem_destroy: close %d\n", xp->fds[0]);*/
				CLOSE(xp->fds[0], xp->flags & XSTEM_SOCKET);
			}
			if (!(xp->flags & XSTEM_NOCLOSE) && xp->fds[1] != xp->fds[0]) {
/*fprintf(stderr, "xstem_destroy: close %d\n", xp->fds[1]);*/
				CLOSE(xp->fds[1], xp->flags & XSTEM_SOCKET);
			}
			return -1;
		}
	}
	if (xp->flags & XSTEM_SOCKET) {
		shutdown(xp->fds[0], SHUT_RDWR);
	}
	if (!(xp->flags & XSTEM_NOCLOSE) && xp->fds[0] != -1) {
/*fprintf(stderr, "xstem_destroy: close %d\n", xp->fds[0]);*/
		CLOSE(xp->fds[0], xp->flags & XSTEM_SOCKET);
	}
	if (!(xp->flags & XSTEM_NOCLOSE) && xp->fds[1] != -1 && xp->fds[1] != xp->fds[0]) {
/*fprintf(stderr, "xstem_destroy: close %d\n", xp->fds[1]);*/
		CLOSE(xp->fds[1], xp->flags & XSTEM_SOCKET);
	}
	free(xp->sc);
	free(xp);
	return 0;
}
