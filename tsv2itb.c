/*	$Id: tsv2itb.c,v 1.18 2011/10/24 06:31:53 nis Exp $	*/

/*-
 * Copyright (c) 2008 Shingo Nishioka.
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
static char rcsid[] = "$Id: tsv2itb.c,v 1.18 2011/10/24 06:31:53 nis Exp $";
#endif

#include <config.h>

#include <sys/param.h>
#include <sys/time.h>

#include <assert.h>
#include <ctype.h>
#include <getopt.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <unicode/utf.h>
#include <unicode/utf8.h>

#include "util.h"

#include "csv.h"

#include <gifnoc.h>

#if ! defined MIN
#define	MIN(a, b)	((a) < (b) ? (a) : (b))
#endif

#define	ST_HEADER	1
#define	ST_BODY		2

struct state {
	size_t hsize, iidx;
	char **h;
	int state;
};

static void usage(void);
static int tsv_procline(struct state *, char **, size_t, FILE *, size_t, size_t);
static int filbuf_utf16(struct csvstream *);
static void stripv(char **, size_t);
static void chopvc(char **, size_t, int);

char *progname = NULL;

static void
usage()
{
	fprintf(stderr, "usage: %s [-o out] tsvfile.txt\n", progname);
	exit(1);
}

int
main(argc, argv)
char *argv[];
{
	int ch;
	char *inf, *outf = NULL;
	FILE *in, *out;

	struct state g0, *g = &g0;
	struct csvstate *s;
	char **v;
	size_t nv;
	size_t ll = 0;

	progname = basename(argv[0]);

	while ((ch = getopt(argc, argv, "o:")) != -1) {
		switch (ch) {
		case 'o':
			outf = optarg;
			break;
		default:
			usage();
			break;
		}
	}

	argc -= optind;
	argv += optind;

	if (argc != 1) {
		usage();
		return 1;
	}
	inf = argv[0];

	if (!(in = fopen(inf, "rb"))) {
		perror(inf);
		return 1;
	}
	if (outf) {
		if (!(out = fopen(outf, "wb"))) {
			perror(outf);
			return 1;
		}
	}
	else {
		out = stdout;
	}

	g->hsize = -1;
	g->h = NULL;
	g->iidx = -1;

	g->state = ST_HEADER;

	if (!(s = csvinit(in))) {
		perror("csvinit");
		return 1;
	}
	csvsetsep(s, '\t');
	csvsetfilbuf(s, filbuf_utf16);
	while ((nv = csvgetline(s, &v)) != -2) {
		size_t pl = csvtellplineno(s);
		ll++;
		if (nv == -1) {
			fprintf(stderr, "error: csvgetline failed\n");
			return 1;
		}
		if (tsv_procline(g, v, nv, out, ll, pl) == -1) {
			return 1;
		}
	}
	csvfree(s);
	return 0;
}

static int
tsv_procline(g, v, nv, stream, ll, pl)
struct state *g;
char **v;
size_t nv;
FILE *stream;
size_t ll, pl;
{
	size_t i;

	stripv(v, nv);
	for (i = 0; i < nv; i++) {
		if (*v[i]) {
			goto not_empty;
		}
	}
	return 0;	/* empty */

