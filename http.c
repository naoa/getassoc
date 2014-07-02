/*	$Id: http.c,v 1.5 2011/09/14 03:06:08 nis Exp $	*/

/*-
 * Copyright (c) 2009 Shingo Nishioka.
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
static char rcsid[] = "$Id: http.c,v 1.5 2011/09/14 03:06:08 nis Exp $";
#endif

#include <config.h>

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/time.h>

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <syslog.h>
#include <unistd.h>

#include "nwam.h"
#include "util.h"
#include "nio.h"

#include <gifnoc.h>

static ssize_t writen_deref(void *, const void *, size_t);

int
send_binary_content(cookie, writefn, ptr, size, hdrp)
void *cookie;
ssize_t (*writefn)(void *, const void *, size_t);
char const *ptr;
size_t size;
{
	if (!writefn) {
		writefn = writen_deref;
	}
	if (hdrp) {
		char buf[1024];	/** XXX **/
		snprintf(buf, sizeof buf, "Content-Length: %"PRIuSIZE_T"\r\n"
#if defined CRLFTEXT
			"Content-Transfer-Encoding: binary\r\n"
#endif
			"\r\n", (pri_size_t)size);
		(*writefn)(cookie, buf, strlen(buf));
	}
	(*writefn)(cookie, ptr, size);
	return 0;
}

#if defined CRLFTEXT
int
send_text_content(cookie, writefn, ptr, size, hdrp)
void *cookie;
ssize_t (*writefn)(void *, const void *, size_t);
char const *ptr;
size_t size;
{
	char *p, *q, *end;
	size_t l;

	if (!writefn) {
		writefn = writen_deref;
	}
	end = ptr + size;
	if (hdrp) {
		char buf[1024];	/** XXX **/
		size = 0;
		for (p = ptr; p < end; p = q) {
			for (q = p; q < end && *q != '\r' && *q != '\n'; q++) ;
			l = q - p;
			size += l + 2;
			if (*q == '\r' && q + 1 < end && *(q + 1) == '\n') q++;
			if (q < end) q++;
		}
		snprintf(buf, sizeof buf, "Content-Length: %"PRIuSIZE_T"\r\n\r\n", (pri_size_t)size);
		(*writefn)(cookie, buf, strlen(buf));
	}
	for (p = ptr; p < end; p = q) {
		for (q = p; q < end && *q != '\r' && *q != '\n'; q++) ;
		l = q - p;
		if (l) (*writefn)(cookie, p, l);
		(*writefn)(cookie, "\r\n", 2);
		if (*q == '\r' && q + 1 < end && *(q + 1) == '\n') q++;
		if (q < end) q++;
	}
	return 0;
}
#endif

int
http_parse_header(cookie, getline, cl, clen, ct)
void *cookie;
char *(*getline)(char * restrict, int, void *);
char **cl, **ct;
ssize_t *clen;
{
	char buf[4096];

	*cl = *ct = NULL;
	*clen = -1;
	while ((*getline)(buf, sizeof buf, cookie)) {
		char *p;
		if (p = index(buf, '\n')) {
			*p = '\0';
			if (p > buf && *(p - 1) == '\r') {
				*(p - 1) = '\0';
			}
		}
		if (!strncmp(buf, "Content-Length: ", 16)) {
			char *end;
			if (!(*cl = strdup(buf))) {
				return -1;
			}
			*clen = strtol(buf + 16, &end, 10);
			if (!(*(buf + 16) && !*end)) {
				return -1;
			}
		}
		else if (!strncmp(buf, "Content-Type: ", 14)) {
			if (!(*ct = strdup(buf))) {
				return -1;
			}
		}
		else if (!*buf) {
			break;
		}
	}
	return 0;
}

static ssize_t
writen_deref(cookie, ptr, len)
void *cookie;
const void *ptr;
size_t len;
{
	return writen(*(int *)cookie, ptr, len);
}
