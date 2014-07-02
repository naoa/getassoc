/*	$Id: common.c,v 1.48 2011/09/14 03:06:08 nis Exp $	*/

/*-
 * Copyright (c) 2005 Shingo Nishioka.
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
static char rcsid[] = "$Id: common.c,v 1.48 2011/09/14 03:06:08 nis Exp $";
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

#include "cxml.h"
#include "xstem.h"

#include "common.h"

#include <gifnoc.h>

static int ecompar(const void *, const void *);

extern char *getaroot;

char *
vec2jsvec(w, dir, v)
WAM *w;
struct xr_vec const *v;
{
	char *s;
	df_t i;
	size_t size, l;
	struct escapedq_t es;

	es.b = NULL;
	es.l = 0;

	size = 24;
	if (!(s = malloc(size))) {
		syslog(LOG_DEBUG, "malloc: %s", strerror(errno));
		return NULL;
	}
	strcpy(s, "[");	/* good */
	l = strlen(s);
	for (i = 0; i < v->elem_num; i++) {
		struct xr_elem const *e;
		char const *name;
		e = &v->elems[i];
		if (name = wam_id2name(w, dir, e->id)) {
			size_t elen;
			char *p;
			if (!(p = escapedq(name, &es))) {
				free(es.b);
				free(s);
				return NULL;
			}
			elen = 5 + strlen(p) + 24 + 1 + 1 + 1;
			if (size <= l + elen) {
				void *new;
				size_t newsize = l + elen;
				newsize = NA(newsize + 1, 32);
				if (!(new = realloc(s, newsize))) {
					free(s);
					free(es.b);
					syslog(LOG_DEBUG, "realloc: %s", strerror(errno));
					return NULL;
				}
				size = newsize;
				s = new;
			}
			if (i > 0) {
				strcpy(s + l, ",");	/* good */
				l += strlen(s + l);
			}
			snprintf(s + l, size - l, "[\"%s\",%"PRIdFREQ_T"]", p, e->freq);
			l += strlen(s + l);
		}
	}
	strcpy(s + l, "]");	/* good */
	free(es.b);
	return s;
}

char *
escapedq(s, es)
char const *s;
struct escapedq_t *es;
{
	size_t i, j;
	size_t len = strlen(s);

	if (es->l <= len * 2 + 1) {
		void *new;
		size_t newsize = len * 2 + 1 + 1;
		newsize = NA(newsize, 32);
		if (!(new = realloc(es->b, newsize))) {
			syslog(LOG_DEBUG, "realloc: %s", strerror(errno));
			return NULL;
		}
		es->l = newsize;
		es->b = new;
	}

	for (i = j = 0; s[i]; i++) {
		int c = s[i];
		assert(j < es->l);
		if (c == '\\' || c == '"') {
			es->b[j++] = '\\';
		}
		es->b[j++] = c;
	}
	assert(j < es->l);
	es->b[j] = '\0';
	return es->b;
}

df_t
wam_get_vec_byname(w, dir, name, v)
WAM *w;
char const *name;
struct xr_vec const **v;
{
	idx_t id;
	if ((id = wam_name2id(w, dir, name)) == 0) {
		return -1;
	}
	return wam_get_vec(w, dir, id, v);
}

struct xr_vec *
dxr_dup2(oldw, neww, dir, oldv)
WAM *oldw, *neww;
struct xr_vec const *oldv;
{
	struct xr_vec *newv;
	df_t i, j;

	if (!(newv = dxr_dup(oldv))) {
		return NULL;
	}

	newv->freq_sum = 0;
	for (i = j = 0; i < oldv->elem_num; i++) {
		struct xr_elem const *ei = &oldv->elems[i];
		char const *nm;
		idx_t id;
		if (!(nm = wam_id2name(oldw, dir, ei->id))) {
			return NULL;
		}
		if (id = wam_name2id(neww, dir, nm)) {
			struct xr_elem *ej = &newv->elems[j++];
			ej->id = id;
			ej->freq = ei->freq;
			newv->freq_sum += ei->freq;
		}
	}
	newv->elem_num = j;
	qsort(newv->elems, newv->elem_num, sizeof *newv->elems, ecompar);
	return newv;
}

