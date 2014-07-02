/*	$Id: csv.c,v 1.7 2009/07/30 00:08:14 nis Exp $	*/

/*-
 * Copyright (c) 2004 Shingo Nishioka.
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
static char rcsid[] = "$Id: csv.c,v 1.7 2009/07/30 00:08:14 nis Exp $";
#endif

#include <config.h>

#include <sys/types.h>
#include <sys/time.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util.h"
#include "csv.h"

#include <gifnoc.h>

#define	CRorLF(c)	((c)=='\r' || (c)=='\n')

struct csvstate {
	struct csvstream stream;
	size_t n, fields_size, buf_size;
	char **fields, *buf, sep;
	int *offs;
	size_t offs_size;
	size_t plineno;
};

#define cgetc(s) (((s)->p == (s)->e ? (*(s)->filbuf)(s) : 0),\
		  ((s)->iseof ? EOF : *(s)->p++))
#define cungetc(s) ((s)->p--) /* do not call `cungetc' after encountering EOF or initial state */

static int filbuf(struct csvstream *);

struct csvstate *
csvinit(stream)
FILE *stream;
{
	struct csvstate *s;
	struct csvstream *c;

	if (!(s = calloc(1, sizeof *s))) {
		perror("calloc");
		return NULL;
	}
	c = &s->stream;
	c->stream = stream;
	c->p = c->e = NULL;
	c->filbuf = filbuf;
	c->enc = 0;
	c->iseof = 0;
	s->n = 0;
	s->fields_size = 0;
	s->fields = NULL;
	s->buf_size = 0;
	s->buf = NULL;
	s->offs_size = 0;
	s->offs = NULL;
	s->sep = ',';
	s->plineno = 0;
	return s;
}

void
csvsetsep(s, sep)
struct csvstate *s;
{
	s->sep = sep;
}

void
csvsetfilbuf(s, f)
struct csvstate *s;
int (*f)(struct csvstream *);
{
	s->stream.filbuf = f;
}

size_t
csvtellplineno(s)
struct csvstate *s;
{
	return s->plineno;
}

void
csvfree(s)
struct csvstate *s;
{
	free(s->offs);
	free(s->fields);
	free(s->buf);
	free(s);
}

ssize_t
csvgetline(s, fieldsp)
struct csvstate *s;
char ***fieldsp;
{
	int c;
	size_t i;

#define CHK(P, I, N) \
	if ((N) <= (I)) { \
		void *new; \
		size_t newsize = (I); \
		newsize = NA(newsize + 1, 1024); \
		if (!(new = realloc((P), newsize * sizeof *(P)))) { \
			perror("realloc"); \
			free(P); \
			(P) = NULL; \
			(N) = 0; \
			return -1; \
		} \
		(N) = newsize; \
		(P) = new; \
	}

	c = cgetc(&s->stream);
	if (c == EOF) {
		return -2;
	}

	i = 0;
	for (s->n = 0; ; c = cgetc(&s->stream)) {
		CHK(s->offs, s->n, s->offs_size);
		s->offs[s->n++] = i;
		if (c == '"') {
			c = cgetc(&s->stream);	/* skip '"' */
			for (; c != EOF; c = cgetc(&s->stream)) {
				if (c == '"' && (c = cgetc(&s->stream)) != '"') {
					goto rest;
				}
				CHK(s->buf, i, s->buf_size);
				s->buf[i++] = c;
				if (c == '\n') {
					s->plineno++;
				}
			}
		}
		else {
		rest:
			for (; c != EOF && c != s->sep && !CRorLF(c); c = cgetc(&s->stream)) {
				CHK(s->buf, i, s->buf_size);
				s->buf[i++] = c;
			}
		}
		CHK(s->buf, i, s->buf_size);
		s->buf[i++] = '\0';
		/* c == {EOF, CRorLF, s->sep} */
		if (c != s->sep) {
			break;
		}
		/* c == s->sep */
	}
	/* c == {EOF, CRorLF} */
	if (c == '\r') {
		c = cgetc(&s->stream);
		if (c != '\n' && c != EOF) {
			cungetc(&s->stream);
		}
	}
	s->plineno++;
	CHK(s->fields, s->n, s->fields_size);
	for (i = 0; i < s->n; i++) {
		s->fields[i] = s->buf + s->offs[i];
	}
	s->fields[i] = NULL;
	*fieldsp = s->fields;
	return s->n;
#undef CHK
}

static int
filbuf(s)
struct csvstream *s;
{
	int c;

	if (s->iseof) {
		return 0;
	}
	if ((c = getc(s->stream)) == EOF) {
		s->iseof = 1;
	}
	else {
		*s->b = c;
		s->p = s->b;
		s->e = s->b + 1;
	}
	return 0;
}
