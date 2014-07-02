/*	$Id: nnet.c,v 1.9 2011/09/14 03:06:09 nis Exp $	*/

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
static char rcsid[] = "$Id: nnet.c,v 1.9 2011/09/14 03:06:09 nis Exp $";
#endif

#include <config.h>

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <sys/un.h>

#include <netinet/in.h>
#include <netinet/tcp.h>

#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <syslog.h>
#include <unistd.h>

#include "util.h"

#include <gifnoc.h>

#if ! defined PF_LOCAL
#if defined 	AF_UNIX
#define PF_LOCAL	AF_UNIX
#endif
#endif

#if defined HAVE_WSASTARTUP
#ifdef close
#undef close
#endif
#define	close(d)	closesocket((d))
#endif

int
ga_nnet_cli(host, serv, localsocket)
char const *host;
char const *serv;
char const *localsocket;
{
	int s = -1;

#if ! defined NO_LOCALSOCKET
	do {
		struct sockaddr_un server;
		int sun_len;

		if (!localsocket) {
			continue;
		}

		sun_len = sizeof server.sun_family + strlen(localsocket) + 1;
		if ((s = socket(PF_LOCAL, SOCK_STREAM, 0)) == -1) {
			syslog(LOG_DEBUG, "socket %s: %s", localsocket, strerror(errno));
			continue;
		}
		memset(&server, 0, sizeof server);
		server.sun_family = PF_LOCAL;
#if HAVE_SUN_LEN
		server.sun_len = sun_len;
#endif
		if (strlen(localsocket) >= sizeof server.sun_path) {
			close(s);
			syslog(LOG_DEBUG, "sizecheck %s: %s", localsocket, strerror(errno));
			continue;
		}
		strcpy(server.sun_path, localsocket);
		if (connect(s, (struct sockaddr *)&server, sun_len) == -1) {
			close(s);
			syslog(LOG_DEBUG, "connect %s: %s", localsocket, strerror(errno));
			continue;
		}
		return s;
	} while (0);
#endif

	if (host && serv) {
		struct addrinfo hints, *res, *res0;
		int error;
		const char *cause;
		cause = "";

		hints.ai_family = PF_UNSPEC;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_flags = 0;
		hints.ai_protocol = 0;
		hints.ai_addrlen = 0;
		hints.ai_canonname = NULL;
		hints.ai_addr = NULL;
		hints.ai_next = NULL;

		if (error = getaddrinfo(host, serv, &hints, &res0)) {
			syslog(LOG_DEBUG, "getaddrinfo %s.%s: %s", host, serv, gai_strerror(error));
			return -1;
		}
		for (res = res0; res; res = res->ai_next) {
			if ((s = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) == -1) {
				cause = "socket";
				continue;
			}

			if (connect(s, res->ai_addr, res->ai_addrlen) < 0) {
				cause = "connect";
				close(s);
				s = -1;
				continue;
			}

			break;
		}
		freeaddrinfo(res0);
		if (s == -1) {
			syslog(LOG_DEBUG, "%s %s.%s: %s", cause, host, serv, strerror(errno));
		}
	}
	return s;
}