/* XXX: a clone is in dwsys.c */
static int
ecompar(va, vb)
void const *va, *vb;
{
	struct xr_elem const *a, *b;
	a = va;
	b = vb;
	if (a->id < b->id) {
		return -1;
	}
	else if (a->id > b->id) {
		return 1;
	}
	return 0;
}

struct syminfo *
syminfo_dup2(oldw, neww, dir, oldv, np)
WAM *oldw, *neww;
struct syminfo const *oldv;
df_t *np;
{
	struct syminfo *newv;
	df_t i, j, n;

	n = *np;

	if (!(newv = malloc(n * sizeof *newv))) {
		return NULL;
	}

	for (i = j = 0; i < n; i++) {
		struct syminfo const *ei = &oldv[i];
		if (ei->v) {
			struct syminfo *ej = &newv[j++];
			*ej = *ei;
		}
		else {
			char const *nm;
			idx_t id;
			if (!(nm = wam_id2name(oldw, dir, ei->id))) {
				return NULL;
			}
			if (id = wam_name2id(neww, dir, nm)) {
				struct syminfo *ej = &newv[j++];
				*ej = *ei;
				ej->id = id;
			}
		}
	}
	*np = j;
	/* syminfo may not be sorted. just keep original order. */
	return newv;
}

struct element *
read_catalogue()
{
	struct document *c;
	struct element *r;
	char path[MAXPATHLEN];
	FILE *f;
	size_t content_size;

	snprintf(path, sizeof path, "%s" DIRECTORY_DELIMITER ETCDIR DIRECTORY_DELIMITER CATALOGUE_XML, getaroot);

	if (!(f = fopen(path, "rb"))) {
		syslog(LOG_DEBUG, "%s: %s", path, strerror(errno));
		return NULL;
	}

	fseek(f, 0L, SEEK_END);
	content_size = ftell(f);
	rewind(f);

	if (!(c = xml2cxml(f, path, content_size))) {
		return NULL;
	}

	if (c->element->type != CElem || strcmp(c->element->name, "catalogue")) {
		return NULL;
	}

	r = c->element;
	c->element = NULL;
	free_document(c);
	fclose(f);
	return r;
}

df_t
csalen_sum(c, nc)
struct cs_elem *c;
df_t nc;
{
	df_t i, s;
	for (i = s = 0; i < nc; i++) {
		s += c[i].csa.n;
	}
	return s;
}

int
mapcsa(c, nc, fn, cookie)
struct cs_elem *c;
df_t nc;
int (*fn)(struct cs_elem *, df_t, struct syminfo *, df_t, void *, df_t);
void *cookie;
{
	df_t i, k;

	for (i = k = 0; i < nc; i++) {
		struct cs_elem *e = &c[i];
		df_t j;
		for (j = 0; j < e->csa.n; j++) {
			if ((*fn)(e, i, &e->csa.s[j], j, cookie, k) == -1) {
				return -1;
			}
			k++;
		}
	}
	return 0;
}

char *datadir_getassoc = NULL;

int
send_file_datadir_relative(rpath, out, type)
char const *rpath, *type;
{
	char path[MAXPATHLEN];
	struct stat sb;
	int textp;
	struct mapfile_t *m = NULL;
	int e = 0;
	char buf[1024];	/** XXX **/

	if (!datadir_getassoc) {
		syslog(LOG_DEBUG, "!datadir_getassoc");
		return -1;
	}

	snprintf(path, sizeof path, "%s" DIRECTORY_DELIMITER "%s", datadir_getassoc, rpath);

	if (stat(path, &sb) == -1) {
		syslog(LOG_DEBUG, "%s: %s", path, strerror(errno));
		return -1;
	}
	textp = !strncmp(type, "text/", 5);

	if (!(m = mapfile(path))) {
		return -1;
	}
	snprintf(buf, sizeof buf, "Content-Type: %s\r\n", type);
	nputs(buf, out);
	e = (*(textp ? send_text_content : send_binary_content))(&out, NULL, m->ptr, m->size, 1);
	mapfile_destroy(m);
	return e;
}
