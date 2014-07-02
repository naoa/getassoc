/*	$Id: sutil.c,v 1.6 2011/09/14 03:06:09 nis Exp $	*/

/*-
 * Copyright (c) 2007 Shingo Nishioka.
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
static char rcsid[] = "$Id: sutil.c,v 1.6 2011/09/14 03:06:09 nis Exp $";
#endif

#include <config.h>

#include <sys/param.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>

#include <assert.h>
#include <fcntl.h>
#include <libgen.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include <unicode/utf.h>
#include <unicode/utf8.h>

#include "nwam.h"
#include "util.h"

#include "stpP.h"

#include <gifnoc.h>

static int dcompar(void const *, void const *);
static char *string_trim(char *, char const *, size_t, char const *, size_t);
static char *string_append(char const *, char const *, char const *);

int
fn_find_pat_match_tail(path, cookie)
const char *path;
void *cookie;
{
	struct find_pat *f0 = cookie;
	size_t l = strlen(path);
	if (l >= f0->tlen && !strcmp(path + l - f0->tlen, f0->t)) {
		if (f0->size <= f0->n) {
			void *new;
			size_t newsize = f0->n;
			newsize = NA(newsize + 1, 32);
			if (!(new = realloc(f0->p, newsize * sizeof *f0->p))) {
				return 1;
			}
			f0->size = newsize;
			f0->p = new;
		}
		f0->p[f0->n++] = strdup(path);
	}
	return 0;
}

int
findafile_match_tail(root, result, size, suffix)
char const *root;
char *result;
size_t size;
char const *suffix;
{
	size_t j;
	struct find_pat f0;

	f0.n = 0;
	f0.size = 0;
	f0.p = NULL;
	f0.t = suffix;
	f0.tlen = strlen(f0.t);
	if (traverse_fn(root, fn_find_pat_match_tail, &f0)) {
		perror(root);
		return -1;
	}
	if (f0.n == 1) {
		if (strlen(f0.p[0]) >= size) {
			return -1;
		}
		strcpy(result, f0.p[0]);
	}
	else if (f0.n == 0) {
		fprintf(stderr, "%s: not found\n", suffix);
		result[0] = '\0';
	}
	else {
		fprintf(stderr, "%s: too many files found\n", suffix);
		for (j = 0; j < f0.n; j++) {
			fprintf(stderr, "%d: %s\n", (int)j, f0.p[j]);
		}
		result[0] = '\0';
	}
	for (j = 0; j < f0.n; j++) {
		free(f0.p[j]);
	}
	free(f0.p);
	return 0;
}

int
pxsort(p, n, prefix, postfix, ref)
char **p;
size_t n;
char const *prefix, *postfix, *ref;
{
	char key[MAXPATHLEN + 256];
	char **q;
	int *u;
	size_t l_prefix, l_postfix;
	size_t j, k;
	FILE *f;

	l_prefix = strlen(prefix);
	l_postfix = strlen(postfix);

	if (!(q = calloc(n, sizeof *q))) {
		perror("calloc");
		return -1;
	}

	for (j = 0; j < n; j++) {
		if (!(q[j] = string_trim(p[j], prefix, l_prefix, postfix, l_postfix))) {
			fprintf(stderr, "string_trim failed: %s %s %"PRIuSIZE_T" %s %"PRIuSIZE_T"\n", p[j], prefix, (pri_size_t)l_prefix, postfix, (pri_size_t)l_postfix);
			return -1;
		}
		free(p[j]);
	}

	qsort(q, n, sizeof *q, dcompar);

	if (!(u = calloc(n, sizeof *u))) {
		perror("calloc");
		return -1;
	}
	j = 0;
	if (f = fopen(ref, "r")) {
		while (fgets(key, sizeof key, f)) {
			char **e;
			chop(key);
			if (e = bsearch(&key, q, n, sizeof *q, dcompar)) {
				k = e - q;
				if (!u[k]) {
					u[k] = 1;
					p[j++] = q[k];
				}
			}
		}
		fclose(f);
	}
	for (k = 0; k < n; k++) {
		if (!u[k]) {
			p[j++] = q[k];
		}
	}
	assert(j == n);
	free(q);
	free(u);

	for (j = 0; j < n; j++) {
		char *r = p[j];
		if (!(p[j] = string_append(prefix, r, postfix))) {
			return -1;
		}
		free(r);
	}

	return 0;
}

static int
dcompar(va, vb)
void const *va;
void const *vb;
{
	char * const *a = va;
	char * const *b = vb;

	return strcmp(*a, *b);
}

char *
ngetline(buf, size, stream)
char *buf;
size_t size;
FILE *stream;
{
	if (!(fgets(buf, size, stream))) {
		return NULL;
	}
	chop(buf);
	return strdup(buf);
}

void
chop(s)
char *s;
{
	size_t l = strlen(s);

	if (l-- > 0 && s[l] == '\n') {
		s[l] = '\0';
		if (l-- > 0 && s[l] == '\r') {
			s[l] = '\0';
		}
	}
}

static char *
string_trim(s, prefix, l_prefix, postfix, l_postfix)
char *s;
char const *prefix, *postfix;
size_t l_prefix, l_postfix;
{
	char *r;
	size_t l = strlen(s);
	size_t newl;
	if (l_prefix + l_postfix >= l) {
		return NULL;
	}
	newl = l - (l_prefix + l_postfix);

	if (strncmp(s, prefix, l_prefix)) {
		return NULL;
	}
	if (strcmp(s + l - l_postfix, postfix)) {
		return NULL;
	}

	if (!(r = malloc(newl + 1))) {
		return NULL;
	}
	memmove(r, s + l_prefix, newl);
	r[newl] = '\0';
	return r;
}

static char *
string_append(a, b, c)
char const *a, *b, *c;
{
	char buf[MAXPATHLEN];

	snprintf(buf, sizeof buf, "%s%s%s", a, b, c);
	return strdup(buf);
}

#if ! defined HAVE_MKDTEMP
char *
mkdtemp(tpl)
char *tpl;
{
	int d;
	if ((d = mkstemp(tpl)) == -1) {
		return NULL;
	}
	close(d);
	unlink(tpl);
	if (mkdir(tpl, 0700) == -1) {
		return NULL;
	}
	return tpl;
}
#endif

int
is_sound_handle_name(s)
char const *s;
{
	unsigned int c;
	c = *s & 0xff;
	if (!(040 < c && c != '.' && c != '/' && c != '\\' && c < 0177)) {
		return 0;
	}

	for (s++; *s; s++) {
		c = *s & 0xff;
		if (!(040 < c && c != '/' && c != '\\' && c < 0177)) {
			return 0;
		}
	}
	return 1;
}

/* see namep */
int
is_sound_prop_name(a)
char const *a;
{
	int first = 1;
	size_t length = strlen(a);
	int i;
	for (i = 0; i < length; ) {
		UChar32 u;
		U8_NEXT(a, i, length, u);
		if (u < 0) {
			fprintf(stderr, "invalid utf-8 encoding\n");
			return 0;
		}
		if (first) {
			if (!(xletterp(u) || u == '_' || u == ':')) {
				fprintf(stderr, "invalid char\n");
				return 0;
			}
			first = 0;
		}
		else {
			if (!(xletterp(u) || xdigitp(u) ||
				u == '.' || u == '-' || u == '_' || u == ':') ||
				xcombiningcharp(u) || xextenderp(u)) {
				fprintf(stderr, "invalid char\n");
				return 0;
			}
		}
	}
	return 1;
}

/* see is_sound_prop_name */
int
namep(s)
char const *s;
{
	int first = 1;
	size_t length = strlen(s);
	int i;
	for (i = 0; i < length; ) {
		UChar32 u;
		U8_NEXT(s, i, length, u);
		if (u < 0) {
			fprintf(stderr, "invalid utf-8 encoding\n");
			return 0;
		}
		if (first) {
			if (!(xletterp(u) || u == '_' || u == ':')) {
				fprintf(stderr, "invalid char\n");
				return 0;
			}
			first = 0;
		}
		else {
			if (!(xletterp(u) || xdigitp(u) ||
				u == '.' || u == '-' || u == '_' || u == ':') ||
				xcombiningcharp(u) || xextenderp(u)) {
				fprintf(stderr, "invalid char\n");
				return 0;
			}
		}
	}

	return 1;
}