not_empty:

	switch (g->state) {
	case ST_HEADER:
		if (*v[0] == '@' || *v[0] == '$') {
			chopvc(v, nv, ' ');
			if (nv < 2) {
				fprintf(stderr, "error: too few fields at line %"PRIuSIZE_T" (%"PRIuSIZE_T")\n", (pri_size_t)ll, (pri_size_t)pl);
				return -1;
			}
			fprintf(stream, "%s=%s\n", v[0], v[1]);
		}
		else {
			chopvc(v, nv, '\0');
			g->hsize = nv;
			if (!(g->h = calloc(g->hsize, sizeof *g->h))) {
				perror("calloc");
				return -1;
			}
			g->iidx = -1;
			for (i = 0; i < g->hsize; i++) {
				if (!(g->h[i] = strdup(v[i]))) {
					perror("strdup");
					return -1;
				}
				if (!strcmp(g->h[i], "i")) {
					if (g->iidx != -1) {
						fprintf(stderr, "error: too many \"i\"s at line %"PRIuSIZE_T" (%"PRIuSIZE_T")\n", (pri_size_t)ll, (pri_size_t)pl);
						return -1;
					}
					g->iidx = i;
				}
			}
			if (g->iidx == -1) {
				fprintf(stderr, "error: missing \"i\" at line %"PRIuSIZE_T" (%"PRIuSIZE_T")\n", (pri_size_t)ll, (pri_size_t)pl);
				return -1;
			}
			g->state = ST_BODY;
		}
		break;
	case ST_BODY:
		chopvc(v, nv, ' ');
		if (nv > g->iidx && *v[g->iidx]) {
			fprintf(stream, "i%s\n", v[g->iidx]);
		}
		else {
			break;
		}
		nv = MIN(nv, g->hsize);
		for (i = 0; i < nv; i++) {
			if (i == g->iidx) {
				continue;
			}
			if (*v[i] || *g->h[i] == '!') {
				if (*g->h[i] == '!') {
					fprintf(stream, "%s%s\n", g->h[i], v[i]);
				}
				else if (*g->h[i]) {
					fprintf(stream, "%s=%s\n", g->h[i], v[i]);
				}
				/* else do nothing */
			}
		}
		for (; i < g->hsize; i++) {
			if (i == g->iidx) {
				continue;
			}
			if (*g->h[i] == '!') {
				fprintf(stream, "%s\n", g->h[i]);
			}
		}
		break;
	default:
		fprintf(stderr, "internal error\n");
		return -1;
	}

	return 0;
}

#define	BE	0
#define	LE	1

static int
filbuf_utf16(s)
struct csvstream *s;
{
	int c;
	unsigned int hi, lo, u;
	int l;
	UBool isError;

	if (s->iseof) {
		return 0;
	}
filbuf_utf16:
	if ((c = getc(s->stream)) == EOF) {
		s->iseof = 1;
		return 0;
	}
	hi = c & 0xff;
	if ((c = getc(s->stream)) == EOF) {
		s->iseof = 1;
		return 0;
	}
	lo = c & 0xff;

	if (s->p == NULL) {
		if (hi == 0xff && lo == 0xfe) {
			s->enc = LE;
			goto filbuf_utf16;
		}
		else if (hi == 0xfe && lo == 0xff) {
			s->enc = BE;
			goto filbuf_utf16;
		}
	}

	u = s->enc == BE ? hi << 8 | lo : lo << 8 | hi;

	if (0xd800 <= u && u < 0xdc00) {
		unsigned int u2;
		if ((c = getc(s->stream)) == EOF) {
			s->iseof = 1;
			return 0;
		}
		hi = c & 0xff;
		if ((c = getc(s->stream)) == EOF) {
			s->iseof = 1;
			return 0;
		}
		lo = c & 0xff;
		u2 = s->enc == BE ? hi << 8 | lo : lo << 8 | hi;
		if (!(0xdc00 <= u2 && u2 < 0xe000)) {
			s->p = s->e = s->b;
			return 0;
		}
		u = ((u & 0x03ff) << 10 | (u2 & 0x03ff)) + 0x10000;
	}

	isError = 0;
	l = 0;
	U8_APPEND((uint8_t *)s->b, l, sizeof s->b, u, isError);
	if (isError) {
		s->iseof = 1;
		return 0;
	}
	s->p = s->b;
	s->e = s->b + l;
	return 0;
}

static void
stripv(v, nv)
char **v;
size_t nv;
{
	int i;
	for (i = 0; i < nv; i++) {
		char *p, *q;
		for (p = v[i]; *p && isspace(*p & 0xff); p++) ;
		v[i] = p;
		if (!*p) {
			continue;
		}
		for (q = p + strlen(p) - 1; q > p && isspace(*q & 0xff); q--) ;
		*(q + 1) = '\0';
	}
}

static void
chopvc(v, nv, c)
char **v;
size_t nv;
{
	int i;
	for (i = 0; i < nv; i++) {
		char *p;
		for (p = v[i]; *p; p++) {
			if (*p == '\r' || *p == '\n') {
				*p = c;
			}
		}
	}
}
