/*	$Id: readln.c,v 1.14 2009/08/28 03:53:16 nis Exp $	*/

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
static char rcsid[] = "$Id: readln.c,v 1.14 2009/08/28 03:53:16 nis Exp $";
#endif

#include <config.h>

#include <sys/types.h>
#include <sys/time.h>

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>

#include "util.h"

#include <gifnoc.h>

struct nfile_d {
#if 0
	ssize_t (*readfn)(void *, char *, size_t);
	void *cookie;	/* for readfn */
#endif
	FILE *stream;
	char *r;
	size_t rs;
	int unread;
};

NFILE *
nopen(path, mode)
char const *path, *mode;
{
	NFILE *p;
	FILE *f;

	if (!(f = fopen(path, mode))) {
		return NULL;
	}

	if (!(p = calloc(1, sizeof *p))) {
		fclose(f);
		return NULL;
	}
#if 0
	p->readfn = NULL;
	p->cookie = NULL;
#endif
	p->stream = f;
	p->r = NULL;
	p->rs = 0;
	p->unread = 0;

	return p;
}

NFILE *
nfopen(f)
FILE *f;
{
	NFILE *p;

	if (!(p = calloc(1, sizeof *p))) {
		return NULL;
	}
#if 0
	p->readfn = NULL;
	p->cookie = NULL;
#endif
	p->stream = f;
	p->r = NULL;
	p->rs = 0;
	p->unread = 0;

	return p;
}

#if 0
NFILE *
nffunopen(readfn, cookie)
ssize_t (*readfn)(void *, char *, size_t);
void *cookie;
{
	NFILE *p;

	if (!(p = calloc(1, sizeof *p))) {
		return NULL;
	}
	p->readfn = readfn;
	p->cookie = cookie;
	p->stream = NULL;
	p->r = NULL;
	p->rs = 0;
	p->unread = 0;

	return p;
}
#endif

char *
readln(f)
NFILE *f;
{
	size_t s;
	size_t newsize;
	void *new;
#define	ALCSZ	8192

	if (f->unread) {
		f->unread = 0;
		return f->r;
	}

	if (f->rs == 0) {
		newsize = ALCSZ;
		if (!(new = malloc(newsize))) {
			syslog(LOG_DEBUG, "malloc: %s", strerror(errno));
			return NULL;
		}
		f->rs = newsize;
		f->r = new;
	}
	f->r[0] = '\0';
	for (s = 0; ; ) {
#if 0
		if (f->readfn) {
			ssize_t l;
			l = (*f->readfn)(f->cookie, f->r + s, f->rs - s - 1);
			if (l >= 0) *(f->r + s + l) = '\0';
			if (l <= 0) break;
		}
		else {
#endif
			if (!fgets(f->r + s, f->rs - s, f->stream)) {
				break;
			}
#if 0
		}
#endif

		s += strlen(f->r + s);
		if (s > 0 && *(f->r + s - 1) == '\n') {
			break;
		}
		newsize = f->rs + ALCSZ;
		if (!(new = realloc(f->r, newsize))) {
			syslog(LOG_DEBUG, "realloc: %s", strerror(errno));
			return NULL;
		}
		f->rs = newsize;
		f->r = new;
	}
	return f->r;
}

void
nfclose(f)
NFILE *f;
{
	if (f->stream) {
		fclose(f->stream);
	}
	free(f->r);
	free(f);
}

int
nfunread(f)
NFILE *f;
{
	if (f->unread) {
		return -1;
	}
	f->unread = 1;
	return 0;
}

struct nfn_file {
	void *cookie;
	ssize_t (*readfn)(void *, void *, size_t);
	ssize_t (*writefn)(void *, const void *, size_t);
	int (*closefn)(void *);
	char *b, *p, *e;
	size_t s;
	int iseof;
};

static void fillbuf(struct nfn_file *);

struct nfn_file *
nfn_fdopen(cookie, readfn, writefn, closefn)
void *cookie;
ssize_t (*readfn)(void *, void *, size_t);
ssize_t (*writefn)(void *, const void *, size_t);
int (*closefn)(void *);
{
	struct nfn_file *f;
	if (!(f = calloc(1, sizeof *f))) {
		return NULL;
	}
	f->cookie = cookie;
	f->readfn = readfn;
	f->writefn = writefn;
	f->closefn = closefn;
	f->p = f->e = NULL;
	f->s = 4096;
	if (!(f->b = malloc(f->s))) {
		free(f);
		return NULL;
	}
	f->iseof = 0;
	return f;
}

static void
fillbuf(f)
struct nfn_file *f;
{
	size_t l;
	if (f->iseof) {
		return;
	}
	if ((l = (*f->readfn)(f->cookie, f->b, f->s)) <= 0) {
		f->iseof = 1;
	}
	else {
		f->p = f->b;
		f->e = f->p + l;
	}
}

int
nfn_read_int_arg(g, buf, nbytes)
void *g;
char *buf;
int nbytes;
{
	return nfn_read(g, buf, nbytes);
}

ssize_t
nfn_read(g, buf, nbytes)
void *g;
void *buf;
size_t nbytes;
{
	struct nfn_file *f = g;
	size_t l;
	if (nbytes <= 0) {
		return 0;
	}
	if (f->e <= f->p) {
		fillbuf(f);
		if (f->iseof) {
			return 0;
		}
	}
	assert(f->p < f->e);
	l = MIN(nbytes, f->e - f->p);
	memcpy(buf, f->p, l);
	f->p += l;
	return l;
}

char *
nfn_fgets(str, size, g)
char *str;
void *g;
{
	struct nfn_file *f = g;
	size_t l;

	if (size <= 0) {
		return NULL;
	}
	if (size <= 1) {
		*str = '\0';
		return NULL;
	}
	if (f->e <= f->p) {
		fillbuf(f);
		if (f->iseof) {
			*str = '\0';
			return NULL;
		}
	}
	assert(f->p < f->e);

	for (l = 0; l < size - 1; ) {
		int c;
		c = *f->p++ & 0xff;
		if ((str[l++] = c) == '\n') {
			break;
		}
		if (f->e <= f->p) {
			fillbuf(f);
			if (f->iseof) {
				break;
			}
assert(f->p < f->e);
		}
	}
	str[l] = '\0';

	return str;
}

ssize_t
nfn_write(g, buf, nbytes)
void *g;
const void *buf;
size_t nbytes;
{
	struct nfn_file *f = g;
	return (*f->writefn)(f->cookie, buf, nbytes);
}

int
nfn_fflush(g)
void *g;
{
	return 0;
}

int
nfn_fclose(g)
void *g;
{
	struct nfn_file *f = g;
	int e;
	e = (*f->closefn)(f->cookie);
	free(f->b);
	free(f);
	return e;
}
